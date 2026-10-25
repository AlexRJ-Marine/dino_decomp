# Campaign and mission data

Status: **solved** for the text layer (7 campaigns, 39 missions, 89
objectives, 206 localisation IDs); objective semantics are partial.

## Directory layout

```text
Data/Design/Scenario/
  CampaignList.ini          7 campaign folder names
  CampaignBase.ini          campaign template ([#INCLUDE] target)
  MissionBase.ini           mission template ([#INCLUDE] target)
  CpgNN/
    CpgNN.ini               includes ..\CampaignBase.ini
    CpgNN_MissionList.ini   count + mission names
    CpgNN_MissionN.ini      includes ..\MissionBase.ini
    CpgNN_Save.wld          initial park state for the campaign
```

The game uses the same INI syntax as the overlay files (`#INCLUDE`, quoted
values, `;` comments); later section/key definitions override the base.

## CampaignBase.ini

| Section | Keys |
| --- | --- |
| `BRIEFING` | `Title`, `DescriptionText`, `IslandSnapshot`, `EndingText`, `EndingVideo`, `EndingVideoName` |
| `LANDSCAPE` | `Island` |
| `MISSION` | `MissionList` |
| `CONTEXT` | `WorldFile`, `Budget`, `SellBackFactor` |

## CampaignList table

| Campaign | Island | Budget | Missions | World file | Size |
| --- | --- | ---: | ---: | --- | ---: |
| Cpg01 | Isl01 | 60,000 | 14 | Cpg01_Save.wld | 200 |
| Cpg02 | Isl02 | 50,000 | 5 | Cpg02_Save.wld | 1,007 |
| Cpg03 | Isl03 | 35,000 | 5 | Cpg03_Save.wld | 672 |
| Cpg04 | Isl04 | 20,000 | 4 | Cpg04_Save.wld | 41,397 |
| Cpg05 | Isl05 | 30,000 | 4 | Cpg05_Save.wld | 1,334 |
| Cpg06 | Isl06 | 50,000 | 4 | Cpg06_Save.wld | 200 |
| Cpg08 | Isl07 | 20,000 | 3 | Cpg08_Save.wld | 200 |
| *Cpg07* | — | — | 3 | Cpg07_Save.wld | 7,712 |

`Cpg07` has a folder, missions and a world file but is **not listed** in
`CampaignList.ini` — treat it as cut/unreachable content.

## MissionBase.ini

| Section | Keys | Meaning |
| --- | --- | --- |
| `BRIEFING` | `Title`, `DescriptionText`, `HintText` | `&loc` IDs |
| `OBJECTIVES` | `Objective1..5` + `_Desc` | command strings |
| `CONTEXT` | `Budget`/`SellBackFactor` + `Medium`/`Hard` variants | per-difficulty |
| `PROFILES` | `VisProfiles` + Medium/Hard | allowed visitor personalities |
| `BUILDINGS` | `Buildings0..9` | allowed buildings |
| `FOODSOURCES` | `FoodSources0..1` | allowed food |
| `FEATURES` | `Features0..9` | allowed park features |
| `SHOWS` | `Shows0..9` | allowed shows |
| `DINOSTORE` | `DinoStore0..9` | available dinosaurs |
| `FRIDGE` | `Eggs0..9` | starting eggs (`<species> <file.gen>`) |
| `LABORATORY` | `Reproduction`, `Modification`, `Cloning`, `Mutation`, `Fridge` | lab unlocks |
| `INTERFACE` | `Laboratory`, `Show`, `Fence`, `Building`, `Infrastructure`, `DinoPurchase`, `EggSale` | UI unlocks |

Example `Cpg01_Mission4`: objectives `genotypedinos hypsilo 2`,
`genotypedinos coelo 2`, `dyingdinos 0`; building `PointOfView`; egg
`Coelophysis coelo.gen`.

## Objective commands

`ObjectiveN` is `<verb> <args...>`. `snoObjective::Load` (`0x005654f0`)
tokenises the line, maps the verb to an id with `0x005658d0`, parses up to two
numeric arguments into `+0x28`/`+0x2c`, and stores an evaluator pointer at
`+0x30`. Evaluators return **0 = pending, 1 = success, 2 = failure**.

All 33 implemented verbs (id, evaluator, arguments):

| Id | Verb | Evaluator | Arguments | Notes |
| ---: | --- | --- | --- | --- |
| 0 | `treasury` | `0x00567c00` | target, [deadline] | below target fails when deadline <= 0 |
| 1 | `marchandizing` | `0x00567c40` | target | merchandise income > target |
| 2 | `dyingdinos` | `0x00569010` | max | death counters sum <= max |
| 3 | `aggressivedinos` | `0x00568680` | count, percent | pattern[1] `+0xbc` aggression |
| 4 | `genotypedinos` | `0x00568750` | species, count | 10-field genotype match |
| 5 | `cutedinos` | `0x00568770` | count, percent | pattern[0] `+0x90` cuteness |
| 6 | `hybriddinos` | `0x00568840` | count, percent | pattern[2] `+0x1e4` hybrid |
| 7 | `notconnectedbuildings` | `0x00567170` | type, count | helper mode 0 |
| 8 | `connectedbuildings` | `0x00567150` | type, count | helper mode 1 |
| 9 | `visitors` | `0x00566590` | profile, count | `all` or a profile name |
| 10 | `scaritydinos` | `0x00568910` | count, percent | pattern[3] `+0x1e8` (unused) |
| 11 | `speeddinos` | `0x00568ab0` | count, speed | pattern[6] `int(+0x7c)` |
| 12 | `staminadinos` | `0x00568b80` | count, percent | pattern[7] `+0xc0` stamina |
| 13 | `cleverdinos` | `0x00568c50` | count, percent | pattern[9] `+0x8c` intelligence |
| 14 | `sizedagilitydinos` | `0x00569040` | size, agility | pattern[5] `+0x98`, pattern[8] `+0x1e0`; count fixed at 1 |
| 15 | `franckstrangedinos` | `0x00569110` | age, score | `+0x1b0==1`, `+0x1b4==0`, `+0x190>=2`, `+0x6c<age`, `+0x3c>score` |
| 16 | `employees` | `0x005665f0` | profile, count | never fails |
| 17 | `hungrydinos` | `0x005689e0` | count, percent | pattern[4] `+0x244` food; count -1 = all |
| 18 | `isolateddinos` | `0x00568d20` | max | pattern[1] `+0xbc`, fail-list count (-2) |
| 19 | `happyvisitors` | `0x005665b0` | profile, percent | every visitor >= percent |
| 20 | `keephappyvisitors` | `0x005665d0` | profile, percent | unused |
| 21 | `carnivordinos` | `0x00568de0` | threshold | `+0x1b0==1`, `+0x70 < threshold` (unused) |
| 22 | `ornateddinos` | `0x00568ef0` | count | dinos with a non-empty 23-slot `+0xE8` array |
| 23 | `cameraaltitude` | `0x00569230` | height | camera altitude >= height |
| 24 | `happyshow` | `0x00567710` | profile, score | any show, profile score >= score |
| 25 | `fightshow` | `0x005677b0` | score | fight show, hooligan score (unused) |
| 26 | `scientificshow` | `0x005678b0` | score, strangeness | show score > score and a dino with `+0x1e4` > strangeness |
| 27 | `happydinoshow` | `0x00567b10` | score, participants | any show, score and dino count |
| 28 | `delaylimit` | `0x00567bb0` | limit | clock <= load-time + limit |
| 29 | `buyalldinos` | `0x00569280` | — | dino store empty |
| 30 | `dinoexclusiveshow` | `0x005692a0` | — | a show has a dino that no other show has |
| 31 | `prettystrangedino` | `0x005694c0` | beauty, strange | `+0x40 > beauty`, `+0x194 < strange` |
| 32 | `bankrupt` | `0x00567c80` | — | fails when the flag is set |

The 89 CampaignList objectives use 28 of these; `scaritydinos`,
`keephappyvisitors`, `carnivordinos`, `fightshow` and `bankrupt` are
implemented but unused. `Cpg07_Mission1` (cut campaign) contains a
`dyinddinos` typo which would hit the `unknown objective` assert;
`Cpg06_Mission1` has a malformed `genotypedinos diplo 1""` line that still
parses because `atoi` stops at the quote.

Known semantics:

```text
treasury       treasury >= target; below target -> failure if deadline <= 0
marchandizing  merchandise income > target
dyingdinos     death counters (+0x14c + +0x150) <= max
employees      employee count >= count
cameraaltitude camera altitude >= height
delaylimit     clock <= clock-at-load + limit, then failure
buyalldinos    dino store is empty
bankrupt       failure once sim+0x95 is set
visitors       visitor count >= count (profile 'all' = total)
happyvisitors  every visitor satisfaction >= percent
```

### Trait pattern (10 fields)

Most dinosaur-trait verbs build a 10-int pattern and call `FUN_00567cb0`
(species filter, pattern, mode 2, required count, -1, 1). The evaluator sets
one or two slots; the rest stay 0, so they are not constrained. The matcher
compares these actor fields in order:

| Slot | Actor field | Trait (verb that uses it) |
| ---: | --- | --- |
| 0 | `+0x90` | cuteness/beauty (`cutedinos`) |
| 1 | `+0xbc` | aggression (`aggressivedinos`, `isolateddinos`) |
| 2 | `+0x1e4` | hybrid/strangeness (`hybriddinos`) |
| 3 | `+0x1e8` | scarcity/rarity (`scaritydinos`) |
| 4 | `+0x244` | food need (`hungrydinos`) |
| 5 | `+0x98` | size (`sizedagilitydinos`) |
| 6 | `int(+0x7c)` | speed (`speeddinos`) |
| 7 | `+0xc0` | stamina (`staminadinos`) |
| 8 | `+0x1e0` | agility (`sizedagilitydinos`) |
| 9 | `+0x8c` | intelligence (`cleverdinos`) |

`genotypedinos` is the same matcher with an empty pattern (all 10 fields 0),
so it filters by species only. It loads the design files from
`Data/Design/scenario/genotype/`.

`franckstrangedinos` and `prettystrangedino` do not use the pattern matcher:

```text
franckstrangedinos age score
    dino+0x1b0 == 1, dino+0x1b4 == 0, dino+0x190 >= 2,
    dino+0x6c < age, dino+0x3c > score

prettystrangedino beauty strange
    dino+0x40 > beauty, dino+0x194 < strange
```

### Show objectives

`FUN_00567190` is the show query. Its show-type argument maps:

| String | Id |
| --- | ---: |
| `race` | 1 |
| `obstacle` | 2 |
| `fight` | 3 |
| `destruction` | 4 |
| `team` | 5 |
| `capture` | 6 |
| `fence` | 7 |
| `all` | any |

It iterates shows of that type, filters by a visitor profile (`all` or a
name), reads the show's per-profile score array (`show+0xEC -> +0x6c`), and
counts shows whose score is >= the threshold. It also enforces a minimum
number of participating dinosaurs (`show+0xB4 - show+0xB0 >> 2`).

```text
happyshow <profile> <score>       any show type, profile score >= score
happydinoshow <score> <count>     any show, score >= score and >= count dinos
fightshow <score>                 "fight" show, "hooligan" profile (unused)
```

`scientificshow` and `dinoexclusiveshow` have their own show loops.
`dinoexclusiveshow` succeeds when a dinosaur appears in exactly one show's
`show+0xAC` vector. `scientificshow <score> <strangeness>` requires a show
whose per-type score is `> score` and a dinosaur in its `show+0xAC` list with
`+0x1E4 > strangeness` (the same hybrid/strangeness field as
`hybriddinos`).

All 206 mission/briefing/objective loc IDs resolve in `MenuEng.loc` /
`ParkEng.loc`.

## Difficulty variants

`FUN_00561bb0` (mission INI applier) reads the difficulty index at
`mission+0x50` and selects the matching context/profile keys:

| Index | Budget | Sell-back | Visitor profiles |
| ---: | --- | --- | --- |
| 0 | `Budget` | `SellBackFactor` | `VisProfiles` |
| 1 | `BudgetMedium` | `SellBackFactorMedium` | `VisProfilesMedium` |
| 2 | `BudgetHard` | `SellBackFactorHard` | `VisProfilesHard` |

Only these three keys have difficulty variants; `Buildings`, `Features`,
`FoodSources`, `Shows` and `DinoStore` are difficulty-independent. The
campaign branch is gated by the fourth argument of `FUN_00561bb0` (free mode
uses its own `FreeGame.ini` budgets).

**No retail mission overrides any `*Medium`/`*Hard` key.** All 39 missions
inherit the `MissionBase.ini` defaults (budget 0, sell-back 1.0, empty
profiles), so the difficulty variants are effectively dead in the campaign;
treat difficulty as 0 unless a future source proves otherwise. Where
`mission+0x50` is set (campaign UI or save) is still open.

## `.wld` initial worlds

`WorldFile` is the campaign's initial park state, serialized with the same
world format as `run/save/*.sav` (see `docs/formats/sav.md` and
`docs/formats/wld.md`). Version dwords: 51 (Cpg01/02/06/08), 50 (Cpg03),
52 (Cpg05), 53 (Cpg04), 2 (Cpg07). The empty-ish 200-byte saves still contain
a camera vector (`524.7, 433.5, 35.0` / `524.7, 443.5, 32.0` in Cpg01).

## Reader

`tools/formats/campaign.py`:

```text
python tools/formats/campaign.py --scan <Scenario-dir>
python tools/formats/campaign.py --dump <CpgNN.ini>
```

Returns campaigns, islands, budgets, world files, missions and parsed
objectives; `OBJECTIVE_VERBS` maps each verb to its internal id and evaluator
address. Validates mission counts, mission files and world files.
Tests: `tests/test_campaign.py` (7/39/89, per-mission counts, loc coverage,
world files, Cpg07 anomaly, 33-verb table).

## Evidence

- `Data/Design/Scenario/CampaignList.ini`, `CampaignBase.ini`,
  `MissionBase.ini`, `Cpg*/`.
- `Data/Config/Scenario.ini` is referenced by the exe but absent from the
  retail tree.
- `FUN_005654f0` (`snoObjective::Load`), `FUN_005658d0` (verb lookup),
  `FUN_00566080` (visitors), `FUN_00566620` (buildings),
  `FUN_00567cb0` (trait/genotype pattern), `FUN_00567190` (shows).
- `FUN_00561bb0` (mission INI applier) — difficulty key selection.
- `notes/objectives-recon.md`, `tools/formats/campaign.py`,
  `tests/test_campaign.py`.

## Open

- Human-readable names for the 10 trait fields, the show score and `+0x1e4`
  strangeness.
- Where `mission+0x50` (difficulty 0/1/2) is set. No retail mission overrides
  the `*Medium`/`*Hard` keys, so difficulty 1/2 would inherit zeros.
- Cpg07's role (cut campaign).
