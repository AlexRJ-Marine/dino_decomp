# `.wld` campaign world files and the shared world-object block

Status: **solved for versions 50–53.** Version 2 (`Cpg07_Save.wld`, cut
content) is legacy and not parsed.

The campaign `WorldFile` and the `.sav` save game share one serialized
`pwdWorldManager` block. `.wld` files are loaded with the **campaign flag**
set; saves load the same block with it clear, which adds the campaign prefix,
objective list, visitor list, unlock arrays and show catalog.

Implementation: `tools/formats/wld.py`
(`read_wld`, `read_world_block`, `read_sav_world`).
Evidence: `notes/world-objects-recon.md`.

## Inventory

| File | Version | Size | Mode | Fences | Shows | Buildings | Dinos | Roads |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `Cpg01_Save.wld` | 51 | 200 | 1 | 0 | 0 | 1 | 0 | 1 |
| `Cpg02_Save.wld` | 51 | 1,007 | 1 | 2 | 0 | 4 | 0 | 2 |
| `Cpg03_Save.wld` | 50 | 672 | 1 | 1 | 0 | 4 | 0 | 3 |
| `Cpg04_Save.wld` | 53 | 41,397 | 1 | 9 | 1 | 57 | 20 | 14 |
| `Cpg05_Save.wld` | 52 | 1,334 | 1 | 2 | 0 | 8 | 0 | 3 |
| `Cpg06_Save.wld` | 51 | 200 | 1 | 0 | 0 | 1 | 0 | 1 |
| `Cpg08_Save.wld` | 51 | 200 | 1 | 0 | 0 | 1 | 0 | 1 |
| `Cpg07_Save.wld` | 2 | 7,712 | — | legacy | | | | |

All seven version 50–53 files parse with zero errors. Cpg07 is cut content
and uses an older waypoint layout.

## File header

```text
0x00  u32 version        # 50..53 in retail
0x04  u32 mode           # 1 in retail; present only when version > 10
0x08  world block
...   u32 road_count
...   road_count x (s32 x0, s32 y0, s32 x1, s32 y1)
...   simulation object lists
...   f32 camera[10]                  # version >= 50 (40 bytes)
```

`0x00403e10` reads the version, then one extra u32 for version > 10 (the mode
above), and passes campaign flag 1 to `0x0051e6c0`. The mode is not otherwise
used.

## World block

The manager load is `0x0053c560`, save `0x0053c9d0`. Layout for version 50–53:

```text
# save games only (campaign flag 0)
u32 campaign_id                 # -1 in free game
u32 mission_id                  # -1 in free game
u32 objective_count
objective_count x (u32 kind, u32 value, u32 target)

u32 fence_count
fence_count x drsFence

u32 show_count
show_count x drsShow

u32 animal_count                # version > 1
animal_count x drsAnimal

u32 employee_count              # version > 2
employee_count x drsEmployee

u32 building_count
building_count x (drsBuilding, u32 link)

# save games only
u32 visitor_count
visitor_count x drsVisitor

u32 dinosaur_count              # version > 1
dinosaur_count x drsDinosaur

# save games only
u32 feature_flag_count + bytes   # 62 in retail
u32 building_flag_count + bytes  # 39 in retail
u32 food_flag_count + bytes      # 10 in retail
u32 misc_flag_count + bytes      # 6 in retail
# then the show catalog (parsed by sav.py)
```

The class is chosen by the **list**, not by an inline tag. The class ids and
constructors are installed by `0x0053a140`:

| id | class | ctor | size |
| ---: | --- | --- | ---: |
| 0x12 | `drsDinosaur` | `0x005e8e30` | 0x11c |
| 0x13 | `drsVisitor` | `0x005e8c60` | 0x03c |
| 0x14 | `drsEmployee` | `0x005e8b40` | 0x034 |
| 0x15 | `drsAnimal` | `0x005e8a00` | 0x030 |
| 0x16 | `drsShow` | `0x005e74f0` | 0x070 |
| 0x17 | `drsFence` | `0x005e87a0` | 0x05c |
| 0x18 | `drsBuilding` | `0x005e8590` | 0x034 |

## Records

### Placed base (`drsFence`, `drsShow`)

```text
u32 object_id
u32 field_14
2 x u32 start
2 x u32 end
u32 waypoint_count
waypoint_count x {
    u32 flags
    u32 key
    u32 x
    u32 y
    u32 z
    u32 extra
}                               # 0x18 bytes
```

Below version 3 the first waypoint dword is synthesised (0); below `0x25` the
second field is stored as `key - 0x1e61`.

### Simple base (`drsVisitor`, `drsEmployee`, `drsAnimal`, `drsDinosaur`)

```text
u32 object_id
u32 field_1c
```

### `drsFence` (0x17)

```text
placed base
u32 class_tag                   # 0x17 in v50-53
u32 field_14                    # fence style/variant
2 x u32 start                   # repeated rectangle
2 x u32 end
u32 field_44
u32 field_48                    # version >= 0x25
```

### `drsShow` (0x16)

```text
placed base
u32 class_tag                   # 0x16
u32 field_44
u32 field_48                    # version >= 0x25
```

### `drsAnimal` (0x15)

```text
simple base
u32 field_20                    # version > 0x32
```

### `drsEmployee` (0x14)

```text
simple base
```

### `drsBuilding` (0x18)

```text
u32 object_id
u32 linked_id
u32 class_tag                   # 0x18 in v50-53
u32 kind                        # index into BuildingList.ini
u32 cell_x
u32 cell_y
u32 state
u32 link                        # per-object extra u32
```

The `link` is written by the list loop, after the record. Kind 9 is
`Entrance`; Cpg01, `slot001.sav` and `auto.sav` all contain the same Entrance
at `(520,376)`.

### `drsVisitor` (0x13)

```text
simple base
u32 class_tag                   # 0x13
u32 field_20
u32 field_24
u32 field_28                    # version >= 5
```

### `drsDinosaur` (0x12)

```text
simple base
u32 field_24                    # version >= 0x25
u32 field_28
u32 field_2c
u32 class_tag                   # 0x12
18 x (u32 allele_a, u32 allele_b)
```

The 18 pairs are the same gene order as a design `.gen` (head/legs/body/tail,
three dummy position/type pairs, skin type/subtype, carapace type/subtype,
diet, aggressiveness, stamina, courage). Cpg04 dinosaur 0 is
`(1,1)x10, (0,0)x2, (2,2)x2, (0,0), (7,5), (4,8), (7,4)` — a valid genotype.

## Save-game prefix and unlock arrays

The campaign prefix is read by `0x0055d710` and the objective list by
`0x0053c560`. `0x005869b0` / `0x00615bb0` add an objective record; the mission
loader `0x00561bb0` fills it from the `OBJECTIVES` section. Retail free-game
saves contain six default records:

```text
(4,30,100), (0,30,100), (2,30,100),
(9,30,100), (1,30,100), (6,30,100)
```

The unlock arrays come from `0x00543d20` / `0x00543e70`:

| Array | Count | Table loader |
| --- | ---: | --- |
| feature flags | 62 | `0x0053ed30` (`featurelist.ini`) |
| building flags | 39 | `0x0053fb00` (`buildinglist.ini`) |
| food-source flags | 10 | `0x005420d0` (`foodsourcelist.ini`) |
| misc flags | 6 | not named yet |

All bytes are `1` in retail free-game saves.

## `.sav` header and world start

```text
0x00  u32 version
0x04  u32 slot
0x08  u32 name_len
0x0c  UTF-16LE name (name_len bytes)
...   12 reserved zero bytes
...   world block
```

The world block starts at **`0x18 + name_len`**:

| Save | `name_len` | World start | Show count | Show catalog |
| --- | ---: | ---: | ---: | ---: |
| `slot001.sav` | 20 | `0x2c` | `0x171` | 39 records |
| `auto/auto.sav` | 2 | `0x1a` | `0x12f` | 39 records |

`read_sav_world` verifies that the parsed block ends exactly at the show
catalog count.

## Simulation roads

After the world block, `.wld` has the simulation manager prefix read by
`0x0058ccc0`:

```text
u32 road_count
road_count x (s32 x0, s32 y0, s32 x1, s32 y1)
```

Cpg04 has 14 road rectangles; Cpg01 has one `(524,392)-(524,452)`. The
remaining simulation object lists are not decoded.

## Reader

```text
python tools/formats/wld.py --dump <file.wld>
python tools/formats/wld.py --scan <scenario dir>
python tools/formats/wld.py --sav <file.sav>
```

Returns a `WldFile` with `version`, `mode`, a `WorldBlock`, `roads`,
`simulation_offset` and `camera`.

### C++ reader

`src/formats/wld.{h,cpp}` mirrors the Python reader for versions 50–53 and
exposes the subset the port needs:

- `read_wld(path)` — campaign file: header, world block, road rectangles and
  the 40-byte camera tail. The intermediate simulation object lists between
  the roads and the camera are not decoded yet, so the reader does not require
  the roads to end at the camera (Cpg04 has ~33 KB of simulation state there).
- `read_sav_world(path)` — save file: skips the header
  (`0x18 + name_len`) and parses the same world block with the campaign flag
  clear. It stops before the show catalog count.
- `WldWorld.buildings` carries `object_id`, `linked_id`, `class_tag`, `kind`,
  `cell_x`, `cell_y`, `state` and the per-object `link`.

`dino_viewer --wld FILE --wld-info --asset-root GAME` prints the counts and
every building record without opening a window; this is the comparison oracle
for the C++ reader. Output matches the Python parser on all seven version
50–53 files and on `slot001.sav`:

```text
Cpg04_Save.wld version=53 ... fences=9 shows=1 ... buildings=57 ... dinosaurs=20 roads=14 camera=1
building: kind=9 name=Entrance x=440 y=200 state=1 obj=1 linked=2 link=0 class=24
```

Tests: `tests/test_viewer_cpp.py::test_cpp_wld_reader_matches_retail`,
`test_cpp_wld_reads_save_world`.


| Function | Purpose |
| --- | --- |
| `read_world_block(data, version, offset, campaign=...)` | parse the shared manager block |
| `read_wld(path)` | `.wld` header + world block + roads + camera |
| `read_sav_world(path)` | `.sav` world block, verified against the show catalog |

Tests: `tests/test_wld.py` (Cpg01/Cpg02/Cpg03/Cpg04/Cpg05 scans, `slot001.sav`
and `auto.sav`).

## Open

- Six misc unlock flags (`world_manager+0x60`).
- Fence/show `field_14`, `field_44`, `field_48` and waypoint fields.
- Dinosaur sub-handles `+0x24/+0x28/+0x2c`.
- Visitor `field_20/24/28`.
- Building `state` values 0..3.
- `.wld` simulation object lists after the roads.
- Version 2 (`Cpg07`) layout.
