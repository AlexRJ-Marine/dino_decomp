# Retail park import (`.wld` / `.sav` → port)

Status: **P2 complete** — campaign `.wld` roads become path cells, fence
rectangles become canonical edges, buildings place through `.bbi`, and
`.sav` visitors, budget, clock and dinosaurs are restored in the port park.
Dinosaur positions are not stored in retail files; they spawn in the saved
fence rectangles (paddocks).

## CLI

```text
--wld-park FILE      import roads, fences, buildings and dinosaurs from a .wld
--sav-park FILE      import a retail .sav park (same data + visitors/budget/clock)
--wld FILE           read the world block only (buildings + --wld-info)
--lnd FILE           terrain/island (required for placement)
--no-import-dinos    skip the dinosaur genotypes (building-only bring-up)
--dino-clip NAME     animation clip for imported dinosaurs (default Walk)
--focus-dinos        frame the first imported dinosaur
--focus-dino N       frame the Nth imported dinosaur
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
dinos: 20/20 spawned allo=7 anky=2 coelo=4 deino=5 trood=2
```

Evidence: `notes/cpg04-park-import.png`, `notes/p2-dinos.png`,
`tests/test_viewer_cpp.py::test_cpp_wld_park_import`,
`test_cpp_wld_park_import_single_road`,
`test_cpp_wld_park_dinosaurs`, `test_cpp_wld_park_no_import_dinos`.

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

## Dinosaurs (working)

Each `drsDinosaur` world record carries the 18-gene genotype and five object
links, but no world position. The species is recovered from the genotype:

```text
gene 0 = (head-type allele A, allele B), dominant allele = min(A, B)
head type 0..19  ->  base species in anim_trans.dic order
```

The 20 base `<species>.gen` files (`allo.gen`..`velo.gen`) have exactly the
20 distinct head types 0..19, so the mapping is one-to-one and comes straight
from retail data. Hybrids use the dominant body plan (e.g. `[18, 0]` → allo).

The animation family comes from `simu_trans.dic` Part 10 (20 indices into the
`anim_trans.dic` family list): HeavyBiped (allo/gigano/trex), LowQuadruped
(anky/proto/trice), LightBiped (coelo/deino/hypsilo/pachy/peleca/trood/velo),
HighQuadruped (brachio/diplo/salta), LightQuadruped (igua/kentro/parasau/
stego). The viewer loads `<species>_lod1.mesh`/`.skel`/`_lod1.skin`, the
`<family>/<family>_Walk.anim` clip and `<species>.tga`, and poses each one
with the same rigid skinning path as the single-dinosaur viewer.

The file has no per-dinosaur position. The port distributes the records
inside the saved `drsFence` rectangles (paddocks), one ring per rectangle,
deterministically; building-only parks fall back to the map centre. The
care-state list at the simulation offset (name/phenotype/needs) is not read
yet, so imported dinosaurs start from the runtime default needs.

`--dino-action-loop` replaces the static clip with the retail chooser
(`src/world/dino_actions.cpp`): the four needs decay slowly, `choose_action`
picks `Exploration`/`Eat`/`EatForSmallies`/`Sleep` from the `.bai`
preconditions and weight tables, and the completed action applies its need
deltas before the next decision. `--dino-needs F,V,S,R` seeds the four needs
and `--dino-action-speed N` scales the 30 s `.bai` durations. The clip link
is inferred (`Eat`→`Chew`, `Sleep`→`Sleep`, `Exploration`→`Walk`, idle→
`Neutral`); the retail mapping table is still open.

## Open

- Visitor profile index is not in the 0x50 record; default 0 until the
  `actor+0x68` writer is found.
- The dinosaur care-state list (name/phenotype/needs) is decoded only in
  notes; the port spawns from the world genotype.
- Imported dinosaurs are static placed actors; the P3 action loop will drive
  their needs and clips.
- Retail fences whose rectangles are not axis-aligned (if any) need a
  segment decomposition.
