# Path and fence placement (SIM-1)

Status: **working** - grid placement, save/load, undo, terrain picking and
retail fence/path visuals; the retail paddock-capacity model is ported for
imported dinosaurs.

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
`paddock_can_add` and `paddock_add`. Imported dinosaurs fill paddocks in
record order, because the save stores no per-dinosaur position; Cpg04 gives
9 paddocks, total capacity 492, and 20 dinosaurs consume 160 (cost index 0).

`--selftest` checks the capacity maths, the cost table, the duplicate rule,
filling and the no-room rejection.

Open: the purchase path (store charge to `DINOACHAT`, store-row removal),
the `WARNING_NO_ROOM_IN_FENCE` panel path, and the runtime range of
`dinosaur+0x1e0` (the cost index). The port uses index 0 until confirmed.
Per-paddock numbers and the all-campaign scan:
`notes/paddock-capacity-verification.md`.

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
mapping, spawns 12 visitors on a 40-cell loop for 900 update steps, verifies
they moved and stayed on path cells, and checks the baked mesh counts. It
prints `selftest: PASS (18 checks)` and is covered by pytest.

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
- `src/world/build.cpp`, `src/world/build.h`.

## Open items

- Validation rules: block steep slopes, water, buildings, dinosaur pens.
- Costs/limits and a HUD (needs the overlay/UI layer).
- Path connectivity / visitors (SIM-2).
- Per-triangle fence material split (material 1 is `laser2.tga`, 8 triangles).
- The paddock fence is 8.8 m tall; other wall types (`.bwi`: LowWood,
  HighSteel, …) need their own mesh generation.
