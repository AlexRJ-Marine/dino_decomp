# Island `.raw` heightmap and occupation mask

Status: **solved** for the two raw resources; the `.lnd` width/height fields
and the exact blocked-cell rules are still open.

## Inventory

| Path | Files | Size | Shape |
| --- | ---: | ---: | --- |
| `Data/Graphics/Textures/landscape/islands/islNN/heightmap_NN.raw` | 11 (isl00–isl10) | 262,144 B | 512 × 512 u8 |
| `Data/Graphics/Textures/landscape/islands/islNN/occupation_NN.raw` | 11 | 1,048,576 B | 1024 × 1024 u8 |
| `Data/Design/landscape/islNN/islNN_occupation.raw` | 10 (isl01–isl10) | 1,048,576 B | 1024 × 1024 u8 |

The `.lnd` header points at the Graphics heightmap and the Design occupation
mask:

```text
heightmap   Data\graphics\TEXTURES\landscape\islands\isl01\heightmap_01.raw
occupation  Data\design\landscape\isl01\isl01_occupation.raw
basemap     Data\graphics\TEXTURES\landscape\islands\isl01\basemap_01.tga   (1024×1024)
detailmap   Data\graphics\TEXTURES\landscape\islands\isl01\detailmap_01.tga (512×512)
```

`isl00` is present in Graphics but has no `.lnd`/Design counterpart (template
or unused slot).

## Heightmap

512 × 512 unsigned bytes, one sample per cell, square. The port maps it onto
a 1024-unit terrain (2 world units per sample). Values 0..255; the island
height range differs per map.

**Runtime row order is bottom-up.** `mgHeightmap::Load` (`FUN_005c06b0`)
reads the file rows from last to first:

```text
for (i = 0; i < height; ++i)
    Read(buffer + (height - 1 - i) * width, width, 1);
```

so runtime buffer row 0 is the file's last row. `FUN_0061ffc0` samples the
runtime buffer with `row = trunc(cell_y * 0.5)`; it does not flip again.
The port's `load_raw_u8(..., flip_rows=true)` mirrors this. See
`notes/terrain-orientation-recon.md` (isl01 Entrance: file row 323 /
shoreline, not row 188 / volcano). The occupation mask uses the same
bottom-up order (`FUN_0061c2d0` writes `grid[height - 1 - i]`).

## Occupation mask

1024 × 1024 unsigned bytes, one byte per **world unit**. Only two values
occur: `0` and `255`.

- **0 = buildable/free land.**
- **255 = blocked** (water, cliffs, dense vegetation/cover).

Evidence:

- The mask's black (0) region matches the island land in the basemap for all
  checked islands (`notes/occupation-masks.png`: isl01, isl04, isl10).
- Height correlation: 0 cells have a higher mean height than 255 cells on
  every island, and **no 0 cell has height 0** (water). Example isl01:
  0 cells mean 32.0, 255 cells mean 6.1, with 73% of 255 cells at height 0.
- isl10: 0 cells are the snow land masses (white in the basemap); 255 cells
  are the dark water/rock.

The mask is therefore the natural build/path validity layer for the port:
reject a cell when `occupation[x][y] == 255`.

## Design vs Graphics copies

The Graphics `occupation_NN.raw` is a near-duplicate of the Design
`islNN_occupation.raw`:

| Island | Differing bytes | Fraction |
| --- | ---: | ---: |
| isl01 | 0 | 0 |
| isl02 | 22 | 0.00002 |
| isl03 | 0 | 0 |
| isl04 | 7,104 | 0.0068 |
| isl05 | 3,629 | 0.0035 |
| isl06 | 6 | 0.000006 |
| isl07 | 5 | 0.000005 |
| isl08 | 8,254 | 0.0079 |
| isl09 | 6,743 | 0.0064 |
| isl10 | 3,234 | 0.0031 |

The `.lnd` reference points at the **Design** copy; use that.

## `.lnd` width/height

`isl01.lnd` reports `width=520`, `height=376` — neither 512 (heightmap) nor
1024 (basemap/mask). It is probably the playable rectangle in world units or
grid cells, but it is not the texture size. The port currently derives the
terrain size from the heightmap (512) and the world size 1024.

## Reader

`tools/formats/raw.py`:

```text
python tools/formats/raw.py --heightmap <heightmap_NN.raw>
python tools/formats/raw.py --occupation <islNN_occupation.raw>
python tools/formats/raw.py --scan <game-root>
```

`read_heightmap` returns a square `Heightmap`; `read_occupation` validates a
1024² binary mask and returns `Occupation` with `at(x, y)`,
`is_buildable(x, y)` and `buildable`. `scan` reports all 10 islands plus the
mean height of buildable vs blocked cells. Tests: `tests/test_raw.py`
(sizes, binary values, 0-is-land correlation, 10-island scan).

## Evidence

- `Data/Design/landscape/islNN/islNN_occupation.raw` (read-only).
- `Data/Graphics/Textures/landscape/islands/islNN/*.raw`, `*.tga`.
- `Data/Design/Landscape/islNN/islNN.lnd` resource strings.
- `notes/occupation-masks.png` (basemap | mask | heightmap).
- `tools/formats/raw.py`, `tests/test_raw.py`.

## Open

- What exactly makes a 255 cell (water vs cliff vs vegetation) — the mask is
  binary, so the distinction is lost.
- The `.lnd` width/height meaning and its relation to the 1024-unit world.
- The purpose of the Graphics occupation copy (nearly identical to Design).
- `isl00`'s role.
