# `.blf` format

Status: **solved** — 24/24 files share one layout; all 48 LOD mesh paths
resolve on disk.

`.blf` is a landscape-object descriptor: two LOD meshes plus the texture
directory that holds their material textures.

## Layout

```text
u32  version        2
u32  lod_count      2
u32  unknown        0
u32  texture_count  2   (base texture + "_lod2" texture)
lod_count x (u32 len + NUL-terminated .mesh path)
1 x (u32 len + NUL-terminated texture directory)
```

Example `palm1.blf` (173 bytes):

```text
DATA\graphics\landscape\trees\palmtrees\palm1_lod1.mesh
DATA\graphics\landscape\trees\palmtrees\palm1_lod2.mesh
DATA\graphics\TEXTURES\buildings
```

The mesh's material table names the texture inside that directory, e.g.
`palm1.tga`; `Data/Graphics/Textures/buildings/palm1.tga` exists. So a renderer
resolves `texture_dir + "\" + mesh.material.texture`.

## Evidence

- 24/24 files parse to exactly three strings and end at EOF.
- Header is always `(2, 2, 0, 2)`.
- Every referenced `_lod1.mesh` / `_lod2.mesh` exists under `game/%MAINDIR%/`.
- Mesh geometry is in metres: `palm1_lod1` is 56 vertices / 37 triangles,
  5.15 m tall; `highacacia1_lod1` is 15 m tall.
- `palm1_lod2` is 13 vertices / 8 triangles — a true distant LOD.

## Reader/API

`tools/formats/blf.py`: `read_blf(path)` → `Blf(lods, texture_dir, ...)`,
`scan(root)`.

Tests: `tests/test_blf.py`.
