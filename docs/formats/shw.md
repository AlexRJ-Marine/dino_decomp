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
| `Dimensions` | 2 × u32 | Arena width × height in world units (19×39, 55×55, …) |
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
- `x`/`y` use the same unit space as `Dimensions` and are always inside it.

The feature files themselves (`.bfi`) give the footprint, meshes and
textures for each index; the `.shw` only places them.

Cross-checks over all 43 shows:

- every position is inside the declared dimensions;
- the number of **cage** features (indices 0..3) equals the number of team
  slots exactly, 174 = 174, in every file;
- every show has at least one judge feature (4..10); the derby arenas place
  nine of them.

### Footprints, rotation and anchoring

The `.bfi` `width`/`height` are the feature's **footprint** in the same unit
space as the `.shw` coordinates, and the `.shw` `(x, y)` is its **min-corner
anchor**:

- rotation 0/2 keeps `(width, height)`;
- rotation 1/3 swaps to `(height, width)`.

Tiling every retail show with these footprints produces **zero overlaps**:
43/43 shows, 1,679 features, **29,500 occupied unit squares**, no shared
square. That pins the anchor, the rotation rule and the `.bfi` footprint
meaning at once.

The anchor is the **min corner**; the render transform rotates the local model
about the **footprint centre**. The feature constructor is the shared
`FUN_00586b80` (the same one used by buildings), whose jump table
`0x586d0c` handles the four orientations and whose yaw basis
(`FUN_0061ffc0` case 0) is state 0 `(1,0)`, 1 `(0,1)`, 2 `(-1,0)`,
3 `(0,-1)` - counter-clockwise 90 deg steps. The `.mesh` local bounds are
centred (e.g. `stonw.mesh` x +/-0.488, y +/-1.99 for a 1x4 beam), confirming
that the pivot is the footprint-rectangle centre rather than the min corner.

The only exceptions to the tiling are 3 **judge** features that overhang the
arena edge by one unit (they are 4x4 and placed at the last row):

| Show | Judge | Position | Arena |
| --- | --- | --- | --- |
| `Cpg2_Obst_1` | 5 JudgeObstacle | (9, 36) | 23x39 |
| `Cpg2_Obst_Mission4` | 5 JudgeObstacle | (9, 84) | 23x87 |
| `Cpg4_Derby_1` | 6 JudgeDestruction | (33, 32) | 71x35 |

Every non-judge feature fits inside the declared dimensions.

### Units (2026-10-27)

`Dimensions`, the feature `x`/`y` and the `.bfi` footprints share one unit
space, and that unit is a **world unit**, not a 4-unit grid cell:

- `FUN_0052d1e0` (`PREDEFINED_SHOW_VALIDATE`) reads the save catalog
  `params[3]`/`params[4]` (the `.shw` `Dimensions`), adds the border
  `2*[show+0x6c] + 1`, and calls `FUN_005f86d0`, which stores the pair in
  `+0x68`/`+0x6c` - the same building rectangle used by `gpkBuilding`
  (`0x5fecf0`), whose `.bbi` footprints are world units
  (`docs/platform/buildings.md`). `FUN_005fc4d0` -> `FUN_0061e9b0` then
  converts the rectangle to terrain-grid samples with the cell size.
- Park scale: the Cpg04 fences span `376..631 x 312..551` (255 x 239 world
  units) and the park's saved show record is `(420,416)-(499,451)` (about
  80 x 36 world units). A 71x35 show read as grid cells would be
  284 x 140 world units and could not fit either.
- The zero-overlap tiling check above stays valid because it only compares
  coordinates; a 4x4 `.bfi` footprint is one 4-unit build cell, not four.

The port's `place_show_at` therefore takes record coordinates 0..1024,
computes `cell = floor(record / cell_size)`, checks every covered cell, and
stores the record-space min corner. It omits the `2*[show+0x6c]+1` border
that retail adds before writing the rectangle.

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

## Edition demo shows

`Data/Edition/Show/{demo,demo8,demofinale}.shw` use an older, smaller schema:

```text
NoName
32              arena width
72              arena height
6               wall type (WallList.ini index: 2 MediumIron, 6 MediumWood)
20              feature count
<20 x 4 ints>   (FeatureList index, rotation, x, y)
```

There is no cost, cage-size or team list. All three are 32x72
with features inside the arena and indices 0..61. `read_shw` rejects them;
`tools/formats/shw.py::read_demo_shw` parses them.

The fourth value is a `WallList.ini` index (2 MediumIron, 6 MediumWood),
not a judge kind. The retail exe has no `Edition`/`demo.shw` string or loader,
so this older schema remains edition/editor content; `read_demo_shw` exposes
the value as `wall_type`.

## Save catalog relation

The save stores a 39-record catalog (see `docs/formats/sav.md`). For all 39
records the `.shw` file exists and every field matches exactly:

| Save `params[i]` | `.shw` field |
| ---: | --- |
| 0 | `Cost` |
| 1 | constant `4` |
| 2 | `Available cage size` packed as 4 bytes: `sum(cage_size[i] << (8*i))` |
| 3 | `Dimensions[0]` (width) |
| 4 | `Dimensions[1]` (height) |
| 5 | `Judge Kind` |

So the catalog is a straight runtime snapshot of the `.shw` fields, not a
separate state. Examples: `Cpg1_Race_1` ? `(1800, 4, 1, 19, 39, 1)`;
`Cpg1_Race_2` ? `(7000, 4, 1, 27, 75, 4)`.

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

- Wall style variants beyond the single `Wall type = 1` used by all 43
  retail shows (no second value exists in the data to decode).
- Runtime catalog fields `param1`/`param2`/`param5`.
- The exact `2*[show+0x6c]+1` border term (the field is read before
  `FUN_005f86d0` overwrites `+0x6c`; `[show+0x6c] == 1` matches the `.shw`
  `Wall type` in every retail file).

The rotation direction and pivot are solved: counter-clockwise 90 deg steps
around the footprint centre, via `FUN_00586b80` / jump table `0x586d0c` and
`FUN_0061ffc0`. The unit space is solved: world units, see "Units" above.

## C++ reader (port)

`src/formats/shw.{h,cpp}` parses the same sections and is exposed by
`dino_viewer --show-info <file.shw|show-dir> --asset-root <game>`. A single
file also lists every feature tuple with its `FeatureList.ini` name and the
cage/team pairs. `tests/test_viewer_cpp.py::test_cpp_show_info` checks
`Cpg1_Race_1` and the 43-show scan.

## Construction (port)

`src/world/show_runtime.{h,cpp}` resolves the active mission's `[SHOWS]`
`Shows0..9` values as `.shw` stems and exposes the catalog. The in-park
`PREDEFINED_SHOW_PANEL` fills `PREDEFINED_SHOW_LIST` with seven columns
(name, judge, price, cage 1..4; `LB_NB_COLUMNS` / `LB_COLUMN0..6_WIDTH`).
`PREDEFINED_SHOW_VALIDATE` (`0x0052d1e0`) arms the placement tool, and the
next ground click places the arena at record coordinates 0..1024. The
`ATTRACTION_LIST_PANEL` rows show the scheduled show, its iteration count
(`show+0x8c`) and lock state (`show+0xd3`). Tests:
`tests/test_campaign_cpp.py::test_cpp_campaign_show_panel_ui`,
`test_cpp_campaign_show_list_scroll`,
`test_cpp_campaign_attraction_list_actions`.
