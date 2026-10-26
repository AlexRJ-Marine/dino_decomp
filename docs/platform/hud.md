# HUD and bitmap text (UI-0)

Status: **working** — retail `.fnt` text renders on screen, with a live HUD,
a start screen and a controls panel.

> **Retail menu is now the default.** `--menu` and interactive launches use the
> original `MenuInterface.Ini` overlay; see
> `docs/platform/ui-original-menu.md`. The old DINO ISLAND screen is available
> only with `--placeholder-menu`; the Tab controls panel is still port-style.

## `.fnt` reader

`src/formats/fnt.cpp` mirrors the validated Python parser
(`docs/formats/fnt.md`):

```text
0x00  9 x u32: version, atlas w/h, font size, cell height, base, line height,
             glyph count, hash
0x24  glyph_count x u16 codes
...   glyph_count x 8-byte metrics (x0,y0,x1,y1)
...   atlas: w*h*2 bytes, 16-bit texels
```

`codes[i]` maps to `metrics[i + 2]`; the last two code words are padding.
`arial12.fnt` gives 245 glyphs.

## Rendering

- The atlas is uploaded as `GL_R8`, taking the **high byte** of each 16-bit
  texel (the coverage value).
- `build_text_mesh` emits one quad per glyph in screen pixels (top-left
  origin), with UVs converted to the atlas (`v - 1` because the shared vertex
  builder adds 1).
- Drawn with the main shader in `u_unlit = 2` mode: fragment alpha is
  `texture(...).r * u_alpha`, colour comes from the vertex colour.
- Orthographic MVP maps pixels to clip space. Note the translation lives at
  `m[3]`/`m[7]` for this upload convention.
- Text uses alpha blending, depth test off; state is restored afterwards.

## Start screen and panels

- Interactive runs (`--frames 0`, no screenshot) open a start screen over the
  island: `DINO ISLAND`, `Press ENTER to begin`, `Esc quits  Tab help`.
  ENTER/SPACE starts, ESC quits.
- `--menu` forces the start screen for screenshots; `--no-menu` skips it.
- **Tab** toggles a controls panel (also `--help-panel` for screenshots).
- Both use the same `draw_2d` path: a translucent screen-space rectangle plus
  text quads.

## HUD

```text
Dino Island   FPS 99
visitors 12   paths 24   fences 0
```

- **F1** toggles the HUD; `--no-hud` starts hidden.
- `--hud-font FILE` selects another `.fnt` (default
  `Data/Fonte/arial12.fnt`).
- The HUD is rebuilt each frame; this is a small mesh (212 verts for the
  current two lines) but a dynamic-buffer path is a future optimisation.

## Park HUD (`ParkInterface.Ini`)

When an island is loaded, the viewer also loads
`ParkInterface.Ini` + `ParkGraphicDB.Ini` (2,694 widgets, 38 roots, 53
visible at park start) and draws the always-on panels over the scene:

- visible roots and descendants only; `RENDERVIEW` is skipped;
- normal state = frame 4 (frame 0 is the gray disabled art);
- `.fnt` text with `TEXT_HEIGHT` scaling and `TEXT_JUSTIFY`;
- runtime fields: live budget, visitor/dino/show counts, park clock (hour and
  day, from `Data/Config/Dino.ini` `GameSpeed`);
- `--no-park-hud` disables it.

At 800×600 this matches the Python oracle (`notes/hud-preview.png`):
right panel MAD 10.8, gauge 4.0, map 2.1 (oracle vs retail: 8.8 / 3.65).

### Panel switching

`src/ui/park.cpp` owns the panel state. The original shows exactly one state
panel at a time; the port follows that:

- right-panel buttons: lab, construction bar, buildings, infrastructure,
  buy-dino, options; gauge buttons: budget, visitors, dino list, show
  calendar, paddock list;
- info-controller panels share one slot (`BUDGET_PANEL`,
  `VISITEURS_STATS_PANEL`, `OPTIONS_PANEL`, `ATTRACTION_LIST_PANEL`,
  `FENCE_LIST_PANEL` for the paddock button, `DINO_LIST_PANEL`,
  `ATTRACTION_STATS_PANEL`, `MISSION_PANEL`);
- construction sub-buttons replace the bar with the kit/fence/ready-made-show
  panel; the kit's list/stats icons open the info panels;
- lab centres (`LAB_BREED/MODIFY/CLONE/FRIDGE_CENTER`) replace the main lab
  screen, use their own 800×600 backdrop and hide the always-on HUD;
- every panel closes through its own close/cancel/validate/put-back button
  or by clicking the same opener again.

The budget ledger's `BUDGET_*_NUMBER` widgets (category quantity/cost and the
two totals) are separate number widgets refreshed by `0x510430`; a fresh park
has no spending yet, so the port fills them with `0`. The gauge budget comes
from the selected difficulty (75,000 / 50,000 / 25,000).

The in-park **Options** panel's Save/Load tabs switch to
`OPTIONS_SAVE_PANEL`; `OPTIONS_SAVE_NAMES_LIST` lists the real
`run/save/*.sav` names using the C++ header reader (`src/formats/sav.cpp`).
The save/load action is wired to the port's park state; see
`docs/platform/save-load.md`.

### Dialog family (warning / request / campaign end)

`DIALOG_BACKGROUND` (FOND_GRIS, black alpha 106) + one of `WARNING_PANEL`,
`REQUEST_PANEL` or `CPGEND_PANEL` is the modal dialog family. `--warning TEXT`,
`--request TEXT` and `--campaign-end TEXT` show each for screenshots/tests.
The panels use `ALIGN_CENTER`, so at 800×600 they sit at `(258,204)`,
`(258,204)` and `(258,125)` respectively (not their authored top-left).

- `WARNING_OUTCOME_ICON` closes the warning (action `close warning`).
- `REQUEST_VALIDATE_ICON` accepts (`accept request`) and
  `REQUEST_CANCEL_ICON` cancels (`cancel request`).
- `CPGEND_OUTCOME_ICON` closes the campaign-end panel.
- Any click behind the dim is consumed while a dialog is open.
- The text is word-wrapped into `WARNING_TEXT_ZONE` / `REQUEST_TEXT_ZONE` /
  `CPGEND_TEXT_ZONE` and drawn above its `*_TEXT_BACK_ZONE` (the authored
  ZPRIORITY alone would put it underneath).

`DIALOG_BACKGROUND` is **not** used here: it belongs to the warning, request
and campaign-end dialogs. Buttons use frame 4 normally, 5 on hover and 6
while pressed (when the strip has those frames).

CLI: `--park-click X,Y` applies a design-space click before the first frame
(repeatable, useful for screenshots/tests); `--park-hover X,Y` sets the hover
point; `--no-park-hud` disables the whole HUD.

### Minimap

`MAP_RENDERVIEW` (652,451,136×136) is a 2-D detail map, not a 3-D camera and
not a basemap crop. The viewer renders the terrain into a 256×256 FBO
top-down, centred on the camera target and rotated so the camera heading is
up. The visible world width is `2 * world_size * scale`:

- scale default 0.25 (whole 1024-unit island), clamped 0.1..2.0;
- the retail zoom buttons are hold-to-zoom at 0.15/s (`+` decreases the
  scale); `--park-click` steps 0.05; `--minimap-scale N` sets the start value;
- a red camera marker with a yellow heading tick is drawn when it falls
  inside the map;
- the FBO is refreshed only when the target, heading or scale changes.

### Resolution and anchoring

The park HUD is **re-anchored, not scaled** (`moWidget::OnResize`). The viewer
keeps the 800×600 authoring rects and re-runs the align pass whenever the
logical window size changes; at 1024×768 the right panel is at 931,168, the
gauge at 293,737 and the map at 840,584. The minimap quad follows
`MAP_RENDERVIEW`'s anchored rect. Menu pages still centre their 800×600
screen. High-DPI uses the drawable/logical ratio for physical scaling after
the logical anchoring pass.

### Park clock

The HUD time/day fields are driven by `ParkClock`, mirroring the time manager
`0x0058c1c0`/`0x0058c2f0`/`0x0058c350`:

- `time_scale` is 60.0 (the schedule manager's float, not `Dino.ini`
  `GameSpeed`); hours/sec = scale/3600, minutes/sec = scale/60;
- the base hour is hard-coded 9 and the day counter starts at 0 (HUD day 1);
- a day is 12 hours; reaching hour 11 sets the end-of-day flag, then the day
  increments, elapsed resets and the HUD shows `day+1`;
- the HUD formats `%02d:%02d` (the retail Free Game screenshot shows 09:44,
  no AM/PM suffix; the engine clears the AM/PM flag);
- `--park-time HH:MM` seeds the clock for screenshots/tests,
  `--park-advance N` advances it N real seconds and
  `--park-time-scale N` overrides 60.0.

### Park list snapshots

The imported `.wld`/`.sav` park state feeds the two bottom list panels:

- `PADDOCK_AMOUNT` and `DINO_AMOUNT` are the real fence-rectangle and
  imported-dinosaur counts.
- `FENCE_LIST_PANEL` fills its five slots with `Paddock N`, the occupant count
  and the capacity percentage. The remaining paddocks are reachable with the
  scroll buttons once scrolling is wired.
- `DINO_LIST_PANEL` fills its five name slots with the species of the first
  five imported dinosaurs (the slot art and need/size indicators are the
  retail TEXTURE frames).

The HUD draw order now follows `moWidget::Draw` (`0x005c5bf0`): walk the widget
tree parent-first, sort each child list by `PRIORITY` and then descending
`ZPRIORITY` (a lower value is nearer the viewer), and draw text-only children
after their parent's art. The previous flat ascending-Z sort hid the list
indicator strips under the snapshot buttons.

Evidence: `notes/park-fence-list.png`, `notes/park-dino-list.png`,
`tests/test_viewer_cpp.py::test_cpp_park_fence_and_dino_lists`.

The five slot thumbnails are still the generic `DINO_SNAPSHOT_SLOT1` frame;
live 3D snapshots per dinosaur remain open.

## Evidence

- `notes/hud.png` — HUD over the isl01 view.
- `notes/menu-retail.png` — retail main menu.
- `notes/menu-free.png` — retail Free Game page.
- `notes/park-hud.png` - retail park HUD over isl01.
- `notes/park-budget.png` - budget ledger from a `--park-click`.
- `notes/park-construction-kit.png` - construction kit panel.
- `notes/park-lab-breed.png` - breeding centre with its own lab backdrop.
- `notes/park-hud-hover.png` - hover frame on the budget gauge button.
- `notes/minimap-isl01.png` / `notes/minimap-isl01-detail.png` - minimap with
  the camera marker.
- `notes/minimap-isl01-zoom-detail.png` - two scripted zoom-in clicks.
- `notes/minimap-isl10-detail.png` - a different island (snow) in the map.
- `notes/park-hud-1024.png` - the park HUD re-anchored at 1024×768.
- `notes/hud-1024.png` - the Python/oracle HUD at the same size.
- `notes/clock-0944.png` / `notes/clock-0944-detail.png` - park clock at 09:44.
- `notes/options-save.png` / `notes/options-save-detail.png` - Options Save
  tab listing the real `.sav` names.
- `notes/warning.png` / `notes/warning-detail.png` - modal warning dialog.
- `notes/request.png` / `notes/campaign-end.png` - request and campaign-end
  dialogs with their text zones.
- `tests/test_viewer_cpp.py::test_cpp_request_dialog`,
  `test_cpp_campaign_end_dialog`.
- `notes/retail-minimap-crop.png` - retail 800×600 map for comparison.
- `notes/hud-preview.png` - Python oracle for the park HUD.
- `tests/test_viewer_cpp.py`:
  - `test_cpp_hud` checks the font load line and bright glyph pixels.
  - `test_cpp_park_hud_load` / `test_cpp_park_hud_shot` check the park HUD.
  - `test_cpp_park_panels` / `_panel_shot` / `_lab_shot` / `_hover_shot`
    check panel switching, the lab backdrop and hover art.
  - `test_cpp_retail_menu_*` check the retail menu.
- `src/formats/fnt.cpp`, `src/formats/fnt.h`.

## Open items

- Kerning/advance beyond glyph width; coloured/shadowed text.
- High-DPI text sharpness at non-integer drawable/logical ratios.
- Slider/checkbox selection in the info panels; live 3D thumbnails in the
  dino/paddock/show list slots.
- Minimap detail-map water colour and exact partition contents.
- `draw_ui_mesh` re-uploads its mesh per widget per frame; batching is the
  next performance step for the park HUD.
