# `.bwi` wall definitions recon (2026-10-26)

Read-only analysis of `Data/Design/attractions/walls`. No writes to `game/`
or `original/`.

## Inventory

24 files, 315–455 bytes, one per `WallList.ini` line (after the leading
count 24). `Fence` and `Show` are the paddock/show fences; the other 20 are
`{Huge, High, Medium, Low} x {Wood, Plates, Bamb, Steel, Elect}`.

## Struct

The files are packed little-endian. The first 0x15 bytes are aligned, then
the seven floats start at 0x15 (so everything after is unaligned):

```
0x00 u32 version            4
0x04 f32 height             2/4/6/8
0x08 u32 100
0x0c u32 cost class         10 wood / 15 others / 30 plates
0x10 u32 30
0x14 u8  electrified
0x15 f32 x7                 1.0, 0.5, 0.25, 0.6, 2.0, height/8, 0.125
0x31 string x7              u32 len (incl NUL) + bytes
      u32 4
      string                material switch key
      string                height switch key
```

The first string length sits at 0x31: a 4-byte-aligned reader looks at 0x32
and reads 0x2d00 = 11520, which is the tell that the float block is
unaligned.

## String table

All 24 files have exactly seven leading strings:

1. base texture: `Data\Graphics\Textures\buildings\wall_NN|bamb|steel|elect.tga`
2. cracked texture: `...crach...`
3. mud/shadow: `boue_pad.tga` (low/medium iron) or `boue_att.tga`
4. ground: `ground_occup_pad.tga` / `ground_occup_att.tga`
5. mesh 1: `fence_paddock_lod1.mesh` (Fence), `fence_show_lod1.mesh` (Show), empty otherwise
6. mesh 2: `fence_lunchdoor.mesh` (Fence), empty otherwise
7. texture dir: `DATA\graphics\TEXTURES\buildings`, empty for most walls

All non-empty paths exist in the retail tree (checked by the test).

## Height / material laws

- `height` 2/4/6/8 for Low/Medium/High/Huge.
- `scales[5] == height/8` for all material walls.
- `field_0c`: wood 10, bamboo/iron/steel/elect 15, plates 30.
- `0x14 == 1` exactly for the four `*Elect` files.

## Switch keys

The final two strings are unique per wall:

```
SWITCH_WALL_<MATERIAL>   SWITCH_WALL_<HEIGHT> or SWITCH_FENCE_<HEIGHT>
```

Bamb/Steel/Elect use `SWITCH_FENCE_*` while Wood/Iron/Plates use
`SWITCH_WALL_*`. The strings (and the `SWITCH_WALL_`/`SWITCH_FENCE_` prefixes)
occur nowhere else in the game tree and are not in the exe, so no lookup can
resolve them: **dead/unused keys**. The loader `0x53d9a0` stores them at the
end of the 0xb4-byte wall record and sets the next byte to 1.

## Loader

`FUN_0053d9a0` reads `WallList.ini`, builds 0xb4-byte records and reads:

```
+0x00..+0x10 five u32/f32
+0x14        u8 flag
+0x24        f32 x7
+0x40/+0x48, +0x4c/+0x54, +0x58/+0x60   first three strings
+0x94        u32 4
             four more strings, then the two switch keys
+0xb0        u8 1
```

This matches the file layout exactly (record stride 0xb4).

## Tooling

- `tools/formats/bwi.py` — reader; validates version, strings, start 0x31,
  exact EOF.
- `tests/test_bwi.py` — 24 files, header constants, families, electrified
  set, scale law, widget prefixes, asset existence.
- `docs/formats/bwi.md` — spec.

## Open

- The seven floats: `scale[5] == height / 8` is the vertical tiling; the
  other six are constant across all 24 walls and are default UV/tiling
  parameters (exact layer mapping not distinguishable from retail data).

## Field semantics (2026-10-26)

`field_0c` is the construction cost. The build path at `0x0052b2dd` loads
`[EDI+0x1c]` and calls `FUN_00585d20` (the CONSTRUCTION budget setter). `EDI`
is the wall-manager wrapper (stride 0x1d8, wall record embedded at `+0x10`),
so wrapper `+0x1c` is the wall's `+0x0c`: the 10/15/30 cost class. `field_08`
(100) and `field_10` (30) are uniform and fit hit points/build time; those two
labels are inferred.
