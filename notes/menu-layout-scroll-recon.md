# Menu layout and scrollbars — evidence and port contract

Status: **list rows and VSCROLL are ported from the retail contract.**
The menu geometry comes from `Data/Overlay/MenuInterface.Ini` (800x600 design
space) and the port's `src/ui/overlay.cpp` parser. The UI click map is
`docs/platform/ui-coordinates.md` / `.json`. The exact VSCROLL/listbox
reverse-engineering is in `notes/ui-scrollbar-recon.md`.

## 1. The "buttons slightly out of position" = list row mapping

The Free Mode island list is not ten equal 28 px slices. Retail defines:

```ini
[ISLAND_LIST]
X = 0, Y = 0, WIDTH = 188, HEIGHT = 285
LB_VSCROLL_WIDTH = 22
LB_LINE_HEIGHT   = 34
LB_NB_COLUMNS    = 1
LB_COLUMN0_WIDTH = 166
```

`ISLAND_LIST_BUTTON` is a native 166x34 frame (`MenuGraphicDB.ini`), and the
UI map click rows are 34 px apart:

| row | click y | island |
| ---: | ---: | --- |
| 0 | 183 | isl01 |
| 1 | 217 | isl02 |
| 2 | 251 | isl03 |
| 3 | 285 | isl04 |
| 4 | 319 | isl05 |
| 5 | 353 | isl06 |
| 6 | 387 | isl07 |
| 7 | 421 | isl08 |

`Menu::click` currently uses `line = widget.height / 10` (28 px) for
`ISLAND_LIST` (`src/ui/menu.cpp`), so the mapping drifts one row per ~3
clicks. Measured with the real menu:

| clicked row | expected | port loaded |
| --- | --- | --- |
| 3 (DINOKO ANKYLO) | isl04 | **isl05** |
| 7 (SANTO DIPLODO) | isl08 | **isl10** |

The fix is to use `widget.list_line_height` (34) and add the list's
first-visible-row offset. The draw loop (`src/viewer/main.cpp`) also always
starts at row 0, so islands 9-10 are unreachable; it needs the same offset.

## 2. List geometry and scrollbar coordinates

The list is at design rect `[175,166,188,285]`; the 22 px scrollbar is the
right strip `x=341..363`. The UI map gives the arrow clicks:

* `ISLAND_LIST`: up **352,177**, down **352,440** (22 px wide).
* `CAMPAIGN_LIST`: up **492,177**, down **492,440** (10 px wide).
* `LOAD_GAME_NAMES_LIST`: up **515,151**, down **515,409** (22 px).
* `CINEMATICS_LIST`: up **498,178**, down **498,441** (10 px).
* The VSCROLL sections carry no X/Y/WIDTH/HEIGHT; the overlay derives the
  strip from the parent list's `LB_VSCROLL_WIDTH` and height. The internal
  child names are `EQUIVALENT = "LbVs"`, `"VsBtnUp"`, `"VsBtnDown"`,
  `"VsBtnThmb"`; textures `CAMPAIGN_TEXT_LIFT`, `_SCROLLUP`, `_SCROLLDOWN`,
  `_THUMB` (native 22x22 arrows, 22x32 thumb, 22x243 track).

Visible rows = `height / LB_LINE_HEIGHT`: island 8 (10 rows, 2 scrollable),
campaign 8 (8 rows), load 14 at 20 px (as many as save slots), cinematics 8.

## 3. Ported state

* `Menu` keeps `island_first` / `campaign_first` / `save_first` /
  `cinematic_first`; rows are drawn and clicked as `first + slot`.
* The list viewport is `height / LB_LINE_HEIGHT` rows; `range =
  max(1, n - visible + 1)`; positions clamp to `0..range-1`; a page entry
  resets the position to 0 (retail list rebuild).
* VSCROLL geometry (retail `0x5c4720`):
  `U = D = LB_VSCROLL_WIDTH`,
  `thumbH = max(5, H - (U + D + 5) - 3*range)`,
  `travel = H - U - D - thumbH`,
  `thumbY = U + pos*travel/(range-1)`. `range == 1` fills the track between
  the arrows.
* Input (retail `0x5c4650`): up/down `±1`, track above/below `∓3`, thumb press
  starts an **absolute** drag (`pos = (mouseY - (up.y+up.h)) * range /
  travel`), wheel one notch = one line, mouse up ends the drag.
* The one-column container width is `list.w - LB_VSCROLL_WIDTH`; retail
  overwrites `LB_COLUMN0_WIDTH` with it (`0x5bda70`), so the load list is
  212 px, not the INI's 220.
* Click-to-line rejects `row >= first + visible` (`0x5bdb10`).
* Tests: `tests/test_viewer_cpp.py::test_cpp_menu_start_loads_selected_island`
  (rows 0/3/7/9) and `test_cpp_menu_start_all_islands` (all 10, with the
  scrollbar for 8/9).

## 4. Open

* Hold-to-repeat is unverified in the trace (`0x5cb9f0`); the port currently
  scrolls once per click.
* List-row hover frame and the separator/colour keys are cosmetic and not
  mapped yet.
* Retail `.sav` slots are a separate menu item (R-16b).

## 5. Campaign briefing scrollbar, text metrics and in-park Quit

* `CAMPAIGN_INFO_DESCRIPTION` is a wrapped LINE list: 17 px rows
  (`LB_LINE_HEIGHT`), 16 visible rows (280/17), wrapped at the list column
  width `235 - 22 = 213 px` — **not** a character count. A greedy pixel wrap
  with the FONT14 advances reproduces every retail line break at 213:
  "Walt Dino has led the way, and now", "considerable flow of visitors. All
  of", "being that his park has become a living", "You are therefore urgently
  requested", ... (retail capture `notes/harness/campaign-info.png`). The port
  drew 14 px font line spacing before; the line pitch is now 17.
* Glyph advance: retail moves the pen one pixel **less** than the glyph's ink
  width (`advance = x1 - x0`), so neighbouring glyphs touch. Measured on the
  retail captures: "PANIC IN THE SAVANNA" = 135 px and
  `sum(width) - (n-1)` = 137; "being that his park has become a living"
  = 213 px; "CAMPAIGNS" (FONT19) = 100 px, `sum(width) - (n-1)` = 100. The
  port used the full ink width, which made every menu/HUD string ~10% too
  wide and overflowed the 213 px briefing column (clipped last words).
* Widget `TEXT` is only drawn when the section sets `TEXT_HEIGHT`: the glyph
  scale is `TEXT_HEIGHT / font_size`, so an absent height is a zero scale.
  Icon buttons (`CAMPAIGN_START_BUTTON`, `*_OK_BUTTON`, ...) carry `TEXT` but
  no `TEXT_HEIGHT`; retail shows bare icon art.
* Scrollbar graphics `CAMPAIGN_TEXT_SCROLLUP/SCROLLDOWN/THUMB` have
  `SKIP_LIST = 1 2 3`. `SKIP_LIST` is a mask of unused *animation* frames and
  the state frames are universal: 0 = disabled, 4 = normal, 5 = hover,
  6 = pressed, 7 = selected. The port draws frame 4 (red arrows, metal
  thumb); frame 0 is the black-triangle disabled art.
* The auto-created VSCROLL is hidden when the list fits (`range == 1`): the
  retail campaign list (7 rows in 8) shows bare panel metal, while the island
  list (10 rows) shows the bar. The port drew a full-track thumb and arrows on
  the campaign list; it now skips the bar for `range <= 1`.
* Retail starts the campaign list with **no row selected**: all rows use the
  normal frame and `CAMPAIGN_START_BUTTON` is disabled (frame 0). Clicking a
  row selects it and enables START (`notes/harness/campaign-list.png` vs
  `campaign-selected.png`). The port defaults `Menu::campaign = -1` and draws
  the disabled START frame until an unlocked campaign is selected.
* In-park options `OPTIONS_MENU_EXIT_ICON` (`ConstructionInterface.Ini`,
  `&QuitGame`) does **not** exit. The widget string (0x662924) is bound in the
  ParkView ctor (0x52408d) to a listener at 0x524180 that shows the REQUEST
  panel with `&REQUEST_RETURN_MENU` ("Do you really want to quit and return to
  the menu?") and a result callback at 0x5242e0. The callback writes byte 0 of
  the ParkView's 3-byte state struct (`ParkView+0x1b4`, passed in by the view
  manager: `FUN_00596630` passes `manager+0xc`, stored by `FUN_00523360`); the
  update then returns to the main menu. There is no save call and no
  save-related text. Validate = return to menu, cancel = stay.

  Raw evidence:
  ```
  ; ParkView ctor, OPTIONS_MENU_EXIT_ICON lookup
  0052408d PUSH 0x662924                 ; "OPTIONS_MENU_EXIT_ICON"
  00524085 CALL 0x4139d0                 ; widget lookup
  005240cb MOV [ECX+0x20], 0x524180      ; listener function pointer
  005240d4 CALL 0x5bbf20                 ; add listener

  ; listener 0x524180 (event id 0 = click)
  00524196 MOV ECX, 0x662c14             ; "&REQUEST_RETURN_MENU"
  00524254 PUSH 0x5242e0                 ; result callback
  00524259 PUSH ESI                      ; text object
  0052425a CALL 0x5e3ba0                 ; request_panel->Show (this=[ebp+0x1bc])

  ; result callback 0x5242e0 (validate)
  005242f0 MOV EAX, [EDI+0x1b4]          ; 3-byte state struct
  0052430e MOV byte [EAX], 1             ; quit-requested
  0052436f CALL 0x405400                 ; click sound only
  ```
  `0x5242e0` contains no save/write call; the main-menu `QUIT_BUTTON`
  (`Menu::quit`) is the only path that exits the application. ParkEng.loc:
  `&REQUEST_RETURN_MENU` = "Do you really want to quit and return to the menu?".

  Port: `ParkHud::return_to_menu_requested()` + `activate_main_menu()`;
  test `test_cpp_park_quit_confirmation_and_menu`; retail captures
  `notes/harness/quit-confirm.png`, `quit-after-confirm.png`,
  `quit-cancel.png`. Runtime confirmation (`quit-confirm-timeline.txt`):
  validate lands on the main menu; the `run-test/save/` snapshots before the
  dialog, while open and 12 s after confirming have identical file lists,
  sizes, mtimes and SHA-256 -- **no autosave and no save warning**. Cancel
  closes the dialog and the park keeps running
  (`quit-cancel-timeline.txt`).
* Text rendering (the visible "textures don't look right"):
  * The `.fnt` atlas is binary 0/255 and retail samples it with **point
    filtering**. The port used `GL_LINEAR`, so every 1-px stroke sampled at a
    fractional position blended to ~70 % and the whole UI looked thin/washed
    out. The atlas is now `GL_NEAREST`.
  * Text quads are snapped to whole pixels (`build_text_mesh` rounds the
    origin); otherwise point sampling can pick a neighbouring glyph's texel.
  * Retail centres a text block on the font **cell height** (12 FONT14, 16
    FONT19) and truncates the half pixel: `(34-12)/2 = 11` for a list row and
    `(27-16)/2 = 5.5 -> 5` for the FONT19 title plaque. The port centred on
    `line_height` (14/19), which put every label 1 px high. After the fix the
    "PANIC" glyph block and the title ink box are pixel-identical to the
    retail captures.
  * The briefing had a hard-coded +4 px x padding; retail starts it at the
    list x (glyph ink +2). Removed.
  * Retail text renders ~6 % transparent (samples `240,244,247`); cosmetic,
    not modelled.
* Evidence: `test_cpp_campaign_info_description_scroll` (text viewport and
  thumb differ after three down-arrow clicks), screenshots
  `build/campaign-info-port11`, `build/campaign-select-port9`,
  `build/campaign-selected-port` (build dir, not tracked) diffed against
  `notes/harness/campaign-info.png`, `campaign-list.png`,
  `campaign-selected.png` (retail dgVoodoo captures); with the fixes the
  title/row ink boxes and the glyphs themselves match exactly, the remaining
  diff is the 6 % text alpha and the retail cursor/watermark.

## Evidence

* `game/%MAINDIR%/Data/Overlay/MenuInterface.Ini` (`ISLAND_LIST`,
  `*_VS` sections), `MenuGraphicDB.ini` (`ISLAND_LIST_BUTTON` 166x34).
* `docs/platform/ui-coordinates.md` (row/scroll clicks).
* Live viewer: row 3 -> isl05, row 7 -> isl10 (`--menu --menu-page 1
  --menu-click ... --menu-loading-time 0`, `start: island=...`).
