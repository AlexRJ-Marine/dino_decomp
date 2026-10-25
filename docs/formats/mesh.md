# `.mesh` format

Status: **geometry decoded**, material block open.

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
| 0x0D | 4 | chunk_count | `1` in all retail meshes |
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

## Example (`cow_lod1.mesh`)

- 93 vertices, 198 indices, 66 triangles, 93 UVs, 93 normals.
- Bounds: `(-1.510, -0.635, -0.012)` to `(1.415, 0.635, 2.333)`.
- OBJ export: `python tools/formats/mesh.py --obj <mesh> <out.obj>`.

## Open questions

- What is the `hash`? Test against candidate payload ranges.
- Tail layout: material colors, texture name, material name, triangle count.
  `cow_lod1` tail contains `cow_lod1.tga`, `15 - Default`, and `0x42` (66).
- Are indices ever 16-bit or triangle strips? All retail files use u32 lists.
- Is there a second mesh chunk? `chunk_count` is 1 everywhere.

## Next

1. Decode the material tail and map texture names.
2. Parse `.skin` / `.skel` / `.anim` (same 5-byte magic).
3. Load a mesh + TGA in the C++ viewer.
