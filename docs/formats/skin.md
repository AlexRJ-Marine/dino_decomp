# `.skin` format

Status: **solved** - container, per-bone proxy blocks, the full-mesh rigid
skin table (vertex partition, weights, inverse-bind matrices), and the
LOD2/LOD3 bind-pose retargeting rule.

## Key finding

`.skin` has two parts:

1. **35 per-bone blocks** (one per skinned bone), each a small proxy mesh
   (vertices + indices + normals + material). These are not the render mesh.
2. **One full-mesh block** (`dino_lod1`) whose body matches the `.mesh`
   topology and whose tail carries the **rigid skin table**: every full-mesh
   vertex is assigned to exactly one bone, with weight `1.0` and a 4×4
   inverse-bind matrix.

All 60 dinosaur skins (20 dinosaurs × 3 LODs) use this scheme, always with
35 skin records.

## Evidence

- 104 `.skin` files; `allo_lod1.skin` = 60,137 bytes.
- Header: magic `01 03 00 00 00`, hash, sentinel `0x02468ace`, block count
  `0x24` = 36 at `0x0D`.
- Ghidra:
  - `FUN_00477b50` is the `mgVMeshXMdl` header reader; class vtable `0x626aac`.
  - `FUN_00477f30` (vtable slot `+0x10`) reads the block count and an array
    of 0xB8-byte block records.
  - `FUN_00472240` is the per-block reader: name, vertices, indices, UVs,
    normals, colors, materials.
  - `FUN_0047d460` reads the `u16` per-triangle array.
  - `FUN_0047d880` reads the skin bone records: name, vertex-index vector,
    weight vector, 64-byte matrix.
  - `FUN_004663a0` reads the 64-byte matrix (16 floats).
- `FUN_0048b870` is the loader wrapper (`0x02468ace` + generic
  `FUN_004714b0`).

## Block layout

### Per-bone block

```text
u32 name_len + NUL-terminated name
u32 vertex_count
vertex_count × 3×f32 positions
u32 index_count
index_count × u32 indices
u8  uv_flag
u32 uv_count
uv_count × 2×f32 UVs
u8  normal_flag
u32 normal_count
normal_count × 3×f32 normals
u8  color_flag
u32 color_count
color_count × 4×u8 colors
u32 material_count
material_count × material record
```

The block tail has no bind matrix. Bind data lives only in the full-mesh tail.

`allo_lod1`: 35 blocks, 1,105 vertices, 614 triangles; first `Bip01`, last
`Bip01_Tail3`.

### Material record

Each material on disk is:

```text
21 bytes fixed  (two RGBA colors, a float 0.1, two more RGBA colors, flag)
u32 texture_name_len + NUL-terminated texture name
u32 material_name_len + NUL-terminated material name
```

`allo_lod1` material: texture `allo.tga`, name `Material #1`.

### Full-mesh block

The body is the same layout as `.mesh` (vertices, indices, UVs, normals).
The tail starts after the normals:

```text
u8  color_flag
u32 color_count
color_count × 4×u8 colors
u32 material_count
material_count × material record
u32 triangle_count
triangle_count × u16 per-triangle material index
u32 unknown0
u32 unknown1
u32 bone_count
bone_count × skin bone record
```

For `allo_lod1` the tail is 6,514 bytes:

| Offset | Content |
| ---: | --- |
| `+0x000` | color flag 0, count 0 |
| `+0x005` | material count 1 |
| `+0x009..0x03B` | `allo.tga`, `Material #1` |
| `+0x03B` | triangle count 382 |
| `+0x03F..0x33B` | 382 × `u16`, all `0` |
| `+0x33B` | unknown `1, 0xFFFFFFFF, 35` |
| `+0x347` | bone count 35 |
| `+0x34B..0x1972` | 35 skin bone records |

The full-mesh vertices are in **centimetres**; the matching `.mesh` is in
metres. It is a separate bind mesh with the same vertex/triangle counts, not
the same vertex positions.

### Skin bone record

```text
u32 name_len + NUL-terminated bone name
u32 vertex_count
vertex_count × u32 full-mesh vertex indices
u32 weight_count
weight_count × f32 weights
16 × f32 inverse-bind matrix (row-major)
```

For every one of the 60 dinosaur skins:

- `vertex_count == weight_count` and all weights are `1.0`.
- The vertex-index lists are a **partition** of the full mesh: every vertex
  appears exactly once, no duplicates.
- `allo_lod1`: 35 records, 317 vertices assigned, 317 unique.

`allo_lod1` examples:

| Bone | Vertices | Notes |
| --- | ---: | --- |
| `Bip01` | 0 | skeleton root, no render vertices |
| `Bip01_Footsteps` | 0 | IK helper |
| `Bip01_Spine` | 22 | |
| `Bip01_Head` | 11 | |
| `Bip01_Ponytail1` | 56 | |
| `Bip01_Tail3` | 1 | |

## Skinning math

Let:

- `v_cm` be a full-mesh bind vertex (centimetres).
- `R_bone` be the record's inverse-bind matrix.
- `W_raw_anim(bone)` be the bone's animated world matrix in raw centimetres
  (skeleton local matrices composed root-to-leaf, root scale removed).

Then rigid skinning is:

```text
v_skinned_cm = v_cm @ R_bone @ W_raw_anim(bone)
v_world      = 0.01 * v_skinned_cm
```

At bind, `R_bone @ W_raw_bind(bone) = I`, so the bind mesh is reproduced.
`R_bone` is the inverse of the **raw** world matrix, not the metre-scaled one.
Verified on `allo_lod1`: max bind error `5.7e-5` cm.

The `.skel` world matrices are in metres (`root_scale=0.01`). To get raw
centimetres, scale the translation and basis by 100:

```text
W_raw = W_scaled @ diag(100, 100, 100, 1)
```

### LOD bind pose and retargeting (solved)

`inverse_bind` is `inv(W_bind)`, the inverse of the bone's raw world matrix at
the **skin's** bind pose. For 59 of the 60 dinosaur LOD skins, `W_bind` is
exactly the skeleton rest world matrix (`pachy.skel` etc.), so using
`inverse_bind` on the `.mesh` rest pose is correct.

The exception is **pachy LOD2/LOD3**: their bind pose has a bent neck/head
(`Bip01_Neck2` ΔT 7.772 cm / ΔR 1.094; `Bip01_Neck1` 7.633 cm;
`Bip01_Head`/`Ponytail1`/`Ponytail2` 4.316 cm). LOD2 and LOD3 store the same
pose. Applying `inverse_bind` to the `.mesh` therefore adds a fixed head
offset (4.316 cm at skeleton rest, 10.905 cm on a walk frame).

Correct bind for the `.mesh` rest pose:

```text
v_skinned_cm = v_mesh_cm @ inv(W_skel_rest_raw) @ W_raw_anim
```

This is identical to `inverse_bind` for the 59 matching skins and removes the
pachy offset. For `--skin-bind skin`, keep the skin's `inverse_bind`:

```text
v_skinned_cm = v_skin_cm @ inverse_bind @ W_raw_anim
```

The skin mode is exact at bind for every LOD; the pachy LOD2/LOD3 bind is
simply the bent-neck pose. The bug was mixing the standing `.mesh` with the
skin's LOD-specific bind.

Evidence and the C++ hand-off are in `notes/lod-bind-recon.md`; regression
tests are `tests/test_skin.py::test_pachy_lod2_bind_pose_retarget` and
`test_only_pachy_lod23_bind_pose_differs`.

### The `.skin` full mesh is not the visual rest pose

The full-mesh block inside a `.skin` is stored in a **crouched bind pose**,
different from the `.mesh` of the same model. On `allo_lod1` both have 317
vertices and identical indices, but same-index vertex distances average
**47.9 cm** (max 152.7 cm); e.g. vertex 0 is `(-37.1, -333.1, 485.6)` cm in
the `.mesh` (scaled) and `(-35.4, -317.7, 459.5)` cm in the `.skin`.

Because `R_bone = inv(W_bind)`, the skin bind mesh is reproduced at `W_bind`.
For the `.mesh` rest pose, use `inv(W_skel_rest_raw)` as the bind (see the
LOD section above); this equals `R_bone` for 58/60 dinosaur skins and keeps
the standing pose for pachy LOD2/LOD3. The viewer exposes the two choices as
`--skin-bind mesh` (default) and `--skin-bind skin`.

Evidence: `notes/isl01-dino-meshbind.png` (standing, `.mesh` bind) vs the
earlier crouched `.skin` bind; `notes/isl01-dino-walk.png` shows
`HeavyBiped_Walk` deforming the standing mesh cleanly.

## Reader/API

`tools/formats/skin.py` exposes:

- `Skin.blocks` — the 35 proxy blocks (name, positions, indices, tail).
- `Skin.full_mesh` — the full-mesh block (cm vertices, UVs, normals).
- `Skin.skin_data` — `SkinData` with materials, triangle materials, unknown
  values, and `SkinBone` records.
- `SkinBone.vertex_indices`, `.weights`, `.inverse_bind`.

Renders: `notes/allo-skin-bind.png` (bind pose from the skin table) and
`notes/allo-skin-pose.png` (head bone rotated 35°, rigid skinning applied).

## Open questions

- Meaning of the three `u32` values after the triangle-material array
  (`1, -1, 35`).
- Full naming of the 21-byte material header.
- Purpose of the 35 proxy bone blocks (collision, shadow, or editor display).
- Whether any prop skin (not dinosaur) stores a bind pose different from its
  skeleton rest.
