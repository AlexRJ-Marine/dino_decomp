# `.bbi` building definition

Status: **header solved** from the retail loader `FUN_0053fb00`; asset/UI
mapping solved. 39/39 files parse, 390 mesh/skel/anim paths exist, 213
build-panel UI references (212 resolve). The numeric tail remains partial.

## Inventory

`Data/Design/buildings/*.bbi` — 39 files, 802–1,396 bytes. `BuildingList.ini`
lists them in order (SmallSeats … DecoCirc). One file per build-menu entry.

## Layout

```text
0..120   121-byte packed header:
           0   u32 footprint width  (world units)
           4   u32 footprint height (world units)
           8   f32 scale
          12   u8  flag
          13   27 x unaligned u32 (13, 17, 21, ... 117)
121..    ten length-prefixed ASCII paths:
           skin, crash LOD1, destroy LOD1, construction LOD1,
           LOD2 x4, skel, anim
...      tail: interleaved numeric bytes and length-prefixed strings:
           texture directory, explosion .ini, sounds, CONSTRUC_* UI names
```

The header is **not** 30 aligned u32: `u32, u32, f32, u8` occupy bytes 0–12,
so every later field is one byte out of phase with a 30-u32 view. The loader
`FUN_0053fb00` reads it in exactly this order, writes a 500-byte runtime record
from element `+0x0c`, and the runtime consumers use `D = element+0x0c` as the
definition base.

## Header fields

| File offset | Runtime `D+` | Type | Meaning | Entrance |
| ---: | ---: | --- | --- | ---: |
| 0 | +0x00 | u32 | footprint width, world units | 24 |
| 4 | +0x04 | u32 | footprint height, world units | 12 |
| 8 | +0x08 | f32 | scale | 1.0 |
| 12 | +0x0c | u8 | flag (1 in all retail files) | 1 |
| 13 | +0x10 | u32 | first of 27 unaligned words | 100 |
| 21 | +0x18 | u32 | **construction cost** (`0x5306e1`/`0x5307ac`) | 30 |
| 25 | +0x1c | u32 | **visitor entry price** (`descriptor+0x1c`) | 50 |
| 37 | +0x28 | f32 | **visit duration** seconds (`FUN_005219f0`) | 10.0 |
| 49 | +0x6c | u32 | **category** (0..12; 10 = Entrance) | 10 |
| 73 | +0x48 | f32 | **appeal scale** (`attraction+0x24`) | 4.0 |
| 105 | +0x68 | u32 | **radius²** (`descriptor+0x58`) | 1,024 |
| 109 | +0x70 | u32 | `model[0]`: 0 = yaw only, 1 = slope-aligned | 0 |
| 113 | +0x74 | f32 | `model[1]`: local Z offset | 0.0 |
| 117 | +0x78 | u32 | model count (2) | 2 |

Every retail file has `model[1] = 0.0f`; only `LunchDoor.bbi` has
`model[0] = 1`. The visitor scorer's price is the **definition** field
`descriptor+0x1c` = file offset 25, not a block field.

**Construction cost is file offset 21** (`D+0x18`, `header_words[2]`), not the
visitor price at 25. `pvwStateBuildingConstruction` (`0x530640`) computes the
definition base `D` from the 500-byte element array and pushes `[EBP+0x18]`
both into the affordability gate `FUN_0052bed0` (`0x5306e1`) and the
CONSTRUCTION budget setter `FUN_00585d20` (`0x5307ac`). Retail values:
Entrance 30, Deco 50, LunchDoor/Security 100, Barak/Farm/Park 500,
MediumSeats 600, LargeSeats 800, Souv/Station 2000, Rest 2500,
Tower/Manege/Tampodino/Haunted/Granderoue 5000.

The five floats at file 53/57/61/65/69 are the per-profile appeal weights,
file 73 is the appeal scale, and files 81/85 are the need-1/need-2 increments
applied while a visitor uses the building (`FUN_00522f30`). The appeal array
has one entry per visitor profile, computed from these weights and the
profile's five floats.

Footprints (world units, not grid cells): `Deco*` 4×4, `Barak*` 4×8,
`SmallSeats` 4×12, `Souv*` 8×12, `Rest*` 12×12, `Manege`/`Tampodino`/
`Haunted`/`Tower` 16×16, `Park*` 12×16, `Entrance` 24×12. Placement adds
`footprint-1` to the saved min-corner cell and centres the model
(`FUN_00586b80` / `FUN_005fecf0`); see `docs/platform/buildings.md`.

Category values (observed across the 39 files):

| Category | Buildings |
| ---: | --- |
| 0 | `Park*` |
| 1 | `Rest*` |
| 2 | `Barak*` |
| 3 | `Souv*` |
| 4 | `Manege`, `Tampodino`, `Haunted`, `Granderoue`, `Tower` |
| 5 | `PointOfView` |
| 6 | `SmallSeats`, `MediumSeats`, `LargeSeats` |
| 7 | `Security` |
| 8 | `Farm` |
| 9 | `Station` |
| 10 | `Entrance` |
| 11 | `LunchDoor` |
| 12 | `Deco*` |

`FUN_00589100` dispatches on this category; 10 wraps the base building ctor in
`FUN_00595c70`, and 9 (`Station`) has the only in-`AddBuilding` special case
(`FUN_0058d6c0`).

## Attraction block (`building+0x74`)

The visitor scorer reads a block at `building+0x74`:

```text
+0x00..0x10   five dwords copied from def D+0x34..0x44
+0x18         float array, one appeal value per park need
+0x24         f32 appeal scale  = def+0x48 (file 73)
+0x28         int              = def+0x50 (file 69)
+0x2c         int              = def+0x54 (file 73 bits)
```

`+0x18` is not stored in `.bbi`: `FUN_005872c0` fills it with
`clamp(Σ_{k<5} (def+0x34+4k) × (need[k]+0x18), −100, +100)`
(`FUN_00576550`). The four fields `def+0x34..0x44` (files 53/57/61/65/69) are
the weights; Entrance has `0, 0, 0, 0, 0`, so every appeal value is 0.

## Tail strings

- Texture directory: `Data\Graphics\TEXTURES\buildings`.
- Explosion files (`explo_1.ini`, `explo_2.ini`).
- Sound names (`fries`, `boat`, …).
- Build-panel widget names: `CONSTRUC_<FAMILY>_BUTTON`,
  `CONSTRUC_<FAMILY>_SMALL_PROFILn`, `CONSTRUC_<FAMILY>_PROFILn_GAUGE`.
  These are the sections the construction panel uses for the selected
  building's attributes.

213 UI names across the 39 files; 212 exist in the interface INIs. The one
missing is `CONSTRUC_SECUANDFOOD_SECU_BUTTON` for `Security.bbi` (probably a
legacy/renamed section; the port should fall back to the file name).

## Model paths

The runtime model record starts at element `+0x7c` (`M`). `FUN_005fecf0` →
`FUN_00603e30` loads:

| `M+` | Element | Asset | Entrance |
| ---: | ---: | --- | --- |
| +0x0c | +0x88 | primary model (`.skin`) | `entrance_lod1.skin` |
| +0x6c | +0xe8 | skeleton | `entrance_lod1.skel` |
| +0x90 | +0x10c | animation | `entrance_lod1.anim` |
| +0xb4 | +0x130 | texture directory | `Data\Graphics\TEXTURES\buildings` |
| +0x54 | +0xd0 | second model (`.mesh`) | `entrance_lod1.mesh` |

The primary `.skin` is the actor model and is centred on the placement pivot;
the second `.mesh` is a static LOD1 with a pivot offset. The ten file paths
are the union of both model records.

## Reader

`tools/formats/bbi.py`:

```text
python tools/formats/bbi.py --dump <file.bbi>
python tools/formats/bbi.py --scan <buildings-dir>
```

`Building` exposes `footprint_w`, `footprint_h`, `scale`, `flag`,
`header_words` (27 unaligned), `category`, `duration`, `price`,
`appeal_scale`, `radius_squared`, `model0`, `model1`, `model_count`, the ten
`meshes`, tail strings and UI names.

The C++ `src/formats/bbi.{h,cpp}` uses the same layout. Tests:
`tests/test_bbi.py` (39-file scan, Entrance values, header invariants, all
paths, UI resolution, BuildingList mapping).

## Evidence

- `Data/Design/buildings/*.bbi`, `BuildingList.ini` (read-only).
- Loader `FUN_0053fb00`; consumer base `D = element+0x0c`.
- `FUN_00586b80` footprint/category, `FUN_005fecf0` model paths,
  `FUN_005872c0` attraction block, `FUN_00576550` appeal values.
- `notes/building-def-record-recon.md`, `notes/building-placement-recon.md`.
- `tools/formats/bbi.py`, `tests/test_bbi.py`.

## Open

- Numeric tail (attribute profile values for the `CONSTRUC_*_PROFIL_GAUGE`
  bars) and the fields not named above.
- The missing `Security` UI section.
- `+0x28/+0x2c` are copied from float fields but read with `FILD` in the
  scorer; the intended meaning is still a retail quirk.
