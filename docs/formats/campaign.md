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
| 3 | `aggressivedinos` | `0x00568680` | threshold, percent | trait predicate open |
| 4 | `genotypedinos` | `0x00568750` | species, count | 9-trait genotype match |
| 5 | `cutedinos` | `0x00568770` | count, percent | trait predicate open |
| 6 | `hybriddinos` | `0x00568840` | count, percent | trait predicate open |
| 7 | `notconnectedbuildings` | `0x00567170` | type, count | helper mode 0 |
| 8 | `connectedbuildings` | `0x00567150` | type, count | helper mode 1 |
| 9 | `visitors` | `0x00566590` | profile, count | `all` or a profile name |
| 10 | `scaritydinos` | `0x00568910` | ? | unused |
| 11 | `speeddinos` | `0x00568ab0` | flag, percent | trait predicate open |
| 12 | `staminadinos` | `0x00568b80` | flag, percent | trait predicate open |
| 13 | `cleverdinos` | `0x00568c50` | flag, percent | trait predicate open |
| 14 | `sizedagilitydinos` | `0x00569040` | flag, percent | trait predicate open |
| 15 | `franckstrangedinos` | `0x00569110` | count, percent | trait predicate open |
| 16 | `employees` | `0x005665f0` | profile, count | never fails |
| 17 | `hungrydinos` | `0x005689e0` | flag, percent | trait predicate open |
| 18 | `isolateddinos` | `0x00568d20` | count | predicate open |
| 19 | `happyvisitors` | `0x005665b0` | profile, percent | every visitor >= percent |
| 20 | `keephappyvisitors` | `0x005665d0` | profile, percent | unused |
| 21 | `carnivordinos` | `0x00568de0` | ? | unused |
| 22 | `ornateddinos` | `0x00568ef0` | count | predicate open |
| 23 | `cameraaltitude` | `0x00569230` | height | camera altitude >= height |
| 24 | `happyshow` | `0x00567710` | profile, percent | show result open |
| 25 | `fightshow` | `0x005677b0` | ? | unused |
| 26 | `scientificshow` | `0x005678b0` | threshold, percent | show result open |
| 27 | `happydinoshow` | `0x00567b10` | count, duration | show result open |
| 28 | `delaylimit` | `0x00567bb0` | limit | clock <= load-time + limit |
| 29 | `buyalldinos` | `0x00569280` | — | dino store empty |
| 30 | `dinoexclusiveshow` | `0x005692a0` | — | show result open |
| 31 | `prettystrangedino` | `0x005694c0` | count, percent | trait predicate open |
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

`genotypedinos` is the bridge to `.gen`: `FUN_00567cb0` loads the design files
from `Data/Design/scenario/genotype/` and compares nine dinosaur trait fields
(`+0x90`, `+0xbc`, `+0x1e4`, `+0x1e8`, `+0x244`, `+0x98`, `+0xc0`, `+0x8c`,
`+0x1e0`) against the requested pattern.

All 206 mission/briefing/objective loc IDs resolve in `MenuEng.loc` /
`ParkEng.loc`.

## `.wld` initial worlds

`WorldFile` is the campaign's initial park state, serialized with the same
world format as `run/save/*.sav` (see `docs/formats/sav.md`). Version dwords:
51 (Cpg01/02/06/08), 50 (Cpg03), 52 (Cpg05), 53 (Cpg04), 2 (Cpg07).
The empty-ish 200-byte saves still contain a camera vector
(`524.7, 433.5, 35.0` / `524.7, 443.5, 32.0` in Cpg01). World-object contents
are decoded by the save research, not here.

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
  `FUN_00566080`/`FUN_00566620`/`FUN_00567cb0` (shared evaluators).
- `notes/objectives-recon.md`, `tools/formats/campaign.py`,
  `tests/test_campaign.py`.

## Open

- Exact predicates for the dino-trait verbs and the nine genotype fields.
- Show evaluator internals (`happyshow`, `scientificshow`, …).
- `scaritydinos` / `carnivordinos` argument semantics.
- Difficulty variants (`*Medium`/`*Hard`) evaluation.
- Cpg07's role (cut campaign).
- `.wld` world-object contents (shared with `.sav`).
