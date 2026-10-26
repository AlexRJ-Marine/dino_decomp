# FreeGame.ini recon (2026-10-26)

Read-only analysis of `Data/Design/Scenario/FreeGame.ini`.

## Sections

`[#INCLUDE]` pulls `MissionBase.ini`; the free-game overrides are
`CONTEXT`, `PROFILES`, `SHOWS`, `DINOSTORE`, `LABORATORY`, `INTERFACE`,
`SELLING`. The inherited `BRIEFING`, `OBJECTIVES`, `BUILDINGS`,
`FOODSOURCES`, `FEATURES`, `FRIDGE` are all empty.

## Context

```
Budget 75000 / SellBackFactor 1.0
BudgetMedium 50000 / SellBackFactorMedium 0.5
BudgetHard 25000 / SellBackFactorHard 0.2
```

Difficulty branch is `mission+0x50` in `FUN_00561bb0`.

## Visitor profiles

Three lists, 6 triplets each `(profile id, first, second)`:

```
easy   retraite 30 100  surfer 30 100  parent 30 100  enfant 30 100
       holligan 30 100  scientific 30 100
medium retraite 20 100  surfer 20 100  parent 20 100  amoureux 20 100
       holligan 20 100  riche 10 100
hard   blase 10 100  riche 10 100  spd 10 100  amoureux 10 100
       parent 10 100  retraite 10 100
```

Sums of the first value: easy 180, medium 110, hard 60. All second values
are 100. `FUN_00561bb0` parses the triplets and calls `FUN_005869b0`
(`FUN_00615bb0`) which appends the two ints to the visitor-profile record.

## Shows

`Shows0..Shows7` list 39 names (3+8+1+1+8+3+11+4); `Shows8`/`Shows9` are
empty. Every name has a `.shw`. Page N matches campaign N+1.

## Dinosaur store

20 entries, pages 0/2/3/4/5/6 (page 1 absent), prices and names identical to
`Market.ini` (checked).

## Laboratory and interface

Lab starts at level 3 in all five tracks (`Reproduction`, `Modification`,
`Cloning`, `Mutation`, `Fridge`). `[INTERFACE]` enables all seven features;
`[SELLING] Dinosaur=1`.

## Tooling

- `tools/formats/freegame.py` ? parser (reuses `MarketEntry`).
- `tests/test_freegame.py` ? context/lab/interface, profiles, 39 shows,
  20 store entries vs Market.ini.
- `docs/formats/freegame.md` ? spec.

## Open

- Second profile value meaning.
- Empty show pages 8/9.
- Empty inherited lists: no restriction vs nothing unlocked.
