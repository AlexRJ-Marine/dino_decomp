# Island terrain and vegetation (WORLD-1)

Status: **isl01 loads and renders** — `.lnd` + 8-bit `.raw` heightmap +
1024×1024 basemap TGA, plus all 1,536 `.imp`-placed trees (52,291 triangles).

## Formats

`src/formats/lnd.cpp` reads the landscape descriptor (see
`docs/formats/lnd.md`):

```text
name_key, basemap, detailmap, heightmap, skybox, water, water2, occupation
f32 unknown (16.0 on isl01)
u32 width/height (520×376 on isl01)
u32 unknown (1)
music
25 x vegetation .imp / "None"
```

`src/world/terrain.cpp`:

- `load_raw_u8`: square 8-bit heightmap. `isl01` is 512×512
  (`heightmap_01.raw`, 262,144 bytes, heights 0..92).
- `build_terrain`: 512×512 grid, world size 1024 units, height scale 1.0.
  - sample 0 sits at world `-512` and the spacing is `1024/512 = 2` units,
    matching the engine heightfield (`FUN_005c0b80`: `map = world * 0.5`).
  - positions `(x - 256) * 2, (y - 256) * 2, h`
  - central-difference normals
  - raw rows are flipped at load (`load_raw_u8(..., flip_rows=true)`):
    runtime row 0 is the file's last row, matching `mgHeightmap::Load`
    `0x5c06b0` and the 1024x1024 landscape grid `0x61c2d0`.
  - UV `u = x/(W-1)`, `v = -y/(H-1)` so the shader's `v_tex = 1+v`
    maps image row 0 to the bottom row after the flip; this keeps the
    top-down basemap aligned with the engine-order heights.
  - 522,242 triangles.

The `.lnd` `width`/`height` (520×376) are kept as metadata. The 1024-unit
world is confirmed by `.imp`: every instance on every island lies in
`0..1024` (`docs/formats/imp.md`).

## Detail map (`detailmap.ini`)

Retail draws the island with `DATA\states\detailmap.ini` (bound by
`0x5f3f10` before `gpkLandscape`): unlit, no fog, stage 0 = the basemap with
roads painted in, stage 1 = **AddSigned** of the `.lnd` detail map
(`detail + basemap - 0.5`) on texture coordinates `world/16`
(`0x61fa1f..0x61facf`, landscape parameter `1/16`). The port does the same in
the terrain pass; the detail texture is mipmapped to avoid aliasing at park
distance (retail requests none). `--terrain-lit` restores the old lit,
fogged basemap. Evidence: `notes/road-network-recon.md`,
`notes/terrain-detail-compare.png`, `test_cpp_terrain_detail_map`.

## Vegetation (`.imp` + `.blf`)

`src/world/vegetation.cpp` follows the `.lnd` vegetation list:

```text
.lnd vegetation entry (e.g. isl01_ImpPalm1.imp)
  -> .imp: model path + placed instances (x, y, scale, angle)
  -> .blf: lod1 mesh + texture directory
  -> lod1 .mesh + its first material texture (e.g. palm1.tga)
```

Each instance is baked into one mesh per entry:

```text
world_x = x - 512 + (cos(a)*lx - sin(a)*ly) * s
world_y = y - 512 + (sin(a)*lx + cos(a)*ly) * s
world_z = height_at(x - 512, y - 512) + lz * s
```

Normals get the same Z rotation and are renormalised. `height_at` bilinearly
samples the 512×512 heightmap with the same `world*0.5` mapping. Trees are
upright; slope tilt is not applied.

The `.imp` positions are already in engine world coordinates and are **not**
flipped. Cross-check on isl01: all 1,536 instances land on terrain
(`height > 16`) in engine orientation, 0% on free buildable cells; in file
order only 71.4% are on land and 40.6% on free cells.

`isl01` result: 9 groups, 1,536 instances, 52,291 triangles; coordinates match
the `.imp` totals exactly.

### LOD and culling (retail gpkLandscape)

Retail files each object into a 15x15 grid of 64-unit sectors
(`(int(y)>>6)*15 + (int(x)>>6)`, centres `32+64*i`, Z from the heightmap).
Every frame a sector measures its distance `d` to the camera, and each object
in it draws `.blf` LOD1 if `d <= 200*K`, LOD2 if `d <= 400*K`, otherwise
nothing. `K` is `Dino.ini [GRAPHIC] LODQuality` (HIGH 2.0, MEDIUM 1.0,
LOW 0.5, AUTO 1.0; retail ships AUTO). Evidence:
`notes/landscape-lod-recon.md` (`0x603ad0`, `0x508c10`, `0x609970`,
`0x5fb8b0`, `0x5fbf90`, `0x51d1d0`).

The port bakes one batch per (entry, sector, LOD) (isl01: 422 batches, LOD2
14,576 triangles) and applies the same rule each frame. Options:
`--lod-quality Q` (default read from `Data/Config/Dino.ini`), `--no-lod`
(everything LOD1, the old behaviour). Vegetation uses the cut-out AlphaTest
(Ref 128, Greater) because the LOD2 textures are mostly transparent.

Retail has no frustum test on this path. The port adds one as a pure
optimisation: a bounding sphere per batch against the six clip planes
(`--no-cull` turns it off); the picture is unchanged. Capture:
`notes/vegetation-lod-ab.png`; tests `test_cpp_vegetation_sector_lod`,
`test_cpp_vegetation_frustum_cull`. Texture paths resolve through the `.blf` directory
(`Data/Graphics/Textures/buildings`).

Options:

```text
--no-vegetation      skip placed objects
```

## Render

`dino_port --lnd <isl01.lnd> --asset-root game/%MAINDIR%` builds the mesh,
loads the basemap TGA, and draws it with the existing GLSL pipeline.

Options:

```text
--lnd FILE           island descriptor
--asset-root DIR     root for paths inside the .lnd
--terrain-size N     world size (default 1024)
--height-scale N     height multiplier (default 1)
```

Example:

```powershell
.\build\mingw\dino_port.exe --frames 3 `
  --screenshot notes\isl01-terrain.bmp --width 1200 --height 760 --no-vsync `
  --lnd "game/%MAINDIR%/Data/Design/landscape/isl01/isl01.lnd" `
  --asset-root "game/%MAINDIR%" --yaw 35 --pitch 35
```

Result: `notes/isl01-terrain.png` (bare terrain) and
`notes/isl01-vegetation.png` (terrain + 1,536 trees). The island, volcano,
beaches, islets, ocean basemap and placed palm/fern/sequoia groups all line up.

## All ten islands and runtime selection

`Data/Design/landscape/IslandList.ini` is the authoritative order
(`10`, then `isl01`..`isl10`). The original builds
`Data\Design\Landscape\<name>\<name>.lnd` from the selected record
(`FUN_0051dad0`); all per-island paths live inside the `.lnd`.

The viewer reads the same list. Starting a Free Game from the retail menu
calls `load_island(name)`, which rebuilds the terrain, basemap, vegetation,
skybox, water plane and their GPU resources and recentres the camera. All ten
islands validate:

| island | heightmap | water Z | notes |
| --- | --- | --- | --- |
| isl01 | 512×512, 0..92 | 16.0 | 9 vegetation groups |
| isl02 | 512×512, 0..70 | 20.0 | |
| isl03 | 512×512, 0..90 | 20.0 | |
| isl04 | 512×512, 0..83 | 20.0 | |
| isl05 | 512×512, 0..59 | 18.0 | |
| isl06 | 512×512, 0..150 | 26.0 | |
| isl07 | 512×512, 0..143 | 17.0 | |
| isl08 | 512×512, 0..192 | 20.0 | highest terrain |
| isl09 | 512×512, 0..144 | 20.0 | |
| isl10 | 512×512, 0..67 | 20.0 | |

Evidence: `notes/menu-start-isl01.png`, `notes/menu-start-isl03.png`,
`notes/menu-start-isl10.png`; `notes/ui-recon.md` ("Free Game start -> park
handoff"); `tests/test_app_cpp.py::test_cpp_menu_start_loads_selected_island`.

## Evidence

- `src/formats/lnd.cpp`, `src/world/terrain.cpp`,
  `src/formats/imp.cpp`, `src/formats/blf.cpp`, `src/world/vegetation.cpp`.
- `notes/building-placement-recon.md` - `FUN_005c0b80` heightfield sampler;
  save camera target `(512,512,32)` = bilinear height **32.0**; isl01 Entrance
  centre `(525.5,387.5)` = **69.4** (the previous `width-1` mapping gave
  68.5/76.0).
- `notes/isl01-heightmap.png`, `notes/isl01-basemap_01.png`.
- `notes/isl01-terrain.png`, `notes/isl01-vegetation.png`.
- `tests/test_app_cpp.py`:
  - `test_cpp_lnd_validate` (terrain + vegetation counts)
  - `test_cpp_lnd_no_vegetation`
  - `test_cpp_terrain_window`
- `tools/formats/imp.py`, `blf.py`; `tests/test_imp.py`, `test_blf.py`
  (78 `.imp` / 24 `.blf`, 0 errors).

## Open questions

- `.lnd` width/height are the free-game Entrance cell; the 0/1/2 value is its
  orientation; the float is the water Z (runtime-confirmed on isl01/isl02,
  `notes/calibration-2026-10-26.md`). The terrain mesh itself is 1024 units
  regardless of these fields.
- Water plane / shoreline animation; currently the basemap ocean is painted on
  the heightfield.
- Terrain material layers beyond the detail map (detail map done).
- `.raw` occupation mask (1024×1024) semantics.
- Slope tilt for vegetation (LOD selection is done, see above).
