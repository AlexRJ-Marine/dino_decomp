# Campaign/mission recon — 2026-10-25

## Layer

`Data/Design/Scenario/` is a text-driven campaign system:

- `CampaignList.ini` — 7 names: Cpg01..06, Cpg08.
- `CampaignBase.ini` — BRIEFING, LANDSCAPE, MISSION, CONTEXT.
- `CpgNN/CpgNN.ini` — includes CampaignBase; sets island, mission list,
  `WorldFile`, budget, ending video.
- `CpgNN_MissionList.ini` — count + names.
- `CpgNN_MissionN.ini` — includes MissionBase; objectives, allowed
  buildings/food/features/shows/dino store, fridge eggs, lab and interface
  unlocks.
- `CpgNN_Save.wld` — initial park state.

## Campaign table

```text
name   island budget missions world_file       size   version
Cpg01  Isl01  60000  14       Cpg01_Save.wld   200    51
Cpg02  Isl02  50000   5       Cpg02_Save.wld  1007    51
Cpg03  Isl03  35000   5       Cpg03_Save.wld   672    50
Cpg04  Isl04  20000   4       Cpg04_Save.wld 41397    53
Cpg05  Isl05  30000   4       Cpg05_Save.wld  1334    52
Cpg06  Isl06  50000   4       Cpg06_Save.wld   200    51
Cpg08  Isl07  20000   3       Cpg08_Save.wld   200    51
Cpg07  -      -       3       Cpg07_Save.wld  7712     2   (not in CampaignList)
```

39 missions total. All 206 briefing/objective loc IDs resolve in
`MenuEng.loc`/`ParkEng.loc`.

## MissionBase sections

`BRIEFING` (title/description/hint), `OBJECTIVES` (5 commands + desc IDs),
`CONTEXT` (budget/sellback per difficulty), `PROFILES` (visitor types),
`BUILDINGS` (10), `FOODSOURCES` (2), `FEATURES` (10), `SHOWS` (10),
`DINOSTORE` (10), `FRIDGE` (10 eggs), `LABORATORY` (5 flags), `INTERFACE`
(7 flags).

`Cpg01_Mission4` example: `genotypedinos hypsilo 2`,
`genotypedinos coelo 2`, `dyingdinos 0`; building `PointOfView`; egg
`Coelophysis coelo.gen`.

## Objectives

89 objective lines, 28 verbs:

```text
genotypedinos 14, happyvisitors 10, connectedbuildings 9, visitors 8,
dyingdinos 5, franckstrangedinos 4, treasury 4, delaylimit 3,
isolateddinos 3, buyalldinos 2, cutedinos 2, dinoexclusiveshow 2,
happydinoshow 2, happyshow 2, hybriddinos 2, notconnectedbuildings 2,
prettystrangedino 2, scientificshow 2, sizedagilitydinos 2,
aggressivedinos 1, cameraaltitude 1, cleverdinos 1, employees 1,
hungrydinos 1, marchandizing 1, ornateddinos 1, speeddinos 1,
staminadinos 1
```

Dispatch solved: `FUN_005654f0` parses the line and stores the verb id at
`+0x34`, two numeric args at `+0x28`/`+0x2c`, and an evaluator pointer at
`+0x30`. `FUN_005658d0` maps 33 verbs to ids 0–32. Evaluators are tri-state:
0 = pending, 1 = success, 2 = failure.

Simple evaluators decoded (treasury, marchandizing, dyingdinos, employees,
visitors, happyvisitors, connected/not-connected buildings, camera altitude,
delay limit, buy-all-dinos, bankrupt); the 10-slot dinosaur trait pattern,
the show query, `carnivordinos`, `ornateddinos`, `scientificshow` and
`dinoexclusiveshow` are mapped (see `notes/objectives-recon.md`). All 33
evaluator predicates are decoded; only field names and weighting are open.

Data quirks: Cpg07's cut mission uses the typo `dyinddinos`; Cpg06_Mission1
has `genotypedinos diplo 1""` (still parses). `scaritydinos`,
`keephappyvisitors`, `carnivordinos`, `fightshow` and `bankrupt` are unused.

## World files

The 8 `CpgNN_Save.wld` files are the initial park states. They share the
world serializer with `.sav`:
- Cpg01/06/08 200 bytes, version 51; Cpg01 ends with a camera vector
  (524.7,433.5,35 / 524.7,443.5,32).
- Cpg04 41,397 bytes, version 53 (large object list).
- Cpg07 version 2, unlisted campaign.
- `Data/Config/Scenario.ini` is referenced by the exe but missing from the
  tree.

## Evidence

- `Data/Design/Scenario/*` (read-only).
- `tools/formats/campaign.py`, `tests/test_campaign.py`,
  `docs/formats/campaign.md`.
- `snoCampaignManager.cpp` / `snoCampaignDataBase.cpp` / `snoObjective.cpp`
  source strings in the exe.
