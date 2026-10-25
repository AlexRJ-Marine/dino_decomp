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
  - positions `(x, y, h)`
  - central-difference normals
  - UV `u = x/(W-1)`, `v = -1 + y/(H-1)` so the shader's `v_tex = 1+v`
    maps image row 0 to world row 0.
  - 522,242 triangles.

The `.lnd` `width`/`height` (520×376) are kept as metadata. The 1024-unit
world is confirmed by `.imp`: every instance on every island lies in
`0..1024` (`docs/formats/imp.md`).

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
samples the 512×512 heightmap. Trees are upright; slope tilt is not applied.

`isl01` result: 9 groups, 1,536 instances, 52,291 triangles; coordinates match
the `.imp` totals exactly. Texture paths resolve through the `.blf` directory
(`Data/Graphics/Textures/buildings`).

Options:

```text
--no-vegetation      skip placed objects
```

## Render

`dino_viewer --lnd <isl01.lnd> --asset-root game/%MAINDIR%` builds the mesh,
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
.\build\mingw\dino_viewer.exe --frames 3 `
  --screenshot notes\isl01-terrain.bmp --width 1200 --height 760 --no-vsync `
  --lnd "game/%MAINDIR%/Data/Design/landscape/isl01/isl01.lnd" `
  --asset-root "game/%MAINDIR%" --yaw 35 --pitch 35
```

Result: `notes/isl01-terrain.png` (bare terrain) and
`notes/isl01-vegetation.png` (terrain + 1,536 trees). The island, volcano,
beaches, islets, ocean basemap and placed palm/fern/sequoia groups all line up.

## Evidence

- `src/formats/lnd.cpp`, `src/world/terrain.cpp`,
  `src/formats/imp.cpp`, `src/formats/blf.cpp`, `src/world/vegetation.cpp`.
- `notes/isl01-heightmap.png`, `notes/isl01-basemap_01.png`.
- `notes/isl01-terrain.png`, `notes/isl01-vegetation.png`.
- `tests/test_viewer_cpp.py`:
  - `test_cpp_lnd_validate` (terrain + vegetation counts)
  - `test_cpp_lnd_no_vegetation`
  - `test_cpp_terrain_window`
- `tools/formats/imp.py`, `blf.py`; `tests/test_imp.py`, `test_blf.py`
  (78 `.imp` / 24 `.blf`, 0 errors).

## Open questions

- Meaning of the `.lnd` float (16.0/20.0): water level or height scale.
  Height 92 with water at ~16 looks plausible.
- Water plane / shoreline animation; currently the basemap ocean is painted on
  the heightfield.
- Detailmap blending and terrain material layers.
- `.raw` occupation mask (1024×1024) semantics.
- Slope tilt and LOD selection for vegetation (lod2 mesh + `_lod2` texture).
