# `.lnd` format

Status: **solved** — 10/10 island descriptors parse, 25 vegetation slots each.

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
f32  unknown                (16.0 .. 26.0)
u32  width                  (heightmap/basemap width)
u32  height
u32  unknown                (0, 1 or 2)
u32 len + NUL-terminated music name
25 x (u32 len + NUL-terminated vegetation .imp path or "None")
```

The file ends after the 25 vegetation strings. No trailing data.

## Inventory

| File | Float | W×H | Unknown | Music | Vegetation |
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

The float and the 0/1/2 integer are not yet named; the float is likely a water
level or detail scale, the integer a biome/theme id. Both are recorded raw.

### Runtime use of the header values

The only full `.lnd` reader is `FUN_0051dad0` (island load, called from the
Free Game start `0x403e10`). It reads the eight resource strings, the float,
`width`, `height`, the 0/1/2 value, the music name and the 25 vegetation
strings. The four header values are read into stack scratch and **never used
again** ? the destination slots are abandoned before the music string. The
second `.lnd` reader `FUN_00506950` only reads the name-key string.

So `width`, `height`, the float and the 0/1/2 value are **editor metadata**;
the retail runtime discards them. The float is therefore not a proven water
level. (The former lead about the terrain builder `0x5fb8b0` reading
`+0x24/+0x25/+0x26` was a misread: those are detail-map parameters set by
`FUN_005f3f10` to 20000 / 1.0 / 0.05.)

## Reader/API

`tools/formats/lnd.py`:

- `read_lnd(path)` → `Lnd` with named resource strings, dimensions, music and
  `vegetation`.
- `scan(root)`.

Tests: `tests/test_lnd.py` (isl01 fields, 10-file scan, truncated rejection).
