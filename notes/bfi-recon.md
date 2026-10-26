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

## Destination mapping (2026-10-26)

The `.bfi` half of `0x53ed30` (before the `0x53fb00` building loader starts at
`0x53ff01`) reads the body into these runtime-record offsets:

```
53f126  1x4  record +0x0c (via [ebx+ecx+0xc])
53f139  1x4  +0x04
53f14c  1x4  +0x08
53f15f  1x4  +0x0c
53f172  1x1  +0x10
53f185  1x4  +0x14
53f198  1x4  +0x18
53f1ab  1x4  +0x1c
53f1be  1x4  +0x20
53f1d5  1x4  temp (string length/count)
53f1f2..53f25a  string read (count from a temp)
53f367  1x4  +? (string length)
53f388  1x4  +?
53f3cb  1x1  byte
53f3f3  1x4  temp
53f422  4x4  block? (count 1, size 4 at 0x53f422? see note)
53f435  1x4
53f463  1x4  temp
53f5f1  1x4  +0x68 region
...      alternating u32/string pairs, then 3x4 at 0x53f9ae, 1x4 at 0x53fa20
```

Key point: the body is not uniform. The string/LOD/enum blocks differ per
feature class, which is why a single fixed parser fails. The per-class shape
must be taken from the class tag: the four tag-specific branches in the
loader correspond to `StaticBait`, `AnimalBait`, `Waypoint`, `Teleport`; all
other classes share the generic path.

Next concrete step: decompile the class-tag branch targets and split the body
into (a) generic fields + (b) the four tagged variants.
