# `.bwi` wall definitions

Status: **solved structurally.** 24/24 files parse, every mesh/texture path
resolves, and the build-panel switch keys are extracted. Field semantics are
partly inferred (cost/resistance/visibility).

## Inventory

`Data/Design/attractions/walls/*.bwi` — 24 files, 315–455 bytes.
`WallList.ini` in the same directory lists the count (24) and the load order:

```text
Fence, Show, MediumIron, LowIron, HugeWood, HighWood, MediumWood, LowWood,
HugePlates, HighPlates, MediumPlates, LowPlates, HugeBamb, HighBamb,
MediumBamb, LowBamb, HugeSteel, HighSteel, MediumSteel, LowSteel,
HugeElect, HighElect, MediumElect, LowElect
```

`Fence` and `Show` are special walls (paddock fence and show-cage fence);
the 20 height/material walls are `{Huge, High, Medium, Low} ×
{Wood, Plates, Bamb, Steel, Elect}`.

## Layout

Packed little-endian, **unaligned after 0x14**:

```text
0x00  u32    version            4
0x04  f32    height             2.0 / 4.0 / 6.0 / 8.0
0x08  u32    100                resistance / hit points?
0x0c  u32    cost class         10 wood, 15 bamboo/iron/steel/elect, 30 plates
0x10  u32    30                 visibility / build time?
0x14  u8     electrified        1 for the four *Elect walls, else 0
0x15  f32[7] texture scales     unaligned:
                                1.0, 0.5, 0.25, 0.6, 2.0, height/8, 0.125
0x31  string[7]                 length-prefixed (u32 byte count including NUL)
        [0] base wall texture   Data\...\buildings\<material>.tga
        [1] cracked texture     Data\...\buildings\<material>crach.tga
        [2] mud/shadow texture  ...\boue_pad.tga (low/medium iron) or boue_att.tga
        [3] ground texture      ...\ground_occup_pad.tga / ground_occup_att.tga
        [4] mesh 1              fence_paddock_lod1.mesh, fence_show_lod1.mesh, or empty
        [5] mesh 2              fence_lunchdoor.mesh, or empty
        [6] texture dir         DATA\graphics\TEXTURES\buildings, or empty
      u32    4                  constant "switch" count
      string                     material switch key (SWITCH_WALL_*)
      string                     height switch key (SWITCH_WALL_* / SWITCH_FENCE_*)
```

The first string starts at **0x31**, because the seven floats begin at 0x15
(one byte after the electrified flag). A naive 4-byte-aligned reader lands one
byte early on the first string length.

### Height and scale

`scales[5]` is exactly `height / 8` for all 20 material walls
(2→0.25, 4→0.5, 6→0.75, 8→1.0). `Fence`/`Show` use 1.0 at height 8.

`field_0c` is the **construction cost** charged by `FUN_00585d20`: the wall
manager wrapper embeds the 0xb4 wall record at `+0x10`, and the build path at
`0x0052b2dd`/`0x0052b342` passes wrapper `+0x1c` (= wall `+0x0c`) to the
CONSTRUCTION budget setter. The values 10/15/30 therefore separate wood,
bamboo/iron/steel/elect and plates.

`field_08` (100) and `field_10` (30) are uniform across all 24 walls. Their
positions and values fit **hit points** and **build time**; those two labels
are inferred, not proven by a second consumer.

### Electrified

`0x14 == 1` exactly for `LowElect`, `MediumElect`, `HighElect`, `HugeElect`.

### Switch keys

The trailing two strings are the only strings not shared with other walls:

| Wall | material key | height key |
| --- | --- | --- |
| LowWood | `SWITCH_WALL_WOOD` | `SWITCH_WALL_LOW` |
| LowIron | `SWITCH_WALL_IRON` | `SWITCH_WALL_LOW` |
| LowPlates | `SWITCH_WALL_PLATES` | `SWITCH_WALL_LOW` |
| LowBamb | `SWITCH_WALL_BAMB` | `SWITCH_FENCE_LOW` |
| LowSteel | `SWITCH_WALL_STEEL` | `SWITCH_FENCE_LOW` |
| LowElect | `SWITCH_WALL_ELECT` | `SWITCH_FENCE_LOW` |
| Fence | `SWITCH_WALL_PLATES` | `SWITCH_WALL_HUGE` |
| Show | `SWITCH_WALL_PLATES` | `SWITCH_WALL_HUGE` |

The bamb/steel/elect rows use `SWITCH_FENCE_*` for the height key while
wood/iron/plates use `SWITCH_WALL_*`; that inconsistency is in the retail
data. The names do **not** occur in any INI, `.loc` file or in the exe (no
`SWITCH_WALL_`/`SWITCH_FENCE_` prefix either), so no name lookup can resolve
them: they are **dead/unused keys**. The loader reads them into the tail of
the 0xb4-byte wall record and sets the following byte to 1, but nothing
consumes them.

## Runtime record

`0x53d9a0` is the `WallList.ini` + `.bwi` loader. It builds 0xb4-byte
records:

```text
+0x00 u32 version        +0x04 f32 height      +0x08 u32 100
+0x0c u32 cost class     +0x10 u32 30          +0x14 u8 electrified
+0x15..0x23 padding
+0x24 f32[7] texture scales
+0x40/+0x48 string 0      +0x4c/+0x54 string 1  +0x58/+0x60 string 2
+0x94 u32 4               then strings 3..6 and the two switch keys
+0xb0 u8 1
```

## Reader

`tools/formats/bwi.py`:

```text
python tools/formats/bwi.py <walls-dir>      # JSON for all 24
python tools/formats/bwi.py Fence.bwi        # one file
```

`read_bwi` validates the version, the string lengths, the 0x31 start and the
exact end of file. `scan` returns all walls in name order. Tests:
`tests/test_bwi.py` (24 files, header constants, height/material families,
electrified set, scale law, widget prefixes, every asset path exists).

## Evidence

- `Data/Design/attractions/walls/*.bwi`, `WallList.ini` (read-only).
- Ghidra: `0x53d9a0` (loader), `0x53ed30` (feature loader sharing the string
  helpers `0x5018e0`).
- `tools/formats/bwi.py`, `tests/test_bwi.py`.

## Open

- Which of the seven floats are UV scale, crack threshold, shadow strength,
  and so on.

The three header ints are understood: `0x08`/`0x10` are the uniform 100/30
(hit points/build time, inferred) and `0x0c` is the construction cost
(10/15/30), proven by the build call `0x0052b2dd` -> `0x00585d20`.
