# `TranslatorConstants.ini` and the genotype translator

Status: **solved** for the 41 constants, their in-memory offsets and the five
dinosaur-need formulas; the gene→part-record mapping is still open.

## Inventory

`Data/Design/Genotype/TranslatorConstants.ini` — 1,179 bytes, cp1252 text,
one `[SIMU_CONSTANTS]` section, 41 `Key=Value` lines. Loaded by
`saSimuGenotypeTranslator` (`0x00615e80`, source
`saSimuGenotypeTranslator.cpp`).

| Key | Value | Key | Value |
| --- | ---: | --- | ---: |
| MaxQuadrupedRange | 150 | ConventionalNeedFactor | 0.5 |
| MinHeightForHeavyBiped | 400 | DangerJawFactor | 0.15 |
| MinWeightForHeavyBiped | 1500 | DangerCombinedAttackFactor | 0.075 |
| SizeCategory1MaxLength | 200 | DangerAggressivityFactor | 0.15 |
| SizeCategory2MaxLength | 620 | CuteNeedBeautyFactor | 0.6 |
| SizeCategory3MaxLength | 900 | SpectacularNeedSizeFactor | 60.0 |
| SizeCategory4MaxLength | 3200 | TamingNeedIntelligenceFactor | 0.45 |
| SizeCategory1Radius | 0 | TamingNeedAgressivityFactor | 0.15 |
| SizeCategory2Radius | 2 | MaxDinosaurSize | 1346.0 |
| SizeCategory3Radius | 4 | RareAlleleMarketPrice | 500 |
| SizeCategory4Radius | 7 | UncommonAlleleMarketPrice | 100 |
| FoodConsumptionRate | 0.1 | CommonAlleleMarketPrice | 50 |
| ViolenceEvolutionRate | 0.4 | MotricityPower | 0.85 |
| WalkingSpeedCoef | 0.1 | IntelligencePower | 0.22 |
| RunningSpeedCoef | 0.4 | MaxPerception | 90 |
| MinMotricityToRun | 0.01 | JumpPercentage | 50 |
| PerceptionHeightPercentage | 1 | VisibilityDivider | 10 |
| DangerVisibilityDivider | 40 | IntelligenceMultiplier | 25 |
| HitPointsDivider | 10 | RegenerationDivider | 1000 |
| PhysicalBonusOnStamina | 100 | ConventionalLimbDisparityFactor | 40 |
| ConventionalLimbHomogeneityFactor | 12 | | |

## In-memory layout

The constructor stores the values in the translator object at:

| Offset | Key | Offset | Key |
| ---: | --- | ---: | --- |
| `+0x228` | MaxQuadrupedRange | `+0x280` | VisibilityDivider |
| `+0x22c` | MinHeightForHeavyBiped | `+0x284` | DangerVisibilityDivider |
| `+0x230` | MinWeightForHeavyBiped | `+0x288` | IntelligenceMultiplier |
| `+0x244` | SizeCategory1MaxLength | `+0x28c` | HitPointsDivider |
| `+0x248` | SizeCategory2MaxLength | `+0x290` | RegenerationDivider |
| `+0x24c` | SizeCategory3MaxLength | `+0x294` | PhysicalBonusOnStamina |
| `+0x250` | SizeCategory4MaxLength | `+0x298` | ConventionalLimbDisparityFactor |
| `+0x254` | SizeCategory1Radius | `+0x29c` | ConventionalLimbHomogeneityFactor |
| `+0x258` | SizeCategory2Radius | `+0x2a0` | ConventionalNeedFactor |
| `+0x25c` | SizeCategory3Radius | `+0x2a4` | DangerJawFactor |
| `+0x260` | SizeCategory4Radius | `+0x2a8` | DangerCombinedAttackFactor |
| `+0x264` | FoodConsumptionRate | `+0x2ac` | DangerAggressivityFactor |
| `+0x268` | ViolenceEvolutionRate | `+0x2b0` | CuteNeedBeautyFactor |
| `+0x26c` | WalkingSpeedCoef | `+0x2b8` | TamingNeedIntelligenceFactor |
| `+0x270` | RunningSpeedCoef | `+0x2bc` | TamingNeedAgressivityFactor |
| `+0x274` | MinMotricityToRun | `+0x2c0` | SpectacularNeedSizeFactor |
| `+0x278` | JumpPercentage | `+0x2c4` | MaxDinosaurSize |
| `+0x27c` | PerceptionHeightPercentage | `+0x2c8` | RareAlleleMarketPrice |
| `+0x2cc` | UncommonAlleleMarketPrice | `+0x2d0` | CommonAlleleMarketPrice |
| `+0x2d4` | MotricityPower | `+0x2d8` | IntelligencePower |
| `+0x2dc` | MaxPerception | | |

`+0x2b4` is a 4-byte gap in the object (not loaded from the file). The
integer keys are read with `0x004094d0`; the float keys with `0x00409510`.

## The five dinosaur needs (`0x00618020`)

`FUN_00618020(translator, unused, phenotype, out[5])` fills five floats. With
`P` = the phenotype struct:

```text
beauty       = P[0x40] * CuteNeedBeautyFactor

danger       = P[0x4c] * DangerJawFactor
             + P[0x6c] * DangerAggressivityFactor
             + (P[0x60] - P[0x4c]) * DangerCombinedAttackFactor

spectacular  = pow(P[0x00] * P[0x0c] * P[0x14], 1/3)
             * SpectacularNeedSizeFactor / MaxDinosaurSize

conventional = P[0x194] * ConventionalNeedFactor

taming       = P[0x3c] * TamingNeedIntelligenceFactor
             + (100 - P[0x6c]) * TamingNeedAgressivityFactor
```

The `pow` exponent is the double at `0x006488e8` = **1/3**
(`0x3fd5555560000000`, bytes `00 00 00 60 55 55 d5 3f`); the base is the
product of three phenotype dimensions (`+0x00`, `+0x0c`, `+0x14`), i.e. a
volume, so the cube root is a linear size measure. These five values are
the dinosaur's contribution to visitor needs (beauty, danger, spectacle,
conventional appeal, tamability).

## Phenotype accumulator

`FUN_00619c70` adds a gene/part record into the accumulator and
`FUN_00619d90` normalizes it. The record fields `+0x128..+0x16c` (additive)
and `+0x170..+0x19c` (multiplicative) map to these accumulator fields:

| Record | Accumulator | Record | Accumulator |
| ---: | ---: | ---: | ---: |
| `+0x128` | `+0x18` | `+0x14c` | `+0x54` |
| `+0x130` | `+0x2c`, `+0x30` (×0.01) | `+0x150` | `+0x58` |
| `+0x134` | `+0x38` | `+0x154` | `+0x5c` |
| `+0x138` | `+0x3c` | `+0x158` | `+0x68` |
| `+0x13c` | `+0x40` | `+0x15c` | `+0x6c` |
| `+0x140` | `+0x48` | `+0x160` | `+0x70` |
| `+0x144` | `+0x4c` | `+0x164` | `+0x74` |
| `+0x148` | `+0x50` | `+0x168` | `+0x78` |
| `+0x16c` | `+0x194` (subtracted) | `+0x170` | `+0x18` scale |
| `+0x178` | `+0x2c`/`+0x30` scale | `+0x17c..0x19c` | scales for the rest |

The accumulator fields are the phenotype used by the need formulas:

```text
+0x00, +0x0c, +0x14   dimensions (height/width/length)
+0x3c                intelligence
+0x40                beauty
+0x4c                jaw attack
+0x60                combined attack
+0x6c                aggressivity
+0x194               conventional appeal
```

## `simu_trans.dic` -> translator object (`0x006180e0`)

The translator constructor calls `0x006180e0`, the `simu_trans.dic` loader.
It walks the ten file parts and stores each block in the translator object:

| Part | Tables | Translator offset |
| ---: | --- | --- |
| 1 | 18 gene translation tables | `+0x1b8` |
| 2 | 4 merge tables (Head, ArmLeg, Body, Tail) | `+0x3bc` |
| 3 | Length (5 parts) | `+0x308` |
| 3 | Width | `+0x200` |
| 3 | Height | `+0x31c` |
| 3 | Weight | `+0x214` |
| 3 | Perception | `+0x330` |
| 3 | Intelligence | `+0x344` |
| 3 | Beauty | `+0x358` |
| 3 | Armor | `+0x36c` |
| 3 | Speed | `+0x2e0` |
| 3 | Attack | `+0x2f4` |
| 3 | Agility | `+0x380` |
| 4 | dummy disposition (3 x 6-wide) | `+0x394` |
| 5 | dummy type (3 x 5-wide) | `+0x3a0` |
| 6 | influence tables 0 and 1 | `+0x80`, `+0x90` |
| 6 | five reference widths | `+0xa4..+0xb4` |
| 7 | skin map (8 x 3) | `+0x20` |
| 7 | the single `582` | `+0xa0` |
| 7 | skin influence tables 0 and 1 | `+0x40`, `+0x50` |
| 8 | carapace map (6 x 3) | `+0x30` |
| 8 | carapace influence tables 0 and 1 | `+0x60`, `+0x70` |
| 9 | 20 minimum values | `+0x404` |
| 9 | 20 maximum values | `+0x460` |
| 10 | 20 default families | `+0x14` |

Each Part 3 group is **five pointers**, one per part in file order
`Head, Arm, Leg, Body, Tail` (`0x006189e0` reads five 20 x 20 tables and
stores the objects consecutively). Part 2's four merge tables sit at
`+0x3bc..+0x3c8`.

## Runtime genotype record

`0x00617250` indexes the genotype as **six ints per gene**: `[3g]` and
`[3g+1]` are the two alleles, `[3g+2]` is not read by the translator. The
18 genes keep the `.gen` order:

| Gene | Field | Gene | Field |
| ---: | --- | ---: | --- |
| 0 | head type | 9 | tail dummy type |
| 1 | legs type | 10 | skin type |
| 2 | body type | 11 | skin subtype |
| 3 | tail type | 12 | carapace type |
| 4 | head dummy position | 13 | carapace subtype |
| 5 | head dummy type | 14 | diet |
| 6 | body dummy position | 15 | aggression |
| 7 | body dummy type | 16 | stamina |
| 8 | tail dummy position | 17 | courage |

`0x00617250` reads pairs at `3*0`, `3*1`, `3*2`, `3*3`, `3*10`, `3*11`,
`3*14..3*17`, which matches this table.

## Translation (`0x00617250` -> 440-byte phenotype)

`0x00617250(translator, genotype, phenotype)` runs `0x00619830` first and
then fills the phenotype. The five-part loop maps a part to a gene:

| Part | 0 | 1 | 2 | 3 | 4 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Name | Head | Arm | Leg | Body | Tail |
| Gene triple | 0 (head type) | 1 (legs type) | 1 (legs type) | 2 (body type) | 3 (tail type) |

For every part it looks up `table[part][alleleB][alleleA]` in five
attribute groups and sums the five parts:

| Attribute | Translator base | Phenotype field |
| --- | ---: | ---: |
| Perception | `+0x330` | `+0x38` (+ height percentage term) |
| Intelligence | `+0x344` | `+0x3c` (written through the cube-root temporary) |
| Beauty | `+0x358` | `+0x40` |
| Armor | `+0x36c` | `+0x68` |
| Agility | `+0x380` | `+0x48`, and `+0x34 = JumpPercentage * agility / 100` |

The Attack group (`+0x2f4`) is looked up per part instead of summed:

```text
phenotype +0x4c = Attack[part 0]  pair (head type)
phenotype +0x50 = Attack[part 3]  pair (head type)   # see note
phenotype +0x54 = Attack[part 1]  pair (legs type)
phenotype +0x58 = Attack[part 2]  pair (legs type)   # zeroed for bipeds
phenotype +0x5c = Attack[part 4]  pair (tail type)
phenotype +0x60 = sum of the five
```

The array is in file order (`Head, Arm, Leg, Body, Tail`), so `+0x50` is
Body attack; the decompiler shows it reading the head-type pair, which is
kept as observed until a runtime check settles it.

Other phenotype fields written by `0x00617250`:

| Field | Meaning (evidence) |
| ---: | --- |
| `+0x00` | total length = body part + tail part |
| `+0x04`, `+0x08` | body and tail length; split in half for quadrupeds |
| `+0x10` | size-category radius (`SizeCategory*Radius`) |
| `+0x18` | volume (length x width x height) |
| `+0x1c` | `volume * FoodConsumptionRate * 0.01` |
| `+0x28` | walk speed = `+0x1b * WalkingSpeedCoef * 0.01` |
| `+0x2c` | run speed (0 below `MinMotricityToRun`) |
| `+0x34` | jump = `JumpPercentage * agility / 100` |
| `+0x3c` | cube root of the volume (linear size) |
| `+0x6c` | aggression merge (`+0x1f4` table, gene 15 pair) |
| `+0x70` | stamina merge (`+0x1f8` table, gene 16 pair) |
| `+0x74` | courage merge (`+0x1fc` table, gene 17 pair) |
| `+0x1ac` | family index (`Part 10`, chosen from parent legs type) |
| `+0x1b0` | `merge[Head][dietB][dietA] - 1`, diet flag |
| `+0x1b4` | 0 = biped (family 2/3), 1 = quadruped |
| `+0x134` | per-species food/scale factor (used by the size accumulator) |

`0x00619830` additionally computes the five part lengths, widths and heights
at `+0x98/+0xac/+0xc0` and the five part scale factors at `+0xd4..+0xe4`
(head, arm, body, leg, tail). Each scale blends the two parents with the
matching Part 2 merge table weight (`+0x3bc` head, `+0x3c0` legs,
`+0x3c8` tail) and normalises by the body part size.

### Conventional appeal and part records

`0x00617250` also computes the conventional need input:

```text
P+0x194 = ConventionalLimbDisparityFactor / N
        + ConventionalLimbHomogeneityFactor * (homozygous parts)
```

The five part pairs are compared; every part whose two alleles are equal adds
`ConventionalLimbHomogeneityFactor` (12), so a pure breed scores
`5 * 12 = 60`. `N` is the part-record cache size (1 for a pure breed), so a
pure dinosaur gets `40 / 1 + 60 = 100` and a five-pair hybrid gets
`40 / 5 + 0 = 8`. The need formula then applies `ConventionalNeedFactor`
(0.5). The decompiler cannot show where `N` is stored; the pure-breed case
pins the scale.

The translator also keeps a per-allele-pair **part record cache**
(`0x0061bab0` find / `0x0061a860` insert). Each record is a 0x10-byte entry
in the table at translator `+0x84` with an `int` array at `+4`.
`0x00619a60` walks 23 phenotype part slots (`P+0xe8..`) and adds seven ints,
one optional int (selector `translator+0x114`) and six more, each scaled by
`part width (P+0xac) / reference width (translator+0xa4)`. `0x00619b80`
then applies the percentage records (`+0x170..+0x19c`, neutral 100) over the
same slots. The 23 slots are the five body parts plus the 18 dummy/decoration
parts driven by genes 4-13 and the skin/carapace tables.

## Reader

`tools/formats/translator_constants.py`:

```text
python tools/formats/translator_constants.py --dump <TranslatorConstants.ini>
```

Returns all 41 values and the `STRUCT_OFFSETS` map; validates that every key
is present and none is extra. `tools/formats/simu_trans.py` now also exposes
`attribute_order` and `part_order` (Part 3 file order). Tests:
`tests/test_translator_constants.py`, `tests/test_simu_trans.py`.

## Evidence

- `Data/Design/Genotype/TranslatorConstants.ini`.
- `FUN_00615e80` — translator constructor; loads the constants and the gene
  tables.
- `FUN_00618020` — five-need calculator.
- `FUN_00619c70` / `FUN_00619d90` — phenotype accumulator (add / normalize).
- `tools/formats/translator_constants.py`,
  `tests/test_translator_constants.py`.

## Open

- The third int of each runtime genotype gene triple (`[3g+2]`).
- Whether the Body attack lookup really uses the head-type pair.
- The 4-byte gap at `+0x2b4`.
- Human-readable names for the remaining phenotype fields (`+0x38`, `+0x48`,
  `+0x50`, `+0x54`, `+0x58`, `+0x5c`, `+0x68`, `+0x70`, `+0x74`, `+0x78`).
- Whether the five needs feed the show score, the visitor AI, or both.
