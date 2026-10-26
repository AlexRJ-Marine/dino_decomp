# Show scoring (judges)

Status: **solved** for the runtime judge score; the `.bfi` attribute keywords
and 200..1000 score steps are loaded but not consumed by this binary. Raw
evidence: `notes/show-score-recon.md`.

## Pipeline

```text
genotype -> FUN_00618020 -> 5 phenotype floats
         (beauty, danger, spectacle, conventional appeal, tamability)

per participant: dino 5 floats -> FUN_00576550(dot) -> clamp [-100, +100]
FUN_00581770: for each active visitor personality (show+0x74 -> +0x34,
              profile+0x18..0x2c): category = max over participants
              + the +0x4f8/+0x4f9 bonus dots
              category = clamp(category, -130, +130)
              smoothed at 0.07 per frame (DAT_00675770)
```

The five "weight vectors" are the **visitor profiles' five preference
weights**, not show data. `show+0x74` is the 0x40-byte profile object at
`simuManager+0x1c` / `spPark+0x78`; its `+0x34` pointer vector is filled by
`FUN_00615bb0` from `FUN_005869b0`, called either by the difficulty/profile
selection `FUN_00561bb0` or by the world/save loader `FUN_0053c560`.
`FUN_00565e60` maps the personality names (`surfer`, `hooligan`, `parent`,
`blase`, `retraite`, `riche`, `scientific`, `amoureux`, `enfant`) to indices
0–9. Every show in a park uses the same profile list, so the score is "how
much each active personality likes this dinosaur".

`FUN_00576550(a, b)` is `Σ a[i] * b[0x18+i]` for `i = 0..4`.
`FUN_00589ba0` feeds `actor+0x4fc`; `FUN_00586690` fills the five floats from
the genotype translator.

There is **no tier lookup** for 200/400/600/800/1000 in the judge code. The
only thresholds are ±100 (dot clamp) and ±130 (category clamp).

## `.bfi` fields

The 0x1d8-byte record stores:

| Offset | Type | Content |
| ---: | --- | --- |
| +0xE8 | `vector<vector<string>>` | five attribute-weight lists |
| +0x104 | `vector<float>` | five score steps (200/400/600/800/1000) |

Loaded by `0x53ed30` (`0x548fc0`, `0x548f20`). No reader exists in the
show/judge path; the keyword vocabulary is not present in the executable
(only the UI display names at `0x6605a0`). Treat both fields as editor
metadata until a consumer is found.

## Judge classes

`FUN_0057c260` dispatches on feature-def `+0x40` (`judge_kind`):

| Kind | Judge | Ctor | Vtable |
| ---: | --- | ---: | ---: |
| 1 | Race | `0x584040` | `0x64350c` |
| 2 | Obstacle | `0x583fa0` | `0x6434e8` |
| 3 | Fight | `0x583ef0` | `0x6434c4` |
| 4 | Destruction | `0x583e90` | `0x6434a0` |
| 5 | Team | `0x583e40` | `0x64347c` |
| 6 | Capture | `0x583dd0` | `0x643458` |

`0x582d70` is the shared **base constructor** (vtable `0x6433a4`), not the
score function. The six subclass vtables have byte-identical slots; the
behaviour difference is data-driven (`judge_kind`, arena mesh
`effects[5]`, score weights), not virtual overrides.

`stShowStatistics` (`0x57f5e0` ctor / `0x57f790` dtor / `0x57f940` reset)
holds five arrays of 0xDB floats at `+0x28..+0x3c` and five per-participant
vectors at `+0x1c`.

## Open

- The final runtime struct offset that holds the `.bvi` five preference
  weights (the chain profile entry -> `+0x4` -> `+0xc` is proven, the last
  copy into `+0x18..0x2c` is inferred).
- The exact per-park profile count (5 observed; the ctor reserves 6).
- The `+0xDB` array length and the 0.07 smoothing time base.
- Whether the `.bfi` attribute lists belong to the editor or a cut variant.

## Evidence

- `notes/show-score-recon.md`
- `0x576550`, `0x581770`, `0x589ba0`, `0x586690`, `0x618020`
- `0x57f5e0`, `0x57f790`, `0x57f940`, `0x57c260`, `0x582d70`
- `docs/formats/bfi.md`, `docs/formats/translator-constants.md`
