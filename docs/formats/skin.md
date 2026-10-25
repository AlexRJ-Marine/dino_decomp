# `.skin` format

Status: **container + first block decoded**, full block grammar open.

## Key finding

`.skin` is **not** a generic per-vertex weight table. It looks like the mesh
split into **one chunk per bone**: each chunk starts with a bone name, then a
mini-mesh (vertices, indices, normals), a material, and a matrix. That makes
rigid per-bone skinning likely, which is much simpler than 4-weight skinning.

## Evidence

- 104 `.skin` files; `allo_lod1.skin` = 60,137 bytes.
- Header: magic `01 03 00 00 00`, hash, sentinel `0x02468ace`,
  block_count `0x24` = 36.
- First block:
  - name length 6, `Bip01`
  - vertex_count = 6
  - 6 positions (3×f32)
  - index_count = 24 (8 triangles)
  - 24 u32 indices
  - normal block (flag + count + 3×f32)
  - material name `Material #2`
  - matrix / tail
  - next block name `Bip01_Footsteps`
- Ghidra: `FUN_0048b870` constructs type `0x02468ace` and calls the generic
  loader `FUN_004714b0`.
- `read_skin` finds **35 bone blocks + 1 full-mesh block** in
  `allo_lod1.skin`. The full block is named `dino_lod1` and contains
  317 vertices / 382 triangles, exactly matching `allo_lod1.mesh`.
- Bone-block union: 1,105 vertices / 614 triangles. This is a higher-resolution
  bind mesh, not the 317-vertex render LOD.
- The full-mesh block tail is 6,514 bytes (material + 6,340 bytes of other
  data; a per-vertex or per-triangle mapping is still open).
- 104 `.skin` files parse: 2,244 bone blocks, 76,923 vertices, 42,976
  triangles; 73 full-mesh blocks match their `.mesh`.
- 35 of the 80 `allo.skel` names appear as length-prefixed blocks in
  `allo_lod1.skin`, at these offsets:
  `0x11 Bip01`, `0x174 Bip01_Footsteps`, `0x329 Bip01_Pelvis`,
  `0x67b Bip01_Spine`, `0xcb4 Bip01_Spine1`, `0x12ee Bip01_Spine2`,
  `0x1928 Bip01_Spine3`, `0x1f62 Bip01_Neck`, `0x22b2 Bip01_Neck1`,
  `0x2603 Bip01_Neck2`, `0x2954 Bip01_Head`, `0x39c0 Bip01_Ponytail1`,
  `0x3d15 Bip01_Ponytail2`, `0x406a Bip01_L_Clavicle`,
  `0x43c0 Bip01_L_UpperArm`, `0x4976 Bip01_L_Forearm`,
  `0x4dfb Bip01_L_Hand`, `0x514d Bip01_L_Finger0`,
  `0x54a2 Bip01_R_Clavicle`, `0x57f8 Bip01_R_UpperArm`,
  `0x5dae Bip01_R_Forearm`, `0x6233 Bip01_R_Hand`,
  `0x6585 Bip01_R_Finger0`, `0x68da Bip01_L_Thigh`,
  `0x6e8d Bip01_L_Calf`, `0x730f Bip01_L_Foot`, `0x7661 Bip01_L_Toe0`,
  `0x79b3 Bip01_R_Thigh`, `0x7f66 Bip01_R_Calf`, `0x83e8 Bip01_R_Foot`,
  `0x873a Bip01_R_Toe0`, `0x8a8c Bip01_Tail`, `0x8ddc Bip01_Tail1`,
  `0x912d Bip01_Tail2`, `0x947e Bip01_Tail3`.
  The header count is 36, so one block is not a named skinned bone (likely
  `Scene_Root` or a material-only block).

## Layout (first block, proven by offsets)

```text
0x11  u32 6                 name length
0x15  "Bip01\0"
0x1b  u32 6                 vertex_count
0x1f  6 × 3×f32             positions
0x67  u32 24                index_count
0x6b  24 × u32              indices
0xcb  ...                   UV/normal flags and material
0x140 u32 12                "Material #2" length
0x144 "Material #2\0"
0x174 u32 16                next name length
0x178 "Bip01_Footsteps\0"
```

The next block begins at `0x178`, so the first block is 0x163 = 355 bytes.

## Open questions

- Is every block one bone, and does the block's vertex set exactly partition
  the `.mesh` vertices?
- Are there per-vertex weights inside a block, or is skinning rigid?
- What is the matrix at the end of each block (bind pose / offset)?
- Does `.skin` carry the parent hierarchy that `.skel` lacks?

## Next

1. Write a block scanner that walks all 36 `allo_lod1` blocks by name length.
2. Compare the union of block vertices/indices with `allo_lod1.mesh`.
3. If it is a partition, implement rigid skinning: apply the named bone's
   matrix to that block's vertices.
4. If it is not, look for weight arrays inside each block.
