# `.lnd` format

Status: **solved** — 10/10 island descriptors parse, 25 vegetation slots each.
Header semantics are runtime-confirmed for the Entrance and water (see below).

## Layout

```text
u32 len + NUL-terminated string  x 8:
    0  name key             ("&Isl01_Name")
    1  basemap TGA
    2  detailmap TGA
    3  heightmap RAW
    4  skybox prefix
    5  water TGA
    6  second water TGA
    7  occupation RAW
f32  water_level            (16.0 .. 26.0; runtime water surface Z)
u32  entrance_cell_x        (free-game Entrance cell, 0..1024)
u32  entrance_cell_y
u32  entrance_state         (0, 1 or 2; 90-degree orientation)
u32 len + NUL-terminated music name
25 x (u32 len + NUL-terminated vegetation .imp path or "None")
```

The file ends after the 25 vegetation strings. No trailing data.

## Inventory

| File | Water Z | Entrance cell | State | Music | Vegetation |
| --- | ---: | --- | ---: | --- | ---: |
| `isl01.lnd` | 16.0 | 520×376 | 1 | `musEquateur` | 25 |
| `isl02.lnd` | 20.0 | 312×504 | 2 | `musAtoll` | 25 |
| `isl03.lnd` | 20.0 | 704×448 | 2 | `musTempere` | 25 |
| `isl04.lnd` | 20.0 | 440×200 | 1 | `musAfricaine` | 25 |
| `isl05.lnd` | 18.0 | 480×200 | 2 | `musAtoll` | 25 |
| `isl06.lnd` | 26.0 | 628×172 | 1 | `musAfricaine` | 25 |
| `isl07.lnd` | 17.0 | 120×492 | 0 | `musTempere` | 25 |
| `isl08.lnd` | 20.0 | 548×96 | 1 | `musEquateur` | 25 |
| `isl09.lnd` | 20.0 | 268×276 | 1 | `musEquateur` | 25 |
| `isl10.lnd` | 20.0 | 488×420 | 1 | `musTempere` | 25 |

`isl01` vegetation starts with `isl01_ImpPalm1.imp` … and ends with `None`
slots. These are the landscape import/prop lists used by `WORLD-1`.

## Header semantics (runtime-confirmed)

The four header values are **not** editor metadata:

- `entrance_cell_x`/`entrance_cell_y` are the free-game Entrance cell.
- `entrance_state` (0/1/2) is the Entrance orientation (90-degree state).
- `water_level` is the water-surface Z.

Evidence: runtime `FUN_00586b80` on new free games gives isl01
`(520,376) state 1` and isl02 `(312,504) state 2`, exactly
`isl01.lnd 520/376/1` and `isl02.lnd 312/504/2`; all seven campaign
`Cpg0*_Save.wld` Entrance records match their island values. Runtime
`FUN_00602cf0` gives water Z `16.0` on isl01 and `20.0` on isl02, matching
the `.lnd` float. See `notes/calibration-2026-10-26.md` and
`notes/lnd-carry-recon.md`.

### Carry path (solved)

`FUN_0051dad0` reads the four values straight into a stack **landscape
descriptor** at `[esp+0x6c]` (frame base `B`):

| Field | Struct offset | Read |
| --- | ---: | --- |
| water level (f32) | `+0x54` | `0x51e0bd`/`0x51e0cb` |
| Entrance cell x | `+0x9c` | `0x51e0d4`/`0x51e0e2` |
| Entrance cell y | `+0xa0` | `0x51e0eb`/`0x51e0f9` |
| orientation | `+0xa4` | `0x51e102`..`0x51e125` |

The descriptor is passed to `FUN_005f3f10(&struct)` at `0x51e24a`:

- **Water:** `0x51e24a -> 0x5f3ffe -> FUN_005fb8b0`; `0x5fbde0` pushes
  `[ESI+0x54]` as the 5th argument of the water ctor `0x602b80`, which
  stores it at `+0x30`; `FUN_00602cf0` reads `+0x30` for the quad Z. The raw
  float is also copied to the terrain object `+0x38` (`0x5fbe0a`). No
  transform.
- **Entrance:** at `0x51e29b` the loader loops the 500-byte building
  definitions (`park+0x8c`, `[def+0x78] == 10`) and calls
  `FUN_0053b330(index, &cell, state)` at `0x51e309`. That ctor stores
  `+0x14 = cell.x`, `+0x18 = cell.y`, `+0x1c = index`, `+0x30 = state`
  (`0x53b364/0x53b377/0x53b37a`) and registers the object. The creation
  listener `FUN_0053d1a0` at `0x53c145` pushes `&obj+0x14` and `[obj+0x30]`
  into `0x586950 -> FUN_00589100` (AddBuilding), and `FUN_00586b80` stores
  `this[2] = cell_x`, `this[3] = cell_y`, `this[0x12] = state`.

The second `.lnd` reader `FUN_00506950` only reads the name-key string. (The
former lead about the terrain builder `0x5fb8b0` reading
`+0x24/+0x25/+0x26` was a misread: those are detail-map parameters set by
`FUN_005f3f10` to 20000 / 1.0 / 0.05.)

## Reader/API

`tools/formats/lnd.py`:

- `read_lnd(path)` → `Lnd` with named resource strings, dimensions, music and
  `vegetation`.
- `scan(root)`.

Tests: `tests/test_lnd.py` (isl01 fields, 10-file scan, truncated rejection).
