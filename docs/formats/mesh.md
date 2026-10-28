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

The material tail grammar is still open.

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
- Tail layout: material colors, texture name, material name, triangle count.
  `cow_lod1` tail contains `cow_lod1.tga`, `15 - Default`, and `0x42` (66).
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
