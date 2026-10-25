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

`ObjectiveN` is `<verb> <args...>`. 28 verbs occur in 89 objective lines:

```text
genotypedinos 14   happyvisitors 10   connectedbuildings 9   visitors 8
dyingdinos 5       franckstrangedinos 4   treasury 4
delaylimit 3       isolateddinos 3
buyalldinos 2      cutedinos 2        dinoexclusiveshow 2
happydinoshow 2    happyshow 2        hybriddinos 2
notconnectedbuildings 2                prettystrangedino 2
scientificshow 2   sizedagilitydinos 2
aggressivedinos 1  cameraaltitude 1   cleverdinos 1
employees 1        hungrydinos 1      marchandizing 1
ornateddinos 1     speeddinos 1       staminadinos 1
```

The verbs name simulation queries (dino counts/traits, visitor happiness,
building connectivity, treasury, camera altitude). Argument semantics are
per-verb and not yet decoded; the mission evaluator is in
`snoObjective.cpp` (`0x6658bc`).

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
objectives; validates mission counts, mission files and world files.
Tests: `tests/test_campaign.py` (7/39/89, per-mission counts, loc coverage,
world files, Cpg07 anomaly).

## Evidence

- `Data/Design/Scenario/CampaignList.ini`, `CampaignBase.ini`,
  `MissionBase.ini`, `Cpg*/`.
- `Data/Config/Scenario.ini` is referenced by the exe but absent from the
  retail tree.
- `tools/formats/campaign.py`, `tests/test_campaign.py`.

## Open

- Per-verb objective argument semantics and mission completion.
- Difficulty variants (`*Medium`/`*Hard`) evaluation.
- Cpg07's role (cut campaign).
- `.wld` world-object contents (shared with `.sav`).
