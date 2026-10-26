# Retail park import (`.wld` / `.sav` → port)

Status: **P2a working** — campaign `.wld` roads become path cells, fence
rectangles become canonical edges, and the existing building import places
the full layout. The `.sav` simulation section (visitors, budget, clock,
dinosaur care state) is the next step.

## CLI

```text
--wld-park FILE      import roads, fences and buildings from a campaign .wld
--wld FILE           read the world block only (buildings + --wld-info)
--lnd FILE           terrain/island (required for placement)
```

`--wld-park` requires `--lnd`; it enables build mode, loads the fence/path
assets, places every `drsBuilding`, then imports the road and fence lists.
`--wld-info` still prints the raw counts.

## Roads → path cells

The `.wld` road list follows the world block:

```text
u32 count
count × (s32 x0, y0, x1, y1)    # world units 0..1024
```

The port grid is centred (`origin = -512`) with 4-unit cells, so the cell
index is:

```text
cell_x = floor(wx / 4)          # NOT floor((wx + 512) / 4)
cell_y = floor(wy / 4)
```

Every cell in the inclusive rectangle becomes a path cell
(`BuildGrid::set_path`). Example: `(380,408)-(656,408)` → cells
`x 95..164, y 102`; the old mapping note's `380 → 223` was off by half the
world and is corrected.

## Fences → canonical edges

`drsFence` stores a world-unit start/end rectangle. Each rectangle is
decomposed into the port's canonical south/east edges:

- south edge of `(x, y0c)` for `x` in the cell span;
- north edge of `(x, y1c)` → south edge of `(x, y1c + 1)`;
- west edge of `(x0c, y)` → east edge of `(x0c - 1, y)`;
- east edge of `(x1c, y)`.

The rectangle span is `floor(min/4) .. floor((max-1)/4)`. Waypoints are not
world coordinates and are ignored for this import.

## Verified layouts

| File | Island | Buildings | Roads | Path cells | Fences | Fence edges |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `Cpg04_Save.wld` | isl04 | 57 | 14 | 301 | 9 | 268 |
| `Cpg01_Save.wld` | isl01 | 1 | 1 | 16 | 0 | 0 |

Run:

```powershell
$G = "game/%MAINDIR%"
.\build\mingw\dino_viewer.exe --frames 2 --no-menu --no-hud --no-vsync `
  --lnd "$G/Data/Design/landscape/isl04/isl04.lnd" --asset-root $G `
  --no-vegetation --no-water `
  --wld-park "$G/Data/Design/Scenario/Cpg04/Cpg04_Save.wld"
```

Expected:

```text
wld: placed 57/57 buildings
build: 301 path cells, 268 fences
park import: roads=14 path_cells=301 fences=9 fence_edges=268
```

Evidence: `notes/cpg04-park-import.png`,
`tests/test_viewer_cpp.py::test_cpp_wld_park_import`,
`test_cpp_wld_park_import_single_road`.

## `.sav` simulation import (working)

`--sav-park FILE` skips the show catalog and reads `spSimuManager`:

```text
u32 show_count + show records (ASCII path, UTF-16 name, 6 u32)
u32 road_count + rects
u32 list0/1/2 counts (0 in retail)
u32 visitor_count + 0x50 records
u32 park_count (1)
0x293-byte park block:
    budget   u32 at block+0x30
    day      u32 at block+0x24a
    elapsed  f32 at block+0x24e
```

Visitor record: `object_id`, `x`/`y` (world 0..1024), `state` at +0x14,
heading `dx/dy` at +0x20/+0x24, and needs at +0x34 (seven u32). The first
three needs are the runtime `+0x48/+0x4c/+0x50`; the other four are 99 in
retail saves.

The viewer builds a `PortState` from the save: roads ? path cells, fences ?
canonical edges, world buildings ? placements, visitor positions ? centered
cells, and budget/day/elapsed ? `park_budget`/`ParkClock`. The visitor state
is set to Choose because the saved target attraction is not in the record;
the first three needs are restored after `apply_port_state`.

Run:

```powershell
.uild\mingw\dino_viewer.exe --frames 2 --no-menu --no-hud --no-vsync `
  --lnd game/%MAINDIR%/Data/Design/landscape/isl01/isl01.lnd `
  --asset-root game/%MAINDIR% --no-vegetation --no-water `
  --sav-park run/save/slot001.sav
```

Expected for `slot001.sav`:

```text
sav park: visitors=2 roads=1 budget=75125 day=0 elapsed=23.214 fences=0 buildings=1
sav import: budget=75125 day=0 elapsed=23.214 visitors=2 paths=16 fences=0 buildings=1
```

Evidence: `tests/test_viewer_cpp.py::test_cpp_sav_park_import`.

## Dinosaurs (open)

The world record carries the 18-gene genotype and object links, but no world
position; the simulation care-state list follows the park block and links by
`field_1c`, carrying the name/phenotype/needs. Spawning a dinosaur still
needs the species/mesh mapping (the record's species id or the name ? folder
link), then a paddock-centre placement. This is the remaining P2 item; the
`WorldDinosaur` C++ record is already exposed.

## Open

- `.sav` show-catalog skip + simulation reader in C++.
- Visitor profile index is not in the 0x50 record; default 0 until the
  `actor+0x68` writer is found.
- Dinosaur world position: the records store genotype/links, not a position;
  spawn at a paddock/fence centre.
- Retail fences whose rectangles are not axis-aligned (if any) need a
  segment decomposition.
