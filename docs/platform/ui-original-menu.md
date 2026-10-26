# Original menu — retail overlay (UI-2)

Status: **Phase A implemented** — the retail menu is rendered from
`MenuInterface.Ini` + `MenuGraphicDB.ini`; the old DINO ISLAND screen remains
only behind `--placeholder-menu`.

Research details: `notes/ui-recon.md` (evidence) and
`docs/formats/overlay-ini.md` (file format).

## Phase A implementation

- `src/ui/ini.cpp` parses the overlay INIs (comments, quotes, `#INCLUDE`,
  Windows-1252).
- `src/ui/overlay.cpp` builds the widget tree (114 widgets, 9 pages) and the
  graphic database (53 rectangles), computes parent-relative absolute rects and
  page visibility.
- `src/ui/menu.cpp` implements the page state machine (0..8), hit-testing and
  the recovered transitions (Free/Campaign/Load/Options/Cinematics/Credits/
  Quit, island/difficulty, Start/Cancel).
- `src/formats/loc.cpp` resolves `&` strings through `MenuEng.loc`.
- Rendering in `src/viewer/main.cpp`: 800×600 design space centred in the
  window (pixels, not scaled); background from `BACKGROUND_*`; sprite frames
  packed per `NB_OBJECTS`/`SKIP_LIST` (compactly, horizontal or vertical);
  `.fnt` text with `TEXT_HEIGHT/font_size` scaling and `TEXT_JUSTIFY`.
- **Frame-helper review:** the current `ui_frame_rect` still assumes skipped
  frames are removed from the end and uses row-major packing; the retail
  model is the used-frame list, a horizontal/vertical strip, and the state
  mapping **0 disabled / 4 normal / 5 hover / 6 pressed / 7 selected**.
  Frame 4 may be transparent (menu/list bars). See
  `notes/ui-frame-handoff.md`; the Python `Graphic.frame_rect` and
  `state_frame` are the reference.
- CLI: `--menu`, `--menu-page N`, `--menu-click X,Y` (deterministic tests),
  `--placeholder-menu`, `--no-menu`.

Screens: page 0 (main), page 1 (Free Game: 10-island list, difficulty, Start),
and pages 2–8 render their retail backgrounds/widgets. Start closes the menu
and resumes the park; other pages are display-only until Phase B/C.

Evidence: `notes/menu-retail.png` (main), `notes/menu-free.png` (Free Game),
`notes/menu-pages-sheet.png` (pages 2–8), `notes/menu-preview.png` (Python
parser cross-check), `tests/test_viewer_cpp.py` (menu load/pages/clicks/shot).

## Placeholder fallback

`--placeholder-menu` still draws the old standalone DINO ISLAND screen (dark
rectangle + `.fnt` text), kept only as a comparison/fallback. It uses no retail
layout, art, sounds or actions.

## Headline

The retail UI is **data-driven**. The extracted tree contains the complete
layout, art atlas rectangles and localized strings. No layout needs to be
reversed from the executable; the binary only supplies semantics (anchoring,
text justification, state transitions, how a button maps to a page).

- Design space: **800×600**, top-left origin, +Y down.
- At 1024×768 the retail UI is **shifted by (112, 84)**, not scaled:
  measured MAD 1.0 versus 40.3 for uniform scaling.
- Menu pages are selected by name from code; park HUD panels are always-on
  and toggled with `VISIBLE`.
- `TEXT`/`HINT` values starting with `&` are `.loc` field IDs.

## Retail assets

| Path | What it is |
| --- | --- |
| `Data/Overlay/MenuInterface.Ini` | 114 widgets, 9 full-screen pages. |
| `Data/Overlay/MenuGraphicDB.ini` | 53 named sprite rectangles; 25 TGAs, all present. |
| `Data/Overlay/ParkInterface.Ini` | 137 sections (136 widgets + include); `#INCLUDE`s 9 panel files (2,558 more sections). |
| `Data/Overlay/ParkGraphicDB.ini` (+ `LabGraphicDB.ini`) | 426 + 180 rects, 605 unique (`LAB_NAME_PANEL` repeats identically). One dangling `TEMP_jaugebas.tga` (unused `TEMP_*` gauges). |
| `Data/Overlay/RadialMenu.Ini` | 22 sections: build/context radial menus. |
| `Data/Overlay/MenuEng.loc` | 56 localized menu strings. |
| `Data/Overlay/ParkEng.loc` | 702 localized park strings. |
| `Data/Sound/MenuSoundDB.ini` | 5 sounds: `clicDefault`, `clic`, `exitClic`, looped `music`, `mixer`. |
| `Data/Sound/ParkSoundDB.ini` | 224 park sounds. |
| `Data/Video/intro.mpg` | Intro movie; skip with `UseIntroVideo=0`. |
| `Data/Overlay/pics/Menu_hors_jeu/*.jpg` | Reference screenshots, including 800×600/1024×768 pairs. |

The comment header of `MenuInterface.Ini`/`ParkInterface.Ini` documents the
widget schema; `docs/formats/overlay-ini.md` is the normalized spec.

## Answers to the seven questions

### 1. State machine

Nine pages, registered in this order by `mvwMenuStateManager`
(`FUN_00517c60`). The index is the state:

| Index | Page | Constructor |
| ---: | --- | --- |
| 0 | `MAIN_MENU_PAGE` | `FUN_0051bbf0` |
| 1 | `FREE_GAME_PAGE` | `FUN_0051b120` |
| 2 | `CAMPAIGN_SELECT_PAGE` | `FUN_0051aa90` |
| 3 | `CAMPAIGN_INFO_PAGE` | `FUN_0051a560` |
| 4 | `LOAD_GAME_PAGE` | `FUN_00519ba0` |
| 5 | `OPTIONS_PAGE` | `FUN_00518830` |
| 6 | `CINEMATICS_PAGE` | `FUN_00518210` |
| 7 | `CREDITS_PAGE` | `FUN_00517fe0` |
| 8 | `LOADING_PAGE` | `FUN_00517e90` |

Each constructor looks its page and widgets up **by section name** and stores
the widget pointers. Button clicks set `page + 0x18 = nextState`:

| Button | Next state |
| --- | ---: |
| `FREE_GAME_BUTTON` | 1 |
| `CAMPAIGN_BUTTON` | 2 |
| `LOAD_GAME_BUTTON` | 4 |
| `OPTIONS_BUTTON` | 5 |
| `CINEMATICS_BUTTON` | 6 |
| `CREDITS_BUTTON` | 7 (handler does not write `+0x18` in its first instructions; likely handled differently) |
| `QUIT_BUTTON` | quit path |
| `FREE_CANCEL_BUTTON` | 0 |
| `FREE_START_BUTTON` | 8 (requires a selected island) |

`LOADING_PAGE` hands control to the park. Startup enters page 0 after the
intro (skip with `UseIntroVideo=0`).

Not yet traced: the pause path (`&PauseGame` exists in `ParkEng.loc`, but no
`PAUSE` section exists in the overlay INIs) and the save-panel button wiring.

### 2. Layout semantics

`FUN_005bca20` (`moWidget::Load`) reads all properties. Flags at
`widget + 0x38`:

`VISIBLE` 0, `USER_MOVABLE` 4, `DRAG_AND_DROP` 5, `ALIGN_LEFT` 10,
`ALIGN_UP` 11, `ALIGN_RIGHT` 12, `ALIGN_DOWN` 13, `ALIGN_CENTER` 14.

`FUN_005bc000` (`OnResize(newW, newH, oldW, oldH)`) recursively re-anchors:

- no flags → fixed;
- right/down only → shift by the resolution delta;
- left+right or up+down → stretch to keep the far margin;
- `ALIGN_CENTER` → `(newW - width) / 2`, `(newH - height) / 2`.

Park HUD top-level panels:

```text
NAVIGATION_PANEL  640,289  83x128  right+down
RIGHT_PANEL       707,0    93x417  right+down
GAUGE_PANEL        69,569 618x31   right+down
MAP_PANEL         616,416 184x184  right+down
RENDERVIEW          0,0   800x600  all four (stretch)
```

The gauge panel contains the five counters in a row: budget
(`BUDGET_AMOUNT_BACKGROUND` at +44), visitors (+154), dinos (+249), shows
(+344), paddocks (+439). Each counter is a 13-px text box at +5,+9 inside a
31-px background, centred, `FONT.FNT`, `TEXT_HEIGHT=12`.

Main menu: `MAIN_MENU_PANEL` at 288,158, 214×285; six buttons 214×23 at a
45-px pitch starting at +65 (Free Mode is the second). `QUIT_BUTTON` at
559,484. Free Game: island list at 175,166 (188×285), island picture at
448,177 (225×190), difficulty buttons at 468,425 (3 × 52 px, 74-px pitch),
Start/Cancel at 502,506 (49×50).

`EQUIVALENT` marks internally created scrollbar sub-widgets; it is not needed
for the first pass.

### 3. Rendering contract

- Page backgrounds are **code-created**, not INI widgets:
  `BACKGROUND_MAIN_MENU`, `BACKGROUND_FREE_GAME`, … in
  `MenuGraphicDB.ini`, each an 800×600 TGA. Code xrefs to
  `BACKGROUND_MAIN_MENU` at `0x0051c16d/0x0051c184/0x0051c1a4`.
- A widget's `TEXTURE` resolves through the matching GraphicDB to
  `(FILENAME, X, Y, WIDTH, HEIGHT)`. Sprite atlases are 256×256; the nine
  menu `BACKGROUND_*` page images are 800×600.
- Text is drawn with `TEXT_FONT` at `TEXT_HEIGHT` pixels, colour
  `TEXT_COLOR`, justified in the widget box (`TEXT_JUSTIFY` bits 1/2/3
  horizontal, 4/8/12 vertical, 15 = both centred).
- Buttons use `NB_OBJECTS=8`; `SKIP_LIST` is a **bitmask** of unused frames.
  Frames are packed compactly (horizontal when the strip fits, else
  vertical). State mapping is **0 disabled, 4 normal, 5 hover, 6 pressed,
  7 selected**; frame 4 may be transparent (menu/list bars). This is
  validated by the Python oracle: Start/Cancel match retail exactly
  (MAD 0.0), gauge icons 3.65.
- Draw order uses `PRIORITY` (0..100) then `ZPRIORITY` (0..1); the exact
  comparator is not recovered.
- `TEXTURE_MOSAIC` tiles instead of stretching.

The port can implement all of this with the existing `.fnt` text path and TGA
loader; no D3D8 overlay code is needed.

### 4. Input and actions

- Buttons have **no action IDs in the INI**. The page constructor binds each
  widget name to a handler; handlers set the next page index at `page + 0x18`.
- Event callback slots are at `widget event object + 0x1c/+0x20/+0x24`.
  Click is event argument 0 (`CMP ECX, 0`).
- Lists (`ISLAND_LIST`, `CAMPAIGN_LIST`, …) use `+0x24/+0x1c` handlers to
  update the selection; `FREE_START_BUTTON` requires `selected island >= 0`.
- The x32dbg click coordinates map exactly to INI rectangles, confirming
  parent-relative layout and no window scaling:

```text
Free Mode 395,234  -> FREE_GAME_BUTTON 288..502 x 223..246
IGUANO    260,183  -> ISLAND_LIST      175..363 x 166..451
easy      494,451  -> FREE_EASY_BUTTON 468..520 x 425..477
Start     526,531  -> FREE_START_BUTTON 502..551 x 506..556
```

A port should hit-test the visible widget tree in reverse draw order, then
dispatch by widget name (or by a small enum assigned by the page builder).

### 5. Localisation

- `TEXT` and `HINT` values beginning with `&` are field IDs in
  `MenuEng.loc` / `ParkEng.loc` (`lcFileL`; format already solved in
  `docs/formats/loc.md`).
- `MenuEng.loc`: `&FreeGame` → `FREE MODE`, `&Campaign` → `CAMPAIGNS`,
  `&LoadGame` → `LOAD GAME`, `&Options` → `OPTIONS`, `&Ok` → `OK`,
  `&Isl01_Name` → the first island's name.
- `ParkEng.loc`: `&Budget` → `Budget`, `&CurrentTime` → `Time`,
  `&CurrentDay` → `Day`.
- Runtime keys that are **not** in `.loc`: `&VisitorAmount`,
  `&BudgetAmount`, `&dinoAmount`, `&ShowAmount`. The game writes those
  values directly; a port should keep the same widget names and bind them to
  live park values.
- Missing-ID fallback is not proven; `&NoText` is the first record in
  `MenuEng.loc` and is a safe placeholder.

### 6. Save/load

- The load screen is `LOAD_GAME_PAGE` (`LOAD_GAME_NAMES_LIST`,
  `LOAD_LOAD_BUTTON`, `LOAD_CANCEL_BUTTON`).
- In-park save/load lives in `ConstructionInterface.Ini`:
  `OPTIONS_MENU_PANEL` has `OPTIONS_MENU_SAVE_BUTTON` and
  `OPTIONS_MENU_LOAD_BUTTON` (28×28, hints `&SaveGame`/`&LoadGame`), and
  `OPTIONS_SAVE_PANEL` (250×361 at 0,50, hidden by default) holds
  `OPTIONS_SAVE_NAMES_LIST`, the `OPTIONS_NAME_SLOT` edit box and the
  `OPTIONS_VALIDATE_BUTTON`/`OPTIONS_UNVALIDATE_BUTTON` pair. The
  `DINO_ACHAT_SAVE_*` sections are the dinosaur **purchase** panel
  (`achat`), not save/load.
- Save files are `run/save/slotNNN.sav`; the original does not create
  `run/save` or `run/save/auto`, so the harness pre-creates them (RUN-2).
- The in-park pause overlay is `OPTIONS_PANEL`, not a separate screen:
  `PauseGame` (bound in `FUN_005257a0`, handler `LAB_00525e80`) opens it, and
  `OPTIONS_MENU_PANEL` switches between `OPTIONS_OPTIONS_PANEL` and
  `OPTIONS_SAVE_PANEL`. Controller: `FUN_0050c1a0`.
- Geometry: `OPTIONS_PANEL` 250,50 250×411; tabs 292,50 112×48; settings
  tab 250,110 250×351; save tab 250,100 250×361; list 282,125 143×216;
  name edit 285,375 128×14; validate/unvalidate 320/350,422 (28×28).
  `notes/save-panel.png` is the `DINO_ACHAT` purchase panel, **not** the
  save panel.
- The port's `save_build`/`load_build` placement format is unrelated and can
  back the same panel later.

### 7. Audio and video

- `MenuSoundDB.ini` (5 entries): `clicDefault` → `clic#3.wav`, `clic` →
  `clic#2.wav`, `exitClic` → `clic#9.wav`, `music` →
  `Musics/musLabo.wav` (loop + streaming), `mixer` → `mixer.wav`.
- The sound DB format is documented in the file header: `FILENAME`,
  `PRIORITY`, `CATEGORY`, `USE_VOLUME`, `USE_FREQUENCY`, `USE_STEREO`,
  `USE_3D`, `LOOP`, `KEEP_IN_MEMORY`, `START_DELAY`, `3D_MIN/MAX_DISTANCE`.
- `Data/Video/intro.mpg`; `UseIntroVideo` in `Data/Config/Dino.ini`.
- MENU-1 (`docs/platform/audio.md`) already plays WAV via SDL3; wiring
  `clicDefault` to button clicks is a small follow-up.

## Implementation plan

### Phase A — parser + main menu (done)

- [x] `src/ui/ini.{h,cpp}`: `#INCLUDE`, comments, quoted values, Windows-1252.
- [x] `src/ui/overlay.{h,cpp}`: widget tree, absolute rects, page visibility,
      GraphicDB lookup.
- [x] `src/ui/menu.{h,cpp}`: pages 0–8, hover/click hit-test, recovered
      transitions, island/difficulty state.
- [x] `src/formats/loc.{h,cpp}`: C++ `MenuEng.loc` reader.
- [x] Viewer rendering: centred 800×600 design space, `BACKGROUND_*`, compact
      frame strips (`NB_OBJECTS`/`SKIP_LIST`), `.fnt` text with
      `TEXT_HEIGHT`/`TEXT_JUSTIFY`.
- [x] `--menu`, `--menu-page`, `--menu-click`, `--placeholder-menu`,
      `--no-menu`.
- Acceptance met: the 800×600 main menu matches the retail reference; Free
  Mode at 395,234 enters page 1; Start at 526,531 closes the menu.

### Phase B — Free Game → park (done)

- [x] Populate `ISLAND_LIST` from the 10 `.loc` island names.
- [x] Island picture (`ISLAND_PIC01..10`, `ile*.tga`), difficulty buttons,
      Start/Cancel selection.
- [x] Start sets state 8 (`LOADING_PAGE`); `loading_800x600.tga` is shown for
      `--menu-loading-time` seconds.
- [x] On timer expiry the viewer loads the selected island at runtime:
      `IslandList.ini` order → `Data/Design/Landscape/<name>/<name>.lnd` →
      terrain, basemap, vegetation, skybox, water and GPU resources are
      rebuilt (`load_island` in `src/viewer/main.cpp`). This mirrors
      `FUN_0051dad0`/`FUN_00403e10`; the loading page itself is display-only
      in the original.
- [x] Free-game budget from `Data/Design/Scenario/FreeGame.ini`
      (`[CONTEXT]`): 75,000 / 50,000 / 25,000 for easy/middle/hard, shown in
      the park HUD. Sell-back factor and visitor profiles are parsed by the
      original but not yet used by the port.
- [x] Acceptance: `--menu-page 1 --menu-click 200,180 --menu-click 494,451
      --menu-click 526,531 --menu-loading-time 0` reaches isl01/easy with
      75,000; row 9 + hard reaches isl10 with 25,000. Adding `--start-dino`
      places the allosaurus at the island centre (vertical-slice demo).
- Evidence: `notes/ui-recon.md` ("Free Game start -> park handoff"),
  `notes/menu-start-isl01.png`, `notes/menu-start-isl03.png`,
  `notes/menu-start-isl10.png`, `tests/test_viewer_cpp.py`
  (`test_cpp_menu_start_loads_selected_island`).
- Open: the original park camera's initial framing is not yet recovered.

### Phase C — park HUD (done at 800×600)

- [x] Load `ParkInterface.Ini` + `ParkGraphicDB.ini` (includes expanded):
      2,694 widgets, 38 roots, 53 visible at park start.
- [x] Render the always-on roots (`RIGHT_PANEL`, `NAVIGATION_PANEL`,
      `GAUGE_PANEL`, `MAP_PANEL`, `MESSAGE_PANEL`); skip `RENDERVIEW`.
- [x] Runtime fields: budget, visitors, dinos, shows/paddocks, time 09:44,
      day 1. Normal state = frame 4 (frame 0 is the disabled art).
- [x] Compare with the Python oracle at 800×600: right panel MAD 10.8,
      gauge 4.0, map 2.1 (oracle vs retail: 8.8 / 3.65).
- [x] Button bindings and panel switching (`src/ui/park.cpp`):
      - 12 right-panel/gauge buttons + the construction sub-buttons;
      - one active state panel at a time (construction, info-controller page,
        lab centre), matching the recovered `pvwParkStateManager`;
      - `OPEN_PADDOCK_LIST_PANEL` opens `FENCE_LIST_PANEL` (info page 2);
      - kit list/stats icons open `ATTRACTION_LIST_PANEL` /
        `ATTRACTION_STATS_PANEL`;
      - lab breed/modify/clone/fridge centres use their own 800×600 backdrop
        and hide the park HUD;
      - no dim backdrop: `DIALOG_BACKGROUND` belongs to
        `WARNING_PANEL`/`REQUEST_PANEL`/`CPGEND_PANEL` only.
      CLI: `--park-click X,Y` (repeatable), `--park-hover X,Y`.
      Evidence: `notes/ui-recon.md` ("Park HUD panel switching"),
      `notes/park-budget.png`, `notes/park-construction-kit.png`,
      `notes/park-lab-breed.png`.
- [x] Minimap: `MAP_RENDERVIEW` (136×136 at 652,451) is a 2-D detail map.
      A 256×256 FBO renders the terrain top-down, centred on the camera
      target and rotated with the camera heading; the retail zoom is a held
      `0.1..2.0` scale (default 0.25, 0.15/s). A red/yellow camera marker is
      drawn when inside the map. See `notes/ui-recon.md` ("Minimap
      implementation").
- [ ] Minimap polish: detail-map water colour and exact partition contents
      (actors unproven), marker clamp at the edge.
- [x] 1024×768 re-anchor pass: `Overlay::layout_for_size` implements
      `moWidget::OnResize`; the right panel lands at 931,168, the gauge at
      293,737 and the map at 840,584, matching the measured retail layout.
      Menu pages still centre their 800×600 screen.
- [ ] Dynamic values inside sub-panels (budget ledger rows, list snapshots,
      sliders).
- [x] Keep the FPS/visitor debug text as an optional debug HUD (`F1`,
      `--no-hud`).

### Phase D — panels, save/load, audio

1. `ConstructionInterface.Ini` build/purchase/save panels; `ListInterface`
   and `Infopanels`.
2. In-park `OPTIONS_PANEL` pause overlay (Options / Save / Load tabs);
   `LOAD_GAME_PAGE` + `OPTIONS_SAVE_PANEL` wired to save files.
3. `MenuSoundDB` clicks/music; optional intro movie.
4. ~~`RadialMenu.Ini`~~ — **legacy/unused**: no executable string references
   its sections or the `RadialMenu` file, and none of its 15 icon names exist
   in `MenuGraphicDB`/`ParkGraphicDB`/`LabGraphicDB`. Do not implement; the
   live build UI is the right-panel + `ConstructionInterface` flow.

## Tests

- Parse counts: MenuInterface 114 sections / 9 pages / 47 buttons;
  ParkInterface 137 sections + includes; MenuGraphicDB 53; loc 56/702.
- `tools/formats/overlay.py` + `tests/test_overlay.py` pin the counts,
  absolute rects and frame strips.
- `tools/viewer/overlay_preview.py` + `tests/test_overlay_preview.py` render
  the main menu, Free Game page and park HUD; the HUD output matches
  `notes/park-after2.png` (right panel MAD 8.8, gauge 3.65) and the Free Game
  page matches `notes/menu-freegame-selected.png` (MAD 2.93; Start/Cancel
  and island picture 0.0).
- All `TEXTURE` names resolve to existing TGAs (allow the known
  `TEMP_jaugebas.tga` miss).
- `TEXT_JUSTIFY` bit values and `TEXT_HEIGHT == font_size`.
- Hit-test: the four harness coordinates hit the expected widgets.
- Anchoring: 800×600 → 1024×768 shifts by (112, 84); `ALIGN_CENTER` centres.
- Screenshot: main menu at 800×600 against the retail reference; start menu
  at 1024×768 for the shift.

## Open items

- Exact selected-vs-pressed frame on bars (6/7) and the `INTRO_BUTTON`
  strip wrap.
- `ZPRIORITY`'s exact helper role in `moWidget::Draw` (render hint; the
  traversal order is solved: `PRIORITY` + declaration order).
- The code path that centres menu pages (no `ALIGN_CENTER` in the INI).
- Exact save-slot validation/naming handler internals (`LAB_0050cc10`,
  `LAB_0050caf0`) and the `PauseGame` key binding.
- Missing `TEMP_jaugebas.tga` (skip the affected `TEMP_*` widgets).
- `TEXTURE_MOSAIC` tiling rule.
- Minimap camera constants (`FUN_00527650`) and zoom mapping
  (`0x00529a90`/`0x00529b60`).

## Evidence

- `notes/ui-recon.md` — addresses, tables and measurements.
- `docs/formats/overlay-ini.md` — format spec and reader contract.
- `tools/formats/overlay.py` + `tests/test_overlay.py` — parser oracle.
- `tools/viewer/overlay_preview.py` + `tests/test_overlay_preview.py` —
  golden menu/Free Game/HUD renders.
- `game/%MAINDIR%/Data/Overlay/*.Ini`, `MenuEng.loc`, `ParkEng.loc`.
- `game/%MAINDIR%/Data/Overlay/pics/Menu_hors_jeu/*.jpg`.
- Ghidra: `FUN_005bca20`, `FUN_005bc000`, `FUN_005c5f00`,
  `FUN_00517c60`, page constructors and handlers listed in the recon note.

No retail assets are copied into `src/`; the menu will read them from
`game/%MAINDIR%` like the rest of the port.
