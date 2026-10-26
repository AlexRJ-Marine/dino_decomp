# `.bfi` show-feature recon (2026-10-26)

Read-only analysis of `Data/Design/attractions/features`. No writes to
`game/` or `original/`.

## Inventory

62 files, 436-1,037 bytes, one per `FeatureList.ini` line (after the count
62). Loader `0x53ed30` builds 0xb4-byte runtime records and compares the
first string against `StaticBait`, `AnimalBait`, `Waypoint`, `Teleport`.

## Header (0x00..0x2c)

12 packed fields, the last one a u8 at 0x10, so everything after is
unaligned. The first string length is at 0x2d (a 4-byte-aligned read gets
0x64 and fails).

```
0x00 u32 width        4 default; cages 8/12/16; ObstacleCar 2; bus 4
0x04 u32 height       4 default; ObstacleBeam5 1; bus 8; car 4
0x08 f32 vertical     4.0 default; CageHuge 8.0; JudgeRace 12.0; fire 0.0
0x0c u32 variant      0 default; CageSmall..Huge 0..3
0x10 u8  flag         1 for the six odd-footprint obstacles
0x11 u32 100/200/400/500
0x15 u32 10 default
0x19 u32 10..1200     Cage 1200, Tile 300, bait/judge 50, waypoint 10
0x1d u32 0..4
0x21 u32 3 default, 0 obstacles
0x25 u32 0/100
0x29 u32 0..2
0x2d string class tag
```

## Class tags

```
Tile       26  (24 Tile* + Danse + 7 trap/hole obstacles)
(empty)    10  Beam1..5, BusLondon, BusSchool, Car, MiniCar, Rock
Judge       6
AnimalBait  5
StaticBait  5
Cage        4
Waypoint    4
LunchBox    1
Teleport    1
```

`Tile` is a placement category, not a mesh class. The empty tag is the
large multi-cell prop category.

## Strings

The body interleaves integers and length-prefixed strings. A scan for
printable length-prefixed strings recovers:

- action/animation names (`EatBait`, `EatBaitForSmallies`, `meuhScared`,
  `Jump1m_Size1..3`, `LinkObstAttackSize1..4`, `lift`, `courage`, `agility`,
  `combat`, `wood`, `feu`);
- 57 `.mesh` LOD paths and 49 `.tga` paths, all present;
- effect INIs (`explo_1bois.ini`, `explo_2bois.ini`, `fin_attraction1.ini`);
- the billboard directory;
- `SWITCH_FEATURE_*` keys in 60/62 files (missing on JudgeCapture/Team).

## Tooling

- `tools/formats/bfi.py` - header + tag + string inventory.
- `tests/test_bfi.py` - 62 files, tag histogram, FeatureList match, asset
  existence, 60 switch keys, cage/obstacle footprints.
- `docs/formats/bfi.md` - spec.

## Open

- Integer fields after the class tag (action bindings, durations, scores).
- JudgeCapture/JudgeTeam switch keys.
- Multi-cell footprint behaviour for the empty-tag props.

## Body read order attempt (2026-10-26)

Capstone extraction of `0x53ed30` gives 73 reader calls (dest, size, count).
After the class tag the loader does a long run of `1 x u32` reads with a
`1 x u8` and a `4 x u32` block, then alternating `u32`/string reads, then a
`3 x u32` block. A simple "6 u32 then a string" hypothesis matches only 7/62
files, so the body is variable-width (the AnimalBait files contain a packed
byte array `01 02 05 06 09` where others have a u32). The next step is to map
the actual destinations of the 73 calls (record offsets), not just the sizes.
