# Buildings: `.bbi` definitions and static placement

Status: **loader + placement transform solved.** Position (footprint centre),
orientation (`state` = 90°), terrain Z, the primary `.skin` model and the
`model[0] = 1` slope basis match the retail functions. `model[1]` is 0.0 in
all files. This is the first world-object bring-up step for WORLD-2/SIM-2.

## Loader

`src/formats/bbi.cpp` reads the retail `.bbi` layout from `docs/formats/bbi.md`:

```text
0..120   121-byte header (30 x u32 + flag)
121..    ten length-prefixed ASCII paths:
           skin, crash LOD1, destroy LOD1, LOD1, LOD2 x4, skel, anim
...      length-prefixed tail strings (texture dir, .ini sounds, CONSTRUC_*)
```

`load_building_list()` reads `BuildingList.ini` (leading count and `//`
comments stripped). `Entrance` is kind 9 in that list; the ten paths are
`Data/Graphics/buildings/entrance/entrance_*`.

The `.bbi` mesh order is: normal LOD1 `.skin`, crash, destroy, then either
the static normal LOD1 `.mesh` (Entrance, seats, doors) or the construction
model. The **static LOD1 `.mesh` at index 3 is the placement model when it
is a normal mesh**: it is in metres and in the placement-pivot space, so the
Entrance pier reaches the beach and the boat sits at the water end. The
`.skin` full mesh is in centimetres and centred on its own bind origin; using
it for the Entrance puts the boat on the beach without the pier. Buildings
whose index 3 is the construction model fall back to the animated `.skin` at
index 0. `--building-model skin|mesh` forces either choice for inspection.
The material texture name is resolved under the `.bbi` texture directory when
present, otherwise `Data/Graphics/Textures/buildings` (the Entrance material
is `boat.tga`).

A static `.mesh` can have several chunks, and retail draws them all. The
port loads every chunk (`dino::load_mesh_chunks`) and draws one part per
chunk and material with its own texture. `entrance_lod1.mesh` is the boat,
radar, `ponton2` (gangway, `boat.tga` + `ponton.tga`) and `ponton` (pier,
posts and gate, `ponton.tga`): 854 vertices, 577 triangles, 5 parts.

### Starting road

When a park starts, retail lays one free road from each Entrance
(`FUN_0051dad0`): the `.bbi` door segment (file 89..101) is rotated into the
building's door rect (`FUN_00586b80`), its centre snapped to the 4-unit grid,
and the road runs 8 units back and 52 units out along the orientation. isl01
gives `(524,392)-(524,452)`, the rectangle in every retail free-game save. The
port lays it on a Free Game start (and, opt-in, for a `--building` Entrance
with `--build --entrance-road`). Evidence: `notes/entrance-dock-road-recon.md`,
`notes/entrance-dock-road.png`.

## CLI

```text
--building FILE       place a .bbi building (primary .skin; needs --lnd)
--building-pos X,Y    footprint min corner 0..1024 (default 520,376)
--building-state N    orientation 0..3 (90-degree steps, swaps footprint)
--building-angle DEG  extra Z rotation (debug)
--building-scale N    scale
--wld FILE            read a .wld/.sav world block
--wld-info            print the world block and exit
--focus-building      frame the first placed building (distance 60)
```

Placement uses the retail rules below. The free-game Entrance is
`(lnd.width, lnd.height)` with orientation `lnd.unknown_int` (0/1/2),
runtime-confirmed: isl01 `(520,376) state 1`, isl02 `(312,504) state 2`, and
all seven campaign Entrance records match. For isl01 this gives centre
`(525.5,387.5)`, yaw 90°.

## Placement transform (solved)

Evidence: `notes/building-placement-recon.md`; functions `0x0053d1a0`
(world object → graphics), `0x00589100` (`spPark::AddBuilding`), `0x00586b80`
(`spBuilding` ctor), `0x005fecf0` (`gpkBuilding` ctor) and `0x0061ffc0`
(landscape placement).

```text
record cell_x/cell_y      # footprint min corner, world units 0..1024
footprint_w/footprint_h   # .bbi file bytes 0/4, world units, not cells
state 0..3                # 90-degree orientation, NOT construction state
if state even: span_x = footprint_w - 1, span_y = footprint_h - 1
else:          span_x = footprint_h - 1, span_y = footprint_w - 1
X = (cell_x + (cell_x + span_x)) * 0.5     # footprint centre
Y = (cell_y + (cell_y + span_y)) * 0.5
Z = engine_height(X, Y) + model[1]       # model[1] = 0.0 in all 39 .bbi
yaw = 90 deg * state
```

- The heightfield is a 512-sample map over the 1024-unit world: sample 0 sits
  at world -512, spacing 2. `FUN_005c0b80` bilinearly samples the u16 map and
  scales by 1/256. The port's `Terrain::height_at` and `build_terrain` use
  the same mapping (`src/world/terrain.cpp`).
- **The runtime heightmap is bottom-up**: `mgHeightmap::Load` `0x5c06b0`
  reads file rows last-to-first, so runtime row 0 is the file's last row.
  The port flips both the heightmap and the occupation mask at load
  (`load_raw_u8(..., flip_rows=true)`). The old top-down sampling put the
  isl01 Entrance on the volcano. See `notes/terrain-orientation-recon.md`.
- No scale and no axis swap: the model is `translate(X,Y,Z) · Rz(90°·state)`.
  The model origin is the placement pivot; it is not re-centred.
- The primary model is the **first `.bbi` path**, a `.skin` whose full mesh is
  centred on the pivot (centimetres). The static `.mesh` at index 3 has a
  pivot offset and must not be used as the actor model. Static meshes are used
  only as a fallback.
- `model[1]` (`.bbi` byte 113, f32) is `0.0` in all 39 retail files, so there
  is no Z offset. The Entrance hull extends to z `-7.18 m` from its pivot;
  retail places the pivot exactly on the heightfield, so the visible sink is
  the asset pivot and matches the original. Only `LunchDoor.bbi` sets
  `model[0] = 1` (slope-aligned basis); the basis is decoded below, the port
  currently uses yaw-only for all.
- `state` is the orientation consumed by `FUN_00586b80` and the jump table at
  `0x586d0c`; the footprint swap is part of the same rule. Default in the
  factory is 1.

### `model[0] = 1` slope basis (`FUN_0061c510`)

For the one `.bbi` with `model[0] = 1` (`LunchDoor`), the placement basis is
recomputed from the terrain normal instead of a pure yaw. The function takes
`(X, Y, forward)` and:

1. scales the world position by the landscape `+0x14` (world→heightmap
   sample, 0.5) and bounds-checks against the heightmap width/height `+4/+8`;
2. builds a normal from central differences at three radii `r = 0.5, 1.0,
   2.0`:
   ```text
   n = ( h(x-r, y) - h(x+r, y),
         h(x, y-r) - h(x, y+r),
         r^2 * height_scale^2 )
   n = normalize(n)
   ```
   then normalizes the sum of the three radii;
3. `R = normalize(forward × n)`;
4. `U = R × n`;
5. sets the node matrix axes to `R`, `U`, `n`
   (`FUN_004664c0` / `FUN_004664e0` / `FUN_00466500`).

The height sampler is the same `FUN_005c0b80` used by the terrain (bilinear
u16 × 1/256). The port implements this in `dino::slope_basis`
(`src/world/terrain.cpp`): three central-difference normals at r = 0.5, 1, 2
world units, a yaw-projected right axis and `up = n × right`; on flat ground
it reduces exactly to the yaw transform. `--selftest` checks the flat and
tilted cases. `LunchDoor` is the only `.bbi` that takes this branch.
`model[1]` is a Z offset and stays 0.0.
- Engine cross-checks: isl01 Entrance at cell `(520,376)` state 1 → centre
  `(525.5,387.5)`, Z **18.0**; the port prints the same. The old 69.4 value was the
  top-down heightmap row (file row 188, the volcano); the engine samples file
  row 323, the shoreline. Campaign Entrances
  sit at each island's `.lnd` width/height (isl01 `520/376`, isl02 `312/504`,
  isl03 `704/448`) and the generated access roads touch the computed centre.

The port exposes `--building-state N`; the campaign/save path uses the
record's `state`. Cpg04 on isl04 places **57/57** buildings, including the 12
skinned ones loaded from their primary `.skin` (zero skin bones are allowed;
`load_skin_auto` finds the full mesh by its material tail).


## Evidence

```powershell
$G = "game/%MAINDIR%"
.\build\mingw\dino_port.exe --frames 3 --no-menu --width 800 --height 600 `
  --no-vsync --no-save-config --no-hud --no-vegetation --no-water `
  --lnd "$G/Data/Design/landscape/isl01/isl01.lnd" --asset-root $G `
  --building "$G/Data/Design/buildings/Entrance.bbi" `
  --building-pos 520,376 --building-state 1 --focus-building
```

- `notes/entrance-save.png` - Entrance at the free-game save cell; the port
  prints `centre=525.5,387.5 z=69.4 state=1 angle=90` (engine values).
- `notes/entrance-center2.png` - the primary `.skin` boat at the island
  centre (texture `boat.tga`).
- `notes/cpg04-buildings.png` - Cpg04 on isl04, all 57 buildings.
- `notes/menu-start-entrance.png` - Free Game start on isl01 with the
  auto-placed Entrance at `(520,376)`, Z `69.4`.
- `notes/building-placement-recon.md` - decompiled call chain and maths.
- `FUN_0061C510` - slope-aligned basis (three-radius normal); `FUN_005C0B80`
  height sampler; `FUN_004664C0/E0/500` basis-axis setters.
- `notes/calibration-2026-10-26.md` - runtime Entrance cells/states and water
  Z (isl01/isl02), plus the visitor need ctor.
- `tests/test_app_cpp.py::test_cpp_building_bbi_place`,
  `test_cpp_building_placement_transform`,
  `test_cpp_wld_reader_matches_retail`.

## Open

- Construction cost is solved: `.bbi` file offset 21 (`Building::
  construction_cost`), charged to the CONSTRUCTION bucket by
  `FUN_00585d20`; the visitor entry price is file offset 25. The
  construction/destruction flags in the simulation object (`+0x64..+0x68`)
  remain open.
- `BuildingList.ini` kind -> `.bbi` mapping is done through the list index;
  placement from the save/campaign path is live, and the construction panel
  path (`CONSTRUC_*_BUTTON` -> `--construct`) now charges the build cost and
  checks footprint overlap (`docs/platform/build.md`).
- Skinned/animated building parts (the `.skel`/`.anim` at indices 8/9) and
  destruction meshes; only the full-mesh bind pose is rendered.
