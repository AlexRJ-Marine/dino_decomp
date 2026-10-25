# `.imp` format

Status: **solved** — 78/78 files parse, 17,059 instances, 17 distinct models.

`.imp` ("import") files place vegetation/billboard models on an island. They
are referenced from the 25 vegetation slots in `.lnd`.

## Layout

```text
u32  path_len
n    model path, NUL-terminated (a .blf file)
u32  instance_count
count x 16 bytes:
     f32 x        world X
     f32 y        world Y
     f32 scale    uniform scale
     f32 angle    Z rotation in radians
```

## Evidence

- All files end exactly after `8 + path_len + count * 16` bytes.
- Coordinates are `0 .. 1024` on **every** island. That fixed range is what
  pins the terrain grid to a 1024-unit square (the `.lnd` width/height fields
  are not the terrain extents).
- The fourth float spans `0 .. 6.28` across the corpus — radians, not degrees.
- The third float is a uniform scale: palms `0.5 .. 2.0` (mean 1.23), ferns and
  sequoias `0.8 .. 1.2` (mean 1.0).

| Island | Files | Instances | X range | Y range |
| --- | ---: | ---: | --- | --- |
| isl01 | 9 | 1,536 | 169..826 | 204..924 |
| isl02 | 7 | 1,893 | 76..943 | 55..942 |
| isl03 | 12 | 1,365 | 223..871 | 177..864 |
| isl04 | 7 | 1,511 | 133..892 | 212..875 |
| isl05 | 5 | 2,008 | 126..873 | 97..915 |
| isl06 | 7 | 1,110 | 183..844 | 278..837 |
| isl07 | 12 | 2,947 | 106..923 | 128..924 |
| isl08 | 10 | 2,176 | 147..922 | 124..938 |
| isl09 | 5 | 1,945 | 141..878 | 131..852 |
| isl10 | 4 | 568 | 301..886 | 216..919 |

Example (`isl01_ImpPalm1.imp`, 196 instances): model
`DATA\Design\Landscape\Objects\palm1.blf`; first instance
`(582.0, 777.0, 1.2198095, 3.6757219)`.

## Reader/API

`tools/formats/imp.py`: `read_imp(path)` → `Imp(model, instances)`,
`scan(root)`, `WORLD_SIZE = 1024.0`.

Tests: `tests/test_imp.py`.
