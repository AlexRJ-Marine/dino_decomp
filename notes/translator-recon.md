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

## 2026-10-26 - `simu_trans.dic` -> translator object and translation

`FUN_006180e0` is the `.dic` loader called from the translator constructor
`FUN_00615e80`. It consumes the ten parts in file order and stores them at:

```text
Part 1  0x12 gene tables                  +0x1b8
Part 2  4 merge tables                    +0x3bc
Part 3  Length       (5 parts)            +0x308
        Width                             +0x200
        Height                            +0x31c
        Weight                            +0x214
        Perception                        +0x330
        Intelligence                      +0x344
        Beauty                            +0x358
        Armor                             +0x36c
        Speed                             +0x2e0
        Attack                            +0x2f4
        Agility                           +0x380
Part 4  3 x 6-wide dummy disposition      +0x394
Part 5  3 x 5-wide dummy type             +0x3a0
Part 6  influence table 0 / 1             +0x80 / +0x90
        5 reference widths                +0xa4..+0xb4
Part 7  skin map (8x3)                    +0x20
        582                               +0xa0
        two 22x18 tables                  +0x40 / +0x50
Part 8  carapace map (6x3)                +0x30
        two 16x18 tables                  +0x60 / +0x70
Part 9  min 20 / max 20                   +0x404 / +0x460
Part 10 20 default families               +0x14
```

`FUN_006189e0(count=5)` reads one attribute's five part tables and stores
five object pointers. `FUN_00618c20` reads the 3 dummy subtables;
`FUN_00619070` reads a single influence table; `FUN_00618e60` reads the
skin/carapace maps; `FUN_00619310` reads the min/max arrays.

The runtime genotype is **18 genes x 3 ints**: `[3g]` = allele A,
`[3g+1]` = allele B, `[3g+2]` unused by the translator. Gene order is the
`.gen` order (0 head, 1 legs, 2 body, 3 tail, 4..9 dummies, 10/11 skin,
12/13 carapace, 14 diet, 15 aggression, 16 stamina, 17 courage). This is why
`FUN_00617250` reads pairs at `3*0, 3*1, 3*2, 3*3, 3*10, 3*11, 3*14..3*17`.

`FUN_00617250(translator, genotype, phenotype)` maps part -> gene:
`Head->0, Arm->1, Leg->1, Body->2, Tail->3`, and for each part looks up
`table[part][alleleB][alleleA]`. It sums five attribute groups over the
parts: Perception (`+0x330` -> P+0x38), Intelligence (`+0x344` -> P+0x3c),
Beauty (`+0x358` -> P+0x40), Armor (`+0x36c` -> P+0x68), Agility
(`+0x380` -> P+0x48, jump at P+0x34).

Attack is per part (`+0x2f4`): P+0x4c head, P+0x50 body, P+0x54 arm,
P+0x58 leg (zero for bipeds), P+0x5c tail, P+0x60 sum.

`FUN_00619830` runs first and writes the five part lengths at P+0x98, the
five widths at P+0xac and the five heights at P+0xc0, then the five part
scale factors at P+0xd4..P+0xe4. Each scale blends the two parents with the
matching Part 2 merge table (`+0x3bc` head, `+0x3c0` legs, `+0x3c8` tail,
indexed `[alleleB][alleleA]`) and normalises by the body part length.

Family: `FUN_00617250` picks `Part 10[allele]` from the legs-type pair and
uses `merge[legs] < 50` to choose the parent; `+0x1b4` is 0 for families
2/3 (biped) and 1 otherwise.

## Open

- The third int of each genotype triple (`[3g+2]`).
- The Body attack lookup shows the head-type pair in the decompiler; verify.

### Conventional appeal and the part-record cache

`FUN_00617250` lines 182-204 compare the two alleles of each of the five
parts and add `ConventionalLimbHomogeneityFactor` (+0x29c = 12) for every
homozygous part (max 60). Line 260 then sets the phenotype field at +0x194:

```text
P[0x194] = ConventionalLimbDisparityFactor(+0x298 = 40) / N + homozygous * 12
```

A pure breed has N = 1 and 5 homozygous parts -> 100; the five-need formula
multiplies by ConventionalNeedFactor 0.5 -> 50. This closes the meaning of
`P[0x194]` in the "conventional" formula.

The same function builds a per-allele-pair part-record cache
(`FUN_00578310` init, `FUN_0061bab0` lower_bound, `FUN_0061a860` insert).
Records live in the table at translator +0x84 (0x10-byte entries, int array
at +4). `FUN_00619a60(translator, phenotype)` walks 23 slots at P+0xe8:

- for each non-null slot, `f = P[0xac + gene*4] / translator[0xa4 + gene*4]`
  (part width / reference width);
- adds 7 ints from the record into translator +0x128 (additive), one extra
  int selected by `translator+0x114` when it is not 0x12, and 6 more into
  translator +0x158;
- `FUN_00619b80` runs the percentage pass: +0x170..+0x19c += record - 100.

`N` (the cache size) is not visible in the decompiled expression; the
pure-breed identity (40 + 60 = 100) fixes the scale.
- Part 3's 20-value diagonal line duplicates `matrix[i][i]` for all 55
  tables; `FUN_006189e0` skips those 20 file lines and builds only the 20x20
  matrix (indexed `[alleleB][alleleA]`).
