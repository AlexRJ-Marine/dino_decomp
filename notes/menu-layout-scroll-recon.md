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

* `CAMPAIGN_INFO_DESCRIPTION` (wrapped briefing) does not scroll yet; the
  retail listbox has a VSCROLL for it.
* Hold-to-repeat is unverified in the trace (`0x5cb9f0`); the port currently
  scrolls once per click.
* List-row hover frame and the separator/colour keys are cosmetic and not
  mapped yet.
* Options page (R-18) and retail `.sav` slots are separate menu items.

## Evidence

* `game/%MAINDIR%/Data/Overlay/MenuInterface.Ini` (`ISLAND_LIST`,
  `*_VS` sections), `MenuGraphicDB.ini` (`ISLAND_LIST_BUTTON` 166x34).
* `docs/platform/ui-coordinates.md` (row/scroll clicks).
* Live viewer: row 3 -> isl05, row 7 -> isl10 (`--menu --menu-page 1
  --menu-click ... --menu-loading-time 0`, `start: island=...`).
