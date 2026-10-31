# Phenotype attributes (runtime trace)

Status: **solved; ported to C++; all 20 live rows exact.**
This extends `notes/phenotype-length-runtime.md`. The 20 Cpg04 dinosaurs were
logged at the `saDinosaur` ctor (`0x56FED0`) with the full phenotype field set;
the table below is the durable copy.

## Offsets read at the ctor

| Field | Meaning | Source in `0x00617250` |
| --- | --- | --- |
| `+0x00` | length (cm) | head + body + tail (scaled) |
| `+0x0c` | width | part width sum/scales |
| `+0x14` | height | part height sum/scales |
| `+0x18` / `+0x1c` | weight (kg) / food consumption (float) | Part 3 Weight x scale |
| `+0x28` | violence evolution (`aggression * ViolenceEvolutionRate * 0.01`) | `+0x6c * 0.4 * 0.01` |
| `+0x2c` | walk speed (float) | `body length * WalkingSpeedCoef * motricity` |
| `+0x30` | run speed (float) | `body length * RunningSpeedCoef * motricity`, 0 below the min |
| `+0x34` | jump | `JumpPercentage * agility / 100` |
| `+0x38` | Perception | Part 3 sum + `PerceptionHeightPercentage * height / 100` |
| `+0x3c` | Intelligence (UI label) | `trunc(25 * Part3IntSum / V^0.22)` (see below) |
| `+0x40` | Beauty | Part 3 sum |
| `+0x48` | Agility | Part 3 sum |
| `+0x4c` | Attack: head | Attack table, head gene pair |
| `+0x60` | Attack total | five part attacks (see residuals) |
| `+0x68` | Armor | Part 3 sum |
| `+0x6c` | Aggressiveness | `Agre` gene expression (10..100) + part records |
| `+0x70` | Stamina | `Stam` gene expression + part records |
| `+0x74` | Courage | `Cour` gene expression + part records |
| `+0x190` | size class | part length category (200/620/900) |
| `+0x1b4` | 0 = biped, 1 = quadruped | default family of the expressed legs type |

The five-part sums use `part -> gene [0,1,1,2,3]` and the Part 3 tables
`Perception/Intelligence/Beauty/Armor/Agility/Attack[part][alleleB][alleleA]`.

## Twenty live rows

`base` = five-part sum of the Part 3 table; `live` = the ctor values.
`Ag/St/Co` = the expressed `Agre`/`Stam`/`Cour` gene value (10..100).

| # | gene 0 | base P/I/B/A/Ar/At | live P/I/B/A/Ar/At | Ag/St/Co expr | live Ag/St/Co | walk |
| ---: | --- | --- | --- | --- | --- | ---: |
| 1 | (1,1) | 10/25/30/25/70/240 | 12/18/56/21/78/243 | 70/70/70 | 82/83/77 | 0.328 |
| 2 | (0,0) | 30/85/20/50/25/335 | 34/38/20/50/25/335 | 80/40/70 | 80/40/70 | 0.320 |
| 3 | (0,0) | 30/85/20/50/25/335 | 34/38/20/50/25/335 | 80/40/70 | 80/40/70 | 0.320 |
| 4 | (0,0) | 30/85/20/50/25/335 | 34/38/20/50/25/335 | 80/40/70 | 80/40/70 | 0.320 |
| 5 | (18,18) | 50/100/45/55/30/180 | 51/105/45/55/30/179 | 70/20/40 | 70/20/40 | 0.280 |
| 6 | (18,18) | 50/100/45/55/30/180 | 51/105/45/55/30/179 | 70/20/40 | 70/20/40 | 0.280 |
| 7 | (0,4) | 42/72/23/53/33/328 | 45/40/23/53/33/332 | 90/50/75 | 90/50/75 | 0.360 |
| 8 | (3,18) | 44/87/49/61/28/202 | 45/91/49/61/28/202 | 60/40/30 | 60/40/30 | 0.240 |
| 9 | (18,0) | 40/92/31/51/25/253 | 42/56/31/51/25/250 | 70/40/50 | 70/40/50 | 0.280 |
| 10 | (18,0) | 40/92/32/44/27/243 | 42/57/32/44/27/259 | 70/40/70 | 70/40/70 | 0.280 |
| 11 | (0,4) | 42/72/21/42/27/322 | 46/34/27/40/33/345 | 80/50/75 | 78/48/80 | 0.312 |
| 12 | (3,3) | 40/75/54/49/30/219 | 41/78/54/49/30/225 | 60/40/40 | 60/40/40 | 0.240 |
| 13 | (1,4) | 32/42/35/57/58/324 | 33/35/39/56/58/305 | 70/40/70 | 75/43/70 | 0.300 |
| 14 | (4,4) | 55/60/30/60/45/325 | 57/48/30/60/45/325 | 90/50/80 | 90/50/80 | 0.360 |
| 15 | (4,4) | 55/60/30/60/45/325 | 57/48/30/60/45/325 | 90/50/80 | 90/50/80 | 0.360 |
| 16 | (4,4) | 55/60/30/60/45/325 | 57/48/30/60/45/325 | 90/50/80 | 90/50/80 | 0.360 |
| 17 | (4,4) | 55/60/30/60/45/325 | 57/48/30/60/45/325 | 90/50/80 | 90/50/80 | 0.360 |
| 18 | (4,4) | 55/60/30/60/45/325 | 57/48/30/60/45/325 | 90/50/80 | 90/50/80 | 0.360 |
| 19 | (3,3) | 40/75/55/70/30/225 | 41/77/55/70/30/225 | 60/30/50 | 60/30/50 | 0.240 |
| 20 | (3,3) | 40/75/55/70/30/225 | 41/77/55/70/30/225 | 60/30/50 | 60/30/50 | 0.240 |

## Max rest (`def+0x188`, 20 live rows)

The same Cpg04 run logged `+0x1d8` at `DINORESET` (`0x570fd0`), which is the
copy of `def+0x188`. `0x617dfb` stores it as
`float(final stamina + x)` after the accumulator; `x` is the int computed at
`0x6179d6..0x617a0a`:

```text
x = (WeightArm[g1b][g1a] + WeightLeg[g1b][g1a])
    * PhysicalBonusOnStamina / weight_sum          (integer truncation)
```

`WeightArm`/`WeightLeg` are the raw Part 3 Weight tables (no part scale),
both indexed by the **raw gene-1 allele pair**, and `weight_sum` is the
pre-accumulator Part 3 weight sum (`0x6173c1`). Live values in row order:

```text
94 47 47 47 40 40 57 61 47 47 55 61 56 64 64 64 64 64 56 56
```

The C++ port computes all 20 exactly (`DINO_DEBUG_PHENO=1`, field `rest`);
`tests/test_app_cpp.py::test_cpp_cpg04_phenotype_attributes` asserts them.

## Max health and regen (`def+0x184`/`+0x186`, 20 live rows)

The same `DINORESET` trace logged `+0x1d4` (dword = max health low int16,
regen high int16). `0x617d47..0x617d7c` computes

```text
max_health = int(sqrt(weight) + 0.5 * cbrt(length*width*height))
regen      = max_health * stamina / RegenerationDivider + 1
```

(all values final, after the accumulator/clamp; `RegenerationDivider` = 1000
default). Live `(max_health, regen)` in row order:

```text
(200,17) (261,11) (261,11) (261,11) (67,2) (67,2) (187,10) (66,3)
(169,7) (166,7) (259,13) (67,3) (94,5) (100,6) (100,6) (100,6)
(100,6) (100,6) (67,3) (67,3)
```

The port matches all 20 (`hp=`/`rg=` in `DINO_DEBUG_PHENO`). Health state
`+0x2bc` was 0 (100 %) on all 20 at spawn. The damage/regen runtime pass is
ported: 5 s `regen` when food/rest > 0.01 and state != 4; 10 s starvation
damage; 67/34/0 % bands and the `0x15` knockdown (see
`docs/decomp/dinosaur-actions.md`).

## Genotype-rarity sell value (`def+0x19c`, 20 live rows)

The `DINORESET` trace also logs `+0x1ec` (the phenotype copy of
`def+0x19c`). `0x61766b..0x6176d4` sums, over the 18 genes,
`price[rarity(A)-1] + price[rarity(B)-1]` with
`price = [Rare 500, Uncommon 100, Common 50]` and the Part 1 rarity weights
(1 rare .. 3 common). The C++ reader now parses all 18 rarity blocks; live
values in row order:

```text
3600 3700 3700 3700 2900 2900 3300 2950 3100 3250
3100 2950 2400 3300 3300 3300 3300 3300 2950 2950
```

All 20 match (`val=` in `DINO_DEBUG_PHENO`). This is the **sell value**
(`dino+0x1ec`), independent of the `Market.ini` purchase price; the sell
credit is `FUN_00585c40(ftol(value*SellBackFactor))` (see
`notes/dino-sell-recon.md`).

## What matches

- **Beauty, Agility, Armor**: the base sum is exact for the 17 rows whose
  dummy/skin/carapace genes are `(0,0)`. Row 1 (all dummy genes `(1,1)`)
  gains +26 beauty, -4 agility, +8 armor; rows 11/13 also gain +6 beauty and
  +6 armor from the dummy part records.
- **Attack total**: row 10 is exact (259) and row 11 exact (345) once the
  tail attack reads the tail allele pair; row 13 is +2 (303 vs live 305).
  Row 1 differs by -14 (229 vs live 243), from the dummy accumulator.
- **Aggressiveness/Stamina/Courage**: equal the `Agre`/`Stam`/`Cour` gene
  expressions for the dummy-zero rows, except rows 11/13 where the part
  records add +-2..5.
- **Perception**: base + 2..4, matching the height percentage term.
- **Walk speed is not the `+0x28` column.** The 0.240..0.360 values in the
  table are `+0x28` = `aggression * 0.004` (violence evolution). The real
  walk/run speeds are `+0x2c`/`+0x30 =
  body_length * (0.1|0.4) * motricity`, `motricity = speed_sum / weight^0.85`.
  The C++ port computes them (`DinoAttributes::walk_speed/run_speed`); the
  live `p2c/p30` values were logged in the same CTOR trace and match the
  base formula. The retail unit is not yet confirmed against actor motion.
- **Port status.** `src/formats/dino_data.*` parses Parts 1-10 and
  `TranslatorConstants.ini`, applies the dummy/skin/carapace accumulator and
  computes the full attribute set (`DinoAttributes`). All 20 Cpg04 rows match
  the live ctor table exactly; `DINO_DEBUG_PHENO=1` prints every field and
  `test_cpp_cpg04_phenotype_attributes` locks the values. The accumulator also
  yields `+0x78/+0x7c/+0x80` (obstacle size/square/radius), e.g. row 1/2/5
  radius 5/10/3.
- **Intelligence (`+0x3c`) solved.** It is an encephalization quotient, not
  the Part 3 sum:

  ```text
  P[0x3c] = trunc( IntelligenceMultiplier(25)
                   * Σ_parts Part3Intelligence[part][alleleB][alleleA]
                   / (P[0x00] * P[0x0c] * P[0x14]) ^ IntelligencePower(0.22) )
  ```

  Then the two near-neutral accumulator passes apply
  (`P = trunc(P * m/100) + trunc(acc)`, clamp 0..200). 18/20 Cpg04 rows are
  exact from the first term (e.g. base 85, 742×260×420 -> 38; base 100,
  170×69×150 -> 105); the two +2/+3 rows are the unported dummy/skin/
  carapace accumulator. The divisor explains why the simple sum (25..100)
  and the live values (18..105) look unrelated: body volume spans
  1.76M..81M cm³ and `V^0.22` spans 23.7..54.9.
  `P[0x18]` is the Part 3 **Weight** group × part scale (kg), not volume.

## Accumulator (ported)

The part-record accumulator (`0x619A60` add / `0x619B80` percentages) applies
the 23 non-base part slots (dummy, skin, carapace) to the float/int
accumulators at translator `+0x128`/`+0x170`:

1. **Part 1 allele tables.** Genes 4-13 (`HDDi`, `HDTy`, `BDDi`, `BDTy`,
   `TDDi`, `TDTy`, `SknTyp`, `SknSub`, `CarTyp`, `CarSub`). The dummy and
   type genes use `translation[min(a,b)]`; the two subtype genes use
   `matrix[b][a]`.
2. **23 slots.** Head (genes 4/5) slots 0-5, Body (6/7) slots 6-11 with body
   cols 0-3, arm col 4 and leg col 5, Tail (8/9) slots 12-17, then literal
   copies of slots 5/10/11/16/17 into 18-22. Each slot uses
   `Part5[type][Part4[pos][col] - 1]` as the Part-6 row (`0` = absent).
3. **Scaling.** Additive `acc[k] += row[k] * (part_width[part] /
   reference_width[part])`; selector column 8/9/10/18/11; percentage
   `pct[k] += row[k] - 100`.
4. **Skin/carapace.** The 8x3 / 6x3 map selects a 22/16-row influence pair;
   column 0 is scaled by `body_length / 582`, columns 1-17 are unscaled.
5. **Apply.** `trunc(P * pct/100 + 1e-4) + trunc(acc)` for ints; the two
   speeds use floats (`acc[2] * 0.01`). Part 9 clamps the result.

Two implementation details matter:

- Parts 6-8 encode negative influence values as their uint32 decimal form
  (`4294967293` = `-3`). The C++ parser wraps through `uint32_t` before
  converting to `int`; a plain `strtol` saturates to `INT_MAX`.
- The attack total must be re-summed **after** the accumulator; row 1's parts
  become `30/100/10/1/102` = 243 (the pre-accumulator total was 229).

The ported values match the 20 live rows exactly:

| # | live P/I/B/A/Ar/At | ported Ag/St/Co | obstacle size/radius |
| ---: | --- | --- | --- |
| 1 | 12/18/56/21/78/243 | 82/83/77 | 22/5 |
| 2 | 34/38/20/50/25/335 | 80/40/70 | 42/10 |
| 5 | 51/105/45/55/30/179 | 70/20/40 | 15/3 |
| 11 | 46/34/27/40/33/345 | 78/48/80 | 44/11 |
| 13 | 33/35/39/56/58/305 | 75/43/70 | 18/4 |

`+0x78` starts at `(VisibilityDivider/2 + height) / VisibilityDivider`, then
the accumulator slot 16 (column 0x78) applies; `+0x7c = +0x78^2` and
`+0x80 = +0x78 * VisibilityDivider / DangerVisibilityDivider` (10/40). The
three recorded runtime radii (5/10/3 for rows 1/2/5) match.

Remaining question: the runtime `GetCourage` cache trace (`+0x2a0`) reported
threat 53/68/40 for rows 1/2/5, one below `(Ag+Atk)/6` = 54/69/41. The
formula and the actor copy offset (`actor+0x94 = phenotype+0x44`) are decoded,
but the exact state at the action-13 completion needs a same-run
`+0x44`/`+0x94` probe.

## Store gauges

`DINO_ACHAT_ATTRIBUTES_{ORIGINALITY,STAMINA,COURAGE,INTELLIGENCE,AGILITY,
SPEED,ARMOR,AGGRESSIVENESS,ATTACK,BEAUTY,PERCEPTION}_{GAUGE,INDICS}` read
these phenotype fields with the maxima in `AttributeRange.ini` (100; Attack
120). The gauges are ported: `Overlay` stores `PB_MAX_PROGRESS`/
`PB_CURRENT_PROGRESS`, `StoreRow::attributes` carries the eleven values in
widget order, `ParkHud::update_store_gauges` fills each `PROGRESS` and
`INDICS`, and the park draw loop paints the proportional fill.

Mapping used:

| Gauge | `DinoAttributes` | Notes |
| --- | --- | --- |
| STAMINA | `stamina` | |
| COURAGE | `courage` | |
| INTELLIGENCE | `intelligence` | |
| AGILITY | `agility` | |
| SPEED | `walk_speed` | raw world units/s (Allosaurus 2) |
| ARMOR | `armor` | |
| AGGRESSIVENESS | `aggression` | |
| ATTACK | `attack_head` | jaw attack; total is 179..345, max is 120 |
| BEAUTY | `beauty` | |
| PERCEPTION | `perception` | |
| ORIGINALITY | `originality` | `100 - conventional_appeal` (`+0x194`) |

`+0x194` conventional appeal (`FUN_00617250`):

```text
pairs       = genotype pairs for genes {0,1,1,2,3}  (Head/Arm/Leg/Body/Tail)
homogeneous = 12 * count(pair.a == pair.b)          // ConventionalLimbHomogeneityFactor
distinct    = number of unique pairs
P           = 40 / distinct + homogeneous           // ConventionalLimbDisparityFactor
P           = trunc(P * 100 / pct[6] + 1e-4) - trunc(acc[17])   // clamp 0..100
originality = 100 - P
```

Allosaurus (all part pairs `(0,0)`) gives P = 100, originality 0; the
Allosaurus Leoparus breed has distinct dummy alleles and gives 67.

Allosaurus from `Market.ini` reproduces the live Cpg04 row: STAMINA 40,
COURAGE 70, INTELLIGENCE 38, AGILITY 50, ARMOR 25, AGGRESSIVENESS 80, ATTACK
80/120, BEAUTY 20, PERCEPTION 34, ORIGINALITY 0. Evidence:
`notes/store-gauges.png`, `notes/store-gauges-originality.png`,
`test_cpp_store_attribute_gauges`. The 3D preview actor remains.

## Reproduction

The `CTOR` logger in `scripts/x32dbg/start-campaign.ps1 -GenotypeTrace` now
writes `p0/pc/p14/p18/p1c/p28/p2c/p34/p38/p3c/p40/p48/p4c/p60/p68/p6c/p70/p74`.
The 20 rows above come from one Cpg04 mission-1 run; `notes/x32dbg-campaign.out.log`
is gitignored, so this table is the durable copy.
