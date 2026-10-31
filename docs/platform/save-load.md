# Port save/load (`DINOPSAV`)

Status: **working** for the port's own park state. This is not the retail
`.sav` layout (that parser is documented in `docs/formats/sav.md`); the port
writes a small text file so the Options Save/Load panel can round-trip the
scene between sessions. A retail `.sav` writer exists as a tool
(`dino_port --sav-write IN OUT`), but the Options panel still uses `.psav`.

## What is stored

`src/world/port_save.{h,cpp}` serialises:

- island folder (`isl01`..`isl10`), difficulty index and park budget;
- park clock: day, elapsed seconds, time scale;
- orbit camera (`target`, yaw, pitch, distance) and the free-camera pose;
- 4-unit build grid: cell size, origin, dimensions, every path cell and every
  canonical south/east fence edge;
- every visitor: world position, angle, walk phase, current/target/previous
  cells, profile index, the three needs, global profile, `need0_at_use`, the
  SIM-2b state (`Choose/Travel/Use/Leave`), target attraction, route index,
  use/decay timers, RNG, done flag and the full route cell list (older saves
  omit the tail and restore `Choose` / `39/99/99`);
- every placed building: `BuildingList.ini` kind, min-corner cell and
  orientation state;
- every imported dinosaur: species/family/clip, position, angle, the four
  needs, the current action id, the remaining action seconds and the clip
  playback phase.

Buildings are re-loaded from `Data/Design/buildings/<name>.bbi` on restore;
dinosaurs are re-created through the same species/animation loader and their
`DinoActionLoop` is restored without re-scoring. The free-game Entrance is
still auto-created by `load_island` when a save has no building list (older
saves). The retail `.sav` world block is not read here.

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
visitor <x> <y> <angle> <phase> <cell_x> <cell_y> <target_x> <target_y> <prev_x> <prev_y> <profile> <need0> <need1> <need2> <global_profile> <need0_at_use> <state> <target_attraction> <route_index> <use_timer> <decay0> <decay1> <decay2> <rng> <done> <route_count> [<x> <y>]...
building <kind> <cell_x> <cell_y> <state>
dino <species> <family> <clip> <x> <y> <angle> <food> <violence> <secu> <repos> <action_id> <action_left> <size_cm> <carnivore> <phase> <threat> <threat_radius> <combined_attack> <obstacle_radius> <id> [<size_class> <size_radius> <body_length_cm> <attack_head> <length_cm> <height_cm>]
```

`state` is 0 `Choose`, 1 `Travel`, 2 `Use`, 3 `Leave`; `route_count`
followed by that many grid cells is the exact remaining path. Older saves
end after the needs / `size_cm`, and the loader defaults the new fields.
The trailing cache fields and the stable `ImportedDino::id` are optional;
when the id is absent the loader assigns the 0-based import index, which is
exactly what older saves stored in the paddock occupant list. New paddock
occupants always store the stable id, so a sale does not shift the UI's
per-dino lookups. The phenotype tail after the id (`size_class`, `size_radius`,
`body_length_cm`, `attack_head`, `length_cm`, `height_cm`) preserves the
values the Eat gate/bite and `--dino-info size` read; older saves end at the
id and keep the defaults.

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
  - `OPTIONS_NAME_SLOT` (an EDIT widget) starts text entry on the Save tab;
    SDL text input appends printable ASCII (24-char cap) and Backspace
    deletes. Clicking another HUD control ends the edit. `Overlay::hit_test`
    now includes EDIT widgets; `--type-text TEXT` replaces the name for
    scripted tests;
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
.\build\mingw\dino_port.exe --frames 2 --no-menu --no-hud --no-vsync `
  --lnd "$G/Data/Design/landscape/isl03/isl03.lnd" --asset-root $G `
  --no-vegetation --place-path 100,100 --place-path 100,101 `
  --place-fence 100,100,0 --visitors 3 --park-time 10:30 `
  --park-time-scale 30 --save-port run\save\test.psav

# load it over isl01 and print the restored counts
.\build\mingw\dino_port.exe --frames 2 --no-menu --no-hud --no-vsync `
  --lnd "$G/Data/Design/landscape/isl01/isl01.lnd" --asset-root $G `
  --no-vegetation --load-port run\save\test.psav
```

Expected: `port load: island=isl03 budget=75000 day=0 visitors=3 paths=2
fences=1 buildings=0 dinos=0` and `build: 2 path cells, 1 fences`.

Dinosaur state round trip (Cpg04, 20 imported dinosaurs):

```powershell
.\build\mingw\dino_port.exe --frames 2 --no-menu --no-hud --no-vsync `
  --lnd "$G/Data/Design/landscape/isl04/isl04.lnd" --asset-root $G `
  --no-vegetation --no-water `
  --wld-park "$G/Data/Design/Scenario/Cpg04/Cpg04_Save.wld" `
  --dino-action-loop --dino-needs 20,90,50,20 `
  --save-port run\save\dinos.psav

.\build\mingw\dino_port.exe --frames 4 --no-menu --no-hud --no-vsync `
  --asset-root $G --load-port run\save\dinos.psav --dino-action-loop
```

Expected on load: `port load: island=isl04 ... buildings=57 dinos=20` and
`dino action: ... Eat score=9009 needs=20/90/50/20`. The `.psav` carries one
`dino` line per imported dinosaur with its needs, action id and remaining
action seconds.

- `notes/port-ui-save.png` - Options Save tab with the port save rows.
- `notes/port-ui-load2.png` - Options Load tab after selecting `[port] test`;
  the HUD shows isl03, 10:30 day 1.
- `tests/test_app_cpp.py::test_cpp_port_save_roundtrip`,
  `test_cpp_port_save_dino_state`, `test_cpp_options_save_and_load_buttons`.

## Open

- The port Options panel writes `.psav`, not retail `.sav`; retail parks are
  imported at startup with `--sav-park`, and `--sav-write` can re-serialize a
  retail save (see `docs/formats/sav.md`).
- Visitor travel state/route and the dinosaur clip playback phase are stored
  (2026-10-26). A restored attraction index is still range-checked by the
  visitor update; a save whose building order changed falls back to
  `Choose`/`Leave`.
