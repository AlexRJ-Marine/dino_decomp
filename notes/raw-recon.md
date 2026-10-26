# Island `.raw` resources recon (2026-10-26)

Read-only analysis of the heightmap/occupation resources. No writes to
`game/` or `original/`.

## Files

```text
Graphics .../islands/isl00..isl10/heightmap_NN.raw    512x512 u8   (11)
Graphics .../islands/isl00..isl10/occupation_NN.raw   1024x1024 u8 (11)
Design   .../landscape/isl01..isl10/islNN_occupation.raw 1024x1024 u8 (10)
```

`isl01.lnd` references:

```text
heightmap   Data\graphics\TEXTURES\landscape\islands\isl01\heightmap_01.raw
occupation  Data\design\landscape\isl01\isl01_occupation.raw
basemap     .../basemap_01.tga    1024x1024
detailmap   .../detailmap_01.tga  512x512
width=520  height=376
```

## Mask semantics

The mask has only 0 and 255. Sampling the 512² heightmap at every second mask
cell (1024² -> 512²):

```text
island  0 cells  mean h   h==0     255 cells  mean h   h==0
isl01    19,477    32.0   0.00%     242,667     6.1   73%
isl02    28,349    34.2   0.00%     233,795     9.1   55%
isl03    41,917    53.6   0.00%     220,227     9.9   70%
isl04    65,357    49.6   0.00%     196,787    14.6   55%
isl05    63,089    28.6   0.00%     199,055    10.2   31%
isl06    47,334    68.3   0.00%     214,810    18.5   30%
isl07    65,201    41.7   0.00%     196,943    17.0   49%
isl08    63,251    70.1   0.00%     198,893    24.2   37%
isl09    50,942    34.5   0.00%     211,202    15.2   34%
isl10    58,232    23.9   0.00%     203,912     5.4   61%
```

So **0 = land/buildable**, **255 = water/blocked**. The visual
`notes/occupation-masks.png` shows the black mask matching the land in the
basemap for isl01 (green island), isl04 (orange interior) and isl10 (snow
land). Some land is still 255 (cliffs/ridges/vegetation), so 255 is "blocked"
rather than strictly "water".

## Design vs Graphics copies

```text
isl01 0        isl02 22        isl03 0        isl04 7104
isl05 3629     isl06 6         isl07 5        isl08 8254
isl09 6743     isl10 3234
```

All differences are <1%; `.lnd` points at the Design copy.

## `.lnd` width/height

520 x 376 does not match 512 (heightmap) or 1024 (basemap/mask). It is likely
the playable rectangle in world units/cells, not a texture size; the port
still derives the terrain grid from the 512² heightmap.

## Tooling

- `tools/formats/raw.py` — heightmap/occupation readers and `--scan`.
- `tests/test_raw.py` — sizes, binary values, 0-is-land correlation, scan.
- `docs/formats/raw.md` — spec.
- `notes/occupation-masks.png` — basemap/mask/heightmap comparison.

## Open

- What the 255 class contains (water/cliff/cover) — binary mask loses it.
- `.lnd` width/height semantics.
- Why the Graphics occupation copy differs from Design.
- `isl00` purpose (no `.lnd`).

## Port usage (2026-10-26)

The port now loads `<island>_occupation.raw` into `Terrain::occupation` and
gates path, fence and construction placement with `Terrain::buildable`.
The 1024x1024 mask maps 1:1 to the 1024-unit world (mx = world_x + 512);
0 = free, 255 = blocked. Verified on all 10 islands (the mask loads without
error) and on isl01 cell (130,94) -> mask (522,378), which is inside the
Entrance footprint and is rejected. See `docs/platform/build.md` and
`tests/test_viewer_cpp.py::test_cpp_occupation_mask_blocks_placement`.

