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

- Rotation direction and pivot corner.
- `Judge Kind`, `Wall type` and the runtime catalog fields `param1`/`param2`/
  `param5` (see `docs/formats/shw.md`).
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
