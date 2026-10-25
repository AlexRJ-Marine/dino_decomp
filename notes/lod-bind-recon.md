# LOD2/LOD3 bind pose — pachy retargeting solved (2026-10-25)

Read-only analysis of the 60 dinosaur LOD skins. No writes to `game/` or
`original/`.

## Question

The `.skin` full-mesh tail stores `inverse_bind = inv(W_bind)` for each bone.
For most skins `W_bind` equals the skeleton rest world matrix, so the viewer
can skin the `.mesh` standing rest pose with `inverse_bind`. `pachy` LOD2/LOD3
were known to deform wrongly. Why, and what is the correct bind?

## Method

For every dinosaur × LOD:

1. `W_rest` = `pachy.skel` world matrices with the root 0.01 scale removed
   (raw centimetres).
2. `W_bind = inv(inverse_bind)` from the `.skin` record.
3. Compare translation (`|ΔT|`) and 3×3 Frobenius difference (`|ΔR|`) per
   bone.

Reproduction: `tools/recon/analyze_lod_bind.py`.

## Result

Only **pachy LOD2 and LOD3** differ. Every other dinosaur/LOD matches
`W_rest` within `1e-4` (58/60 skins; pachy LOD1 also matches). The pachy
LOD2 and LOD3 deltas are identical:

| Bone | ΔT (cm) | ΔR (Frobenius) |
| --- | ---: | ---: |
| `Bip01_Neck2` | 7.772 | 1.0938 |
| `Bip01_Neck1` | 7.633 | 0.0123 |
| `Bip01_Head` | 4.316 | 0.0 |
| `Bip01_Ponytail1` | 4.316 | 0.0 |
| `Bip01_Ponytail2` | 4.316 | 0.0 |
| `Bip01_Neck` | 0.006 | 0.4059 |
| all other 29 bones | 0.0 | 0.0 |

So the LOD2/LOD3 skins store a **bent-neck bind pose** (head lowered) while
`pachy.skel` and `pachy_lod1.skin` use the straight rest pose. The
`.skin` full mesh is a crouched pose (mean 27.3 cm from the `.mesh` on LOD2),
and this head pose is part of that bind.

## Impact

The viewer's `--skin-bind mesh` uses:

```
v_skinned_cm = v_mesh_cm @ inverse_bind @ W_raw_anim
```

At skeleton rest (`W_raw_anim = W_rest`), this is **not** the identity for
pachy LOD2/LOD3: the head/neck vertices move by up to **4.316 cm**. On a
`LightQuadruped_Walk` frame the max displacement reaches **10.905 cm**
(LOD2) / 4.316 cm (LOD3), because the `Neck2` rotation acts on the head.

## Fix

For the `.mesh` bind, use the **skeleton rest** inverse instead of the skin's
`inverse_bind`:

```
v_skinned_cm = v_mesh_cm @ inv(W_skel_rest_raw) @ W_raw_anim
```

- At skeleton rest this is exactly `v_mesh` for every bone.
- For the 58 matching skins it is numerically identical to `inverse_bind`.
- For pachy LOD2/LOD3 it removes the fixed head/neck offset.

For `--skin-bind skin` keep the skin's `inverse_bind`, because the skin full
mesh is in the skin bind pose:

```
v_skinned_cm = v_skin_cm @ inverse_bind @ W_raw_anim
```

That mode is exact at bind for every LOD; the pachy LOD2/LOD3 skin bind is
simply the bent-neck pose. The bug was mixing the standing `.mesh` with the
skin's LOD-specific bind. Both modes are now consistent:

| Mode | Bind matrix | Rest pose |
| --- | --- | --- |
| `mesh` | `inv(W_skel_rest_raw)` | `.mesh` standing pose |
| `skin` | `inverse_bind` | `.skin` crouched/bent bind pose |

### C++ hand-off

`src/viewer/main.cpp` `build_skinned_mesh` currently builds:

```cpp
skin_matrices[i] = dino::multiply(skin.bones[i].inverse_bind,
                                  raw[skel_index]);
```

Add a bind-matrix choice. The skeleton rest world can be computed once with
`skeleton.world_matrices()`; convert to raw centimetres with
`dino::raw_centimetres(...)` and invert (the matrices are affine, so a 4×4
affine inverse or a general inverse both work):

```cpp
std::vector<dino::Mat4> bind_matrices(skin.bones.size());
for (std::size_t i = 0; i < skin.bones.size(); ++i) {
    const int skel_index = bone_skel[i];
    if (skel_index < 0) { bind_matrices[i] = dino::identity(); continue; }
    bind_matrices[i] = bind_from_skin
        ? skin.bones[i].inverse_bind
        : dino::inverse(dino::raw_centimetres(rest_world[skel_index]));
}
```

`src/formats/math.h` has no `inverse` helper yet; add one (or invert the
3×3 + translation directly). Apply the same matrix to normals as before.

## Evidence

- `pachy_lod1.skin`: max ΔT 0.000 cm, max ΔR 0.0000.
- `pachy_lod2.skin` / `pachy_lod3.skin`: table above.
- All other 19 dinosaurs × LOD1–3: 0 mismatch > 0.01 cm / 1e-3.
- `tests/test_skin.py::test_pachy_lod2_bind_pose_retarget`,
  `test_only_pachy_lod23_bind_pose_differs`.
- `tools/recon/analyze_lod_bind.py`.

## Open

- Why the pachy LOD2/LOD3 exporter used a bent-neck bind (probably a
  lower-LOD mesh exported from a different pose).
- Whether any prop skin has a similar mismatch (the scan covered dinosaurs
  only; `allo`/props were not).
