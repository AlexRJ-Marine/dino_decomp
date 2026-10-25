# UI recon — retail overlay, park HUD and menu (UI-2)

Date: 2026-10-25
Scope: read-only research for the other agent's UI-2 handoff
(`docs/platform/ui-original-menu.md`). No files under `game/` or `original/`
were modified.

## Summary

The retail UI is **data-driven** and the data is in the extracted tree:

- `Data/Overlay/MenuInterface.Ini` — 114 widgets, 9 full-screen pages.
- `Data/Overlay/ParkInterface.Ini` — 137 sections (136 widgets + `#INCLUDE`),
  including 9 more files (2,558 sections): 2,694 widgets and **38 top-level
  panels** once expanded.
- `Data/Overlay/MenuGraphicDB.ini` — 53 named texture rectangles; all 25
  referenced TGAs exist.
- `Data/Overlay/ParkGraphicDB.Ini` — 426 entries + 180 lab entries = 605
  unique (`LAB_NAME_PANEL` repeats identically); 34 unique TGAs, one missing
  (`TEMP_jaugebas.tga`, referenced by nine unused `TEMP_*` gauges).
- `Data/Overlay/MenuEng.loc` (56 records) and `ParkEng.loc` (702).
- `Data/Sound/MenuSoundDB.ini` (5 sounds), `ParkSoundDB.ini` (224).
- `Data/Overlay/pics/Menu_hors_jeu/` — six reference screenshots of the
  original menus, including 800×600 and 1024×768 pairs.

The layout is authored on an **800×600** top-left pixel grid. At other
resolutions widgets keep their pixel size and are **re-anchored**, not scaled
(measured below).

## Evidence: layout semantics

### `moWidget::Load` — `FUN_005bca20`

Reads every property name. Xrefs to the property-name strings all land here:

| Property string | Address |
| --- | --- |
| `TEXT_JUSTIFY` | `0x0066d658` |
| `TEXT_HEIGHT` | `0x0066d674` |
| `TEXT_FONT` | `0x0066d668` |
| `ALIGN_RIGHT` | `0x0066d718` |
| `ZPRIORITY` | `0x0066d774` |
| `TEXTURE_MOSAIC` | `0x0066d6d8` |

Widget flags live at `widget + 0x38`. Observed setters:

| Bit | Property | Setter site (in `OnEvent`) |
| ---: | --- | --- |
| 0 | `VISIBLE` | before the excerpt |
| 4 | `USER_MOVABLE` | before the excerpt |
| 5 | `DRAG_AND_DROP` | before the excerpt |
| 9 | unknown | opcode `0x2a` |
| 10 | `ALIGN_LEFT` | opcode `0x23` |
| 11 | `ALIGN_UP` | opcode `0x25` |
| 12 | `ALIGN_RIGHT` | opcode `0x24` |
| 13 | `ALIGN_DOWN` | opcode `0x26` |
| 14 | `ALIGN_CENTER` | opcode `0x27` |
| 15 | `ALIGN_USER_DEFINED` | opcode `0x28` |

`ALIGN_CENTER` is a real property (used by the lab interfaces, e.g.
`LabInterfaceBreed.Ini:132`) and is present as a string at `0x0066d6fc`.

`TEXT_JUSTIFY` is a bitmask. `FUN_005bca20` compares tokens against
`CENTER`, `X`, `Y`, `LEFT`, `RIGHT`, `UP`, `DOWN` (strings at
`0x0066d650`, `0x0064f7f0`, `0x0064f2ac`, `0x0066d648`, `0x0066d640`,
`0x0066d63c`, `0x0066d634`):

| Token | Bits |
| --- | ---: |
| `LEFT` | 1 |
| `RIGHT` | 2 |
| `X` | 3 |
| `UP` | 4 |
| `DOWN` | 8 |
| `Y` | 12 |
| `CENTER` | 15 |

The INI parser ignores `;` comments, including `;;ALIGN_RIGHT` on child
widgets, so those children stay fixed relative to their parent.

### Resize — `FUN_005bc000`

`OnResize(newW, newH, oldW, oldH)` walks the tree and repositions children:

- no flags → position unchanged;
- `ALIGN_RIGHT` only → `x += newW - oldW`;
- `ALIGN_DOWN` only → `y += newH - oldH`;
- `ALIGN_CENTER` (bit 14) → `x = (newW - width) / 2`,
  `y = (newH - height) / 2`;
- left+right → stretch to keep the right margin;
- up+down → stretch to keep the bottom margin.

Top-level park panels: `NAVIGATION_PANEL`, `RIGHT_PANEL`, `GAUGE_PANEL` and
`MAP_PANEL` are right+down; `RENDERVIEW` and `DIALOG_BACKGROUND` have all four
flags and stretch.

### Measured proof that the UI does not scale

Retail reference JPEGs in `Data/Overlay/pics/Menu_hors_jeu/`:

- `MENU_interf_princip_800x600.jpg` and `MENU_interf_princip_1024x768.jpg`.
- `MENU_jeu_libre_800x600.jpg` and `MENU_jeu_libre_1024x768.jpg`.

Compare the 1024×768 image cropped at `(112, 84)` (half of the resolution
delta) with the 800×600 image:

```text
shift (112,84) MAD   = 1.011     (JPEG noise only)
uniform scale MAD    = 40.331
```

Blue-region bounding boxes:

```text
MENU_interf_princip 800x600  blue bbox 198,155..623,572  (426x418)
MENU_interf_princip 1024x768 blue bbox 310,234..735,656  (426x423)
MENU_jeu_libre      800x600  blue bbox  98,40..512,524   (415x485)
MENU_jeu_libre      1024x768 blue bbox 210,124..624,608  (415x485)
```

Identical pixel sizes, shifted by exactly half the resolution delta. So the
original centres the 800×600 UI at 1024×768; it does not scale it. The menu
page roots are centred by the page code even though `MenuInterface.Ini` does
not set `ALIGN_CENTER` on them (exact helper still open).

## Evidence: menu state machine

### Pages

`MenuInterface.Ini` has nine `PARENT = "ROOT"` pages, all 800×600,
`VISIBLE = 0` until the page is activated:

| Index | Page | Constructor | Widgets looked up |
| ---: | --- | --- | --- |
| 0 | `MAIN_MENU_PAGE` | `FUN_0051bbf0` | page, Free/Campaign/Load/Options/Cinematics/Credits/Quit buttons |
| 1 | `FREE_GAME_PAGE` | `FUN_0051b120` | page, `ISLAND_LIST`, Start/Cancel, Easy/Middle/Hard |
| 2 | `CAMPAIGN_SELECT_PAGE` | `FUN_0051aa90` | page, `CAMPAIGN_LIST`, Start/Cancel |
| 3 | `CAMPAIGN_INFO_PAGE` | `FUN_0051a560` | page, description, Start/Cancel |
| 4 | `LOAD_GAME_PAGE` | `FUN_00519ba0` | page, `LOAD_GAME_NAMES_LIST`, Load/Cancel |
| 5 | `OPTIONS_PAGE` | `FUN_00518830` | page, 3 HSCROLLs, 6 indications, OK/Cancel |
| 6 | `CINEMATICS_PAGE` | `FUN_00518210` | page, `CINEMATICS_LIST`, Start/Cancel |
| 7 | `CREDITS_PAGE` | `FUN_00517fe0` | page, `CREDITS_OK_BUTTON` |
| 8 | `LOADING_PAGE` | `FUN_00517e90` | page |

`FUN_00517c60` (`mvwMenuStateManager` ctor) constructs those nine objects in
exactly that order and appends them to a list. Page base ctor
`FUN_0051c510(index, name, ctx)` stores the page index and display name.

### Transitions

Each page constructor writes a handler pointer into its button's event
structure at `+0x1c`, `+0x20` or `+0x24`. Clicking (event argument 0) sets
`page + 0x18 = nextState`. Constants read from the handlers:

| Button | Handler | Next state |
| --- | --- | ---: |
| `FREE_GAME_BUTTON` | `0x0051be70` | 1 (`FREE_GAME`) |
| `CAMPAIGN_BUTTON` | `0x0051a710` | 2 (`CAMPAIGN_SELECT`) |
| `LOAD_GAME_BUTTON` | `0x0051bef0` | 4 (`LOAD_GAME`) |
| `OPTIONS_BUTTON` | `0x0051bf70` | 5 (`OPTIONS`) |
| `CINEMATICS_BUTTON` | `0x0051bff0` | 6 (`CINEMATICS`) |
| `CREDITS_BUTTON` | `0x0051c070` | (no `+0x18` write in first 34 insns) |
| `QUIT_BUTTON` | `0x0051c0f0` | (no `+0x18` write; quit path) |
| `FREE_CANCEL_BUTTON` | `0x00518080` | 0 (`MAIN_MENU`) |
| `FREE_START_BUTTON` | `0x0051b370` | 8 (`LOADING`) |

`FREE_START_BUTTON` requires a selected island (`page + 0x40 >= 0`) before
setting state 8. `ISLAND_LIST` and the Easy/Middle/Hard buttons use
`+0x24`/`+0x1c` handlers to update selection state.

The page index is the state: the loading page (8) hands control to the park.
Entry state is page 0 after the intro; the intro is skipped with
`UseIntroVideo=0` in `run/Data/Config/Dino.ini`.

### Click-coordinate cross-check

The x32dbg harness clicks window coordinates. They fall inside the INI
rectangles computed from the page tree:

| Click | Window point | Widget | Absolute rect |
| --- | --- | --- | --- |
| Free Mode | 395,234 | `FREE_GAME_BUTTON` | 288..502 × 223..246 |
| IGUANO | 260,183 | `ISLAND_LIST` | 175..363 × 166..451 |
| easy | 494,451 | `FREE_EASY_BUTTON` | 468..520 × 425..477 |
| Start | 526,531 | `FREE_START_BUTTON` | 502..551 × 506..556 |

This confirms the parent-relative coordinate model with no window scaling.

## Evidence: rendering

- Page backgrounds are **not** in `MenuInterface.Ini`. `MenuGraphicDB.ini`
  defines `BACKGROUND_MAIN_MENU`, `BACKGROUND_FREE_GAME`,
  `BACKGROUND_CAMPAIGN_SELECT`, `BACKGROUND_CAMPAIGN_INFO`,
  `BACKGROUND_LOAD_GAME`, `BACKGROUND_OPTIONS`, `BACKGROUND_CINEMATICS`,
  `BACKGROUND_CREDITS`, `BACKGROUND_LOADING` — all 800×600, e.g.
  `interf_princip_800x600.tga`. Code at `0x0051c16d`, `0x0051c184`,
  `0x0051c1a4` references `BACKGROUND_MAIN_MENU`, presumably a page-background
  helper that creates the widget.
- Sprites: `TEXTURE` names resolve through the matching `*GraphicDB.ini` to
  `FILENAME` + atlas rect `(X, Y, WIDTH, HEIGHT)`.
- Buttons carry `NB_OBJECTS = 8` and a `SKIP_LIST` bitmask. Frames are
  packed compactly using only the complement of the mask; see the frame
  section below.
- Text: `TEXT_FONT` is loaded through `moFontCtrl` (`FUN_004227d0`), which
  calls the known `mgFontDx8::LoadFile` (`0x430fb0`). All HUD strings use
  `TEXT_HEIGHT == font_size` (12/14/19), i.e. scale 1.0 at 800×600.
- Z-order: `PRIORITY` (integer 0..100) and `ZPRIORITY` (float 0..1). The
  exact comparator was not recovered.

## Follow-up: frame strips (`NB_OBJECTS` / `SKIP_LIST`)

`FUN_00421630` (`moGraphicCtrl::Load`) parses GraphicDB entries. Newly
decoded properties: `ALIGN_USER_DEFINED` (bit 15), `SIZE` (X/Y/W/H tuple),
and `TEXTURE_FRAME` (base frame).

- `SKIP_LIST` is a **bitmask**: the loader does
  `mask |= 1 << value` for each token and stores it at `entry + 0x28`.
- Only the complement frames are stored, packed compactly from `(X, Y)`.

Measured evidence (alpha-channel analysis, `notes/ui-atlas-frames.png`):

| Entry | Rect / frames | Storage |
| --- | --- | --- |
| `BUDGET_PANEL_BUTTON` | 0,62 31×31, skip 1 2 3 | 5 bag icons at x = 0, 31, 62, 93, 124 (horizontal) |
| `MINI_MAP_ZOOMIN_BUTTON` | 176,139 16×16, skip 1 2 3 | 5 icons at x = 176..256 (horizontal, exactly fits) |
| `EASY/MIDDLE/HARD_MODE_BUTTON` | Y = 0/52/104, 52×52, skip 0 1 2 3 | 4 icons per row |
| `MAIN_MENU_BUTTON` | 0,0 213×23, skip 0 1 2 3 | vertical; slot 0 blank, bars at y ≈ 25, 48, 71 |
| `ISLAND_LIST_BUTTON` | 0,55 166×34, skip 0 1 2 3 | vertical; slot 0 blank, bars at y ≈ 97, 131, 165 |

So the strip is horizontal when it fits the texture width, otherwise
vertical. The first stored slot can be blank/disabled: **frame 0 is not
always the normal state**. On the main menu the first visible bar is normal,
the next two are hover/pressed.

Runtime storage: the widget keeps the graphic index in the low 16 bits of
`widget + 4`, the frame index in bits 16..23, and mosaic in bit 24
(`FUN_005c3a20`, `FUN_005c3a60`, `FUN_005c3a40`).

### Observed state frames

The retail 800×600 park screenshot (`notes/park-after2.png`) pinned the
normal frames; the first attempt (first inked frame) rendered gray disabled
icons, which the screenshot comparison exposed.

| Control family | Disabled | Normal | Hover | Pressed | Selected |
| --- | ---: | ---: | ---: | ---: | ---: |
| Menu/list bars | — | 5 (dark) | 6 | 7 | 7 (bright) |
| Right-panel icons | 0 (gray) | 4 (colour) | 5 (green) | 6 | 7 (yellow) |
| Difficulty icons | — | 4 (blue) | 5 (orange) | 6 (green) | 7 (yellow ring) |
| Start/Cancel | — | 0 (gray) | 4 | 5 | 7 |

This is empirical (art + screenshot), not yet the engine's state table. The
exact table and the `INTRO_BUTTON` strip wrap remain open.

## Text/style edge cases

- One widget renders text at a non-default scale: `BREED_SWITCH_PHYSICAL`
  (`ParkInterface.Ini` + `LabInterfaceBreed.Ini`) uses `FONT.FNT` (12 px)
  with `TEXT_HEIGHT=16`, i.e. scale 1.33.
- `MESSAGE_PANEL` lines 2–4 fade: `TEXT_COLOR` alpha 192/128/64 with matching
  black `LINE*_BACKGROUND` alphas. The renderer must honour `TEXT_COLOR`
  alpha.
- 2,422 `HINT` fields exist; 20 do not resolve in either loc file, mostly
  `RadialMenu.Ini` keys and `&Labomodify` (likely a typo/case mismatch).
- Park start visibility: `RENDERVIEW`, `NAVIGATION_PANEL`, `RIGHT_PANEL`,
  `GAUGE_PANEL`, `MAP_PANEL`, `MESSAGE_PANEL` (ENABLE=0). `DECO_BOUT_GAUCHE`
  is `VISIBLE=0` at park start (the retail bottom-left piece is the gauge
  panel's left edge at x=69); it is shown later by the build/UI code.

## Retail property usage

Across the 20 overlay INIs (expanded, 5,388 widgets):

- Types: 3,900 `WIDGET`, 899 `BUTTON`, 370 `PROGRESS`, 95 `GROUP`,
  50 `EDIT`, 22 `LIST`, 16 `RENDER`, 15 `VSCROLL`, 12 `CURVE`,
  9 `HSCROLL`. **No `WINDOW`, `COMBO` or `RADIO` usage.**
- `TEXT_JUSTIFY`: `CENTER` 617, `LEFT` 16, `DOWN` 6. No `RIGHT`/`X`/`Y`/`UP`.
- `BTN_SUB_TYPE`: `SWITCH` 334. No `CHECK_BOX`.
- `ALIGN_CENTER`: 23 (lab backgrounds/panels). `ALIGN_USER_DEFINED`,
  `SIZE` and `TEXTURE_MOSAIC`: **0 uses** — parse them, but they are not
  needed for parity.
- `EQUIVALENT`: 108, all internal scrollbar/list-line widgets
  (`LbVs`, `VsBtnUp`, `L0i0`, …).
- `PRIORITY`: 1,339 (1,327 at 10, 12 at 1); the rest default 0.
  `ZPRIORITY`: 1,636 widgets, range 0.004–0.9, 20 distinct values.

## In-park options / pause (partial)

`FUN_0050c1a0` is the in-park options controller. It looks up and binds:

| Widget | Handler |
| --- | --- |
| `OPTIONS_PANEL` | root container |
| `OPTIONS_MENU_PANEL` | Options / Save / Load buttons (`+0x24 = LAB_0050c7a0` in a 3-iteration loop) |
| `OPTIONS_OPTIONS_PANEL` | settings tab |
| `OPTIONS_SAVE_PANEL` | save/load tab |
| `OPTIONS_SAVE_NAMES_LIST` | `+0x24 = LAB_0050cc10`, `+0x1c = LAB_0050cd50` |
| `OPTIONS_NAME_SLOT` | `+0x24 = LAB_0050cec0` |
| `OPTIONS_VALIDATE_BUTTON` | `+0x20 = LAB_0050caf0` |
| `OPTIONS_MUSIC_HS`, `OPTIONS_BRUITAGE_HS`, `OPTIONS_GAMESPEED_HS` | settings sliders |

`FUN_005257a0` is the camera/navigation controller. It binds the
`CameraKey*` actions and `PauseGame`; the `PauseGame` action handler is
`LAB_00525e80`, which opens this options overlay. The same function binds
`NAVIGATION_GORIGHT/GOLEFT/GOTOP/GOBOTTOM/TURNRIGHT/TURNLEFT/ZOOM/DEZOOM`
with click/release handlers (`+0x1c`/`+0x20`).

So there is no separate pause screen: **ESC/pause opens `OPTIONS_PANEL`**,
with `OPTIONS_MENU_PANEL` tabs for options, save and load. The `&PauseGame`
loc string is the action/key label, not a panel title.

## Golden preview oracle

`tools/viewer/overlay_preview.py` renders a page/HUD from the same data:

```text
python tools/viewer/overlay_preview.py --page MAIN_MENU_PAGE \
  --out notes/menu-preview.png
python tools/viewer/overlay_preview.py --page FREE_GAME_PAGE \
  --island 1 --difficulty easy --out notes/menu-freegame-preview.png
python tools/viewer/overlay_preview.py --hud --out notes/hud-preview.png
```

The main-menu and Free Game previews match the retail screenshots: same art,
button bars, island rows, selected-row highlight, difficulty ring and
Start/Cancel icons. The HUD preview matches `notes/park-after2.png` on the
right panel (MAD 8.8), gauge (12.0) and nav panel; the remaining difference
is the background and the empty minimap render target. It is the oracle the
C++ `src/ui/` layer is checked against.

## Evidence: text and sound

- `TEXT` values beginning with `&` are field IDs into the `.loc` files
  (`lcFileL.cpp`, string at `0x00656584`).
- `MenuEng.loc`: 56 records, resolves `&FreeGame` → `FREE MODE`,
  `&Options` → `OPTIONS`, `&Ok` → `OK`.
- `ParkEng.loc`: 702 records; resolves `&Budget` → `Budget`,
  `&CurrentTime` → `Time`, `&CurrentDay` → `Day`.
- Some INI text keys are **runtime values**, not loc strings:
  `&VisitorAmount`, `&BudgetAmount`, `&dinoAmount`, `&ShowAmount` do not
  exist in either loc file and are set from code.
- `MenuSoundDB.ini`: `clicDefault` (`data/Sound/Interface/clic#3.wav`),
  `clic` (`clic#2.wav`), `exitClic` (`clic#9.wav`),
  `music` (`data/Sound/Musics/musLabo.wav`, looped/streamed), `mixer`.
  Every menu button handler references `clicDefault`; only `QUIT_BUTTON`
  references `exitClic`, so those are the two click sounds to wire first.
- `Data/Video/intro.mpg`; config `UseIntroVideo` at `0x0065e910`.
- `RadialMenu.Ini` (22 sections) defines the parked build/context menus
  (`SHOW_RADIAL_MENU`, `BUILDING_RADIAL_MENU`, `PADDOCK_RADIAL_MENU`, …).
- Save/load: `LOAD_GAME_PAGE` is the main-menu load screen. In the park,
  `ConstructionInterface.Ini` holds `OPTIONS_MENU_PANEL` with
  `OPTIONS_MENU_SAVE_BUTTON` / `OPTIONS_MENU_LOAD_BUTTON` (28×28, hints
  `&SaveGame`/`&LoadGame`) and `OPTIONS_SAVE_PANEL` (250×361, hidden by
  default) with `OPTIONS_SAVE_NAMES_LIST`, the `OPTIONS_NAME_SLOT` edit box
  and `OPTIONS_VALIDATE_BUTTON`/`OPTIONS_UNVALIDATE_BUTTON`. The
  `DINO_ACHAT_SAVE_*` sections are the dinosaur-purchase panel, not save.
  Save paths are already known from RUN-2 (`run/save/slotNNN.sav`).

## Asset inventory (overlay INIs)

| File | Sections | Types |
| --- | ---: | --- |
| `ParkInterface.Ini` | 137 | 99 WIDGET, 26 BUTTON, 6 EDIT, 3 LIST, 2 RENDER |
| `ConstructionInterface.Ini` | 705 | 434 WIDGET, 150 BUTTON, 101 PROGRESS, … |
| `Infopanels.Ini` | 677 | 607 WIDGET, 42 BUTTON, 17 PROGRESS, 6 CURVE, … |
| `ListInterface.Ini` | 336 | 222 WIDGET, 91 BUTTON, 13 GROUP, 10 EDIT |
| `LabInterfaceBreed.Ini` | 245 | 130 WIDGET, 82 BUTTON, 20 GROUP, 11 PROGRESS, … |
| `LabInterfaceMain.Ini` | 159 | 124 WIDGET, 22 PROGRESS, 7 BUTTON, 2 RENDER, … |
| `MissionInfo.Ini` | 136 | 124 WIDGET, 8 BUTTON, 2 LIST, 2 VSCROLL |
| `LabInterfaceModify.Ini` | 105 | 83 WIDGET, 12 PROGRESS, 7 BUTTON |
| `LabInterfaceFridge.Ini` | 100 | 72 WIDGET, 12 BUTTON, 11 PROGRESS |
| `LabInterfaceClone.Ini` | 95 | 75 WIDGET, 11 PROGRESS, 5 BUTTON |
| `MenuInterface.Ini` | 114 | 55 WIDGET, 47 BUTTON, 5 LIST, 3 VSCROLL, 3 HSCROLL, 1 GROUP |
| `RadialMenu.Ini` | 22 | 18 BUTTON, 4 WIDGET |

## Reference parser

`tools/formats/overlay.py` implements this note:

```text
python tools/formats/overlay.py dump <ini> [--root NAME]
python tools/formats/overlay.py scan <overlay-dir>
python tools/formats/overlay.py graphic <db.ini> <NAME>
```

It handles `#INCLUDE`, comments, quoted values, the parent tree, absolute
rects, `TEXT_JUSTIFY`, loc resolution, the resize pass and the frame-strip
model. `tests/test_overlay.py` pins the retail counts and geometry:

- `MenuInterface.Ini`: 114 widgets, 9 pages, 47 buttons.
- `ParkInterface.Ini` expanded: 2,694 widgets, 38 roots, 2,595 sections.
- `MenuGraphicDB.ini`: 53; `ParkGraphicDB.ini` + lab: 605 unique.
- `FREE_GAME_BUTTON` at 288,223 214×23; `VISITOR_AMOUNT` at 228,578;
  `PARK_CURRENT_TIME` at 729,389.
- Frame strips: budget 5 across; zoom at 176..256; island list vertical at
  y = 55, 89, 123, 157.

There are no duplicate section names in `MenuInterface.Ini` or
`ParkInterface.Ini`; the only GraphicDB name collision is the identical
`LAB_NAME_PANEL` in both park and lab databases.

## Not done / open

- Dynamic run at 1024×768 in the park to confirm HUD anchoring (the menu
  reference screenshots prove the model, but not the park HUD).
- Exact button state→frame table (normal/hover/pressed) and the
  `INTRO_BUTTON` strip wrap.
- `PRIORITY`/`ZPRIORITY` draw ordering.
- `EQUIVALENT` and `USER_ID` semantics for internal scrollbar widgets.
- The code path that centres menu pages (no `ALIGN_CENTER` in the INI).
- The pause-menu flow (`&PauseGame` exists in `ParkEng.loc`; no `PAUSE`
  section exists in the overlay INIs).
- Exact save-panel button wiring.

## Evidence paths

- `game/%MAINDIR%/Data/Overlay/*.Ini`, `MenuEng.loc`, `ParkEng.loc`.
- `game/%MAINDIR%/Data/Overlay/pics/Menu_hors_jeu/*.jpg`.
- `game/%MAINDIR%/Data/Sound/MenuSoundDB.ini`.
- Ghidra functions/addresses listed above; binary `/DinoIsland.exe-ebea17`.
