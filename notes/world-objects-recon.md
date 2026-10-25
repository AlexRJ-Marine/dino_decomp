# World object graph (`.wld` and `.sav`)

Date: 2026-10-25
Scope: static decompilation plus retail-file validation. Read-only; no game
writes.

Status: **solved for versions 50–53.** The world-object class table, every
list layout, the save-game unlock arrays and the `.wld` simulation road block
are decoded and validated against seven campaign worlds and two retail saves.
Version 2 (`Cpg07`, cut content) is documented as legacy and not parsed.

## 1. The shared manager

`pwdWorldManager` is serialized by `0x0053c9d0` (save) and read by
`0x0053c560` (load). The wrappers are:

- save: `0x00403340` -> `0x0051e7e0` -> `0x0053c9d0`
- load: `0x00402bc0` / `0x00402f10` -> `0x0051e6c0(reader, version, 0)`
- campaign world: `0x00403e10` -> `0x0051e6c0(reader, version, 1)`

The third argument is the **campaign flag**:

| Flag | Source | Extra world data |
| ---: | --- | --- |
| 0 | save game | campaign/mission ids, objective list, visitor list, unlock arrays, show catalog |
| 1 | `.wld` world file | none of the save-only fields |

This is why `.wld` files are much smaller than saves even with the same park.

## 2. Object classes

`0x0053a140` installs seven class ids. The constructor sizes and name getters
are:

| id | class | ctor | size | name getter | name string |
| ---: | --- | --- | ---: | --- | --- |
| 0x12 | `drsDinosaur` | `0x005e8e30` | 0x11c | `0x005e8fe0` -> `0x005e8ff0` | `drsDinosaur` @ `0x0065e810` |
| 0x13 | `drsVisitor` | `0x005e8c60` | 0x03c | `0x005e8db0` -> `0x005e8dc0` | `drsVisitor` @ `0x0065e4c4` |
| 0x14 | `drsEmployee` | `0x005e8b40` | 0x034 | `0x005e8bf0` -> `0x005e8c00` | `drsEmployee` @ `0x00670070` |
| 0x15 | `drsAnimal` | `0x005e8a00` | 0x030 | `0x005e8ad0` -> `0x005e8ae0` | `drsAnimal` @ `0x00670030` |
| 0x16 | `drsShow` | `0x005e74f0` | 0x070 | `0x005e7ea0` -> `0x005e7eb0` | `drsShow` @ `0x0065e464` |
| 0x17 | `drsFence` | `0x005e87a0` | 0x05c | `0x005e8980` -> `0x005e8990` | `drsFence` @ `0x0066fff4` |
| 0x18 | `drsBuilding` | `0x005e8590` | 0x034 | `0x005e8710` -> `0x005e8720` | `drsBuilding` @ `0x0065e3ac` |

**The list determines the class, not an inline tag.** Disassembly of
`0x0053c560` shows a hardcoded class id per list:

| Stream list | Class | Evidence |
| --- | --- | --- |
| 1 | `drsFence` (0x17) | `0x53c647` `MOV [ESP+0x1c],0x17` |
| 2 | `drsShow` (0x16) | `0x53c6b1` `MOV [ESP+0x18],0x16` |
| 3 | `drsAnimal` (0x15) | `0x53c722` `MOV [ESP+0x1c],0x15` |
| 4 | `drsEmployee` (0x14) | `0x53c789` `MOV [ESP+0x18],0x14` |
| 5 | `drsBuilding` (0x18) | `0x53c7e9` `MOV EBX,0x18` |
| 6 | `drsVisitor` (0x13) | `0x53c8a7` / `0x53c908` |
| 7 | `drsDinosaur` (0x12) | later in the same function |

The inline values that happen to equal the class id in version 50–53 files
(`0x18` for buildings, `0x13` for visitors, `0x12` for dinosaurs) are
redundant tags. `drsFence` and `drsShow` write a **kind** there, so it is not
safe to use that field for class dispatch in older files.

Save-side manager list offsets (from `0x0053c9d0`): fences `+0x50`, shows
`+0x40`, animals `+0x80`, employees `+0x90`, buildings `+0x70`, visitors
`+0x60`, dinosaurs `+0x24`.

## 3. Per-class serialized layout (versions 50–53)

Common helpers:

- placed base (`drsFence`, `drsShow`): save `0x005e9620`, load `0x005e93e0`
  - `u32 object_id`
  - `u32 field_14`
  - `2 x u32 start`
  - `2 x u32 end`
  - `u32 waypoint_count`, then `waypoint_count x 0x18` bytes
  - waypoint = `u32 flags, u32 key, u32 x, u32 y, u32 z, u32 extra`
- simple base (`drsVisitor`, `drsEmployee`, `drsAnimal`, `drsDinosaur`): save
  `0x005e9820`, load `0x005e97f0`
  - `u32 object_id`
  - `u32 field_1c`

### `drsFence` (0x17)

Save `0x005e8910`, load `0x005e8860`.

```text
placed base
u32  class_tag          # 0x17 in v50-53
u32  field_14           # fence style / variant
2 x u32 start           # repeated rectangle
2 x u32 end
u32  field_44
u32  field_48           # v >= 0x25
```

Cpg04 fence 0: id 3, start `(452,312)`, end `(467,343)`, 1 waypoint,
`field_44=4`, `field_48=55425`.

### `drsShow` (0x16)

Save `0x005e7860`, load `0x005e7670`.

```text
placed base
u32 class_tag           # 0x16
u32 field_44
u32 field_48            # v >= 0x25
```

Cpg04 show 0: id 194, start `(420,416)`, end `(499,451)`, **71 waypoints**,
`field_44=195`, `field_48=63526`.

### `drsAnimal` (0x15)

Save `0x005e8aa0`, load `0x005e8a70`.

```text
simple base
u32 field_20            # only when version > 0x32 (50)
```

Cpg04 animal 0: id 263, `field_1c=264`, `field_20=0`.

### `drsEmployee` (0x14)

Save `0x005e8be0`, load `0x005e8bc0`: only the simple base.

### `drsBuilding` (0x18)

Save `0x005e86a0`, load `0x005e8610`.

```text
u32 object_id
u32 linked_id
u32 class_tag           # 0x18 in v50-53
u32 kind                # index into BuildingList.ini
u32 cell_x
u32 cell_y
u32 state
u32 link                # per-object extra u32, written by the list loop
```

Cpg01 building 0: id 1, linked 2, kind 9 = **Entrance**, cell `(520,376)`,
state 1, link 0. Cpg04 building 0: Entrance at `(440,200)`.

### `drsVisitor` (0x13)

Save `0x005e8d60`, load `0x005e8cf0`.

```text
simple base
u32 class_tag           # 0x13
u32 field_20
u32 field_24
u32 field_28            # v >= 5
```

`slot001.sav` visitor 0: id 3, `field_1c=4`, `field_20=6`, `field_24=1`,
`field_28=5`; visitor 1: id 5, `field_1c=6`, `field_20=9`, `field_24=1`,
`field_28=3`.

### `drsDinosaur` (0x12)

Save `0x005e8f80`, load `0x005e8ef0`; payload `0x00584ed0`.

```text
simple base
u32 field_24            # v >= 0x25; older versions synthesise from +0xc
u32 field_28
u32 field_2c
u32 class_tag           # 0x12
18 x (u32 allele_a, u32 allele_b)   # 18-gene genotype
```

The 18 pairs match the design `.gen` gene order exactly. Cpg04 dinosaur 0:

```text
(1,1) x10, (0,0) x2, (2,2) x2, (0,0),
(7,5) aggressiveness, (4,8) stamina, (7,4) courage
```

That is a valid genotype under `simu_trans.dic` (see `docs/formats/gen.md`).
Cpg04 has 20 dinosaurs, Cpg02/03/05 have none.

## 4. Save-only prefix and unlock arrays

When the campaign flag is 0, `0x0055d710` writes the campaign/mission ids and
`0x0053c560` reads the objective list:

```text
u32 campaign_id          # -1 in free game
u32 mission_id           # -1 in free game
u32 objective_count      # 6 in retail
objective_count x (u32 kind, u32 value, u32 target)
```

`0x005869b0` -> `0x00615bb0` + `0x00589ba0` adds one objective record; the
mission loader `0x00561bb0` fills it from the `OBJECTIVES` section. Retail
free-game saves contain:

```text
(4,30,100), (0,30,100), (2,30,100),
(9,30,100), (1,30,100), (6,30,100)
```

The unlock arrays come from `0x00543d20` (load) / `0x00543e70` (save):

| Array | Count | Record stride | Loader |
| --- | ---: | ---: | --- |
| feature flags | 62 | 0x1d8 | `0x0053ed30` (`featurelist.ini`) |
| building flags | 39 | 0x1f4 (500) | `0x0053fb00` (`buildinglist.ini`) |
| food-source flags | 10 | 0xc8 (200) | `0x005420d0` (`foodsourcelist.ini`) |
| misc flags | 6 | — | not yet named |

All retail free-game bytes are `1` (unlocked).

## 5. File headers and section offsets

### `.wld`

```text
0x00  u32 version        # 50..53 in retail
0x04  u32 mode           # 1 in all retail .wld files (only if version > 10)
0x08  world block
...   road_count + road rectangles   # simulation manager prefix
...   simulation object lists
...   f32 camera[10]                  # version >= 50
```

`0x00403e10` reads the version, then (for version > 10) one extra u32 and
ignores it. It passes campaign flag 1 to `0x0051e6c0`.

### `.sav`

```text
0x00  u32 version
0x04  u32 slot
0x08  u32 name_len
0x0c  UTF-16LE name (name_len bytes)
...   12 reserved zero bytes
...   world block (campaign flag 0)
...   show catalog
...   simulation manager
```

The world block starts at **`0x18 + name_len`**. Verified offsets:

| Save | name_len | world start | show count | show catalog |
| --- | ---: | ---: | ---: | ---: |
| `slot001.sav` | 20 | `0x2c` | `0x171` | 39 |
| `auto/auto.sav` | 2 | `0x1a` | `0x12f` | 39 |

## 6. Simulation roads in `.wld`

After the world block, `.wld` continues with the simulation manager prefix
read by `0x0058ccc0`: `u32 road_count`, then `road_count x (s32 x0, s32 y0,
s32 x1, s32 y1)`. Cpg04 has 14 rectangles, Cpg01 one `(524,392)-(524,452)`.
The rest of the simulation object lists are not decoded yet.

## 7. Validation

`tools/formats/wld.py` parses all seven version 50–53 campaign worlds:

| File | Version | Fences | Shows | Animals | Employees | Buildings | Visitors | Dinos | Roads |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Cpg01 | 51 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 1 |
| Cpg02 | 51 | 2 | 0 | 0 | 1 | 4 | 0 | 0 | 2 |
| Cpg03 | 50 | 1 | 0 | 0 | 0 | 4 | 0 | 0 | 3 |
| Cpg04 | 53 | 9 | 1 | 2 | 6 | 57 | 0 | 20 | 14 |
| Cpg05 | 52 | 2 | 0 | 0 | 2 | 8 | 0 | 0 | 3 |
| Cpg06 | 51 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 1 |
| Cpg08 | 51 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 1 |
| Cpg07 | 2 | — legacy, not parsed — | | | | | | | |

Retail saves:

| Save | World start | Buildings | Visitors | Features | Buildings | Food | Misc |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `slot001.sav` | `0x2c` | 1 Entrance | 2 | 62 x 1 | 39 x 1 | 10 x 1 | 6 x 1 |
| `auto/auto.sav` | `0x1a` | 1 Entrance | 0 | 62 x 1 | 39 x 1 | 10 x 1 | 6 x 1 |

The Cpg01 Entrance at `(520,376)` is the same building as in both saves, which
cross-checks the `.wld` and `.sav` paths.

## 8. Open

- The six misc unlock flags (`world_manager+0x60`).
- Fence/show `field_14`, `field_44`, `field_48` and waypoint field meanings.
- Dinosaur sub-handles `+0x24/+0x28/+0x2c` (four sequential registry ids).
- Visitor `field_20/24/28` (profile/state/target candidates).
- Building `state` values 0..3.
- The `.wld` simulation object lists after the roads.
- `Cpg07` version 2 layout (cut content; base uses 0x14-byte waypoint entries).

## 9. Evidence

- `0x0053a140` — world class table.
- `0x0053c560` / `0x0053c9d0` — world manager load/save.
- `0x0053c620`..`0x0053c930` — per-list class dispatch (fence/show/animal/
  employee/building/visitor/dino).
- `0x005e9620` / `0x005e93e0` — placed-object base save/load.
- `0x005e9820` / `0x005e97f0` — simple-object base save/load.
- `0x005e8910` / `0x005e8860` — fence.
- `0x005e7860` / `0x005e7670` — show.
- `0x005e86a0` / `0x005e8610` — building.
- `0x005e8d60` / `0x005e8cf0` — visitor.
- `0x005e8aa0` / `0x005e8a70` — animal.
- `0x005e8be0` / `0x005e8bc0` — employee.
- `0x005e8f80` / `0x005e8ef0`, `0x00584ed0` — dinosaur and genotype.
- `0x005869b0`, `0x00615bb0`, `0x00561bb0` — objective records.
- `0x00543d20` / `0x00543e70` — unlock arrays.
- `0x0053ed30`, `0x0053fb00`, `0x005420d0` — feature/building/food loaders.
- `0x0058ccc0` / `0x0058cd60` — simulation road rectangles.
- `0x00403e10` — `.wld` loader reads version + extra u32, campaign flag 1.

Files: `tools/formats/wld.py`, `tests/test_wld.py`, `docs/formats/wld.md`.
