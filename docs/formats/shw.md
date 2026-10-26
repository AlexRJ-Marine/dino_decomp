# `.shw` show scenario

Status: **solved** for the text structure and the feature tuples. 43/43
retail files parse, 1,679 feature lines, 174 team slots. Only some runtime
catalog fields remain open.

## Inventory

`Data/Design/Scenario/Show/*.shw` — 43 files, 442 bytes to 2,228 bytes.
They are plain Windows-1252 text with CRLF and `// Section` comments.

## Layout

All 43 files share the same sections in this order:

```text
// Name
&cpg1_race_1

// Cost
1800

// Judge Kind
1

// Available cage size
1
0
0
0

// Dimensions
19
39

// Wall type
1

// Number of features
14

// Features
4 0 0 18
0 1 12 4
...

// Team assignments
2

// Cage number
0
// Team
1
// Cage number
1
// Team
0
```

| Section | Values | Meaning |
| --- | --- | --- |
| `Name` | one string | Localisation id (`&cpg1_race_1`) |
| `Cost` | u32 | Build/entry cost (1,800 for the first race) |
| `Judge Kind` | u32 | Judge/referee variant (1 or 2) |
| `Available cage size` | 4 × u32 | Cage-size flags, e.g. `1 0 0 0` or `0 1 0 0` |
| `Dimensions` | 2 × u32 | Arena width × height in grid cells (19×39, 55×55, …) |
| `Wall type` | u32 | Wall style (1 in all 43 files) |
| `Number of features` | u32 | N |
| `Features` | N × 4 × u32 | Arena props; likely `(type, rotation/team, x, y)` |
| `Team assignments` | u32 | M |
| `Cage number` / `Team` | M × (u32, u32) | Cage → team mapping |

### Feature tuples

Each tuple is `(feature_index, rotation, x, y)`:

- `feature_index` indexes `FeatureList.ini` (62 entries, 0..61). All 1,679
  retail tuples are in range; the full set 0..61 occurs. Examples:
  `0` CageSmall, `3` CageHuge, `4` JudgeRace, `26..29` WaypointTeam1..4,
  `42` ObstacleBeam5 (the most common, 617), `50` Danse, `60/61`
  TileQuad/TileSleep.
- `rotation` is 0..3 (quarter turns). It is **not** the team: the team comes
  from the separate cage?team assignment list.
- `x`/`y` are grid cells, always `< Dimensions`.

The feature files themselves (`.bfi`) give the footprint, meshes and
textures for each index; the `.shw` only places them.

Cross-checks over all 43 shows:

- every position is inside the declared dimensions;
- the number of **cage** features (indices 0..3) equals the number of team
  slots exactly, 174 = 174, in every file;
- every show has at least one judge feature (4..10); the derby arenas place
  nine of them.

### Judge Kind and cage flags

`Judge Kind` 1..4 picks the judge feature directly:

| Judge Kind | Feature | Show family |
| ---: | ---: | --- |
| 1 | 4 `JudgeRace` | race |
| 2 | 5 `JudgeObstacle` | obstacle |
| 3 | 7 `JudgeFight` | fight |
| 4 | 6 `JudgeDestruction` | destruction |

The 39 shows split 9/15/11/8. Feature 10 `JudgeTeam` appears as an extra
marker in some obstacle/destruction shows (team variants).

`Available cage size` is exactly the set of cage features used:
`cage_size[i] == 1` iff a cage of feature index `i` (0..3 = small..huge)
appears in the feature list. This holds for all 43 files. Ten distinct flag
combinations occur, e.g. only-small, only-medium, small+high, all-small..high.

## Save catalog relation

The save stores a 39-record catalog (see `docs/formats/sav.md`). For all 39
records the `.shw` file exists and:

- `param0 == Cost` — 39/39.
- `param3/param4 == Dimensions` — 39/39.
- `param1` is `4` in all 39 records (unknown runtime field).
- `param2` is `1` or `256` and does not equal `Judge Kind`.
- `param5` is `1` or `2` and does not equal `cage_size[0]`.

So the catalog is a **runtime snapshot** of the show manager: static fields
(cost, dimensions) match the text file; the remaining two fields are state
(judge/flags/cage variant), not literal `.shw` values.

## Reader

`tools/formats/shw.py`:

```text
python tools/formats/shw.py --dump <file.shw>
python tools/formats/shw.py --scan <show-dir>
```

`read_shw()` returns name, cost, judge kind, cage size, dimensions, wall type,
features and teams; it validates `feature_count` and `team_count`.
Tests: `tests/test_shw.py` (sample + all 43 files + save correlation).

## Evidence

- `game/%MAINDIR%/Data/Design/Scenario/Show/*.shw` (read-only).
- `tools/formats/shw.py`, `tests/test_shw.py`.
- `notes/save-recon.md` — save catalog parser and params.
- `FUN_0053a400` creates the show manager with
  `DATA\design\scenario\show\`.

## Open

- `Judge Kind` and `Wall type` behaviour.
- Whether the rotation is applied clockwise or counter-clockwise and around
  which cell corner.
- Runtime catalog fields `param1`/`param2`/`param5`.
- Whether the arena grid is row-major and how 19×39 maps to world units.
