# `.skel` format

Status: **solved** — container, 4×4 bone transforms, hierarchy, names, and flags.
Validated on all 64 retail skeletons (20 dinosaurs × 80 bones + 44 props).

## Evidence

- 64 `.skel` files, 2,113 bones total; all 20 dinosaur skeletons are
  7,788 bytes / 80 bones.
- Ghidra: the skeleton class vtable is `0x625ee8` (constructor
  `FUN_00475e30`, which passes the type id `0x56789abc` to
  `FUN_004743a0`).
- Vtable slot `+0x08` is the generic `mgXFileImport` loader
  `FUN_004714b0`; it calls the type-specific import at slot `+0x10`.
- The actual reader is **`0x477890`**. It reads, in order:
  1. `u32 count`, then `count × 64` bytes via `FUN_004663a0` into the vector
     at `this+0x18` (16 floats per bone).
  2. `u32 count`, then per bone a `u32 child_count` + child indices via
     `FUN_0047cc90` into `this+0x28` (16-byte elements).
  3. `u32 count`, then `count × u32` into `this+0x38` — the parent array.
  4. `u32 count`, then per bone a length-prefixed string into `this+0x48`
     (12-byte `mcString` elements).
  5. `u32 count`, then `count × u32` flags into `this+0x58`.
- The file order matches that reader byte-for-byte. The earlier "19 floats per
  bone" parse was **8 bytes out of alignment**: it started at `0x19` instead of
  `0x11`, so every record straddled two matrices. That produced the fake
  translation at floats 16–18 and the cross-shaped render.
- Writer/export counterpart: `0x477610` (vtable slot `+0x0c`) writes the same
  64-byte matrices and child lists. `0x4762d0` wraps the header.

## Layout

| Offset | Size | Field |
| ---: | ---: | --- |
| 0x00 | 5 | magic `01 03 00 00 00` (shared with `.mesh`/`.skin`/`.anim`) |
| 0x05 | 4 | hash |
| 0x09 | 4 | sentinel `0x56789abc` |
| 0x0D | 4 | `bone_count` |
| 0x11 | 64 × N | bone matrices, 16 × f32, row-major |
| ... | 4 | child-list count (`== N`) |
| ... | varies | N × (`u32 child_count` + `child_count × u32`) |
| ... | 4 | parent count (`== N`) |
| ... | 4 × N | N × `i32` parent (`-1` = root) |
| ... | 4 | name count (`== N`) |
| ... | varies | N × (`u32 length` + NUL-terminated name) |
| ... | 4 | flag count (`== N`) |
| ... | 4 × N | N × `u32` flag (all `1` in retail data) |

No trailing bytes in any of the 64 files.

For `allo.skel` (80 bones, 7,788 bytes):

| Offset | Content |
| ---: | --- |
| `0x11` | matrices, end `0x1411` |
| `0x1411` | child count 80, lists end `0x1691` |
| `0x1691` | parent count 80, parents end `0x17d5` |
| `0x17d5` | name count 80, names start `0x17d9`, end `0x1d28` |
| `0x1d28` | flag count 80, flags end `0x1e6c` = EOF |

## Bone matrix

The 64-byte block is a 4×4 float32 matrix in **row-major** order using the
3ds Max / row-vector convention:

```text
[ R00 R01 R02  0 ]
[ R10 R11 R12  0 ]
[ R20 R21 R22  0 ]
[ Tx  Ty  Tz   1 ]
```

The upper-left 3×3 is rotation × scale; translation is **row 3**
(floats 12–14). The last column is always `0 0 0 1`.

`Bip01` (bone 1):

```text
 0.00000  -1.00000   0.00000   0.00000
 1.00000   0.00000   0.00000   0.00000
 0.00000   0.00000   1.00000   0.00000
-0.02238  28.54713 358.87494   1.00000
```

`Bip01_Pelvis` (bone 3) and `Bip01_Spine` (bone 4):

```text
Pelvis                              Spine
 0.00000   0.00000   1.00000  0     0.17294   0.98493   0.00000  0
 1.00000   0.00000   0.00000  0    -0.98493   0.17294   0.00000  0
 0.00000   1.00000   0.00000  0     0.00000   0.00000   1.00000  0
-0.00484   0.00000   0.00001  1    -0.49036  42.72385  -0.00013  1
```

These are **local** transforms. Compose root-to-leaf with the row-vector rule:

```text
world = local @ parent_world
```

`Scene_Root` (bone 0) is `0.01 × I`, so the dinosaur is authored in centimetres
and becomes metres after composition. There is no separate global-scale header
field; the old parser read that `0.01` out of the first matrix. Allosaurus
world-space joint bounds are:

```text
min  (-0.906, -4.757, -0.032)
max  ( 0.908,  5.433,  5.286)
size ( 1.814, 10.189,  5.317)
```

Projected views are in `notes/skel-allo-views.png`: tail, spine, neck, head,
two legs and arms are all connected. This is the visual check that the old
"cross" failed.

## Hierarchy

The parent array is authoritative:

- `parents[0] == -1` (`Scene_Root`)
- `parents[1] == 0` (`Bip01`)
- `parents[3] == 1` (`Bip01_Pelvis`)
- `parents[4] == 3` (`Bip01_Spine`)

The child lists are consistent with the parent array for **all 20 dinosaur
skeletons**. In 44 prop skeletons the child lists store each child index one
lower than the parent array implies, and some include the bone itself; example
`coinkute_lod1.skel` stores `children[0] = [0]` while `parents[1] = 0`. Read
hierarchy from `parents`, not from the child lists.

## Flags

One `u32` per bone, all `1` in every retail file. Purpose unknown; probably an
"active/visible" flag.

## Reader/implementation

`tools/formats/skel.py` implements this layout and exposes:

- `read_skel(path)` — parses header, matrices, hierarchy, names, flags.
- `Skeleton.world_matrices()` / `world_positions()` / `world_bounds()` —
  composes `local @ parent_world`.
- `SkelBone.matrix`, `.translation`, `.rows`, `.parent`, `.children`, `.flag`.
- `scan(root)` — validates all 64 files.

Tests: `tests/test_skel.py` (hierarchy, world transforms, props, full scan).

## Open questions

- Meaning of the per-bone flags.
- Whether matrix rotations are ever non-orthonormal other than the root scale.
- How `.skin` bone blocks map to these bones (next task).
