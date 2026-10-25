# `anim_trans.dic` / `graphic_trans.dic` / `proportion.dat`

Status: **solved** for the text layouts, all table contents and the
proportion table shape; the meaning of the merge codes and the exact part
column order are partial.

## Inventory

| File | Size | Contents |
| --- | ---: | --- |
| `Data/Design/Genotype/anim_trans.dic` | 5,727 | 20 dinosaurs, 5 animation families, 51 clip names, four 20×20 merge tables |
| `Data/Design/Genotype/graphic_trans.dic` | 89 | five reference widths |
| `Data/Design/Genotype/proportion.dat` | 404 | 20 × 5 part scale factors |

The two `.dic` files are cp1252 text with `//` headers and blank-line
separation. `proportion.dat` is binary.

## `anim_trans.dic`

```text
// Dinosaurs
20
allo anky brachio ... velo          (20 lines)

// Families
5
HighQuadruped
LightQuadruped
HeavyBiped
LightBiped
LowQuadruped

// Animations
51
Neutral Walk Run ... Slip           (51 lines)

// Head merge translation table
<20 rows x 20 ints>

// ArmLeg merge translation table
<20 rows x 20 ints>

// Body merge translation table
<20 rows x 20 ints>

// Tail merge translation table
<20 rows x 20 ints>
```

The 51 clip names are exactly the suffixes of the `.anim` files in every
family folder (`Data/Graphics/Actors/Dinos/animations/<family>/`), 51 files
per family (255 total). `Neutral` is index 0, `Slip` index 50.

### Merge tables

Rows and columns use the `Dinosaurs` order. The diagonal is `0` (self).

| Table | Values | Pattern |
| --- | --- | --- |
| Head | 0, 50 | identity + 50 everywhere else |
| Body | 0, 50 | identity + 50 everywhere else |
| Tail | 0, 50 | identity + 50 everywhere else |
| ArmLeg | 0, 1, 50, 99 | directional 10/10 split |

The ArmLeg table splits the species into two groups:

```text
A (10): allo coelo deino gigano hypsilo pachy peleca trex trood velo
B (10): anky brachio diplo igua kentro parasau proto salta stego trice
```

- `A -> B` = 1, except `trex -> stego` = 99.
- `B -> A` = 99.
- `A -> A` = 50.
- `B -> B` = 50, except `anky -> trice` = 1.

The codes look like a directional merge/retarget verdict (0 self, 1
compatible, 50 same group/default, 99 incompatible), but the executable
consumer has not been decompiled. `TranslatorConstants.ini` supplies the size
rules that likely select the animation family
(`MaxQuadrupedRange=150`, `MinHeightForHeavyBiped=400`,
`MinWeightForHeavyBiped=1500`, `SizeCategory*MaxLength` 200/620/900/3200).

## `graphic_trans.dic`

```text
// Reference width for members, used in dummy size computation
100
100
100
300
150
```

Five widths, one per family in the `Families` order. The comment says they
are reference widths for the "dummy" member size computation.

## `proportion.dat`

```text
u32  count = 20
20 x 5 f32    records in the Dinosaurs order
```

Each record reads as `[head, arm, body, leg, tail]`; column 3 is always
`1.0` (legs are the reference scale). The order is inferred from the data —
the loader has not been decompiled. The other columns track the visually
obvious part sizes:

| Species | Head | Arm | Body | Leg | Tail |
| --- | ---: | ---: | ---: | ---: | ---: |
| allo | 1.1200 | 0.3800 | 1.7750 | 1.0 | 1.5900 |
| anky | 0.7389 | 0.6637 | 0.9912 | 1.0 | 0.9425 |
| brachio | 1.1375 | 1.1770 | 1.4227 | 1.0 | 1.1684 |
| diplo | 1.3366 | 1.1780 | 1.4244 | 1.0 | 2.0927 |
| trex | 1.1050 | 0.2350 | 1.7750 | 1.0 | 1.2300 |
| trice | 0.6573 | 0.6613 | 0.9879 | 1.0 | 0.8710 |
| velo | 1.0213 | 0.6596 | 2.0426 | 1.0 | 1.3191 |

T-rex's 0.235 arm, Diplodocus's 2.093 tail and the near-1.0 forelimbs of the
quadrupeds support the interpretation. The table pairs with the four part
genes in `.gen` (head/armleg/body/tail, species 0..19) when the lab assembles
a hybrid: each part is scaled by its donor species' factor.

## Reader

`tools/formats/anim_trans.py`:

```text
python tools/formats/anim_trans.py --dump <anim_trans.dic>
python tools/formats/anim_trans.py --dump-proportion <proportion.dat>
python tools/formats/anim_trans.py --scan <Genotype-dir>
```

`read_anim_trans` returns the three name lists and all four matrices and
validates square dimensions; `read_graphic_trans` returns the five widths;
`read_proportion` returns the 20 × 5 scale table. Tests:
`tests/test_anim_trans.py` (20/5/51/4, matrix value sets, the group split and
its two exceptions, clip-name coverage in all five family folders, widths,
the 20 × 5 proportion table and its 1.0 leg column).

## Evidence

- `Data/Design/Genotype/anim_trans.dic`, `graphic_trans.dic`,
  `proportion.dat`.
- `Data/Graphics/Actors/Dinos/animations/<family>/*.anim` — 51 per family.
- `Data/Design/Genotype/TranslatorConstants.ini` — size/family thresholds.
- `notes/anim-trans-recon.md`, `tools/formats/anim_trans.py`,
  `tests/test_anim_trans.py`.

## Open

- Meaning of the merge codes 1/50/99 and which part is the donor.
- Which table is used by the hybrid lab and whether the 50 tables allow any
  cross-species head/body/tail merge.
- Whether `graphic_trans.dic` widths map to the five families in order.
- Exact `proportion.dat` column order (`head, arm, body, leg, tail` is
  inferred from the values, not from a decompiled loader).
