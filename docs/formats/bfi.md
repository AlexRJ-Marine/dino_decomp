# `.bfi` show-feature definitions

Status: **header + string table solved; nested body partial.** 62/62 files
parse, the class tags and all mesh/texture paths are extracted, and every
asset path resolves. The integers after the class tag (scores, durations,
action names) are not fully mapped yet.

## Inventory

`Data/Design/attractions/features/*.bfi` — 62 files, 436–1,037 bytes.
`FeatureList.ini` in the same directory lists the count (62) and the load
order; the file stem is the feature name.

The loader is `0x53ed30`. It reads `FeatureList.ini`, opens each `.bfi`
through the shared string reader, and builds 0xb4-byte runtime records. The
class tag is compared against `StaticBait`, `AnimalBait`, `Waypoint` and
`Teleport` to set a type flag at record `+0x34`.

## Header

Packed little-endian, unaligned after 0x10:

```text
0x00  u32   footprint width     4 default; cages 8/12/16; car 2, bus 4
0x04  u32   footprint height    4 default; beam 1, bus 8, car 4
0x08  f32   vertical size       4.0 default; CageHuge 8.0; JudgeRace 12.0
0x0c  u32   variant             0 default; CageSmall..Huge 0..3
0x10  u8    flag                1 for the six odd-footprint obstacles
0x11  u32   100                 100 cages/tiles; 200/400/500 obstacles
0x15  u32   10                  build time / weight?
0x19  u32   10..1200            Cage 1200, Tile 300, bait/judge 50,
                                waypoint 10, ObstacleCar 800
0x1d  u32   0..4                flag
0x21  u32   3 default / 0       flag (0 for obstacles)
0x25  u32   0 or 100            flag
0x29  u32   0..2                flag
0x2d  string                    class tag (see below)
```

The class tag is a normal length-prefixed string at **0x2d**: the tell is the
first string length (`0x64` would be a wrong read at 0x31; the real length is
at 0x2d).

### Class tags (62 files)

| Tag | Files | Examples |
| --- | ---: | --- |
| `Tile` | 26 | TileDevour, TileHeal, TileSpeed, plus `Danse` and the trap/hole obstacles (ObstacleFire, ObstacleMine, ObstacleHole, ?) |
| *(empty)* | 10 | the large props: ObstacleBeam1..5, ObstacleBusLondon/School, ObstacleCar, ObstacleMiniCar, ObstacleRock |
| `Judge` | 6 | JudgeRace, JudgeFight, JudgeCapture |
| `AnimalBait` | 5 | AnimalBaitCow/Goat/Pig/Sheep/Ostrich |
| `StaticBait` | 5 | StaticBaitApple/Flower/Hay/Pumpkin/Watermelon |
| `Cage` | 4 | CageSmall/Medium/High/Huge |
| `Waypoint` | 4 | WaypointTeam1..4 |
| `LunchBox` | 1 | LunchBox |
| `Teleport` | 1 | Teleport |

So `Tile` is the **placement category** (a 4x4 grid tile) rather than a
mesh class: `Danse` and the traps/holes are tiles too. The empty tag is the
large multi-cell prop category. The loader only special-cases the four
bait/waypoint/teleport names.

## Body

After the class tag the file interleaves 1/4-byte integers and
length-prefixed strings. The strings are the useful part and are extracted by
scanning for length-prefixed printable strings; they fall into:

- **action / animation names** — `EatBait`, `EatBaitForSmallies`, `Danse`,
  `meuhScared`, `Jump1m_Size1..3`, `LinkObstAttackSize1..4`, `lift`,
  `courage`, `agility`, `combat`, `wood`, `feu`;
- **mesh LODs** — e.g. `obst01intact_lod1.mesh`, `obst01_lod1.mesh`,
  `judge_lod1.mesh`, `course.mesh`;
- **texture paths** — base texture, texture directory;
- **explosion / effect INIs** — `explo_1bois.ini`, `explo_2bois.ini`,
  `fin_attraction1.ini`;
- **billboard directory** — `DATA\graphics\TEXTURES\billboards\`;
- **construction switch key** — `SWITCH_FEATURE_*` (60/62 files; missing on
  `JudgeCapture` and `JudgeTeam`).

Counts across the 62 files: **57 `.mesh` paths and 49 `.tga` paths**, all
present in the retail tree.

## Reader

`tools/formats/bfi.py`:

```text
python tools/formats/bfi.py <features-dir>
python tools/formats/bfi.py CageHigh.bfi
```

`read_bfi` returns the header fields, `class_tag`, an ordered
`strings` list of `(offset, text)` and the derived `meshes`, `textures`,
`switch_key` and `identifiers`. Tests: `tests/test_bfi.py` (62 files, tag
histogram, FeatureList match, 57 meshes / 49 textures exist, 60 switch keys,
cage/obstacle footprints).

## Evidence

- `Data/Design/attractions/features/*.bfi`, `FeatureList.ini` (read-only).
- Ghidra: `0x53ed30` (feature loader; class-tag comparisons against
  `StaticBait` `0x665068`, `AnimalBait` `0x664f54`, `Waypoint` `0x66505c`,
  `Teleport` `0x665050`).
- `tools/formats/bfi.py`, `tests/test_bfi.py`.

## Open

- The integer fields after the class tag: action bindings, durations, scores,
  and the meaning of `0x11`, `0x15`, `0x19`, `0x1d`, `0x21`, `0x25`, `0x29`.
- Why `JudgeCapture` / `JudgeTeam` have no `SWITCH_FEATURE_*` key.
- Whether the odd-footprint obstacles (`ObstacleBeam5` 1x4, `ObstacleCar`
  2x4, buses 4x8) use the footprint to reserve multiple grid cells.
