# `.shw` show scenario

Status: **solved** for the text structure; feature codes and some runtime
fields remain open. 43/43 retail files parse, 1,679 feature lines, 174 team
slots.

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
| `Wall type` | u32 | Wall style (1 in all inspected files) |
| `Number of features` | u32 | N |
| `Features` | N × 4 × u32 | Arena props; likely `(type, rotation/team, x, y)` |
| `Team assignments` | u32 | M |
| `Cage number` / `Team` | M × (u32, u32) | Cage → team mapping |

Feature tuples are four ints, e.g. `4 0 0 18`, `26 1 4 8`, `30 1 8 2`.
Types seen: 0, 4, 26, 27, 30. The second value is 0/1/2/3, the third and
fourth fit inside the arena dimensions (x/y). Exact semantics are open.

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

- Feature tuple semantics (type codes 0/4/26/27/30, second value, x/y).
- `Judge Kind` and `Wall type` behaviour.
- Runtime catalog fields `param1`/`param2`/`param5`.
- Whether the arena grid is row-major and how 19×39 maps to world units.
