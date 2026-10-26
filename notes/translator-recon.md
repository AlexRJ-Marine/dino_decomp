# Genotype translator / TranslatorConstants recon (2026-10-25)

Read-only decompilation of `saSimuGenotypeTranslator` (`0x00615e80`, source
`saSimuGenotypeTranslator.cpp`). No writes to `game/` or `original/`.

## Constants

`TranslatorConstants.ini` has 41 keys in `[SIMU_CONSTANTS]`. The constructor
loads them into the translator object at `+0x228..+0x2dc`:

```text
+0x228 MaxQuadrupedRange        +0x264 FoodConsumptionRate
+0x22c MinHeightForHeavyBiped   +0x268 ViolenceEvolutionRate
+0x230 MinWeightForHeavyBiped   +0x26c WalkingSpeedCoef
+0x244 SizeCategory1MaxLength   +0x270 RunningSpeedCoef
+0x248 SizeCategory2MaxLength   +0x274 MinMotricityToRun
+0x24c SizeCategory3MaxLength   +0x278 JumpPercentage
+0x250 SizeCategory4MaxLength   +0x27c PerceptionHeightPercentage
+0x254 SizeCategory1Radius      +0x280 VisibilityDivider
+0x258 SizeCategory2Radius      +0x284 DangerVisibilityDivider
+0x25c SizeCategory3Radius      +0x288 IntelligenceMultiplier
+0x260 SizeCategory4Radius      +0x28c HitPointsDivider
                                +0x290 RegenerationDivider
                                +0x294 PhysicalBonusOnStamina
                                +0x298 ConventionalLimbDisparityFactor
                                +0x29c ConventionalLimbHomogeneityFactor
                                +0x2a0 ConventionalNeedFactor
                                +0x2a4 DangerJawFactor
                                +0x2a8 DangerCombinedAttackFactor
                                +0x2ac DangerAggressivityFactor
                                +0x2b0 CuteNeedBeautyFactor
                                +0x2b8 TamingNeedIntelligenceFactor
                                +0x2bc TamingNeedAgressivityFactor
                                +0x2c0 SpectacularNeedSizeFactor
                                +0x2c4 MaxDinosaurSize
                                +0x2c8 RareAlleleMarketPrice
                                +0x2cc UncommonAlleleMarketPrice
                                +0x2d0 CommonAlleleMarketPrice
                                +0x2d4 MotricityPower
                                +0x2d8 IntelligencePower
                                +0x2dc MaxPerception
```

`+0x2b4` is a gap. Ints use `0x004094d0`, floats `0x00409510`.

## Five need formulas (`0x00618020`)

The function signature is `(translator, unused, phenotype, float out[5])`.
Assembly at `0x618020`–`0x6180d1`:

```text
out[0] = (float)[P+0x40] * [T+0x2b0]                       ; beauty
out[1] = (float)[P+0x4c] * [T+0x2a4]
       + ((float)[P+0x60] - (float)[P+0x4c]) * [T+0x2a8]
       + (float)[P+0x6c] * [T+0x2ac]                       ; danger
out[2] = pow((float)[P+0x00] * [P+0x0c] * [P+0x14], 1/3)
       * [T+0x2c0] / [T+0x2c4]                             ; spectacular
out[3] = (float)[P+0x194] * [T+0x2a0]                      ; conventional
out[4] = (float)[P+0x3c] * [T+0x2b8]
       + (100 - [P+0x6c]) * [T+0x2bc]                      ; taming
```

`FUN_0059aef0` is `pow`; the exponent is the double at `0x006488e8`, bytes
`00 00 00 60 55 55 d5 3f` = **1/3** (`0.33333334`). The base is a product of
three dimensions, so the cube root is a linear size.

## Phenotype accumulator

`FUN_00619c70` (additive) and `FUN_00619d90` (normalizing) take a gene/part
record in `EDI` and the accumulator in `ESI`:

```text
[ESI+0x18] += ftol([EDI+0x128])
[ESI+0x2c] += [EDI+0x130] * 0.01      [ESI+0x30] += same
[ESI+0x38] += ftol([EDI+0x134])
[ESI+0x3c] += ftol([EDI+0x138])
[ESI+0x40] += ftol([EDI+0x13c])
[ESI+0x48] += ftol([EDI+0x140])
[ESI+0x4c] += ftol([EDI+0x144])
[ESI+0x50] += ftol([EDI+0x148])
[ESI+0x54] += ftol([EDI+0x14c])
[ESI+0x58] += ftol([EDI+0x150])
[ESI+0x5c] += ftol([EDI+0x154])
[ESI+0x68] += ftol([EDI+0x158])
[ESI+0x6c] += ftol([EDI+0x15c])
[ESI+0x70] += ftol([EDI+0x160])
[ESI+0x74] += ftol([EDI+0x164])
[ESI+0x78] += ftol([EDI+0x168])
[ESI+0x194] -= ftol([EDI+0x16c])
```

`FUN_00619d90` then multiplies/normalizes each field by the record's
`+0x170..+0x19c` scales, using `0.01` at `0x626a7c` and `0.0001` at
`0x626a78` (the latter is an integer-rounding nudge before `__ftol`).

The accumulator fields consumed by the need formulas are:

```text
+0x00, +0x0c, +0x14   dimensions
+0x3c                intelligence
+0x40                beauty
+0x4c                jaw attack
+0x60                combined attack
+0x6c                aggressivity
+0x194               conventional appeal
```

## Translator object

`spSimuManager+0x0c` holds the translator (`FUN_00586200`:
`FUN_00615e80(param_2+0x18)`). The constructor also loads `simu_trans.dic`
via `FUN_006180e0`, reads `TranslatorConstants.ini`, and builds a large
per-entry table (`0x81c` bytes per entry) that is probably the gene/part
translation records. The exact record layout is still open.

## Evidence

- `TranslatorConstants.ini` (read-only).
- `0x00615e80` constructor, `0x00618020` need formulas,
  `0x00619c70`/`0x00619d90` accumulator.
- `tools/formats/translator_constants.py`,
  `tests/test_translator_constants.py`,
  `docs/formats/translator-constants.md`.

## Open

- Gene translation values → part records `+0x128..+0x19c`.
- The `+0x2b4` gap.
- Names of the remaining accumulator fields.
