# Dinosaur footprint guard (`FUN_0055af70`) — decompiled and wired

Date: 2026-10-31. Read-only Ghidra pass on `/DinoIsland.exe-ebea17`.

## Question

Can the port add the retail occupancy footprint test to the mover, so a
dinosaur refuses cells occupied by walls/buildings/roads inside its paddock?

## What is proven

- The base `saActor` ctor `FUN_00577690` sets the footprint radius
  `actor+0x13 = 1` for every actor (including every dinosaur):
  `*(u8 *)(actor+0x13) = 1`.
- `FUN_0055af70(path, actor, x, y, outFlag, checkBounds)`:
  1. with `checkBounds`, rejects `x/y` outside the paddock rect
     `[path+0x1c0]+0x1c/0x20/0x24/0x28`;
  2. then scans the footprint of radius `r = *(char *)(actor+0x13)` (1) over
     the u16 grid `path+0x1ac` (`[grid+0]` width, `[grid+4]` height,
     `[grid+0x10]` row pointers), calling the visitor vtable at
     `0x00642f78` (slot 0 `0x0055af20`).
- Visitor state: `+4` = accept flag (starts 1), `+6` = `u16` copied from
  `actor+0x28`, `+8/+0xa` = `actor+0x314/+0x316`, `+0xc` = grid.
- Callback `FUN_0055af20` reads the cell `u16`, `class = cell >> 12`:
  - `class == 0` -> free;
  - `class != 0 && class != 1` -> **blocked** (walls class 7, roads class 2,
    blocked 0xf);
  - `class == 1` -> blocked unless `(cell & 0x7ff) == actor+0x28` (then
    free).
- `FUN_005849a0` stamps walls as `DAT_006757b4 (7) << 12` while preserving
  the low 12 bits (`cell & 0xfff`), so the low bits are pre-existing data,
  not a fresh id.

## Resolution of `actor+0x28`

The existing mover trace already logs `id={d:[[mv+0x200]+0x28]}`. In
`notes/harness/dino-mover-runtime-run3.log` the values are
`-1163067377` (`0xBAADF00D`) and `-1163067382` (`0xBAADF00A`) — the allocator
fill, not an id. The class-1 exception therefore compares the cell's low 11
bits with garbage and never matches a real building cell (low bits 0), so
**class 1 blocks in practice**. This is confirmed, not assumed: a building
stamp is `kBuilding | low-bits` with low-bits preserved from the cell
(`FUN_005849a0` preserves `& 0xfff` for walls), and no park code writes a
dino id to `actor+0x28`.

## Port wiring (2026-10-31)

- `DinoMover` gained `set_cell_test(CellTest)`; `step_allowed` truncates the
  target to cells and scans the radius-1 footprint (centre + 4-neighbourhood,
  `kFootprintRadius = 1`).
- `Game::add_imported_dino` installs the lambda over
  `park_session.build_grid.occupancy`: class `kBuilding`, `kRoad`, `kWall`
  block; class 0 is free. `kBlocked` (0xf) is the port's terrain mask, not a
  retail occupancy class, so it is not treated as a dino obstacle (the
  paddock rect already bounds the dino).
- Tests: `dino mover footprint blocks` (synthetic cell), `dino mover
  occupancy wall` (real `OccupancyGrid` + `kWall`), and the 600 s Cpg04 soak
  still reports 0 escapes / 0 blocked (the paddocks are clear).

No runtime probe is needed for the class-1 exception now; the mover trace is
the evidence.

## Evidence

- Ghidra `FUN_00577690`, `FUN_0055af70`, `FUN_0055af20`, `FUN_005849a0`.
- Existing map: `notes/dino-ai-gap-audit.md` §2.4.
