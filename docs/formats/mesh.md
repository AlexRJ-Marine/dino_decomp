# `.mesh` format

Status: **geometry + vertex colors decoded**, material block open.

## Evidence

- 536 `.mesh` files parsed; 0 errors; 47,171 vertices; 35,190 triangles.
- Parser: `tools/formats/mesh.py`; tests: `tests/test_mesh.py`.
- Header inventory: `notes/mesh-scan.json`.
- Ghidra: magic check in `FUN_00477b50` at `0x478161` / `0x4782F5`;
  file loader path enters `FUN_004714b0`.

## Header

All `.mesh` files start with:

| Offset | Size | Field | Notes |
|---:|---:|---|---|
| 0x00 | 5 | magic | `01 03 00 00 00` (shared with `.skin`, `.skel`, `.anim`) |
| 0x05 | 4 | hash | u32 LE; varies per file |
| 0x09 | 4 | sentinel | `78 56 34 12` = `0x12345678` |
| 0x0D | 4 | chunk_count | 1 in 516 files; 2..4 in 20 (see Chunks) |
| 0x11 | 4 | name_len | includes trailing NUL |
| 0x15 | name_len | name | ASCII, NUL-terminated |

## Body

| Field | Size | Notes |
|---|---:|---|
| vertex_count | 4 | u32 |
| positions | 12 × vertex_count | 3 × f32 per vertex |
| index_count | 4 | u32; multiple of 3 |
| indices | 4 × index_count | u32; values < vertex_count |
| uv_flag | 1 | `1` = UV block follows |
| uv_count | 4 | equals vertex_count |
| UVs | 8 × uv_count | 2 × f32 |
| normal_flag | 1 | `1` = normal block follows |
| normal_count | 4 | equals vertex_count |
| normals | 12 × normal_count | 3 × f32 |
| tail | rest | material/color data; not decoded |

All 536 files have both UV and normal blocks, with counts equal to
`vertex_count`.

## Tail: vertex colors (partially decoded)

The tail starts with a per-vertex RGBA color table:

| Offset | Size | Field |
| ---: | ---: | --- |
| +0 | 1 | flag (`1`) |
| +1 | 4 | color count (equals `vertex_count`) |
| +5 | 4 × count | RGBA8 per vertex |

For `allo_lod1.mesh`: flag `1`, count `317`, 1,268 bytes of colors, all alpha
`255`. Average color is `(81, 116, 165)` — the blue tint of the dinosaur.
65 of the 536 meshes carry this table (the dinosaur actor meshes); the other
471 start with flag `0` and no color table. After the color table (when
present) comes the material block; for `allo_lod1` it contains the strings
`allo.tga` and `Material #1` and the triangle count `0x17e` (382).

## Tail: material table (decoded 2026-10-28)

After the optional color table (the flag byte is always present in the retail
chunks, `0` or `1`) comes:

| Offset | Size | Field |
| ---: | ---: | --- |
| +0 | 4 | material count (1..64) |
| +4 | 21 × count | per-material header (see below) |
| ... | var | per-material texture name and material name (NUL-terminated) |
| ... | 4 | triangle count (must equal `indices.size()/3`) |
| ... | 2 × triangles | material index per triangle (u16) |
| ... | 16 | reserved, zero in every retail chunk |

Per-material 21-byte header (D3D colour order, bytes B,G,R,A):

| Offset | Size | Field |
| ---: | ---: | --- |
| +0x00 | 4 | diffuse (`fence_paddock` `0xff969696` = 150,150,150) |
| +0x04 | 4 | ambient (`0xffffffff` white) |
| +0x08 | 4 | power (`0x3dcccccd` = 0.1f) |
| +0x0c | 4 | colour (`0xe6e6e6ff` fence, `0xffff0000` laser) |
| +0x10 | 4 | specular (`0x000000ff` black) |
| +0x14 | 1 | flag (0 fence body, 1 laser) |

Example `fence_paddock_lod1.mesh`: 67 vertices, 35 triangles, 2 materials —
`fence_paddock.tga` (`2 - Default`, 27 tris) and `laser2.tga`
(`9 - Default`, 8 tris). `load_mesh_chunks` keeps both the material table and
the per-triangle indices; `split_by_material` uses them to draw the additive
lamp billboards separately. `load_mesh` keeps only the first material's
texture name (the single-material callers do not need the table).

## Rendering notes

- The UVs are stored with a bottom-left origin (v is negative in the retail
  files). To sample a top-left-origin image such as a PIL/TGA load, use
  `v_tex = 1 + v`. Sampling with `-v` flips the atlas vertically and puts the
  leg texture on the head.
- Multiplying the texture by the per-vertex colors and applying simple
  diffuse lighting reproduces the retail look closely enough for validation.
- Reference renders: `notes/allo-3q-vc-flip.png`,
  `notes/allo-side-vc-flip.png`, `notes/allo-head-flip.png`.

## Example (`cow_lod1.mesh`)

- 93 vertices, 198 indices, 66 triangles, 93 UVs, 93 normals.
- Bounds: `(-1.510, -0.635, -0.012)` to `(1.415, 0.635, 2.333)`.
- OBJ export: `python tools/formats/mesh.py --obj <mesh> <out.obj>`.

## Open questions

- What is the `hash`? Test against candidate payload ranges.
- `cow_lod1` tail follows the decoded grammar: `cow_lod1.tga`,
  `15 - Default`, triangle count `0x42` (66), one u16 material index per
  triangle, 16 reserved bytes.
- Are indices ever 16-bit or triangle strips? All retail files use u32 lists.
- Second chunks: **yes** (corrected 2026-10-28). 20 retail meshes have
  2..4 chunks, e.g. `entrance_lod1.mesh` = boat, radar, ponton2, ponton
  (the dock). See Chunks below.

## Chunks

`chunk_count` chunks follow the 0x11 header field, each:

```text
u32 name_len, name (NUL)
body (vertex_count .. normals; a cleared UV/normal flag still has a u32 0)
u8 colour flag, u32 colour count, RGBA x count (if flag)
u32 material count, {21 bytes, string texture, string name} x count
u32 triangle count, u16 material index x triangle count
16 zero bytes
```

Every one of the 536 retail meshes ends exactly after its last chunk
(counts 1: 516, 2: 13, 3: 2, 4: 5). Evidence:
`notes/entrance-dock-road-recon.md`, `tools/formats/mesh.py`
`read_mesh_chunks`, `tests/test_mesh.py::test_all_meshes_chunk_layout`;
C++ `dino::load_mesh_chunks`.

## Next

1. Decode the material tail and map texture names.
2. Parse `.skin` / `.skel` / `.anim` (same 5-byte magic).
3. Load a mesh + TGA in the C++ viewer.
