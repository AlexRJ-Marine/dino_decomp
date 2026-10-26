# Terrain orientation: engine raw rows are bottom-up

Status: **solved and ported.** The heightmap and occupation `.raw` files are
stored top-down, but both engine loaders fill their runtime buffers from the
last file row to the first. The port had been sampling the files top-down,
which put the isl01 Entrance boat (cell 520,376) on the volcano flank
(height 69.4) instead of the shoreline (18.0).

## Evidence

### Heightmap loader `FUN_005c06b0` (`mgHeightmap.cpp`)

The raw file is read one row at a time:

```text
for (i = 0; i < height; ++i)
    Read(dest = buffer + (height - 1 - i) * width, width, 1);
```

So runtime buffer row 0 is the file's **last** row. The function then builds
the 3x3-smoothed height buffer at `this+0xc` (u16, average of the 9
neighbours), which is the buffer read by `FUN_005c0b80`.

The heightmap object is created by `FUN_0061c4b0(512, 512, 2.0f)` from
`FUN_00587dd0`: `+4 = 512`, `+8 = 512`, `+0x14 = 0.5` (world-cell to sample
scale).

### Placement sampler `FUN_0061ffc0`

`FUN_0061ffc0` receives the footprint-centre cell `(cx, cy)` and samples:

```text
u = trunc(cx * 0.5)
v = trunc(cy * 0.5)
height = buffer[v * width + u] / 256
```

with bilinear interpolation. It sets the object position to
`(cx, cy, height)`; there is no second Y transform. Therefore world cell
`(520,376)` on isl01 samples runtime row `188`, which is file row
`511 - 188 = 323`.

- File `heightmap_01.raw` at `(260,323)` is 4 (bilinear 18.0 after the
  port's raw-height sampling). The Boat sits at the shoreline.
- File row 188 is the mountain: 76 raw / 69.4 bilinear. That was the port's
  old value.

### Landscape grid `FUN_0061c2d0`

The 1024x1024 landscape/occupation raw is read byte-by-byte and written to:

```text
grid[height - 1 - i][x]
```

so the occupation mask is also bottom-up. The port now flips it at load; the
`Terrain::buildable` world-cell mapping is unchanged.

### Basemap UV

The basemap TGA is top-down in `dino::Image` (`load_tga` normalises the TGA
origin). The terrain mesh uses the engine bottom-left convention, and
`build_vertices` adds 1 to V, so the correct terrain V is `-y/(H-1)`:
`1 - y/(H-1)` after the vertex shader. The old `-1 + y/(H-1)` sampled the
matching file row before the height flip; the new value keeps the basemap
aligned after the flip.

## Port

- `load_raw_u8(path, width, height, flip_rows = true)` for both the
  heightmap and the occupation mask in `main.cpp` (initial load and
  `load_island`).
- `build_terrain` writes `{u, -v}`.
- `test_cpp_building_placement_transform` now expects `z=18.0`.
- `test_cpp_occupation_mask_blocks_placement` uses engine-orientation cells
  `(158,90)` blocked / `(157,90)` free.
- Save round-trip paths are `(128,128)/(128,129)` on isl03; the budget
  construction uses `(516,288)` on isl04 (12x12 free footprint).

## Vegetation cross-check

The 1,536 isl01 `.imp` instance positions are already in engine world
coordinates and must **not** be flipped. With the flipped heightmap:

- engine orientation: **100%** of instances are on land (`height > 16`,
  min 18, mean 34.9) and 0% fall on free buildable cells (trees occupy the
  blocked/cover cells);
- file/top orientation: only 71.4% are on land and 40.6% fall on free
  buildable cells.

So the `.imp` loader stays as-is; only the heightmap and occupation raw need
the row flip.

## Verification

- `notes/entrance-fixed2.png`: the Entrance boat on the isl01 shoreline
  (wide shot; the boat is the small gold/red model in the centre).
- `notes/entrance-fixed2-boat.png`: the same frame with the boat boxed.
- `notes/entrance-fixed-close.png`: close-up of the boat at the waterline.
- `notes/isl01-start-fixed.png`: free-game start view; matches the retail
  `notes/park-after2.png` framing (green grass, palms, water top-left). The
  Entrance is outside this initial frame; it is north of the camera target.
- `notes/menu-start-entrance-fixed.png`: the actual Free Game start handoff
  (isl01, easy) after the fix.
- `notes/isl01-overhead2.png`: whole-island top-down with the basemap.
- `notes/uvtest2.png`: UV quadrant check after the `-v` fix.

The remaining boat offset was the model choice, not the terrain sampler: the
port was rendering the `.skin` bind mesh instead of the static LOD1 `.mesh`.
See `notes/entrance-model-recon.md` and
`notes/entrance-retail-reference.png`.
