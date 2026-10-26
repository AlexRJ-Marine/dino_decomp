# Port save/load (`DINOPSAV`)

Status: **working** for the port's own park state. This is not the retail
`.sav` layout (that parser is documented in `docs/formats/sav.md`); the port
writes a small text file so the Options Save/Load panel can round-trip the
scene between sessions.

## What is stored

`src/world/port_save.{h,cpp}` serialises:

- island folder (`isl01`..`isl10`), difficulty index and park budget;
- park clock: day, elapsed seconds, time scale;
- orbit camera (`target`, yaw, pitch, distance) and the free-camera pose;
- 4-unit build grid: cell size, origin, dimensions, every path cell and every
  canonical south/east fence edge;
- every visitor: world position, angle, walk phase, current/target/previous
  cells;
- every placed building: `BuildingList.ini` kind, min-corner cell and
  orientation state.

Buildings are re-loaded from `Data/Design/buildings/<name>.bbi` on restore;
the free-game Entrance is still auto-created by `load_island` when a save has
no building list (older saves). The retail `.sav` world block is not read
here.

## Format

```text
DINOPSAV 1
island isl01
difficulty 1
budget 75000
clock <day> <elapsed_seconds> <time_scale>
camera <tx> <ty> <tz> <yaw> <pitch> <distance> <free 0/1>
free <x> <y> <z> <yaw> <pitch>
grid <cell_size> <origin_x> <origin_y> <width> <height>
path <x> <y>
fence <x> <y> <edge>          # edge 0 south, 1 east
visitor <x> <y> <angle> <phase> <cell_x> <cell_y> <target_x> <target_y> <prev_x> <prev_y>
building <kind> <cell_x> <cell_y> <state>
```

Floats are written with 9 significant digits. Unknown keys are ignored on
load; a missing `DINOPSAV` header or a different version is an error.

## Viewer wiring

- `--save-port FILE` writes the state after startup (and continues).
- `--load-port FILE` restores it at startup; it also enables build mode so the
  path/fence/visitor assets are available.
- `--save-name NAME` selects the Options Save target, default `quicksave`.
- Options Save/Load panel:
  - `OPTIONS_MENU_SAVE_BUTTON` / `OPTIONS_MENU_LOAD_BUTTON` switch tabs;
  - `OPTIONS_SAVE_NAMES_LIST` selects a row (12-px lines);
  - `OPTIONS_VALIDATE_BUTTON` emits `save port` or `load port` for the active
    tab; the viewer writes/reads `run/save/<name>.psav` or the selected
    `[port]` row and hides `OPTIONS_PANEL`;
  - `OPTIONS_UNVALIDATE_BUTTON` cancels.
- `.psav` files appear in the list with a `[port]` prefix. Retail `.sav` rows
  are still displayed (name from the retail header) but are not loadable by
  this path.
- Loading a save from a session that was not in build mode calls
  `ensure_build_ready`: it loads the paddock fence mesh, `ground.tga`, the
  fence texture and the visitor walk meshes, then rebuilds the path/fence and
  visitor GPU meshes. Island changes go through `load_island`, which also
  re-places the free-game Entrance and reloads sky/water/vegetation.

`--park-click` actions issued before the first frame (`save port`/`load port`)
are queued and executed once the terrain, grid and camera exist.

## Evidence

```powershell
$G = "game/%MAINDIR%"
# write a state from isl03 with two paths, one fence and three visitors
.\build\mingw\dino_viewer.exe --frames 2 --no-menu --no-hud --no-vsync `
  --lnd "$G/Data/Design/landscape/isl03/isl03.lnd" --asset-root $G `
  --no-vegetation --place-path 100,100 --place-path 100,101 `
  --place-fence 100,100,0 --visitors 3 --park-time 10:30 `
  --park-time-scale 30 --save-port run\save\test.psav

# load it over isl01 and print the restored counts
.\build\mingw\dino_viewer.exe --frames 2 --no-menu --no-hud --no-vsync `
  --lnd "$G/Data/Design/landscape/isl01/isl01.lnd" --asset-root $G `
  --no-vegetation --load-port run\save\test.psav
```

Expected: `port load: island=isl03 budget=75000 day=0 visitors=3 paths=2
fences=1 buildings=0` and `build: 2 path cells, 1 fences`.

- `notes/port-ui-save.png` - Options Save tab with the port save rows.
- `notes/port-ui-load2.png` - Options Load tab after selecting `[port] test`;
  the HUD shows isl03, 10:30 day 1.
- `tests/test_viewer_cpp.py::test_cpp_port_save_roundtrip`,
  `test_cpp_options_save_and_load_buttons`.

## Open

- Import retail `.sav` park state (the Python `sav_graph` parser is ready;
  the C++ reader only has the header).
- Text entry for `OPTIONS_NAME_SLOT`; the port uses `--save-name` or the
  selected row instead.
- Save/load the dino placement and animation time.
