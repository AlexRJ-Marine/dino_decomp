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

## Show rectangle units and construction panel (2026-10-27)

### `Dimensions` are world units, not grid cells

`FUN_0052d1e0` is the `PREDEFINED_SHOW_VALIDATE` step. It reads the save
catalog entry (stride `0x1c`; `params[3]`/`params[4]` are the `.shw`
`Dimensions`), adds the border `2*[show+0x6c] + 1` to each axis, and calls
`FUN_005f86d0(show, catalog_index, &rect)`. `FUN_005f86d0` stores the pair in
`+0x68`/`+0x6c`, the same building rectangle used by `gpkBuilding`
(`0x5fecf0`), whose `.bbi` footprints are world units
(`docs/platform/buildings.md`). The placement validator reached through
`FUN_005fc4d0` -> `FUN_0061e9b0` then maps the rectangle to the terrain grid
with the cell size.

Park scale confirms it. The Cpg04 fences span `376..631 x 312..551`
(255 x 239 world units) and the park's saved show record is
`(420,416)-(499,451)` (about 80 x 36 world units). A 71x35 show read as grid
cells would be 284 x 140 world units and could not fit either.

Consequence: the feature tuple `(x, y)` and the `.bfi` footprint share the
same unit space as `Dimensions` (the zero-overlap tiling check stays valid;
the earlier "cell" wording above means a 1-unit coordinate square, while a
4x4 `.bfi` footprint is one 4-unit build cell), but the arena is 71 x 35
world units = about 18 x 9 build cells, not 71 x 35 cells. The port uses this in `place_show_at`: record coordinates 0..1024,
`cell = floor(record / cell_size)`, and a buildable check over every covered
cell. The port omits the `2*[show+0x6c]+1` border (retail adds it before the
rectangle is written).

### Panel widgets

`FUN_00514ad0` (panel ctor) resolves the ten row widgets
(`ATTRACTION_LIST_SELECTION_LIGHT1..10`, `PERMUTATION1..10`, `NAME1..10`,
`LOCK1..10`, `UNLOCK1..10`, `ITERATION_UP/DOWN1..10`,
`ITERATION_INDIC1..10`, `PROFIL1..10`) into ten `0x28`-byte row records at
`panel+0xd0`. `FUN_00516850` is the open/refresh entry: `mode == 0` hides the
rows, otherwise it calls `FUN_00516dd0`, which for every row:

- occupied: sets `NAME` from `show+4` and appends `"%u"` of `show+0x8c`
  (format string at `0x64d894`), and toggles the lock/unlock icon from
  `show+0xd3`;
- empty: copies `DAT_006743c8` (the zero-filled scratch string) into the row
  text, i.e. clears it.

The hour labels are **separate widgets**: `ATTRACTION_LIST_TIME1..10` carry
`&Schedule10AM` .. `&Schedule7PM` (`ListInterface.Ini`), so the name rows
hold only the show name. An empty row is blank: `FUN_00516dd0` copies
`DAT_006743c8` (zero-filled scratch) into `ATTRACTION_LIST_NAME<n>`, while
the row's INI `TEXT = "10:00 AM"` is only the pre-refresh template. The port
matches this: `ATTRACTION_LIST_NAME`/`ITERATION_INDIC` are skipped when the
runtime text is empty, and the `TIME` widgets draw the hours
(`notes/attraction-list-empty.png`).

For an occupied row, retail appends `"%u"` of `show+0x8c` to the show name
(format string at `0x64d894`); the port keeps the name and the iteration
count in the separate `ITERATION_INDIC` cell. That is a deliberate
readability divergence.

### Port wiring

`src/ui/park.cpp` maps the same controls: `PREDEFINED_SHOW_LIST` and its
right scroll strip -> `show select <index>` / `show scroll up|down`,
`PREDEFINED_SHOW_LIST_VS_BTN_UP/DOWN` -> scroll, `PREDEFINED_SHOW_VALIDATE`
-> `show place <index>`; `ATTRACTION_LIST_LOCK/UNLOCK/ITERATION_UP/
ITERATION_DOWN/PERMUTATION/SELECTION_LIGHT<n>` -> `attraction <verb> <slot>`.
`src/viewer/main.cpp` arms `build_tool = 5` with `pending_show_index`,
converts the next ground click to record coordinates, and calls
`ShowRuntime::place`; `show+0x8c` iterations clamp to 1..10, `show+0xd3` is
the lock, `show+0x100` the hour slot (1..10).

Tests: `tests/test_campaign_cpp.py::test_cpp_campaign_show_panel_ui`,
`test_cpp_campaign_show_list_scroll`,
`test_cpp_campaign_attraction_list_actions`.
