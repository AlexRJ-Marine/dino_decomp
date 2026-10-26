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

## Next: `.sav` simulation section

After the show catalog, the save carries:

```text
u32 road_count + rects
u32 list0/1/2 counts          # 0 in retail
u32 visitor_count
visitor_count × 0x50 records  # x, y, state, needs[7], heading
u32 park_count (1)
0x293-byte park block         # budget at block+0x30, day at +0x24a,
                              # elapsed f32 at +0x24e
...
```

Plan: port the show-catalog skip and these records to C++, restore the saved
visitors (position, state, needs; profile index defaults to 0), seed
`park_budget`/`ParkClock`, and place dinosaurs from the care-state list at a
paddock centre (the world record stores no position).

## Open

- `.sav` show-catalog skip + simulation reader in C++.
- Visitor profile index is not in the 0x50 record; default 0 until the
  `actor+0x68` writer is found.
- Dinosaur world position: the records store genotype/links, not a position;
  spawn at a paddock/fence centre.
- Retail fences whose rectangles are not axis-aligned (if any) need a
  segment decomposition.
