# `.skel` format

Status: **container + names decoded**, transform/hierarchy semantics open.

## Evidence

- 64 `.skel` files. The 20 dinosaur skeletons are all 7,788 bytes / 80 bones;
  props/buildings have 1–28 bones.
- Samples: `Data/Graphics/Actors/Dinos/allo/allo.skel`,
  `Data/Graphics/buildings/coin/coinkute_lod1.skel`,
  `Data/Graphics/attractions/seats/seats3_lod1.skel`.
- Ghidra: sentinel `0x56789abc` appears at `0x475e7d`; constructor
  `FUN_00475e30` calls `FUN_004743a0(0x56789abc, ...)`.

## Layout

| Offset | Size | Field |
|---:|---:|---|
| 0x00 | 5 | magic `01 03 00 00 00` |
| 0x05 | 4 | hash |
| 0x09 | 4 | sentinel `0x56789abc` |
| 0x0D | 4 | bone_count |
| 0x11 | 4 | global scale? (`0.01` f32) |
| 0x15 | 4 | unknown (0) |
| 0x19 | 76 × bone_count | bone records, 19 × f32 each |
| ... | varies | names: `u32 length` + NUL-terminated string, per bone |
| ... | 4 + 4 × bone_count | `u32 bone_count` + one `u32` flag per bone (all 1) |

For `allo.skel`: `0x19 + 80×76 = 0x17d9`, names end at `0x1d28`,
flags are `[80, 1, 1, ...]`.

## Bone record

19 floats. Strongly resembles a 4×4 transform plus a 3-float translation
(or a 4×4 matrix plus scale), but the exact packing is not proven.

Example `Bip01_Pelvis` (bone 3):

```text
-0.0048  0.0     0.0     1.0
 0.1729  0.9849 -0.0     0.0
-0.9849  0.1729 -0.0     0.0
-0.0     0.0     1.0     0.0
-0.4904 42.7239 -0.0001
```

## Transform experiment

`tools/formats/solve_skel_joints.py` tries 14 candidate layouts and 25 scale
combinations, scoring parent/child joint continuity on the 35 `allo` blocks.

Best result:

| Layout | Scale R/T | Mean joint | Median joint |
|---|---|---:|---:|
| 3×3 at record[0:9], translation record[16:19] | 0.01 / 0.01 | 1.33 | 0.58 |
| 3×3 at record[0:9], translation record[9:12] | 0.01 / 0.01 | 1.56 | 0.54 |

Rendering the winning layout (`render_assembled_skin.py`) still produces a
cross/stick shape, not a dinosaur. So chain continuity alone is degenerate;
the layout is not yet correct, or the bone-local blocks are auxiliary shapes
rather than the render mesh.

## Open questions

- Where is the parent hierarchy? The flag table is all 1, not parent indices.
  Candidates: encoded in the bone record, in `.skin`, in `.anim`, or inferred
  from the 3ds Max Biped naming convention.
- Is the matrix local or global? Spine translations of ~30–40 units suggest
  local bind offsets.
- What does the global `0.01` scale mean? Possibly centimetres → metres.
- Are the `.skin` bone blocks the render mesh, or a higher-resolution bind
  mesh? The union is 1,105 verts / 614 tris vs. 317 / 382 for the LOD1 mesh.
- The full-mesh tail is 6,514 bytes; it may contain the per-vertex/per-triangle
  bone mapping needed for rigid skinning.

## Next

1. Decompile the virtual `read` method for type `0x56789abc` (skeleton) and
   `0x02468ace` (skin). The vtables are at `0x625f0c` and `0x625e8c`.
2. Decode the full-mesh tail (material + 6,340 bytes) as the likely skin map.
3. Re-run the joint fit once the transform layout is known.
