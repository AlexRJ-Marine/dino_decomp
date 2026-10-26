# Phenotype attributes (runtime trace)

Status: **solved; ported to C++ (accumulator residual remains).**
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
- **Port status.** `src/formats/dino_data.*` now parses all Part 3 groups,
  the Part 1 Agre/Stam/Cour matrices, Part 10 and `TranslatorConstants.ini`,
  and computes the full attribute set (`DinoAttributes`). The Cpg04 test
  reproduces 16/20 live rows exactly; the residuals are the unported
  dummy/skin/carapace accumulator. `DINO_DEBUG_PHENO=1` prints every field.
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

## Residual source

The part-record accumulator (`0x619A60` add / `0x619B80` percentages) applies
the 23 non-base part slots (dummy, skin, carapace) to the accumulator at
translator `+0x128`. Even `(0,0)` dummy/skin/carapace types have Part 7/8
influence rows, which explains the small deltas on rows 11/13 and the attack
residual. Porting it needs:

1. Part 1 gene translations for genes 4-13 (48/8/3/6-allele matrices).
2. Part 6 reference widths (`+0xa4..+0xb4`) and the two influence tables
   (`+0x80`/`+0x90`).
3. Part 7/8 skin/carapace maps and influence tables (`+0x20`/`+0x40`/`+0x50`,
   `+0x30`/`+0x60`/`+0x70`).
4. The 23-slot add order and the percentage pass.

## Store gauges

`DINO_ACHAT_ATTRIBUTES_{PERCEPTION,BEAUTY,ATTACK,AGGRESSIVENESS,ARMOR,
WALKSPEED,AGILITY,INTELLIGENCE,COURAGE,STAMINA,STRANGENESS}_{GAUGE,INDICS}`
read these phenotype fields with the maxima in `AttributeRange.ini`
(100, Attack 120). Once the accumulator is ported, the store panel can fill
them; the base sums already give Beauty/Agility/Armor/Attack/WalkSpeed and the
gene expressions give Aggressiveness/Stamina/Courage.

## Reproduction

The `CTOR` logger in `scripts/x32dbg/start-campaign.ps1 -GenotypeTrace` now
writes `p0/pc/p14/p18/p1c/p28/p2c/p34/p38/p3c/p40/p48/p4c/p60/p68/p6c/p70/p74`.
The 20 rows above come from one Cpg04 mission-1 run; `notes/x32dbg-campaign.out.log`
is gitignored, so this table is the durable copy.
