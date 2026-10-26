# `FreeGame.ini` ? free-game setup

Status: **solved.** Every section parses; the 39 show names and the 20 store
entries cross-check against the retail files.

## Inventory

`Data/Design/Scenario/FreeGame.ini` ? 1,866 bytes, cp1252. It includes
`MissionBase.ini` with `[#INCLUDE]`, so it carries the campaign schema plus
these overrides:

| Section | Content |
| --- | --- |
| `[CONTEXT]` | budgets and sell-back factors per difficulty |
| `[PROFILES]` | visitor personality lists per difficulty |
| `[SHOWS]` | show names per page (`Shows0..Shows9`) |
| `[DINOSTORE]` | the free-game dinosaur store subset |
| `[LABORATORY]` | starting lab upgrade levels |
| `[INTERFACE]` | enabled interface features |
| `[SELLING]` | selling permissions |
| `[BRIEFING]`/`[OBJECTIVES]`/`[BUILDINGS]`/`[FOODSOURCES]`/`[FEATURES]`/`[FRIDGE]` | empty (inherited from `MissionBase.ini`) |

## `[CONTEXT]`

| Key | Easy | Medium | Hard |
| --- | ---: | ---: | ---: |
| `Budget` | 75,000 | 50,000 | 25,000 |
| `SellBackFactor` | 1.0 | 0.5 | 0.2 |

The difficulty index comes from `mission+0x50` (`0/1/2`) in
`0x00561bb0`; the viewer already uses these budgets.

## `[PROFILES]`

Three strings (`VisProfiles`, `VisProfilesMedium`, `VisProfilesHard`), each a
flat list of **triplets**:

```text
<profile id> <first> <second>
```

Each list has 6 profiles:

| Difficulty | List |
| --- | --- |
| easy | `retraite 30 100 surfer 30 100 parent 30 100 enfant 30 100 holligan 30 100 scientific 30 100` |
| medium | `retraite 20 100 surfer 20 100 parent 20 100 amoureux 20 100 holligan 20 100 riche 10 100` |
| hard | `blase 10 100 riche 10 100 spd 10 100 amoureux 10 100 parent 10 100 retraite 10 100` |

All second values are 100. The first is the per-profile starting count: easy
6 ? 30 = 180, medium 5 ? 20 + 10 = 110, hard 5 ? 10 + 10 = 60. So harder
difficulties start with fewer visitors. (The second value is passed with the
first to the profile record by `0x005869b0`; it is interpreted as a cap.)

## `[SHOWS]`

`Shows0..Shows7` list the 39 retail shows, `Shows8`/`Shows9` are empty:

| Page | Shows |
| ---: | ---: |
| 0 | 3 (Cpg1 race) |
| 1 | 8 (Cpg2 obstacle/race) |
| 2 | 1 (Cpg2_Obst_Mission4) |
| 3 | 1 (cpg3_obst_1) |
| 4 | 8 (Cpg4 derby/fight) |
| 5 | 3 (Cpg5) |
| 6 | 11 (Cpg6) |
| 7 | 4 (Cpg7) |
| 8, 9 | 0 |

The page index matches the campaign index (Cpg1..Cpg7); every name has a
`.shw` file.

## `[DINOSTORE]`

A subset of `Market.ini`: 20 entries in pages 0,2,3,4,5,6 (page 1 absent),
with the same names, prices and `.gen` files. Page counts 3/4/4/4/4/1.

## `[LABORATORY]`

```text
Reproduction=3 Modification=3 Cloning=3 Mutation=3 Fridge=3
```

The free game starts with lab upgrade level 3 in all five tracks (see
`docs/formats/laboratory.md`).

## `[INTERFACE]` and `[SELLING]`

`[INTERFACE]` sets `Laboratory`, `Show`, `Fence`, `Building`,
`Infrastructure`, `DinoPurchase` and `EggSale` to 1; `[SELLING]` sets
`Dinosaur=1`. The other inherited mission sections are empty, i.e. free game
adds no building/food/feature/fridge restrictions.

## Reader

`tools/formats/freegame.py`:

```text
python tools/formats/freegame.py <FreeGame.ini>
```

Returns `FreeGame` with `budgets`, `sellback`, `profiles`, `shows`,
`dinostore` (reusing `MarketEntry`), `laboratory`, `interface`, `selling`.
Tests: `tests/test_freegame.py` (context, lab, profiles, 39 shows with files,
20 store entries matching `Market.ini`).

## Evidence

- `Data/Design/Scenario/FreeGame.ini`, `MissionBase.ini`, `Market.ini`.
- `Data/Design/Scenario/Show/*.shw`.
- Ghidra: `0x00561bb0` (difficulty + profile parse), `0x005869b0`
  (profile record).
- `tools/formats/freegame.py`, `tests/test_freegame.py`.

## Open

- Whether the empty inherited lists mean "no restriction" or "nothing
  unlocked".

The profile triplet's second value is passed through unchanged by
`FUN_00561bb0` -> `FUN_005869b0` -> `FUN_00615bb0`; all 18 retail triplets use
100, so the file gives no variation to decode. `Shows8`/`Shows9` are empty
because only campaigns 1-7 exist (page N matches campaign N+1; Cpg07 is cut
content), not because of a loader rule.
