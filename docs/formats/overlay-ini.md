# Overlay `.ini` and graphic database

Status: **solved** for layout, widget tree, text properties and frame-strip
packing. Draw order and the per-state frame table are partial (see Open
items).
Covers `MenuInterface.Ini`, `ParkInterface.Ini`, the other
`*Interface*.Ini` files, and the matching `*GraphicDB.ini`.

## Inventory

| File | Sections | Role |
| --- | ---: | --- |
| `Data/Overlay/MenuInterface.Ini` | 114 | Main/free-game/options/campaign/load/credits/loading pages |
| `Data/Overlay/ParkInterface.Ini` | 137 | Always-on park HUD; `#INCLUDE`s nine panel files |
| `Data/Overlay/ConstructionInterface.Ini` | 705 | Construction, purchase and save panels |
| `Data/Overlay/Infopanels.Ini` | 677 | Dinosaur/building info panels |
| `Data/Overlay/ListInterface.Ini` | 336 | Lists (dinos, paddocks, staff, …) |
| `Data/Overlay/LabInterface*.Ini` | 95–245 each | Lab screens |
| `Data/Overlay/MissionInfo.Ini` | 136 | Mission/objective panels |
| `Data/Overlay/RadialMenu.Ini` | 22 | Legacy radial build/context menus — **unused** (no exe refs; icons missing) |
| `Data/Overlay/AttributeRange.ini` | 2 | Attribute gauge ranges |
| `Data/Overlay/MenuGraphicDB.ini` | 53 | Named sprite rectangles for menus |
| `Data/Overlay/ParkGraphicDB.ini` | 426 + `LabGraphicDB.ini` 180 = 605 unique (`LAB_NAME_PANEL` repeats identically) | Named sprite rectangles for the park/UI |
| `Data/Overlay/LabGraphicDB.ini` | 180 | Lab sprites |

Text files are Windows-1252 (French comments in the headers). Names are
case-sensitive when looked up from code (`FREE_GAME_PAGE`, `MAIN_MENU_PAGE`).

## Text syntax

```ini
; comment
[#INCLUDE]
FILE0 = "./ConstructionInterface.ini"
FILE1 = "./MissionInfo.ini"

[VISITOR_AMOUNT]
TYPE           = "WIDGET"
PARENT         = "VISITOR_AMOUNT_BACKGROUND"
X = 5
Y = 9
WIDTH = 40
HEIGHT = 13
TEXT           = "&VisitorAmount"
TEXT_COLOR     = 255 255 255 255
TEXT_HEIGHT    = 12
TEXT_FONT      = "DATA/FONTE/FONT.FNT"
TEXT_JUSTIFY   = CENTER
```

- Section = one widget. `#INCLUDE` is a normal section with `FILE0..N`.
- `;` comments; `;;ALIGN_RIGHT` lines are deliberately ignored.
- Values may be quoted; colours are `R G B A` bytes.
- There are no duplicate section names in `MenuInterface.Ini` or
  `ParkInterface.Ini`. Later definitions would override earlier ones if a
  file did repeat a name.
- No escapes; `TEXT` may be empty.

## Coordinates

- Top-left origin, **+Y down**, units are pixels in an **800×600** design
  space (`[RENDERVIEW]`).
- `X`/`Y` are relative to the parent widget's top-left.
- `WIDTH`/`HEIGHT` are the widget's box.
- Child `TEXT` is justified inside the widget box (see below), not at the
  baseline origin.
- The menu pages are 800×600 and are centred by code on larger screens;
  the park HUD panels use `ALIGN_RIGHT`/`ALIGN_DOWN`.

## Object types

`WIDGET`, `BUTTON`, `COMBO`, `EDIT`, `CURVE`, `GROUP`, `HSCROLL`, `LIST`,
`PROGRESS`, `RENDER`, `VSCROLL`, `WINDOW`.

Engine enum (`FUN_005ba6b0`): `WIDGET` 0, `BUTTON` 1, `EDIT` 2, `PROGRESS` 3,
`GROUP` 4, `HSCROLL` 5, `VSCROLL` 6, `LIST` 7, `COMBO` 8, `CURVE` 9,
`RENDER` 10. `WINDOW` is not handled and does not occur in retail data. The
`0x005bf900` class is the progress bar, not a button; the button is
`0x005c0380`.

The header of `MenuInterface.Ini`/`ParkInterface.Ini` documents the full
parameter list. Confirmed properties:

### Common

| Key | Meaning | Values |
| --- | --- | --- |
| `TYPE` | Object class | see above |
| `PARENT` | Parent section name | `ROOT` for top-level |
| `PRIORITY` | Draw/update priority | integer 0..100 |
| `ZPRIORITY` | Z within priority | float 0..1 |
| `EQUIVALENT` | Marks internally created widgets (scrollbar parts) | name |
| `X`, `Y`, `WIDTH`, `HEIGHT` | Geometry | pixels, parent-relative |
| `SIZE` | Geometry as one tuple | `X Y WIDTH HEIGHT` |
| `VISIBLE` | Shown | 0/1 |
| `ENABLE` | Accepts input | 0/1 |
| `USER_MOVABLE` | Mouse-draggable | 0/1 |
| `DRAG_AND_DROP` | Unused | 0/1 |
| `BORDER_COLOR` | Border colour | `R G B A` |
| `BACK_COLOR` | Background colour | `R G B A` |
| `TEXTURE` | Sprite name in the matching GraphicDB | string |
| `TEXTURE_FRAME` | Base frame index into the sprite strip | 0..255 |
| `TEXTURE_MOSAIC` | Tile instead of stretch | 0/1 |
| `TEXT_COLOR` | Text colour | `R G B A` |
| `TEXT_HEIGHT` | Text pixel height | integer |
| `TEXT_FONT` | `.fnt` path | string |
| `TEXT_JUSTIFY` | Justification bitmask | see table |
| `TEXT` | Text or `&loc` field | string |
| `HINT` | Tooltip or `&loc` field | string |
| `USER_ID` | Caller value | integer |
| `ALIGN_LEFT` | Keep left edge on resize | 0/1 |
| `ALIGN_UP` | Keep top edge on resize | 0/1 |
| `ALIGN_RIGHT` | Keep right edge on resize | 0/1 |
| `ALIGN_DOWN` | Keep bottom edge on resize | 0/1 |
| `ALIGN_CENTER` | Centre horizontally and vertically on resize | 0/1 |
| `ALIGN_USER_DEFINED` | Skip the normal pass; layout is set from code | 0/1 |

Not every file's comment header lists `ALIGN_CENTER`, but it is parsed and
used by the lab screens.

### Per type

| Type | Extra keys |
| --- | --- |
| `BUTTON` | `BTN_SUB_TYPE` (`NORMAL`/`SWITCH`/`CHECK_BOX`), `BTN_START_STATE` (`DOWN`/`UP`) |
| `EDIT` | `EDIT_MAX_LENGTH`, `EDIT_TO_UPPER`, `EDIT_TO_LOWER`, `EDIT_TYPE` (`NORMAL`/`S32`/`U32`/`F32`/`ALPHA`/`ALPHANUM`/`FILENAME`/`IP`) |
| `HSCROLL` | `HS_POSITION`, `HS_RANGE` |
| `VSCROLL` | `VS_POSITION`, `VS_RANGE` |
| `LIST` | `LB_VSCROLL_WIDTH`, `LB_FIRST_LINE_SEPARATOR`, `LB_LINE_SEPARATOR`, `LB_SELECTED_SEPARATOR_COLOR`, `LB_SELECTED_LINE_COLOR`, `LB_LINE_HEIGHT`, `LB_NB_COLUMNS`, `LB_COLUMNx_WIDTH` |
| `COMBO` | `CB_REDUCED_HEIGHT`, `CB_VSCROLL_WIDTH`, `CB_VSCROLL_LINE_HEIGHT` |
| `PROGRESS` | `PB_CURRENT_PROGRESS`, `PB_MAX_PROGRESS`, `PB_VERTICAL` |
| `CURVE` | `GC_MIN_SCALE`, `GC_MAX_SCALE`, `GC_AUTO_SCALE`, `GC_NB_CURVES`, `GC_SAMPLES_PER_CURVE`, `GC_UPDATE_TIME`, `GC_CURVEx_COLOR`, `GC_CURVEx_VISIBLE` |

The overlay creates internal children for scrollbars (`HsBtnUp`, `HsBtnDown`,
`HsBtnThmb`, `VsBtnUp`, `VsBtnDown`, `VsBtnThmb`, `LbVs`, `CbLb`, `CbEdit`,
`CbBtnDeploy`, `ROOT`) and marks them with `EQUIVALENT`.

### Retail usage

Across the 20 overlay INIs the parser sees 5,388 widgets: 3,900 `WIDGET`,
899 `BUTTON`, 370 `PROGRESS`, 95 `GROUP`, 50 `EDIT`, 22 `LIST`, 16 `RENDER`,
15 `VSCROLL`, 12 `CURVE`, 9 `HSCROLL`. There is **no `WINDOW` or `COMBO`**
usage. `TEXT_JUSTIFY` is `CENTER` (617), `LEFT` (16) or `DOWN` (6);
`BTN_SUB_TYPE` is only `SWITCH` (334). `ALIGN_CENTER` appears 23 times
(lab backgrounds/panels). `SIZE`, `TEXTURE_MOSAIC` and
`ALIGN_USER_DEFINED` are parsed by the engine but unused in retail data.
`PRIORITY` is 10 (1,327) or 1 (12); `ZPRIORITY` has 1,636 uses over 0.004–0.9.
So a first port needs widgets, buttons (normal + switch), lists, progress
bars, group/edit, text and sprites — not windows or combo boxes.

## `TEXT_JUSTIFY` bits

Proven from `FUN_005bca20` (see `notes/ui-recon.md`):

| Token | Bits | Horizontal | Vertical |
| --- | ---: | --- | --- |
| `LEFT` | 1 | left | — |
| `RIGHT` | 2 | right | — |
| `X` | 3 | centre | — |
| `UP` | 4 | — | top |
| `DOWN` | 8 | — | bottom |
| `Y` | 12 | — | centre |
| `CENTER` | 15 | centre | centre |

Tokens combine with `|` in the header's description, but the parser accepts a
single token per `TEXT_JUSTIFY` line in the retail files. `CENTER` is the
usual value for numbers and button labels; `LEFT` for lists and descriptions.

## `TEXT_HEIGHT`

`TEXT_HEIGHT` is the rendered pixel height, not a point size. Every retail
HUD/menu string sets it equal to the font's `font_size` (12, 14 or 19), so the
scale is 1.0 at 800×600. A renderer should compute
`scale = TEXT_HEIGHT / font.font_size` to support other values.

## Text resolution

`TEXT` and `HINT` values starting with `&` are `.loc` field IDs
(`Data/Overlay/MenuEng.loc`, `ParkEng.loc`; format in `docs/formats/loc.md`).
Examples: `&FreeGame` → `FREE MODE`, `&Options` → `OPTIONS`,
`&Budget` → `Budget`.

Some keys are runtime values and are **not** in `.loc`:
`&VisitorAmount`, `&BudgetAmount`, `&dinoAmount`, `&ShowAmount`. The game
fills those widgets at runtime; a port can keep the same convention (map the
label to a live value) or render the value directly.

## Graphic database

```ini
[VISITOR_AMOUNT_BACKGROUND]
X          = 94
Y          = 0
WIDTH      = 64
HEIGHT     = 31
FILENAME   = "./DATA/OVERLAY/TEXTURES/menu_bas.tga"
NB_OBJECTS = 1
```

A `TEXTURE` value in an interface INI is a section name in the matching
GraphicDB (`MenuGraphicDB.ini` for `MenuInterface.Ini`, `ParkGraphicDB.ini`
for the park, plus `LabGraphicDB.ini` via include). The entry gives the
atlas rectangle inside one TGA.

- `NB_OBJECTS = 1` — a single sprite.
- `NB_OBJECTS = 8` — a button/state strip (see below).
- Sprite atlases are 256×256; the nine menu `BACKGROUND_*` page images are
  800×600.
- `FILENAME` is relative to the game root (`./DATA/OVERLAY/TEXTURES/...`).
- Park DB has nine `TEMP_*` entries pointing at `TEMP_jaugebas.tga`, which is
  absent from the retail tree. A loader must degrade gracefully (skip the
  sprite, keep the widget).

### Frame strips (`NB_OBJECTS` / `SKIP_LIST`)

`FUN_00421630` (`moGraphicCtrl::Load`) parses each entry:

- `NB_OBJECTS` is the number of logical frames (default 1).
- `SKIP_LIST` is **not** a keep-list. It is a 32-bit bitmask:
  `mask |= 1 << value` for each token. The loader stores the mask at
  `entry + 0x28`.

Only the frames **not** in the mask are stored, packed compactly from the
entry's `(X, Y)`:

- `BUDGET_PANEL_BUTTON` (0,62 31×31, skip 1 2 3) stores five bag icons in a
  row at x = 0, 31, 62, 93, 124.
- `MINI_MAP_ZOOMIN_BUTTON` (176,139 16×16, skip 1 2 3) stores five 16-px
  icons at x = 176..256, exactly fitting the atlas width.
- `EASY/MIDDLE/HARD_MODE_BUTTON` are three entries at Y = 0/52/104, each
  storing four 52-px icons in a row.
- `MAIN_MENU_BUTTON` (0,0 213×23, skip 0 1 2 3) cannot fit four 213-px
  frames across, so its strip is **vertical**: slot 0 is transparent and the
  three visible 21-px bars sit at y ≈ 25, 48 and 71.
- `ISLAND_LIST_BUTTON` (0,55 166×34, skip 0 1 2 3) is the same pattern: a
  vertical strip whose first stored cell is blank, with bars at y ≈ 97, 131
  and 165.

So: use the complement of `SKIP_LIST`, lay the frames out horizontally when
the strip fits the texture width, otherwise vertically. The first stored
slot is frame 4 and is the **normal** state; it may be fully transparent
(menu/list bars), so the page art shows through. Frame 0 is the disabled
art. See "Observed state frames" below.

At runtime a widget stores the resolved graphic index in the low 16 bits of
`widget + 4`, the `TEXTURE_FRAME` value in bits 16..23, and `TEXTURE_MOSAIC`
in bit 24 (`FUN_005c3a20`, `FUN_005c3a60`, `FUN_005c3a40`). `TEXTURE_FRAME`
offsets the frame chosen for the current button state.

### Observed state frames

The state frames are universal; the art differs per control:

| State | Frame | Notes |
| --- | ---: | --- |
| disabled | 0 | gray variants; absent when `SKIP_LIST` masks 0 |
| normal | 4 | **may be fully transparent** (menu/list bars), so the page art shows through |
| hover | 5 | dark/medium bar, green icon border |
| pressed | 6 | brighter bar, green border |
| selected | 7 | brightest bar, yellow icon border |

`SKIP_LIST` removes animation frames 1–3, not states. Do **not** skip
transparent frames when mapping states: frame 4 is stored and is the normal
state even when its alpha is zero.

This model is validated by the Python previews: Start/Cancel match the retail
Free Game screenshot exactly (MAD 0.0), the park gauge icons match at MAD
3.65, and the full Free Game page differs only in text anti-aliasing
(MAD 2.93). The remaining per-control ambiguity is only which of 6/7 is used
for selected vs pressed on bars; normal=4 is certain.

Evidence images: `notes/ui-atlas-frames.png`,
`notes/ui-atlas-outcome4-full.png`, `notes/ui-atlas-outcome.png`,
`notes/hud-preview.png`, `notes/hud-preview-compare.png`,
`notes/freegame-compare.png`.

## Anchoring / resize

`moWidget::OnResize(newW, newH, oldW, oldH)` (`FUN_005bc000`) is recursive:

```text
if ALIGN_CENTER:      x = (newW - width) / 2;  y = (newH - height) / 2
elif ALIGN_LEFT and ALIGN_RIGHT:  stretch to keep the right margin
elif ALIGN_RIGHT:     x += newW - oldW
else:                 x unchanged

same for y with UP/DOWN
```

The retail UI is **not scaled**: a 1024×768 frame is the 800×600 layout
shifted by (112, 84) - measured at MAD 1.0 versus MAD 40.3 for uniform
scaling (`notes/ui-recon.md`). The port implements this pass in
`Overlay::layout_for_size` and uses it for the park HUD; menu pages remain
centred at 800×600. High-DPI physical scaling is applied after the logical
anchoring pass.

## Widget tree and page activation

- All root widgets use `PARENT = "ROOT"`. `[RENDERVIEW]` is the 800×600
  scene view; the rest are overlays.
- `MenuInterface.Ini` defines nine pages as 800×600 root widgets with
  `VISIBLE = 0`; code looks them up by name, shows one page and hides the
  rest. The page order is the state index (0 main menu … 8 loading).
- `ParkInterface.Ini` is not paged: its root panels are created once and
  toggled with `VISIBLE`/`ENABLE` by the park code. The nine included files
  add 30 more top-level panels, so the expanded interface has **38 roots**
  and 2,694 widgets; every included panel starts hidden.

## Reader contract

1. Parse the file, honour `#INCLUDE` (paths relative to the including file).
2. Build `name → widget`; resolve `PARENT` links; reject cycles.
3. Compute absolute rects top-down (parent origin + local X/Y).
4. Resolve `TEXTURE` through the GraphicDB (name → TGA + rect).
5. Resolve `TEXT`/`HINT` through `.loc` when the value starts with `&`;
   otherwise treat it as a literal.
6. On resize, run the anchoring pass above.
7. Draw each widget first, then its children by `PRIORITY` ascending and
   declaration order (the engine's child list); skip `VISIBLE = 0`.
   `ZPRIORITY` is a render hint, not the traversal order.

## Evidence

- `FUN_005bca20` — `moWidget::Load` (property reader), xrefs to
  `TEXT_JUSTIFY`/`TEXT_HEIGHT`/`TEXT_FONT`/`ALIGN_RIGHT`/`ZPRIORITY`.
- `FUN_005bc000` — `moWidget::OnResize` (anchor bits 10–14).
- `FUN_005c5f00` — `moWidget::OnEvent` (flag setters, opcodes `0x23`–`0x28`).
- `FUN_004227d0` — `moFontCtrl` font cache → `mgFontDx8::LoadFile` `0x430fb0`.
- Xrefs to `BACKGROUND_MAIN_MENU` at `0x0051c16d`, `0x0051c184`,
  `0x0051c1a4`.
- `notes/ui-recon.md` — full tables and the 800/1024 measurement.

## Open items

- Exact button state→frame selection and the `INTRO_BUTTON` strip wrap.
- `PRIORITY`/`ZPRIORITY` comparator.
- `EQUIVALENT`/`USER_ID` handling for internally created widgets.
- `ALIGN_CENTER` being applied to menu pages from code rather than the INI.
- `WINDOW` and `COMBO` runtime behaviour (no retail usage found yet).
- `TEXTURE_MOSAIC` tiling rule.
