# `.bfi` show-feature definitions

Status: **solved.** All 62 files parse as a strict sequential stream to an
exact EOF. The class tag only selects the construction category: the body
layout is identical for every class, so a single parser handles `Tile`,
`Cage`, `Judge`, the two bait classes, `Waypoint`, `Teleport`, `LunchBox` and
the empty-tag mesh props.

## Inventory

`Data/Design/attractions/features/*.bfi` — 62 files, 436–1,037 bytes.
`FeatureList.ini` in the same directory lists the count (62) and the load
order; the file stem is the feature name.

The loader is `0x53ed30` (called from the game-data constructor
`0x53d4d0`, which passes `param_2[2]`). It reads `FeatureList.ini`, opens
each `.bfi`, and grows a vector of 0x1d8-byte records at `gdt+0x7c`.

## Header

Packed little-endian, unaligned after 0x10:

| Offset | Type | Meaning | Observed values |
| --- | --- | --- | --- |
| 0x00 | u32 | footprint width | 4 default; cages 8/12/16; car 2, bus 4 |
| 0x04 | u32 | footprint height | 4 default; beam 1, bus 8, car 4 |
| 0x08 | f32 | vertical size | 4.0 default; CageHuge 8.0; judges 12.0 |
| 0x0c | u32 | variant | 0 default; CageSmall..Huge 0..3 |
| 0x10 | u8 | mesh-prop flag | 1 for the ten mesh props (beams, buses, cars, rock, lunchbox) |
| 0x11 | u32 | `field_11` | 100 default; 200/400/500 for large props |
| 0x15 | u32 | `field_15` | 10 default; 1/3/5 for large props |
| 0x19 | u32 | **price** | 10–2500 (bait 10–50, cages 400–2400, tiles 50–2500) |
| 0x1d | u32 | **waypoint team** | 1..4 for `WaypointTeam1..4`, 0 otherwise |
| 0x21 | u32 | **display type** | 0 mesh prop/judge, 1 cage, 3 ground tile |
| 0x25 | u32 | `field_25` | 100 only on `ObstacleMine`, else 0 |
| 0x29 | u32 | `field_29` | 0..2 |
| 0x2d | string | class tag | see below |

The class tag is an unaligned length-prefixed string at **0x2d**. The four
tagged names (`StaticBait`, `AnimalBait`, `Waypoint`, `Teleport`) only set a
boolean at record `+0x34`; they do **not** change the body shape.

### Class tags (62 files)

| Tag | Files | Examples |
| --- | ---: | --- |
| `Tile` | 26 | TileDevour, TileHeal, TileSpeed, plus `Danse` and the trap/hole obstacles (ObstacleFire, ObstacleMine, ObstacleHole, …) |
| *(empty)* | 10 | the large props: ObstacleBeam1..5, ObstacleBusLondon/School, ObstacleCar, ObstacleMiniCar, ObstacleRock |
| `Judge` | 6 | JudgeRace, JudgeObstacle, JudgeFight, JudgeDestruction, JudgeTeam, JudgeCapture |
| `AnimalBait` | 5 | AnimalBaitCow/Goat/Pig/Sheep/Ostrich |
| `StaticBait` | 5 | StaticBaitApple/Flower/Hay/Pumpkin/Watermelon |
| `Cage` | 4 | CageSmall/Medium/High/Huge |
| `Waypoint` | 4 | WaypointTeam1..4 |
| `LunchBox` | 1 | LunchBox |
| `Teleport` | 1 | Teleport |

### Static bait variants (ground tiles)

All five `StaticBait*` records share width/height 4, vertical 4.0,
`display_type 3`, `category 2`, `state_count 1`, `anim_speed 0.1`, and one
texture in mesh slot 6 (`DATA\graphics\TEXTURES\buildings\animaux.tga`); they
have no `.mesh`. The feeder fields and the construction-kit switch key are:

| Feature | `switch_key` | price | `food_kind` | `field_3c` | `field_40` | actions |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| `StaticBaitFlower` | `SWITCH_BAIT_FLOWER` | 10 | 0 | 10000 | 70 | `EatBait`, `EatBaitForSmallies` |
| `StaticBaitApple` | `SWITCH_BAIT_FRUITS` | 10 | 1 | 10000 | 70 | same |
| `StaticBaitHay` | `SWITCH_BAIT_HAY` | 30 | 4 | 10000 | 70 | same |
| `StaticBaitPumpkin` | `SWITCH_BAIT_CITROUILLE` | 50 | 3 | 10000 | 70 | same |
| `StaticBaitWatermelon` | `SWITCH_BAIT_PASTEQUE` | 40 | 2 | 10000 | 70 | same |

The retail placement is the generic `FUN_0052bbb0`/`FUN_0052b0d0` feature
path (4x4 overlap, budget gate, CONSTRUCTION charge); the 16-dish cap
`0x527150` only applies to the `LunchBox` tag. The `AnimalBait*` records are
live bait actors, not static feeders. See `notes/static-bait-recon.md`.

## Body

The body starts right after the class-tag string and is read in this exact
order (counts are u32; strings are u32 byte-length + NUL-terminated bytes):

```text
u32    judge_kind          0 = not a judge; 1 Race, 2 Obstacle, 3 Fight,
                           4 Destruction, 5 Team, 6 Capture
u32 n + n bytes              show-type list (values 1..6, same vocabulary)
u32    food_kind           .bsi/.bni kind id (0..9), 0 when not food
u32    field_3c            10000; Waypoint 50000
u32    field_40            100; StaticBait 70; Waypoint 500
u32 n + n strings            interaction action names (ActionList.ini)
u32    anim_flag           0 for cages, 1 everywhere else
f32    anim_speed          0.0 / 0.1 / 0.25
u32    state_count         1..3 visual states
8 strings                   mesh/texture slots (see below)
string                      texture directory
f32    model_scale         1.0 / 0.25
7 strings                   effect / explosion / decal .ini names
3 x u32                     field_154, field_158, field_15c
                            (0; judge third value is 8.0)
string                      billboard directory
u32 n + n x (u32 m + m strings)   attribute-weight lists (5 slots)
u32 n + n x u32             score steps (200/400/600/800/1000)
3 strings                   sound / event names
f32, f32                    duration pair (see below)
u32    category            2 default, 1 ObstacleBeam5, 4 LunchBox
string                      construction-kit switch key (SWITCH_*)
string                      trailing string (always empty)
u8     tail_flag           1 only LunchBox
```

### Field semantics

**Solved by value correlation**

- `judge_kind` — exactly the six judge classes; `.shw` judge kinds 1..4
  (`JudgeRace`/`JudgeObstacle`/`JudgeFight`/`JudgeDestruction`) match, and
  `JudgeTeam`/`JudgeCapture` extend the set to 5/6.
- `food_kind` — equals `kind_id` in `.bsi` (`Flower 0`, `Apple 1`,
  `Watermelon 2`, `Pumpkin 3`, `Hay 4`) and `food_kind` in `.bni`
  (`Ostrich 5`, `Goat 6`, `Sheep 7`, `Pig 8`, `Cow 9`). The two bait classes
  are the only features with a non-zero value.
- header `0x19` **price** — 50 for every judge, 10 for every waypoint, 70-ish
  for bait, and a strict ladder for beams (300/400/500/750) and cages
  (400/800/1200/2400). This is the construction price.
- header `0x1d` **waypoint team** — `WaypointTeam1..4` carry 1..4, all other
  files 0.
- header `0x21` **display type** — 0 for every mesh prop and judge, 1 for
  cages, 3 for every ground tile/bait/waypoint/teleport.
- `state_count` — the mesh renderer `FUN_00621770` loops `0..state_count-1`
  and builds model *i* from slots *i* and *i+3*. 1 for tiles/baits/cages/
  waypoints, 2 for judges, beams, buses and cars.

**Attribute lists and score steps**

Each of the five `attribute_sets` slots is a list of attribute keywords; a
repeated keyword encodes a weight (e.g. `ObstacleBeam4` has `agility` ×5).
Across the 62 files only slots 0, 1 and 2 are ever used. The keyword
vocabulary is `agility`, `courage`, `combat`, `protection`, `perception`,
`intelligence`, `agress`; the file spelling is inconsistent
(`Intelligence` vs `intelligence`), which is harmless because the renderer
never compares the strings.

The five `score_steps` are always `200, 400, 600, 800, 1000`.

**No runtime consumer exists.** The record stores the lists at `+0xe8` and
the steps at `+0x104` (loader `0x53ed30` -> `0x548fc0`/`0x548f20`), but no
show/judge function reads either field, and the keyword vocabulary is absent
from the executable. The actual judge score is a five-float weighted sum
(`FUN_00576550`, `FUN_00581770`) over the genotype phenotype values, with
±100/±130 clamps and 0.07 smoothing; see `docs/decomp/show-score.md`.
Treat both fields as editor metadata until a consumer is found.

**Duration pair**

The last two floats are in a fixed relation: the second is **15 ×** the
first for every feature except the two bait classes, where it is **12 ×**
(`0.8 / 9.6`). Values are 4.0/60 (judges), 5.0/75 (tiles), 6.0/90 (cages),
7.5/112.5 (BusLondon), … This is a time pair (seconds and frames at 15 fps,
12 fps for bait); treat the units as inferred.

### Mesh/texture slots

```text
slot 0  state 0 LOD1      slot 3  state 0 LOD2
slot 1  state 1 LOD1      slot 4  state 1 LOD2
slot 2  state 2 LOD1      slot 5  state 2 LOD2
slot 6  ground/effect texture (tiles, baits, cages)
slot 7  second waypoint texture
```

`ObstacleCar` uses 0,1,3,4 (intact LOD1/LOD2 + destroyed LOD1/LOD2),
judges 0,1 (two states, one LOD each), `ObstacleRock` 0,3 (one state),
`WaypointTeam*` 6,7 (two ground textures) and the tiles/baits 6.
`texture_dir` is `Data\Graphics\TEXTURES\buildings` for 22 files and empty
for 40.

The judge classes store their arena mesh in **`effects[5]`**, not in a slot:
`course.mesh` (Race), `obstacle.mesh`, `boxe.mesh`, `derby.mesh`,
`foot.mesh`, `capture.mesh`.

### Action names

The action list holds `ActionList.ini` names, but the file spelling is not
canonical: `ObstaclePiege` stores `piege` and `TileSmellbad` stores
`smellbad` while the list has `Piege`/`Smellbad`. The loader resolves names
case-insensitively. Every other action name matches exactly.

### Switch keys

`switch_key` names the construction-kit button widget, e.g.
`SWITCH_FEATURE_TILE_HEAL` is a `[section]` in
`Data/Overlay/ConstructionInterface.Ini` with `TEXTURE`, `HINT`, position
and parent group. 60/62 features have one; `JudgeCapture` and `JudgeTeam` do
not (the two judges that cannot be built from the standard judge page).
`LunchBox` uses `SWITCH_FENCE_LUNCHBOX` from the fence page.

## Reader

`tools/formats/bfi.py`:

```text
python tools/formats/bfi.py <features-dir>
python tools/formats/bfi.py CageHigh.bfi
```

`read_bfi` performs the strict sequential parse above, raises
`BfiFormatError` on overrun or leftover bytes, and exposes every field plus
the derived `meshes`, `textures`, `judge`, `switch_key` and `identifiers`.

Tests: `tests/test_bfi.py` — 62/62 exact EOF, tag histogram, FeatureList
match, 57 meshes / 49 textures exist, 60 switch keys resolve to
Construction/Infopanel sections, judge kinds, food kinds, prices, waypoint
teams, state counts, mesh-slot usage, score steps, action names and effect
files.

## Evidence

- `Data/Design/attractions/features/*.bfi`, `FeatureList.ini` (read-only).
- Ghidra `0x53ed30`: sequential read order (capstone: 73 reader calls),
  record offsets +0x0c…+0x1d5, tag comparisons against `StaticBait`
  `0x665068`, `AnimalBait` `0x664f54`, `Waypoint` `0x66505c`, `Teleport`
  `0x665050` (only sets record `+0x34`).
- `0x5475c0` record copy documents the sub-object boundaries at +0xc, +0x68,
  +0x84, +0x18c and +0x1b8; `0x54a630` confirms the +0x84 group (3 u32, 8
  strings, texture dir, float, 7 strings, 3 u32, billboard dir, two
  vectors, switch strings, flag).
- `0x621770` (`gatAttractionMeshFeature.cpp`) reads `state_count` and builds
  model *i* from slots *i* / *i+3*, and reads the texture directory.
- `0x622240` (`gatShowTileFeature.cpp`) handles the flat tile features.
- `0x585d20` construction budget setter + `0x52b2dd` confirm the wall price
  path; the `.bfi` price ladder matches the construction UI.

## Open

- The `.bfi` `attribute_sets`/`score_steps` have **no runtime consumer**
  (solved); the real show score is the 5-float weighted sum in
  `docs/decomp/show-score.md`. The per-show weight vectors at `show+0x74`
  still need an authoring source.
- `field_3c` / `field_40` (10000/50000, 100/70/500): likely hit points and
  score/price; no consumer isolated yet.
- `anim_flag` / `anim_speed` (record +0x84/+0x88): 0 for cages, 0.0/0.1/0.25
  otherwise; renderer role not isolated.
- `field_11` / `field_15` (100/10, 200/1 … 500/5): likely score and
  difficulty; pairs track the large props.
- `field_154..15c`: always zero except the judge third value 8.0.

## C++ reader (port)

`src/formats/bfi.{h,cpp}` parses all 62 files to an exact EOF (header, class tag, judge kind, category, footprint, 8 mesh slots, arena `effects[5]`, switch key). `dino_port --show-info` joins every `.shw` tuple with its BFI footprint and tiles the show: Cpg1_Race_1 is 224 cells / 0 overlaps and **43/43 shows tile with 0 overlaps** (29,500 cells). Test: `test_cpp_show_info`.
