# `.shw` show-cage feature grid recon (2026-10-26)

Follow-up to FMT-8: the feature tuples are now decoded.

## Tuple semantics

`Features` lines are N × 4 ints: `(feature_index, rotation, x, y)`.

- `feature_index` indexes `FeatureList.ini` (62 entries). All 1,679 tuples
  across the 43 retail shows are in range 0..61 and the full range occurs.
  Top indices: 42 (617), 26 (115), 27 (111), 43 (87), 0 (68), 29 (57),
  1 (57), 15 (51), 2 (43), 28 (38), 50 (33).
- `rotation` is 0..3 (quarter turns). It is **not** the team; teams come from
  the `(cage number, team)` assignment list.
- `x`/`y` are grid cells and are always `< Dimensions`.

`FeatureList.ini` order (first entries): 0 CageSmall, 1 CageMedium,
2 CageHigh, 3 CageHuge, 4 JudgeRace, 5 JudgeObstacle, 6 JudgeDestruction,
7 JudgeFight, 8 JudgeTeam, 9 JudgeCapture, 10 ObstacleCar, 26..29
WaypointTeam1..4, 42 ObstacleBeam5, 50 Danse, 60 TileQuad, 61 TileSleep.

## Cross-checks

Over all 43 shows:

- 1,679 features total; 1606 distinct tuples.
- Cage features (indices 0..3) = 68+57+43+6 = 174 = the 174 team slots,
  and they match per file (0 mismatches). So every team slot has exactly one
  cage feature.
- Every show has at least one judge feature (4..10). Derby arenas place nine
  of them.
- Every position is inside the declared dimensions (0 outside).
- x range 0..67, y range 0..84 across all arenas.

## Example

`Cpg1_Race_1.shw`: dimensions 19×39, 2 team slots `(0,1),(1,0)`, features
include cages `(0,1,12,4)` and `(0,1,4,4)` (both CageSmall, rotation 1),
judge `(4,0,0,18)`, waypoints 26/27 at rotations 1 and 3.

`Cpg1_Race_2.shw`: 27×75, 4 teams; cages `(0,3,2,30)`, `(0,1,2,40)`,
`(0,1,22,40)`, `(0,3,22,30)` — mirrored left/right, rotations 3/1.

`Cpg2_Obst_1.shw`: judge is `(5,3,9,36)` = JudgeObstacle; four CageSmall at
rotation 1 along y=2.

## Tooling

- `tools/formats/shw.py` now also exposes `read_feature_names(features_dir)`
  returning the 62-entry `FeatureList.ini` order.
- `tests/test_shw.py::test_feature_indices_resolve_and_cages_match_teams`
  checks indices, rotations, positions, cage/team equality and judge presence.

## Open

- `Judge Kind`, `Wall type` and the runtime catalog fields `param1`/`param2`/
  `param5` (see `docs/formats/shw.md`).

## Rotation direction and pivot

The feature constructor is the shared `FUN_00586b80` (also used by buildings).
Its jump table `0x586d0c` handles the four orientations; the yaw basis from
`FUN_0061ffc0` case 0 is state 0 `(1,0)`, 1 `(0,1)`, 2 `(-1,0)`, 3 `(0,-1)` -
counter-clockwise 90 deg steps. The `.shw` `(x,y)` is the footprint min corner,
but the render pivot is the footprint centre: `stonw.mesh` (1x4 beam) has local
bounds x +/-0.488, y +/-1.99, i.e. centred.
## Judge Kind and cage flags

`Judge Kind` 1..4 selects the judge feature:

```
1 -> feature 4 JudgeRace        (9 shows)
2 -> feature 5 JudgeObstacle    (15)
3 -> feature 7 JudgeFight       (11)
4 -> feature 6 JudgeDestruction (8)
```

Feature 10 `JudgeTeam` is an extra marker in some obstacle/destruction
shows. So the `.shw` judge kind is not a free variant: it determines which
judge actor the show uses.

`Available cage size` is the set of cage sizes actually placed:
`cage_size[i] == 1` iff a cage feature of index `i` (0..3 = small..huge)
occurs. 43/43 exact. Ten combinations occur:
`0100` x16, `1000` x9, `0010` x9, `1010` x2, `0001` x2, plus one each of
`1110`, `1100`, `0110`, `0111`, `1001`.

`Wall type` is 1 in all 43 files (one wall style).

## Edition demo shows (2026-10-26)

`Data/Edition/Show/{demo,demo8,demofinale}.shw` are a different, older
schema:

```
NoName
32        width
72        height
6|2       wall type (index into WallList.ini: 2 MediumIron, 6 MediumWood)
20|5|8    feature count
features (4 ints each)
```

No cost/cage/team fields; the fourth value is the wall type. All 3 are 32x72; features use the same
(FeatureList index, rotation, x, y) tuple and stay inside the arena.
`read_shw` rejects them; `read_demo_shw` parses them.

## Save catalog fields (2026-10-26)

All 39 catalog records are a straight snapshot of the `.shw`:

```
params[0] = Cost
params[1] = 4 (constant)
params[2] = cage flags packed as 4 bytes (sum(cage_size[i] << (8*i)))
params[3] = Dimensions[0]
params[4] = Dimensions[1]
params[5] = Judge Kind
```

Verified 39/39 against `run/save/slot001.sav`; `tests/test_shw.py` checks it.
The earlier note that `param2` is "1 or 256" and `param5` is "1 or 2" came
from a partial sample: the full set includes 0x10000/0x1000000 combinations
(multi-cage shows) and judge kinds 1..4.

## Footprint tiling (2026-10-26)

Using `.bfi` width/height as the grid footprint, rotation 0/2 = (w,h) and
1/3 = (h,w), and the tuple (x,y) as the min corner, all 43 shows tile with
**zero overlaps**: 1,679 features -> 29,500 cells, no shared cell. This
proves the anchor, rotation and footprint semantics.

Only 3 judge features overhang the arena edge by one cell (4x4 at the last
row): Cpg2_Obst_1 (9,36 in 23x39), Cpg2_Obst_Mission4 (9,84 in 23x87),
Cpg4_Derby_1 (33,32 in 71x35).
