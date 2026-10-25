# Visitor agents (SIM-2)

Status: **working** — visitors spawn on the placed path network and walk
cell-to-cell using the retail two-pose walk meshes.

## Model

Retail people are **not skinned** (no `.skel`/`.skin` in
`Data/Graphics/Actors/people`). The `.bvi` descriptor lists one mesh per pose:

```text
woman_standing_LOD1.mesh
woman_walk1_LOD1.mesh     woman_walk2_LOD1.mesh
woman_run1/2/3_LOD1.mesh  woman_seated_LOD1.mesh
woman_happy1/2/3_LOD1.mesh  woman_unhappy1/2/3_LOD1.mesh
texture dir: DATA\graphics\TEXTURES\actors\people\customers
texture: wom.tga
```

`woman_walk1_LOD1.mesh` is 72 vertices / 52 triangles / 1.66 m tall. The walk
cycle is a flip book between `woman_walk1` and `woman_walk2`.

## Behaviour

- `VisitorSystem::spawn(grid, count)` collects path cells and places each
  visitor at a random cell centre, facing a random path neighbour (fixed seed
  `0x51ce5eed`, so runs are reproducible).
- `update(grid, terrain, dt)` moves at 1.6 world units/s toward the next cell
  centre. On arrival it records the previous cell, picks a random neighbour
  (never the one just left unless it is a dead end) and turns to face it.
- Walk phase advances at 3 cycles/s; the mesh is `walk1` on even phases and
  `walk2` on odd phases.
- `build_mesh` bakes all visitors into one world-space mesh at
  `terrain.height_at(x, y)`, so the whole crowd is one draw call.

## Controls/CLI

```text
--visitors N           spawn N visitors (requires --build paths)
--visitor-pose1 FILE   walk pose A (default woman_walk1_LOD1.mesh)
--visitor-pose2 FILE   walk pose B (default woman_walk2_LOD1.mesh)
--visitor-texture FILE texture (default wom.tga)
```

Example (path loop around the dinosaur):

```powershell
$G = "game/%MAINDIR%"
.\build\mingw\dino_viewer.exe --lnd "$G/Data/Design/landscape/isl01/isl01.lnd" `
  --asset-root $G --visitors 8 --build --dino-pos 512,512 `
  --place-path 126,126 ... --place-path 132,132 ...
```

## Evidence

- `notes/visitors.png` — 8 visitors walking a 24-cell loop around the
  allosaurus.
- `tests/test_viewer_cpp.py::test_cpp_visitors_validate` asserts
  `visitors: 8 spawned on 24 path cells`.
- `src/world/visitors.cpp`, `src/world/visitors.h`.

## Open items

- Entrance-based spawning and park entry/exit (needs the entrance/`.bbi`
  building).
- Needs/attraction use, queues, spending (SIM-4).
- More visitor types (`.bvi` files) and a descriptor parser.
- Avoid visitor overlap and pick a less repetitive neighbour rule.
