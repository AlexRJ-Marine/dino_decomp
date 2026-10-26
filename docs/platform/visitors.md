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

## Behaviour (SIM-2b)

Visitors use the retail state machine from `docs/decomp/visitor-ai.md`:

```text
Choose -> Travel -> Use -> Choose ...   (1 in 5 leaves instead of choosing)
Leave -> walk to the entrance -> despawn
```

- **Profiles:** the free-game easy table (Old/Surfer/Parent/Kid/Hooligan/
  Scientist) is loaded from the six `.bvi` files; each contributes its five
  preference weights, walk speed and spawn weight (30 each).
- **Attractions:** every placed building with category 1..9 becomes an
  attraction. `appeal[profile] = clamp(sum(bbi_weight[k] * profile_weight[k]),
  -100, 100)`; `radius` is the `.bbi` field at 105 (world units), price is
  file 25, duration file 37, need deltas files 81/85.
- **Score:** `rand()%6000 - 3000 + (appeal/appeal_scale)*(99-need0)^2 +
  need_delta1*(99-need1)^2 + need_delta2*(99-need2)^2 - 300*distance -
  500*price`, Manhattan cell distance. The best enabled/in-radius attraction
  wins; a BFS route follows path cells.
- **Needs:** start 39/99/99 (the actor ctor sets 99 then need0 = 99*0.4).
  Timed decay (10 s need1, 7.5 s need2, 12.5 s need0) and the attraction-use
  snapshot rule from `FUN_00576530` / `FUN_005763f0` are implemented. Need1
  decay pauses while using an attraction with nonzero duration; need0 may go
  negative and then leaves.
- **Exit:** the Entrance building (category 10) cell; if absent, the path
  cell nearest the grid origin. Visitors despawn on arrival.

Use `--visitor-stats` to print a state/need summary at exit.
`--visitors N` still spawns them on the placed path network; buildings come
from `--building` or `--wld`/`--load-port`.

## Legacy wander mode

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

- More visitor types (all 24 `.bvi` files; the port currently uses the six
  easy free-game profiles) and profile-specific meshes.
- Avoid visitor overlap and queue/patience at crowded attractions.
- Shows (kind 6) and the show schedule window; current attractions are
  building definitions only.
- Visitor spending/budget integration.
