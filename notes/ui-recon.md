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

## Draw order (solved)

- `moWidget::Draw` is `FUN_005c5bf0` (vtable slot `+0x38`). It draws the
  widget itself, then walks the child linked list at `widget + 4`. For each
  child it checks the `VISIBLE` flag (bit 0) and calls the child vtable
  `+0x34` (`FUN_005c5d60`), which calls `Draw` (`+0x38`) then `+0x3c`. The
  special child at `widget + 0x44` is drawn last.
- Children are kept in a priority-ordered list built by the widget manager
  (`FUN_005bae40`) with the priority key at `widget + 0x34`; equal priority
  keeps INI declaration order. So draw order is: parent first, then children
  by `PRIORITY` ascending, insertion order within a priority.
- `ZPRIORITY` (`widget + 0x40`) is read by `Draw` and passed to helper calls
  guarded by flag bits `0x80`/`0x40`; it is a per-widget render hint, not the
  child traversal order. Exact helper role (shadow/border/offset) still open.
- The Python preview sorts by `(PRIORITY, declaration order)`; retail MAD is
  unchanged versus the previous `ZPRIORITY` tie-break, so this is the model
  to port.

## Frame strips (follow-up)

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
vertical. The first stored slot (frame 4) is the **normal** state and may be
fully transparent; the visible bars are frames 5–7 (hover/pressed/selected).
Frame 0 is the disabled art, not normal.

Runtime storage: the widget keeps the graphic index in the low 16 bits of
`widget + 4`, the frame index in bits 16..23, and mosaic in bit 24
(`FUN_005c3a20`, `FUN_005c3a60`, `FUN_005c3a40`).

### Observed state frames

The mapping is universal: **0 = disabled, 4 = normal, 5 = hover,
6 = pressed, 7 = selected**. `SKIP_LIST` removes animation frames 1–3, not
states. Frame 4 may be fully transparent (menu/list bars) — that is why
normal rows show no bar; do not skip it.

The first attempt used "first inked frame", which rendered gray disabled
icons and dark bones. Comparing with the retail screenshots fixed it:

- Start/Cancel with frame 4 now match the retail Free Game screenshot
  exactly (MAD 0.0).
- Park gauge icons with frame 4: MAD 3.65 (was 12.0 with frame 0).
- Full Free Game page: MAD 2.93, mostly text anti-aliasing.

Remaining ambiguity: whether selected vs pressed on bars is frame 6 or 7;
normal = 4 is certain.

The preview oracle is the implementation reference.

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

## Widget type enum and constructors

`FUN_005ba6b0` maps the `TYPE` string to an enum; `FUN_005ba8f0` calls the
constructor:

| Type | Enum | Constructor |
| --- | ---: | --- |
| `WIDGET` | 0 | `0x005bc220` |
| `BUTTON` | 1 | `0x005c0380` |
| `EDIT` | 2 | `0x005c3ed0` |
| `PROGRESS` | 3 | `0x005bf900` |
| `GROUP` | 4 | `0x005bec80` |
| `HSCROLL` | 5 | `0x005be620` |
| `VSCROLL` | 6 | `0x005c45b0` |
| `LIST` | 7 | `0x005bd5d0` |
| `COMBO` | 8 | `0x005c4ba0` |
| `CURVE` | 9 | `0x005c5140` |
| `RENDER` | 10 | (render view, no widget ctor in the switch) |

`WINDOW` is not handled (and unused). Note: **`0x005bf900` is the PROGRESS
bar** (the 8-colour gradient/threshold array), not a button. The plain
button constructor is `0x005c0380`.

## Progress bars

`PROGRESS` (type 3) is `FUN_005bf900`. Its constructor seeds four colour
stops at 25/50/75/100 with colours `(0,0,255)`, `(0,127,255)`,
`(0,255,255)`, `(255,0,0)` — a blue→cyan→red gauge. Retail usages are tiny
attribute gauges, e.g. `CONSTRUC_RESTAURANT1_PROFIL1_GAUGE` 20×4 with
`PB_MAX_PROGRESS=100`, `PB_CURRENT_PROGRESS=50`; they set no texture or
colours. Draw a filled rect of `current/max` with the threshold colour (exact
stop mapping open), plus `PB_VERTICAL` for orientation.

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

Geometry at 800×600 (parent-relative absolute):

| Widget | Rect | Default |
| --- | --- | --- |
| `OPTIONS_PANEL` | 250,50 250×411 | hidden |
| `OPTIONS_MENU_PANEL` (tabs) | 292,50 112×48 | visible |
| `OPTIONS_OPTIONS_PANEL` | 250,110 250×351 | visible |
| `OPTIONS_SAVE_PANEL` | 250,100 250×361 | hidden |
| `OPTIONS_SAVE_NAMES_LIST` | 282,125 143×216 | visible |
| `OPTIONS_NAME_PANEL` | 278,370 143×27 | visible |
| `OPTIONS_NAME_SLOT` (edit) | 285,375 128×14 | visible |
| `OPTIONS_VALIDATE_BUTTON` | 320,422 28×28 | visible |
| `OPTIONS_UNVALIDATE_BUTTON` | 350,422 28×28 | visible |

**Do not use `notes/save-panel.png` as the save-panel reference**: it is the
`DINO_ACHAT_SAVE_PANEL` dinosaur-purchase screen, not `OPTIONS_SAVE_PANEL`.

## Minimap render view

- `MAP_RENDERVIEW` is a `RENDER` widget: 136×136 at panel offset (36,35), i.e.
  absolute (652,451) at 800×600. It is a render target, **not** a basemap
  crop.
- `FUN_00529880` binds `MAP_PANEL`, `MAP_RENDERVIEW`,
  `MAP_PANEL_ZOOMIN_BUTTON` and `MAP_PANEL_ZOOMOUT_BUTTON`. Zoom handlers:
  `0x00529a90`/`0x00529b60` (click), `0x00529b40`/`0x00529c10` (release).
- `pvwPark3DRenderView` (`FUN_00527650`) creates the park camera/renderer.
  Candidate camera constants copied to the camera object (meaning not fully
  decoded): `40.0, 1.57, -0.7853, 10000.0, 2.0, 10.0, 10.0, 1000.0, 1000.0,
  3.0`. The `-0.7853` is −45°, which matches the retail minimap's rotated
  island; `40.0` is likely the field of view.
- Practical port spec: render the terrain (and later actors) top-down into
  the 136×136 view with the park camera's yaw (≈−45°), orthographic or a
  high perspective camera; zoom buttons scale the view. A basemap crop is a
  fallback, not parity.

## HUD at 1024×768 (anchor pass)

Applying `moWidget::OnResize` to `ParkInterface` at 1024×768 gives:

```text
RENDERVIEW          0    0  1024x768   (stretch)
NAVIGATION_PANEL  864  457    83x128   (right+down)
RIGHT_PANEL       931  168    93x417   (right+down)
GAUGE_PANEL       293  737   618x31    (right+down)
MAP_PANEL         840  584   184x184   (right+down)
DECO_BOUT_GAUCHE    0  572    70x47    (fixed)
MESSAGE_PANEL     100   20   202x82    (fixed)
```

Note that the right panel moves **down** by 168 as well as right: it has
`ALIGN_RIGHT`/`ALIGN_DOWN` but not `ALIGN_UP`, so the top-right area is
empty at 1024×768. Menu pages behave differently: they are centred
(measured +112,+84), not anchored. The current viewer centres the 800×600
overlay in the window, which is a QOL choice rather than the original park
behaviour; QOL-2 should decide whether to re-anchor, letterbox or scale.
Rendered reference: `notes/hud-1024.png`.

## Park HUD button bindings

`FUN_00527d30` binds the in-park HUD buttons (click handlers at event slot
`+0x24`; click is event argument 0). Handler labels:

| Widget | Handler |
| --- | --- |
| `OPEN_LABORATORY` | `0x005285e0` |
| `OPEN_CONSTRUCTION` | `0x00528510` |
| `OPEN_BUILDING` | `0x005286a0` |
| `OPEN_TRANSPORT` | `0x00528770` |
| `OPEN_PURCHASE` | `0x00528840` |
| `OPEN_OPTIONS` | `0x00528910` |
| `OPEN_BUDGET_PANEL` | `0x005289e0` |
| `OPEN_VISITOR_PANEL` | `0x00528ab0` |
| `OPEN_DINO_LIST_PANEL` | `0x00528b80` |
| `OPEN_SHOW_LIST_PANEL` | `0x00528ba0` |
| `OPEN_PADDOCK_LIST_PANEL` | `0x00528df0` |
| `OPEN_SELLING` | `0x00528f90` |

The handlers call the park state manager (`[this+0x10]`) to switch the active
panel; `OPEN_BUDGET_PANEL` sets state 5. The six right-panel buttons open the
lab/construction/building/transport/purchase/options panels; the five gauge
buttons open the budget/visitor/dino-list/show-list/paddock-list panels.
`FUN_00525340` is an older binder for the same six right-panel buttons.

## Unused / legacy data

- `RadialMenu.Ini` describes a clean 8-slot radial menu (200×200 container,
  40×40 buttons at `(80,5)`, `(133,27)`, `(155,80)`, `(133,133)`, `(80,155)`,
  `(27,133)`, `(5,80)`, `(27,27)`), but it is **dead data**: the executable
  has no string references to `RadialMenu` or its section names, and none of
  its 15 icon names exist in any GraphicDB. Do not implement it.
- `WINDOW`, `COMBO`, `CHECK_BOX`, `SIZE`, `TEXTURE_MOSAIC` and
  `ALIGN_USER_DEFINED` also never occur in retail data; parse them for
  completeness but they are not needed for parity.

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
Start/Cancel icons. The Free Game page is MAD 2.93 overall (Start/Cancel and
the island picture 0.0). The HUD preview matches `notes/park-after2.png` on
the right panel (MAD 8.8) and gauge (3.65); the nav-panel difference is
background only (the panel pixels match). The remaining deltas are text
anti-aliasing and the empty minimap render target. It is the oracle the
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

## Park HUD panel switching (implementation follow-up)

The C++ `src/ui/park.cpp` now follows this recovered state model.

`FUN_00527d30` binds the 12 right-panel/gauge buttons. Each handler runs
`if (arg2 == 0)`, reads the state manager at `widget+0x5c`, and writes the
next state id to `S+0x18` before calling `S->Deactivate()` (vtable `+8`):

| Widget | Handler | Next state |
| --- | --- | --- |
| `OPEN_LABORATORY` | `0x005285e0` | `0` (LAB_MENU) |
| `OPEN_CONSTRUCTION` | `0x00528510` | `0xC`, or `5` when already `0xC` |
| `OPEN_BUILDING` | `0x005286a0` | `0x11`, or `5` |
| `OPEN_TRANSPORT` | `0x00528770` | `0x10`, or `5` |
| `OPEN_PURCHASE` | `0x00528840` | `0x13`, or `5` |
| `OPEN_OPTIONS` | `0x00528910` | `5` + info page 7/8 |
| `OPEN_BUDGET_PANEL` | `0x005289e0` | `5` + info page 4/8 |
| `OPEN_VISITOR_PANEL` | `0x00528ab0` | `5` + info page 5/8 |
| `OPEN_SHOW_LIST_PANEL` | `0x00528ba0` | `5` + info page 1/8 |
| `OPEN_PADDOCK_LIST_PANEL` | `0x00528df0` | `5` + info page 2/8 |
| `OPEN_DINO_LIST_PANEL` | `0x00529740` | `5`, info page 0/8 |
| `OPEN_SELLING` | `0x00528f90` | `0x14` (VK_INSERT toggle) |

The info-panel controller is `ivwInfoPanelController::SetActivePanel`
`0x0050be00`; slots 0..8 map to `DINO_LIST_PANEL`, `ATTRACTION_LIST_PANEL`,
`FENCE_LIST_PANEL`, `ATTRACTION_STATS_PANEL`, `BUDGET_PANEL`,
`VISITEURS_STATS_PANEL`, `MISSION_PANEL`, `OPTIONS_PANEL`, "none".
Consequences:

- The info panels share **one** active slot: opening one closes the previous.
- `OPEN_PADDOCK_LIST_PANEL` opens `FENCE_LIST_PANEL`, not `CAGE_LIST_PANEL`.
  `CAGE_LIST_PANEL` is never selected by the HUD controller (legacy data).
- `CONSTRUCTION_KIT_LIST_ICON` opens `ATTRACTION_LIST_PANEL`; the kit's stats
  icon and `ATTRACTION_LIST_OUTCOME_STATS` open `ATTRACTION_STATS_PANEL`
  (page 3); `ATTRACTION_STATS_OUTCOME_CANCEL` closes (page 8).
- Construction sub-states `0xD` (kit), `0xE` (predefined show) and `0xF`
  (fence) deactivate state `0xC`, so the three-button `CONSTRUCTION_PANEL`
  bar is hidden while one of them is open. The bar is not a permanent tab
  strip.
- `DIALOG_BACKGROUND` is referenced by exactly one function (`0x00523ef3`)
  and is cached as `iface+0xbc`; the only children of that root are
  `WARNING_PANEL`, `REQUEST_PANEL` and `CPGEND_PANEL`. No info panel, lab or
  purchase state shows it, so the port does not draw a dim backdrop behind
  park panels.
- Lab state 0 (`LAB_MENU`) shows `DINO_LAB_MAIN_CENTER` and activates
  `LAB_MAIN_BACKGROUND`; its Deactivate hides both. Each sub-centre
  (`LAB_BREED/MODIFY/FRIDGE/CLONE_CENTER`) has its own 800x600 background
  (`LAB_*_BACKGROUND`, stored `VISIBLE=0` and shown by the state), so the
  port shows exactly one lab centre at a time.
- State `0x13` (`DINOSAUR_PURCHASE`) activates only `DINO_ACHAT_SAVE_PANEL`
  and no lab background.

Evidence: decompiled handlers above; `FUN_00529120` registers the state
ctors; state table from `FUN_0052bd10(id, name, ...)` calls. The C++ table is
`kOpeners` / `kClosers` / `kLabTabs` in `src/ui/park.cpp`.

## Free Game start -> park handoff

The menu never loads the island itself; the loading page is only a cover
frame. Recovered chain:

1. `FREE_START_BUTTON` handler `0x0051b370`:
   - requires `page+0x40 >= 0` (selected island; the page ctor initialises it
     to -1 and `ISLAND_LIST` updates it through `0x0051b8a0`);
   - writes the game context `Y+0x40 = 0` (free-game mode) and
     `Y+0x24 = 999` (no campaign);
   - writes controller action `0` and requests menu state `8`;
   - plays `clicDefault` and calls the page Deactivate.
   It does **not** read difficulty.
2. Difficulty buttons call `0x0051bb30(page, 0/1/2)`, which writes `Y+0x44`.
3. `LOADING_PAGE` (`FUN_00517e90`, vtable `0x6428d4`): Activate `0x517ef0`
   sets `controller+0x10 = 1` (start request); Update `0x51c580` is an empty
   stub. There is no loader call in the page.
4. `daMainApplication` update `0x402460` consumes the request, destroys the
   menu interface and calls `0x00403e10` (new game) or `0x00402f10` (load).
5. `FUN_00403e10` for free game calls `FUN_0051d1d0` (park init) and
   `FUN_0051dad0(record+0xc, 1)`.
6. `FUN_0051dad0` builds `Data\Design\Landscape\<name>\<name>.lnd`; `<name>`
   comes from `IslandList.ini` (`0x65ea4c`, read by `FUN_00506530`), whose
   order is `isl01..isl10`. Everything else (basemap, heightmap, skybox,
   water, occupation, music, 25 vegetation `.imp` slots) is inside the
   `.lnd`.
7. Free-game scenario `Data/Design/Scenario/FreeGame.ini` (`FUN_0055dab0`),
   difficulty at `scenario+0x50` (`FUN_00561bb0`):
   - easy: `Budget=75000`, `SellBackFactor=1.0`, `VisProfiles`;
   - middle: `BudgetMedium=50000`, `SellBackFactorMedium=0.5`,
     `VisProfilesMedium`;
   - hard: `BudgetHard=25000`, `SellBackFactorHard=0.2`, `VisProfilesHard`.
   DINOSTORE and LABORATORY are not difficulty-suffixed.

Port mapping: `src/viewer/main.cpp` reads `IslandList.ini` and the three
budgets; on the loading timer expiry it calls `load_island(name)`, which
rebuilds terrain/vegetation/basemap/sky/water and the GPU resources, sets the
HUD budget and recentres the camera. `--menu-click` sequences exercise the
same path as the retail screens.

Open: the original park camera's initial position is not yet recovered; the
port still uses its orbit auto-framing (`bounds.extent() * 1.7`).

## Minimap implementation (follow-up)

The minimap is a **2-D detail map**, not a second 3-D camera:

- `MAP_RENDERVIEW` is the RENDER widget type (10) with `RV_ZMIN`/`RV_ZMAX`
  fields; drawing is delegated to the owner (event id 10) and the detail-map
  partition renders into a **256x256** target, displayed in the 136x136
  widget.
- The four zoom handlers do not touch the camera. They set held flags
  `+0x24`/`+0x25` on the map view; `FUN_005fb7e0` applies
  `+/- dt*0.15` per second to the map scale at `+0x38`. Default **0.25**,
  clamped **0.1..2.0**.
- The map quad is centred on the **camera target**:
  `FUN_005fb120` writes `x = tgt.x/1024`, `y = 1 - tgt.y/1024` and
  `angle = 90deg + atan2(tgt.y-pos.y, tgt.x-pos.x)`, i.e. it rotates with the
  park camera and is not fixed at -45 degrees.
- The park camera is the only 3-D camera (`pvwPark3DRenderView+0x10`); per
  frame `FUN_00529d00` sets FOV 80 deg, aspect 4:3, near >= 1, far 1000. The
  ten constants at `0x5278b5` belong to `pvwParkObserver`, not the minimap:
  1.57 rad turn rate, height clamps 2..10000, X/Y clamps 10..1000.

Port mapping (`src/viewer/main.cpp`):

- 256x256 FBO, displayed at `MAP_RENDERVIEW` (652,451,136x136);
- top-down orthographic terrain render centred on the camera target and
  rotated so the camera heading is up;
- visible world width = `2 * world_size * scale`; default 0.25 shows the whole
  1024-unit island;
- zoom buttons are hold-to-zoom at 0.15/s in `0.1..2.0`; `--park-click` uses
  0.05 per scripted click; `--minimap-scale N` overrides the start value;
- red camera marker with a yellow heading tick, only when inside the map;
- the FBO is re-rendered only when the target, heading or scale changes.

Open: the detail-map water colour (retail teal vs our basemap/clear edge) and
the exact detail-map texture/partition contents (actors unproven). Evidence:
`notes/minimap-isl01.png`, `notes/minimap-isl01-detail.png`,
`notes/minimap-isl01-zoom-detail.png`, `notes/minimap-isl10-detail.png`,
`notes/retail-minimap-crop.png`.

## 1024x768 park HUD anchoring (implementation follow-up)

`Overlay::layout_for_size(view_w, view_h)` now implements
`moWidget::OnResize` for the park HUD:

- every widget is reset to its `BASE_X/Y/WIDTH/HEIGHT` (the 800x600 authoring
  values);
- roots resize against the 800x600 design space, children against their
  parent's base size;
- `ALIGN_CENTER` centres both axes; `ALIGN_LEFT+ALIGN_RIGHT` stretches width
  by `newW-oldW`; right-only shifts x; the same rules apply vertically;
- the park HUD is re-laid out whenever the logical window size changes; the
  minimap quad and its marker use `MAP_RENDERVIEW`'s anchored rect.

At 1024x768 this gives exactly the measured retail rects:

```text
NAVIGATION_PANEL  864  457    83x128
RIGHT_PANEL       931  168    93x417
GAUGE_PANEL       293  737   618x31
MAP_PANEL         840  584   184x184
MAP_RENDERVIEW    876  619   136x136
```

Port screenshot: `notes/park-hud-1024.png`; oracle: `notes/hud-1024.png`.
Panel MAD at 1024: right 12.7, gauge 4.45 (nav background differs as before).
Menu pages remain centred at 800x600 (unchanged).

## Park clock (implementation follow-up)

`park_clock` in `src/viewer/main.cpp` mirrors the time manager:

- `time_scale` is **not** `GameSpeed`: the schedule manager receives a float
  from the park/scene object (`+0x1a0`); `GeneralParams.bgd` stores `60.0`
  next to the `10` written at `+0x1a0`, and 60.0 matches the observed pace.
  GameSpeed only sets the scheduler timer;
  hours/sec = scale/3600, minutes/sec = scale/60;
- `advance`: `int((elapsed+dt)*hours_per_sec) >= 11` sets the end-of-day flag
  and returns; a changed integer hour sets `new_hour`; otherwise elapsed
  advances (0x0058c2f0);
- end of day: day++, flags clear, elapsed=0 (0x0058c2d0), matching
  `0x00524630`; the simulation/economy daily passes are not simulated yet;
- read: `hour = int(elapsed*hps)+base_hour`, `minute = int(elapsed*mps)%60`,
  `day` (0x0058c350); HUD `time = "%02d:%02d"` and `day = day+1`
  (0x00526010). The retail Free Game screenshot `notes/park-after2.png`
  shows 09:44 with no AM/PM suffix; `0x5234e0` clears `panel+0x8d`, so the
  port always uses 24-hour text;
- the base hour is **9** (hardcoded `PUSH 9` in the schedule manager ctor);
  the day counter starts at 0 (HUD day 1);
- `--park-time HH:MM` seeds elapsed for screenshots/tests, and
  `--park-time-scale N` overrides 60.0.

Evidence: `notes/clock-0944.png`, `notes/clock-0944-detail.png`,
`tests/test_viewer_cpp.py::test_cpp_park_clock`, `docs/decomp/park-time.md`.

## Start camera probe (negative)

`pvwPark3DRenderView` (`FUN_00527650`) sets the observer to eye
`(10,10,140)`, target `(10,600,0)`, up `(0,0,1)` in the original 0..1024
park space. Mapping that to the port's centred 1024 world
(`(-502,-502,140) -> (-502,88,0)`) and screenshotting a Free Game start
shows open ocean and two islets, not the park. That camera is therefore a
construction default that the retail start path overrides (camera script /
`camSwitcher`) before the first visible park frame. The port keeps its orbit
auto-framing until the override is found.

Next probe: trace writes to the observer position/target after `0x00527650`
(the save's 10 camera floats and `camSwitcher` are candidates; see
`docs/formats/sav.md`).

## Start camera solved (correction to the negative probe)

The ctor constants were misread: `0x43c80000` is **400.0**, not 10.0, so
`pvwPark3DRenderView` builds eye `(400,400,140)`, target `(400,600,0)`,
FOV 62 deg. Those values are overwritten on the first park frame by
`camManager/camSwitcher::Update` (`FUN_005e5960`), called from
`pvwParkObserver::Update` (`FUN_00529d00`). The saved camera is the same
10-float state (`FUN_005e5fb0`): eye, target, progress, yaw.

A new Free Game starts from the camera defaults + `Data/Config/camera.ini`:

```text
target  = island centre (512,512, terrain_z)      # terrain_z = 32 on isl01
progress= 0.1
distance= lerp(DistanceMin 10, DistanceMax 300, 0.1) = 39.0
elevation=lerp(ElevationMin 3, ElevationMax 500, 0.1) = 52.7 (added to Z)
yaw     = 0
eye     = target + (39*cos yaw, 39*sin yaw, 52.7)
        = (551.0, 512, 84.7)
```

Confirmed by `run/save/slot001.sav`:
`(550.998, 512, 84.696, 512, 512, 32, 0.1, 0, 0, 0)`.
The per-frame projection is FOV **80 deg**, aspect 4:3, near >= 1, far 1000
(`FUN_00529d00`); 62 deg is only the ctor default. Campaign `.wld` files
store the same camera state; a new Free Game is fixed in code/camera.ini.

Port: `load_island` with `reset_view` sets the centred-world equivalent:
target `(0,0,terrain_z)`, horizontal 39, elevation 52.7, yaw 0, and the
park scene renders with FOV 80. `--start-dino` keeps a close demo camera so
the allosaurus is visible.

## Draw order: missing PRIORITY (2026-10-27)

- `CONSTRUCTION_ROAD_METRO_PANEL`: `CONSTRUC_ROAD_BUTTON` / `CONSTRUC_METRO_BUTTON`
  / `METRO_TITLE_PANEL` set neither `PRIORITY` nor `ZPRIORITY`; their
  sibling weave panels are `P=10 Z=0.49` and declared earlier. With a
  missing `PRIORITY` read as 0 the port drew the buttons first and the weave
  covered them (the Road button was invisible).
- Decompiled: the widget ctor `0x5bc330` stores `PRIORITY = -1`
  (`+0x34`, `or ecx,-1`) and `ZPRIORITY = 0.0` (`+0x40`); Load `0x5bca20`
  calls the setters only when the key is present; the child list `0x5bae40`
  / `0x5bb850` compares priorities **signed**. `Draw` `0x5c5bf0` passes
  `+0x40` minus small epsilons to the background/frame/texture/text helpers,
  so ZPRIORITY is a per-primitive depth.
- These raw defaults do not reproduce retail on their own: `-1` would still
  sort the Road button under the weave, and a pure `Z=0` depth would put the
  opaque `BUDGET_LEFTWOOD_MIDLEFT1_PANEL` (no P/Z) over the budget numbers.
- Port model (**inferred**): a widget without `PRIORITY` / `ZPRIORITY`
  inherits the nearest ancestor's value (`src/ui/overlay.cpp`), then siblings
  sort by PRIORITY, ZPRIORITY (far first), declaration order. It keeps all
  existing UI tests green and shows the road panel buttons
  (`tests/test_viewer_cpp.py::test_cpp_road_panel_buttons_visible`).
  A retail screenshot of the open Infrastructure panel would confirm it.
