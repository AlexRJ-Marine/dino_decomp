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

## Reader

`tools/formats/translator_constants.py`:

```text
python tools/formats/translator_constants.py --dump <TranslatorConstants.ini>
```

Returns all 41 values and the `STRUCT_OFFSETS` map; validates that every key
is present and none is extra. Tests:
`tests/test_translator_constants.py`.

## Evidence

- `Data/Design/Genotype/TranslatorConstants.ini`.
- `FUN_00615e80` — translator constructor; loads the constants and the gene
  tables.
- `FUN_00618020` — five-need calculator.
- `FUN_00619c70` / `FUN_00619d90` — phenotype accumulator (add / normalize).
- `tools/formats/translator_constants.py`,
  `tests/test_translator_constants.py`.

## Open

- How the 18 gene translation values map onto the part records
  (`+0x128..+0x19c`) that feed the accumulator.
- The 4-byte gap at `+0x2b4`.
- Human-readable names for the remaining phenotype fields (`+0x38`, `+0x48`,
  `+0x50`, `+0x54`, `+0x58`, `+0x5c`, `+0x68`, `+0x70`, `+0x74`, `+0x78`).
- Whether the five needs feed the show score, the visitor AI, or both.
