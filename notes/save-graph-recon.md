# Save-state object graph recon (`.sav`) — 2026-10-25

Status: **manager order, simulation-state prefix and the AI/camera tail solved;
world object contents are still partial.**

All offsets below are relative to the **state blob returned by
`tools/formats/sav.py`**, i.e. the first byte after the 39-show catalog. That
blob starts with the `spSimuManager` section, not with `pwdWorldManager`.

## 1. Top-level section order

The save/load wrappers are:

| Direction | Wrapper | Campaign | World | Sim | AI | Scenario |
| --- | --- | --- | --- | --- | --- | --- |
| save | `FUN_0051e7e0` | `0x0055d870` | `0x0053c9d0` | `0x00586b30` | `0x0051f400` | `0x005e5940` |
| load | `FUN_0051e6c0` | `0x0055d710` / `0x0055d980` | `0x0053c560` | `0x00586ad0` | `0x0051f290` | `0x005e5920` |

The stream convention is:

- `[stream->vtable + 4]` = **write**
- `[stream->vtable + 8]` = **read**

`FUN_0055d870` writes exactly two u32 values. In both retail saves they are
`0, 0`, so the campaign section is 8 bytes.

`FUN_0053c9d0` / `FUN_0053c560` are the world manager writer/reader. The show
manager is serialized **last** inside the world section:

- `FUN_00558340` = show manager write, records of `0x2c` bytes.
- `FUN_00558230` = show manager read.
- `FUN_00557a30` = show manager constructor (`DATA\design\scenario\show\`).

This is why `sav.read_sav` locates the show catalog and treats everything after
it as the next manager.

## 2. World object class IDs

`FUN_0053a140` registers seven serializer classes:

| Class ID | Constructor | Object size | Read | Write |
| ---: | --- | ---: | --- | --- |
| `0x12` | `FUN_005e8e30` | `0x11c` | `FUN_005e8ef0` | `FUN_005e8f80` |
| `0x13` | `FUN_005e8c60` | `0x3c` | `FUN_005e8cf0` | `FUN_005e8d60` |
| `0x14` | `FUN_005e8b40` | `0x34` | — | — |
| `0x15` | `FUN_005e8a00` | `0x30` | — | — |
| `0x16` | `FUN_005e74f0` | `0x70` | — | `FUN_005e7860` |
| `0x17` | `FUN_005e87a0` | `0x5c` | — | `FUN_005e8910` |
| `0x18` | `FUN_005e8590` | `0x34` | — | `FUN_005e86a0` |

Class `0x12` contains an 18-gene genotype table (`FUN_00584ed0` /
`FUN_00585100`). The other six are world objects; their semantic names are
still open. The save writes each list as `u32 count` followed by objects that
serialize through their own `vtable + 0x4c`.

`FUN_0053c9d0` writes these world lists in order:

1. one `u8` per large (`0x1d8`-byte) object, only for non-auto saves;
2. one `u8` per `500`-byte object;
3. one `u8` per `200`-byte object;
4. a fixed 6-byte flag block;
5. lists at manager `+0x50`, `+0x40`, `+0x80`, `+0x90`, `+0x70`, (`+0x60`
   manual only), `+0x24`;
6. the show manager.

This is enough to segment the world section but not yet enough to name every
object list.

## 3. `spSimuManager` section

`sav.read_sav().state` starts here. Version 53 layout, verified on
`slot001.sav` and `auto/auto.sav`:

```text
u32 road_count
road_count x (s32 x0, s32 y0, s32 x1, s32 y1)

u32 list0_count              # 0 in both retail saves
u32 list1_count              # 0
u32 list2_count              # 0

u32 visitor_count
visitor_count x 0x50-byte visitor record

u32 park_count               # 1
park_count x 0x293-byte park block

u32 dino_store_count         # 20
dino_store_count x record:
    u32 name_bytes (UTF-16LE, NUL included)
    UTF-16LE name
    u32 store_id / price
    u32 path_bytes (ASCII, NUL included)
    ASCII path
    u32 description_bytes (ASCII, NUL included)
    ASCII description

u32 visitor_model[5]         # 3,3,3,3,3 in both retail saves
u32 lab_count                # 8
lab_count x u8 present_flag  # 1 = empty; 0 = variable genotype data
u32 ai_counts[5]             # AI lists; fifth has the entries
if non-auto: entries x 17 bytes
f32 camera[10]                 # camSwitcher state
```

Offsets:

| File | road | visitors | park count | park block | dino store | lab | AI + camera |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `slot001.sav` | `0x0000` | `0x0020` (2) | `0x00c4` | `0x00c8` | `0x035b` | `0x0b65` | `0x0b71` (94 B) |
| `auto/auto.sav` | `0x0000` | `0x0020` (0) | `0x0024` | `0x0028` | `0x02bb` | `0x0ac5` | `0x0ad1` (60 B) |

The park-block starts are exactly `visitors_end + 4`; the dino store starts
exactly `park_block + 0x293`; the store ends exactly where `visitor_model`
begins. This is strong evidence for the layout.

### Road network

`FUN_0058ccc0` writes/reads a count and `count x 4 u32`. In both saves there is
one rectangle: `(524, 392) - (524, 452)`. It is the park road/entrance network.

### Visitor record (`0x50` bytes)

Observed layout:

```text
+0x00 u32 object id         4 / 6 in the manual save
+0x04 s32 tile x            524
+0x08 s32 tile y            433 / 393
+0x0c u32 flags             0x00010001
+0x10 u32 unknown           0xbaadf00d when unset
+0x14 u32 state             1
+0x18 f32 x                 524.317 / 524.080
+0x1c f32 y                 432.577 / 392.639
+0x20 f32 dir x            -0.0206 / -0.1249
+0x24 f32 dir y             0.9998 / -0.9922
+0x28 u32 unknown
+0x2c u32 zero
+0x30 u32 visitor id         38 / 39
+0x34 u32 needs[7]           97,96,99,... / 99,98,99,...
```

The first dword is the object registry ID, not the personality profile. The
seven `needs` are the visitor satisfaction/status values. The exact need names
are still open.

### Park block (`0x293` bytes)

The first `0x28` bytes are an object header; `FUN_00585240` (park read) starts
its fields at `block + 0x28`. The budget is the u32 at `block + 0x30`
(relative `+0x08`).

| Relative to `block + 0x28` | Meaning (verified/observed) |
| ---: | --- |
| `+0x08` | **budget** (75,125 manual, 75,000 auto) |
| `+0x0c` | float `1.0` |
| `+0x10` | 0 |
| `+0x14` | 0 |
| `+0x18` | 0 |
| `+0x1c` | 0 |
| `+0x20` | 125 manual / 0 auto; total income or visitor count |
| `+0x24` | 13 x u32; first is 2 manual / 0 auto |
| `+0x58` | 13 x u32; all 0 |
| `+0x94`/`+0x95` | two version-gated u8 fields (0 in retail) |
| `+0x98` | version > 52 field, float/0 |

`FUN_00585bd0` is the setter used by the park constructor: it writes the same
`+0x08` budget and `+0x98` float, which confirms the field mapping.

### Dinosaur store

`FUN_00593a60` reads the store. The record layout was verified across all 20
entries in both saves:

```text
Allosaurus         9000   DATA\Design\Scenario\Genotype\allo.gen
Ankylosaurus       5500   DATA\Design\Scenario\Genotype\anky.gen
Brachiosaurus     42000   DATA\Design\Scenario\Genotype\brachio.gen
...
Velociraptor      15000   DATA\Design\Scenario\Genotype\velo.gen
```

The `store_id` values look like purchase prices, but that is still a
hypothesis. The `.gen` path and description are stored as strings; the
18-gene table itself is **not** in this record (it is loaded from the `.gen`
file when the record is read).

### Lab store

`FUN_00592060` reads five u32 (`3,3,3,3,3` in both saves), then
`FUN_00592a00` reads `lab_count = 8` and one presence byte per record. All
eight flags are `1` (empty) in both saves. A zero flag would be followed by a
variable genotype record; that path is not decoded yet.

### AI manager + camera tail

The bytes after the lab flags are:

```text
u32 ai_list_count[5]        # first four are 0 in retail
if non-auto:
    ai_list_count[4] x 17-byte AI entries
f32 camera[10]              # 40 bytes
```

- manual: `(0,0,0,0,2)`, two 17-byte AI entries, then camera
  `(550.998, 512, 84.696, 512, 512, 32, 0.1, 0, 0, 0)`.
- auto: `(0,0,0,0,0)`, no AI entries, camera
  `(128, 128, 250, 512, 512, 50, 0.1, 0, 0, 0)`.

The manager functions are `FUN_0051f290` (AI load) / `FUN_0051f400` (AI save).
The camera block is the `camSwitcher` save (`FUN_005e5b80` / `0x5e5ba0` →
`camSwitcher.cpp`); the ten floats are the camera position/target/up/FOV
family. The exact field split is still open, but the byte layout is fixed.

## 4. Tool

`tools/formats/sav_graph.py` parses the sections above:

```text
python tools/formats/sav_graph.py run/save/slot001.sav --visitors --dinos
```

It returns:

- `road`: the road/entrance rectangles;
- `visitors`: parsed `0x50` records;
- `park`: budget and park fields;
- `dino_store`: 20 name/id/path/description records;
- `visitor_model` and `lab_flags`;
- `ai_counts`, `ai_entries` and the 10-float `camera` block;
- `tail_extra` (empty in both retail saves).

Tests: `tests/test_sav_graph.py` (retail manual + auto saves, synthetic save,
rejection of unknown object lists, rejection of populated lab records).

## 5. Open

- World object list semantics and the `0x12`–`0x18` class names.
- Park fields beyond budget (`+0x20`, `+0x24`, `+0x58`) and the `0x28`-byte
  block header.
- Whether the dinosaur `store_id` is a price or an ID.
- The semantic split of the 10 camera floats (position/target/up/FOV).
- Populated lab genotype records (flag `0`).
- The variable campaign/world header before the show catalog.

## Evidence

- Ghidra decompilation: `FUN_0051e6c0`, `FUN_0051e7e0`, `FUN_0053a140`,
  `FUN_0053c560`, `FUN_0053c9d0`, `FUN_00558340`, `FUN_00558230`,
  `FUN_00585240`, `FUN_00585bd0`, `FUN_0058aed0`, `FUN_0058ccc0`,
  `FUN_00592060`, `FUN_00592a00`, `FUN_00593a60`, `FUN_0051f290`,
  `FUN_0051f400`, `FUN_005e5b80`, `FUN_005e5ba0`, `FUN_005e5920`,
  `FUN_005e5940`, `FUN_005e8e30` … `FUN_005e8f80`.
- Retail files: `run/save/slot001.sav`, `run/save/auto/auto.sav`.
- `tools/formats/sav.py` (header + show catalog), `sav_graph.py` (new).

## Correction (2026-10-26): park block is a sequential stream

The park block is **not** a raw object dump at runtime offsets. `0x00585780`
writes a 0x30-byte block header followed by a sequential stream in this order:

```
+0x08 budget, +0x98 sellback, +0x0c, +0x10, +0x14, +0x18, +0x8c, +0x90,
+0x94/95 (u8), +0x1c x2, +0x24 x13, +0x58 x13, +0x9c..+0xb8,
+0xbc x2, +0xc4..+0xd0, +0xd4 x13, +0x108 x13, +0x13c..+0x16c,
+0x170 x2, +0x178..+0x184, +0x188 x13, +0x1bc x13, +0x1f0..+0x220
```

The budget is the first stream dword (`block + 0x30`). The manual save's
`125/2` pair is the current-day ticket income (stream `+0x18/+0x1c`), not
`+0x20/+0x24`. The three computed totals (stream `+0x12a/+0x12e/+0x132`)
contain `0xBAADF00D` in a fresh save. `sav_graph._read_park_stream` and
`tools/formats/economy.py` now use this order; see `docs/formats/sav.md`.
