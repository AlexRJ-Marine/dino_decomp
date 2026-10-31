# Dinosaur footprint guard (`FUN_0055af70`) — decompiled, not wired

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

## Why it is not wired yet

`actor+0x28` is the only unknown. It is not set by the base ctor or by
`FUN_005701f0`/`FUN_00586690`; the `DINORESET` trace read `+0x28` as
`BAADF00D` (uninitialized at that point). The class-1 exception would allow a
building cell when the low 11 bits happen to equal this value, so wiring
"block class 1" without knowing the field could diverge from retail.

## Next probe (settles it)

Break on `0x55af20` for one walking Cpg04 dinosaur and log
`{p:[esp+0x10]}` (actor pointer), `{p:[actor+0x28]}` and the cell `u16` for a
step next to a building/structure. Then a port adapter can check the class
and the low-bit exception exactly.

The port already has the occupancy grid (`BuildGrid::occupancy`, class
constants `kBuilding=0x1000`, `kRoad=0x2000`, `kWall=0x7000`,
`kBlocked=0xf000`) but `DinoMover::step_allowed` only checks the paddock
rect. Do not add the grid check until the probe above is done.

## Evidence

- Ghidra `FUN_00577690`, `FUN_0055af70`, `FUN_0055af20`, `FUN_005849a0`.
- Existing map: `notes/dino-ai-gap-audit.md` §2.4.
