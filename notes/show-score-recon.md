# Show scoring path (Ghidra)

Status: **solved for the judge score path; the `.bfi` attribute keywords and
200..1000 steps are stored but have no runtime consumer.** Follow-up to
`docs/formats/bfi.md` and the six judge classes.

## `.bfi` fields are loaded, not scored

The 0x1d8-byte feature record array at `gdt+0x7c` stores:

| Record offset | Type | Content |
| ---: | --- | --- |
| +0xE8 | `vector<vector<string>>` | five attribute-weight lists |
| +0x104 | `vector<float>` | five score steps (200/400/600/800/1000) |

Loader `0x53ed30` calls `FUN_00548fc0` (string lists) and `FUN_00548f20`
(float list) with dest `record+0xe8` / `record+0x104` (`0x53f9c5` /
`0x53f9d5`). Strings are kept as strings; there is no keyword→id table in
the executable (raw scan finds no `combat`/`protection`/`courage`/
`perception`/`intelligence`; `agility` only inside `sizedagilitydinos`).

No function reads `def+0xe8` or `def+0x104` in the show/judge code. The only
0x104 hits in `0x58xxxx` are park object-removal. The lists are editor/demo
metadata in this binary.

## Actual score path

The show statistics object is `stShowStatistics` (`0x57f5e0` ctor,
`0x57f790` dtor, `0x57f940` reset):

- five arrays of 0xDB floats at `+0x28..+0x3c`;
- five per-participant vectors at `+0x1c`.

`FUN_00581770` aggregates the participants:

```text
per participant: 5 floats
weight vectors: *(*(show+0x74)+0x34) + i*4, each object has 5 floats at +0x18
category = max_i dot(participant, weight_i)          // FUN_00576550
category = clamp(category, -130, +130)               // 0x643314..0x64331c
smooth at 0.07 per frame                            // DAT_00675770
```

`FUN_00576550(float* a, int b)` is `Σ a[i] * b[0x18+i]` for `i=0..4`,
clamped to **[-100, +100]**. `FUN_00589ba0` feeds per-actor `actor+0x4fc`
through the same dot product; `FUN_00586690` fills the five floats from
`FUN_00618020` (genotype → phenotype): beauty, danger, spectacle,
conventional appeal, tamability (see
`docs/formats/translator-constants.md`).

There is no 200/400/600/800/1000 tiering anywhere in the judge code; the
only thresholds are the ±100 dot clamp and the ±130 category clamp. All 62
`.bfi` files carry the identical step list, so no per-file tier behaviour can
be observed.

## Six judge classes

Dispatch `FUN_0057c260` keys on feature-def `+0x40` = `judge_kind`:

| Kind | Judge | Ctor | Vtable |
| ---: | --- | ---: | ---: |
| 1 | Race | `0x584040` | `0x64350c` |
| 2 | Obstacle | `0x583fa0` | `0x6434e8` |
| 3 | Fight | `0x583ef0` | `0x6434c4` |
| 4 | Destruction | `0x583e90` | `0x6434a0` |
| 5 | Team | `0x583e40` | `0x64347c` |
| 6 | Capture | `0x583dd0` | `0x643458` |

Base ctor `0x582d70` / vtable `0x6433a4` allocates two 0x40-float arrays at
`+0x50/+0x60`, stores show refs at `+0x40/+0x44/+0x48`. All six subclass
vtables are byte-identical over their 9 slots (`0x583e10` dtor,
`0x583000` reset, `0x57da50` load, `0x57da90` save, `0x57dad0`/`0x57db00`
reset flags, `0x587340` empty, `0x582ff0`, `0x583ed0`); behaviour differences
are data-driven, not virtual overrides. **`0x582d70` is not a score
function** — it is the shared base ctor.

## Open

- Whether the `.bfi` attribute lists/score steps belong to the editor or to a
  cut show variant; the runtime ignores them.
- The five per-show weight vectors at `show+0x74` are show-definition data;
  where they are authored (`.shw`? `Shows.ini`?) is not traced.
- The `+0xDB` array length and the 0.07 smoothing time base.

## Evidence

- `0x53ed30` (`.bfi` loader), `0x548fc0`/`0x548f20` (list readers).
- `0x576550` (5-float dot), `0x581770` (aggregation), `0x589ba0`
  (actor feed), `0x586690` (phenotype → 5 floats), `0x618020` (genotype
  need calculator).
- `0x57f5e0`/`0x57f790`/`0x57f940` (`stShowStatistics`).
- `0x57c260` (judge dispatch), `0x582d70` (base ctor), six ctor/vtable pairs.
- Strings `ATTRIBUTES_*` at `0x6605a0..0x6607da` are UI-only.
