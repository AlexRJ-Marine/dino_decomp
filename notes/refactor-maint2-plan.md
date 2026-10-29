# MAINT-2 plan — split the four remaining long bodies

Date: 2026-10-29. Follow-up to MAINT-1 (`docs/platform/app-shell.md`).
Status: **in progress.** `draw_menu` is done (`ec4d97e`, `7859319`,
`a844904`, `11885fe`); `update_placement`, `handle_input` and
`draw_park_hud` remain. Every step below is a verbatim move plus a call
site — no behaviour change, no signature change beyond receiving the state
the body already used through the `Game` object.

Targets (measured on `5fcd5ce` + `fad10bd`):

| Function | File:line | Lines | Natural split |
| --- | --- | ---: | --- |
| `draw_park_hud` | `game_ui.cpp:535` | 758 | widget text + widget draw + dialogs |
| `handle_input` | `game_input.cpp:7` | 638 | injected input, menu, keys, mouse |
| `update_placement` | `game_input.cpp:646` | 638 | fence cursor, store, build, park |
| `draw_menu` | `game_ui.cpp:106` | 428 | widget draw, list, hscroll, text |

## 1. `draw_park_hud` (758)

Structure today: setup (`hovered`, `dialog_zone`, `is_text_zone`, `dpi`,
`px`/`py`) → draw-order construction (roots + children, ZPRIORITY) → a
200-line `runtime_text` lambda mapping widget names to values → one
435-line `for (const int i : order)` draw loop → campaign text zones.

Order of extraction:

1. `std::string hud_widget_text(const std::string& name)` — the
   `runtime_text` lambda body (607-808). It reads park/economy/clock state
   only; no draw calls. ~200 lines out.
2. `draw_park_hud_widget(int index, ...)` — the current loop body
   (810-1246), taking `order`/`hovered`/`px`/`py` or recomputing them; the
   per-widget behaviour stays one dispatcher.
3. `draw_park_hud_dialogs()` — the `campaign_runtime` text zones (1248-1290).
4. `draw_park_hud` keeps: early-out, `hovered`, draw-order roots/children
   sort and the three calls. Target ≈ 120 lines.

The 435-line widget dispatcher is the hard part: do **not** invent a
dispatch table in this pass. Extract it as step 2 so the entry point is
readable, then split it by widget family in a later pass only if a real
boundary appears (BUDGET/MARKET/lists/dialogs).

## 2. `handle_input` (638)

Structure today: injected key/click parser (8-56) → `no_input` early-out →
menu (`start_screen`) 113-line block → per-event chain
(TEXT_INPUT / KEY_DOWN / three MOUSE_BUTTON_DOWN cases / MOUSE_BUTTON_UP /
MOUSE_MOTION / MOUSE_WHEEL / WINDOW_RESIZED).

Order:

1. `apply_injected_input()` — the `DINO_INPUT`/`--inject-*` parser (8-56),
   returns bool consumed or leaves the event pump. ~50 lines.
2. `handle_menu_input(const SDL_Event&)` — the `start_screen` block (59-171).
3. `handle_key_input(const SDL_Event&)` — the KEY_DOWN chain (182-296).
4. `handle_park_mouse_down(const SDL_Event&)` — the three mouse-down cases
   (296-554). Split by state if it is still long: HUD clicks, park clicks,
   placement clicks.
5. `handle_park_mouse_up`, `handle_park_mouse_motion`,
   `handle_park_mouse_wheel` — the remaining event cases.
6. `handle_input` keeps the pumped-event loop and the dispatch chain; target
   < 150 lines.

Risk: these blocks mutate many members; the `Game` object makes that a
mechanical move. The only care point is the early `if (running)`/`return`
paths — preserve them exactly.

## 3. `update_placement` (638)

Structure today: fence tool arming (648-654) → fence cursor preview
(657-830) → click dispatch: store (831-961), build tool (961-1209), plain
park (1209-1260) → `--pick-test` block (1264-1281).

Order:

1. `update_fence_cursor()` — the pick/preview/rebuild block (657-830).
2. `place_selected_store_item()` — the store branch (831-961).
3. `apply_build_tool_click()` — the build-tool branch (961-1209).
4. `apply_park_click()` — the plain-park branch (1209-1260).
5. `update_placement` keeps `pending_click` gating, the three calls in
   order and the pick-test hook; target < 120 lines.

Risk: the branches share `hit`, `cx`, `cy`, `visible`, `ok` from the pick
block. Pass them as a small struct or keep the pick at the top of
`update_placement` and pass the four values to each handler — do not
duplicate the pick.

## 4. `draw_menu` (428)

Structure today: per-widget loop with island-picture overrides, hover
frame, texture/back-colour quad, HSCROLL (73), LIST (203), text (15).

Order:

1. `draw_menu_widget(std::size_t i)` — the loop body outside the two
   complex types.
2. `draw_menu_hscroll_widget(const dino::ui::Widget&, ...)`.
3. `draw_menu_list_widget(const dino::ui::Widget&, ...)`.
4. `draw_menu` keeps the background, `ui_texture` helper lambdas and the
   loop; target < 100 lines.

**Done** (`ec4d97e`, `7859319`, `a844904`, `11885fe`): `draw_menu` is 26
lines and calls `draw_menu_widget` per overlay widget; the sprite lookup
(`draw_menu_sprite`), the HSCROLL block and the LIST block are private
methods that receive `dpi`/`ox`/`oy`/`view_w`/`view_h` and rebuild the two
one-line `px`/`py` lambdas locally. The widget body was proven textually
verbatim against the pre-move file; only `i` -> `widget_index`,
`continue` -> `return` and the `frame` -> `sprite_frame`/`chosen_frame`
renames changed (a `Game` member is named `frame`, so MAINT-3's shadow
guard requires them). Menu shot: `notes/harness/maint2-menu.bmp`.

Risk: the two helpers need the texture/rect local lambdas
(`draw_graphic`, `ui_frame_rect`); pass the resolved texture/rect or make
the helpers private methods that recompute them. Prefer parameters.

## Gates and order

Per step: build `build/refactor`, `--selftest` (194), the fast suite (350,
plus the headless app tests once MAINT-4 lands). Per target: the full suite
(500) and a park screenshot (`notes/harness/maint2-*.bmp`) compared against
`notes/park-retail-windowed.png` for HUD/menu steps.

Suggested order: `draw_menu` (smallest, UI-only) → `update_placement` →
`handle_input` → `draw_park_hud` (largest, most pixel-sensitive). Commit per
extraction, never a move plus a fix.

Companion guards (MAINT-3) should land first if cheap: the scene-pixel
assertion catches the class of bug that a HUD-only gate misses.
