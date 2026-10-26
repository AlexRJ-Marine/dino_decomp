# Path and fence placement (SIM-1)

Status: **working** - grid placement, save/load, undo, terrain picking,
retail fence/path visuals, the retail paddock-capacity model, the
`<island>_occupation.raw` buildability mask and building placement through
the retail construction panel.

## Grid

- World is the 1024-unit island square; the default cell is **4 units**
  (256×256 cells).
- **Paths** own cells.
- **Fences** own cell edges. Only the *south* edge of a cell and the *east*
  edge are stored, so a shared edge has one canonical owner:
  - south edge `(x, y)` == north edge of `(x, y-1)`
  - east edge `(x, y)` == west edge of `(x+1, y)`
- `BuildGrid` keeps a change stack for undo; only actual toggles are recorded,
  so `undo()` restores the opposite value and fixes the counts.

## Interaction

```text
1        path tool
2        fence tool
0        tool off
U        undo last placement
click    place/remove at the terrain point under the cursor
```

A click is a left press/release with less than 5 px of drag, so orbit/pan
still works. Placement picks the terrain by ray-marching the heightfield from
the camera through the mouse pixel and bisecting the last step (24 iterations);
the hit must be inside the island bounds. The fence tool chooses the nearest
cell edge (west/east/south/north) and toggles its canonical edge.

## Buildability mask

Each island ships `Data/Design/Landscape/<island>/<island>_occupation.raw`:
a 1024×1024 `u8` map over the same 1024-unit world (1 unit per cell). The
values are `0` (free/buildable) and `255` (blocked: water, cliff, vegetation
or an existing building — the 77,923 free isl01 cells exclude the Entrance
footprint). `Terrain::buildable(world_x, world_y)` maps the centred world
coordinate to the mask (`mx = x + 512`, `my = y + 512`) and returns true only
for `0`.

- Path, fence and construction placement check the mask; a blocked cell
  prints `placement: blocked cell`, and the CLI placement prints
  `place-path: blocked cell X,Y` / `place-fence: ...` and skips it.
- The fence check tests both cells adjacent to the edge.
- `try_construct` checks every cell of the `.bbi` footprint.
- Retail `.wld`/`.sav` roads and fences are imported unfiltered (they are
  already valid in the source data).
- Missing masks (e.g. a synthetic test island) keep the previous permissive
  behaviour.

## Visuals

- Path tiles are terrain-conforming quads at `height + 0.05`, textured with
  `Data/Graphics/Textures/buildings/ground.tga` (default).
- Fence segments instance the retail paddock fence
  `Data/Graphics/paddock/fences/fence_paddock_lod1.mesh`, scaled so one
  segment spans one cell. The mesh has two materials
  (`fence_paddock.tga` 27/35 triangles, `laser2.tga` 8 triangles); the port
  currently draws all triangles with material 0 (`fence_paddock.tga`) because
  per-triangle material splitting is not implemented yet.
- Both are baked into one mesh per type and rebuilt only when placement
  changes.

## Paddock capacity

A `.wld` fence rectangle is a paddock (`stFence`, `0x57dc60`/`0x57eb50`):

```text
capacity = ((x1-x0+1) / cell) * ((y1-y0+1) / cell)   // float division
cost      = 8 << index, index 0..3 -> 8 / 16 / 32 / 64
can_add   = dinosaur not already inside && capacity - used >= cost
add       = append occupant, used += cost, fill = used / capacity
```

`src/world/build.cpp` exposes `Paddock`, `paddock_capacity`, `paddock_cost`,
`paddock_size_class`, `paddock_can_add` and `paddock_add`. The size class is
the **phenotype length category** (`DinoPhenotypeTable` in
`src/formats/dino_data.*`): `length = int(HeadLen * head_ratio / HeadLen *
body) + BodyLen + int(TailLen * tail_ratio / TailLen * body)` (each part
truncated), with `proportion.dat` blended by the Part 2 merge and the
Part 3 Head/Body/Tail Length; thresholds 200/620/900 (class 3 above 900). Imported
dinosaurs fill paddocks in record order; Cpg04 gives 9 paddocks, total
capacity 492, and 20 dinosaurs consume **336**, matching retail exactly.
`DINO_DEBUG_PHENO=1` prints each dinosaur's genotype, length and class.

`--selftest` checks the capacity maths, the cost table, the duplicate rule,
filling and the no-room rejection.

Open: the purchase path (store charge to `DINOACHAT`, store-row removal) and
the `WARNING_NO_ROOM_IN_FENCE` panel path. The per-individual
`dinosaur+0x1e0` size class is solved and ported; see
`notes/paddock-size-class-runtime.md`.
Per-paddock numbers and the all-campaign scan:
`notes/paddock-capacity-verification.md`.

## Building construction (P4b)

The retail panel path is wired end-to-end:

```text
OPEN_CONSTRUCTION (right panel) -> OPEN_BUILDING -> category tab
  -> CONSTRUC_*_BUTTON (from the `.bbi` tail) -> click the ground
```

`ParkHud::click` exposes the category tabs (`CONSTRUCTION_BUILDINGS_BUTTON`,
`CONSTRUCTION_BARAQUES_BUTTON`, ...) and every `CONSTRUC_*_BUTTON` as a
`construct <widget>` action; the viewer resolves the widget to its
`BuildingList.ini` index by scanning the `.bbi` `tail_strings`.

The charge is **`.bbi` file offset 21** (`D+0x18`), proven by
`pvwStateBuildingConstruction` (`0x5306e1` gate, `0x5307ac`
`FUN_00585d20`); file offset 25 is the visitor entry price, not the build
cost. The port checks the footprint against existing buildings and the
island bounds, then deducts the cost from `park_budget` (the HUD
`BUDGET_AMOUNT` follows immediately). A failed check prints the retail
category name (`WARNING_NOT_ENOUGH_MONEY`) and places nothing.

```text
--construct NAME        select by BuildingList name or CONSTRUC_*_BUTTON
--construct-pos X,Y     place the selected building (repeatable, map units)
--park-budget N         override the start budget (tests/debug)
```

`--park-click 748,177 --park-click 582,28 --park-click 577,243
--construct-pos 600,300` selects `RestCirc` through the panel and places it:
cost 2,500, budget 75,000 -> 72,500. A second placement at the same cell is
rejected as overlapping.

## Dinosaur purchase

The store catalog (`Market.ini`, 54 entries) and the paddock model are wired
together for a headless purchase:

```text
--buy-dino NAME        a Market.ini display name (e.g. Zarbosaurus)
--buy-paddock N        optional paddock index; default first with room
```

The flow mirrors `pvwStateFenceInsertion`/`stFence`:

1. resolve the full 18-gene genotype from the catalog, map its dominant head
   gene to the 20 base species and derive the phenotype size class;
2. find a paddock with `capacity - used >= 8 << class` (a missing/duplicate
   entry prints `WARNING_NO_ROOM_IN_FENCE` and does not charge);
3. check `park_budget >= price` (`WARNING_NOT_ENOUGH_MONEY`);
4. spawn the dinosaur, add its cost to `used`, and deduct the price.

Cpg04: `--buy-dino Zarbosaurus --park-budget 10000` → price 4,900, budget
5,100, 21 dinosaurs, paddock used 400. Both failures show the retail warning
panel when the park HUD is loaded (`WARNING_NO_ROOM_IN_FENCE` → “There is not
enough room in the enclosure.”, `WARNING_NOT_ENOUGH_MONEY` → “Not enough
money!”; `notes/buy-warning.png`). The retail store-row removal
(`FUN_00594270`), the `DINOACHAT` count/total bucket and the interactive
insertion state are not modelled yet; this is the headless core of the
purchase branch.

## CLI (also used by tests)

```text
--build                  start with the path tool
--build-cell N           cell size (default 4)
--build-load FILE        load a placement file
--build-save FILE        save placements on exit
--place-path X,Y         add a path cell (cell coordinates, repeatable)
--place-fence X,Y,E      add a fence edge, E=0 south / 1 east (repeatable)
--pick-test X,Y          print the terrain cell under a pixel on frame 0
--selftest               run grid/visitor logic checks and exit
```

`--selftest` builds a grid, toggles paths/fences, checks undo and world→cell
mapping, spawns visitors and runs the SIM-2b scorer, checks the dinosaur
action chooser and the paddock capacity/cost/duplicate/fill rules, and checks
the baked mesh counts. It prints `selftest: PASS (N checks)`; pytest asserts
the count.

Placement file (`notes/build-test.txt` style):

```text
cell_size 4
size 256 256
path 128 128
fence 128 128 0
path 129 128
```

`--build-load` recreates the grid from `size` × `cell_size`; unknown or
out-of-range cells are ignored.

## Evidence

```text
build: cell 4.0, grid 256x256, 2 path cells, 1 fences; mesh 2 tiles, 1 segments
```

- `notes/build-preview.png` — L-shaped path + fence line, allosaurus on the
  path.
- `notes/build-preview2.png` — wider view of the same layout.
- `tests/test_viewer_cpp.py`:
  - `test_cpp_build_save_load` (counts, file contents, reload)
  - `test_cpp_pick_terrain` (ray pick returns a cell)
  - `test_cpp_occupation_mask_blocks_placement` (blocked isl01 cell rejected)
- `src/world/build.cpp`, `src/world/build.h`, `src/world/terrain.cpp`
  (`Terrain::buildable`, mask load).
- `notes/raw-recon.md`, `notes/occupation-masks.png` — the `.raw` mask
  (0 = buildable, 255 = blocked).

## Open items

- Validation rules beyond the occupation mask: steep slopes and dinosaur
  pens.
- Costs/limits and a HUD (needs the overlay/UI layer).
- Path connectivity / visitors (SIM-2).
- Per-triangle fence material split (material 1 is `laser2.tga`, 8 triangles).
- The paddock fence is 8.8 m tall; other wall types (`.bwi`: LowWood,
  HighSteel, …) need their own mesh generation.
