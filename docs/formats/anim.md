# `.anim` format

Status: **solved** — header, per-bone key times, and local 4×4 matrices.
All 299 retail clips parse with zero errors.

## Evidence

- 299 `.anim` files; 9,129 bone tracks; 285,507 keyframe matrices.
- 255 clips are 79,040 bytes with 35 bones × 31 keys over duration 4,800.
- Six 25-byte clips are empty (bone count 0).
- Ghidra:
  - animation constructor `0x474320` calls
    `FUN_004743a0(0xdef01234, ...)`; class vtable `0x625ec4`.
  - generic loader `FUN_004714b0`; type-specific reader `0x475990`
    (vtable slot `+0x10`).
  - `0x475990` reads the 8-byte duration/start pair, the bone count, and an
    array of 0x2c-byte bone records via `0x4839e0`.
  - `0x4839e0` reads the name, key count, `u64` key times, matrix count, and
    64-byte matrices via `0x4663a0`.
- Parser: `tools/formats/anim.py`; tests: `tests/test_anim.py`.

## Layout

```text
0x00  5   magic          01 03 00 00 00
0x05  4   hash
0x09  4   sentinel       0xdef01234
0x0d  4   duration       time units (4800 for most clips)
0x11  4   start          usually 0
0x15  4   bone_count
0x19  ... per-bone blocks:
          u32 name_len + NUL-terminated name
          u32 key_count
          key_count × u64 time (strictly increasing)
          u32 matrix_count (== key_count)
          matrix_count × 64 bytes (16 × f32, row-major 4×4)
```

The file ends exactly after the last matrix. There is no trailing table.

### Example (`HeavyBiped_Armor.anim`)

- Hash `0x8a504920`, duration `4800`, start `0`, 35 bones, 1,085 keys.
- `Bip01`: 31 keys at `0, 160, 320, …, 4800`.
- First `Bip01` matrix:

```text
 0.00000  -1.00000   0.00000   0.00000
 1.00000   0.00000   0.00000   0.00000
 0.00000   0.00000   1.00000   0.00000
 0.00000   0.00000 358.18000   1.00000
```

## Matrix semantics

The 64-byte record uses the same convention as `.skel`:

- row-major 4×4;
- upper-left 3×3 rotation (no scale; row norms are 1);
- translation in row 3;
- last column `0 0 0 1`.

All 285,507 matrices satisfy this. They are **local** bone transforms, not
world matrices. Replace the corresponding `.skel` local matrix with the
sampled animation matrix and compose root-to-leaf as usual:

```text
world = local_anim @ parent_world
```

The 35 animated bones are exactly the 35 skin bones (the skeleton's 80 bones
also contain `Scene_Root`, dummies, Nubs and other non-rendered helpers).

At time 0 the animation is close to, but not identical with, the `.skel` bind
pose (for example `Bip01` has a different local translation). Treat `.anim` as
the authoritative pose and `.skel` as the hierarchy/bind fallback.

## Time and interpolation

- Times are `u64` and strictly increasing. Most clips use 160-unit steps over
  0..4,800.
- The exact time unit is not proven. Normalize playback by `time / duration`;
  at 4,800 units the 31-key clips loop exactly (first key == last key).
- Sampling in `anim.py` uses linear translation and quaternion slerp for the
  rotation, then rebuilds the row-vector matrix. Exact key times return the
  stored matrix unchanged.

## Retargeting and skinning

`tools/formats/anim.py` exposes `Animation.sample(time)` returning a dict of
bone name → local 4×4 matrix. To draw a skinned dinosaur:

1. Start from `Skeleton` local matrices.
2. Override the 35 animated bones with the sampled matrices.
3. Compose `world = local @ parent_world` (root `Scene_Root` is `0.01 × I`).
4. Convert to raw centimetres: `W_raw = W_scaled @ diag(100, 100, 100, 1)`.
5. Skinning: `v_cm @ inverse_bind @ W_raw`, then `× 0.01` for metres.

This is implemented in the validation renderer. Frames:
`notes/allo-anim-0000.png` … `notes/allo-anim-4800.png`, and the loop
`notes/allo-anim.gif`. The clip rears the allosaurus up and returns to the
start pose; `t=0` and `t=4800` differ by less than `1e-6` m.

## Open questions

- Exact time unit / intended FPS (160 units per key at 4,800 duration).
- Root motion: some clips may move `Bip01` in world space; the entity/controller
  may consume that separately.
- How the engine blends between clips.
- Clips for non-dinosaur rigs (track counts differ; all parse).
