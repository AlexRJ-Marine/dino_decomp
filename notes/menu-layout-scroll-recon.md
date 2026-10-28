# Menu layout and scrollbars — evidence and port contract

Status: **click mapping bug proven; retail VSCROLL behaviour being traced.**
The menu geometry comes from `Data/Overlay/MenuInterface.Ini` (800x600 design
space) and the port's `src/ui/overlay.cpp` parser. The UI click map is
`docs/platform/ui-coordinates.md` / `.json`.

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

## 3. Port state to add

* `first_visible` per list (island, campaign, load, cinematics).
* Draw row `first_visible + i`; clamp the offset to `max(0, rows - visible)`.
* Click row = `first_visible + (y - abs_y) / LB_LINE_HEIGHT`.
* Scrollbar up/down = one line; track click = one page; thumb drag =
  proportional (exact retail steps in `notes/ui-scrollbar-recon.md`).
* Thumb position and size must come from `VS_POSITION/VS_RANGE` and the
  visible count, not from the selected row (the current draw uses
  `selected_row`, so it lies whenever the view is scrolled).
* Wheel: only if the retail trace proves a wheel path.

## 4. Other menu pages (audit seed)

* `CAMPAIGN_LIST` has 8 rows and 8 visible; its scrollbar is inert but not
  needed for Cpg01-08.
* `LOAD_GAME_NAMES_LIST` needs the scroll offset when `save_count > 14`.
* `CINEMATICS_LIST` has 8 rows and 8 visible.
* Campaign Info is wrapped text, not a row list.
* Options page (R-18) and retail `.sav` island selection are owned by the
  concurrent MENU-1 slice; do not duplicate here.

## Evidence

* `game/%MAINDIR%/Data/Overlay/MenuInterface.Ini` (`ISLAND_LIST`,
  `*_VS` sections), `MenuGraphicDB.ini` (`ISLAND_LIST_BUTTON` 166x34).
* `docs/platform/ui-coordinates.md` (row/scroll clicks).
* Live viewer: row 3 -> isl05, row 7 -> isl10 (`--menu --menu-page 1
  --menu-click ... --menu-loading-time 0`, `start: island=...`).
