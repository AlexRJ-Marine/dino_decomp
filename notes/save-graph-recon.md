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
    u32 price
    u32 path_bytes (ASCII, NUL included)
    ASCII path
    u32 description_bytes (ASCII, NUL included)
    ASCII description

u32 visitor_model[5]         # 3,3,3,3,3 in both retail saves
u32 lab_count                # 8
lab_count x record:
    u8 present_flag          # 1 = empty; 0 = genotype + name follow
    if present_flag == 0:
        18 x (u32 allele_a, u32 allele_b)  # 0x90 bytes
        u32 name_bytes (ASCII, NUL included)
        ASCII name
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

The price field is proven: all 20 names, prices and `.gen` paths match
`Data/Design/Scenario/FreeGame.ini` `[DINOSTORE]` in file order, and those
prices also appear in `Market.ini`. The `.gen` path and description are stored
as strings; the 18-gene table itself is **not** in this record (it is loaded
from the `.gen` file when the record is read).

### Lab store

`FUN_00592060` reads five u32 (`3,3,3,3,3` in both saves), then
`FUN_00592a00` reads `lab_count = 8` and one record per slot. The record byte
is a presence flag: `1` = empty, `0` = populated. All eight flags are `1`
(empty) in both retail saves, so the populated path is covered by a synthetic
test.

A populated slot (`flag == 0`) is:

```text
u8   flag
18 x (u32 allele_a, u32 allele_b)   # 0x90 bytes
u32  name_bytes                     # string length + 1
...  ASCII name, NUL included
```

Evidence: the writer `FUN_00592b20` writes the flag, calls the genotype writer
`FUN_00585100` (18 iterations, two u32 each), then writes
`string.length + 1` and the bytes. The reader `FUN_00592a00` mirrors it:
flag, `FUN_00584ed0` (18 iterations, two u32 each for version > 21), then the
u32 length and bytes. Runtime lab records are `0xe8` bytes (`flag` at `+0`,
string object at `+4`, genotype at `+0x10`); the file stores only the flag,
the two alleles per gene and the name. The genotype is the same 18-gene order
as `.gen` (`docs/formats/gen.md`).

### AI manager + camera tail

The bytes after the lab records are:

```text
u32 ai_list_count[5]        # first four are 0 in retail
if non-auto:
    ai_list_count[4] x 17-byte AI entries
f32 camera_eye[3]          # 0..2
f32 camera_target[3]       # 3..5
f32 camera_progress        # 6, interpolation progress
f32 camera_reserved[3]     # 7..9, zero in retail
```

- manual: `(0,0,0,0,2)`, two 17-byte AI entries, then eye
  `(550.998, 512, 84.696)`, target `(512, 512, 32)`, progress `0.1`.
- auto: `(0,0,0,0,0)`, no AI entries, eye `(128, 128, 250)`, target
  `(512, 512, 50)`, progress `0.1`.

The manager functions are `FUN_0051f290` (AI load) / `FUN_0051f400` (AI save).
The camera block is the `camSwitcher` save (`FUN_005e5b80` / `0x5e5ba0` →
`camSwitcher.cpp`). The manual eye/target match the verified start-camera
framing; the last three floats are zero in both retail saves.

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
populated lab record round-trip, rejection of unknown object lists, rejection
of implausible lab name lengths).

## 5. Open

- World object list semantics and the `0x12`–`0x18` class names.
- Park fields beyond budget (`+0x20`, `+0x24`, `+0x58`) and the 0x30-byte
  block header. The reader `FUN_0058aed0` consumes the leading `2` as a
  **record count** and then a 4-byte type id + payload per record, so the
  `class id` label is provisional; the record type ids (manual 0, autosave 3)
  are unnamed.
- The 17-byte AI-entry field split.
- The variable campaign/world header before the show catalog.

## Evidence

- Ghidra decompilation: `FUN_0051e6c0`, `FUN_0051e7e0`, `FUN_0053a140`,
  `FUN_0053c560`, `FUN_0053c9d0`, `FUN_00558340`, `FUN_00558230`,
  `FUN_00585240`, `FUN_00585bd0`, `FUN_0058aed0`, `FUN_0058ccc0`,
  `FUN_00592060`, `FUN_00592a00`, `FUN_00592b20`, `FUN_00584ed0`,
  `FUN_00585100`, `FUN_00593a60`, `FUN_0051f290`,
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
`+0x20/+0x24`. The stream is 0x21a bytes; the three computed totals are the
2nd..4th dwords of the 13-dword totals group (stream `+0x132/+0x136/+0x13a`,
and again in the cumulative block at `+0x1e6/+0x1ea/+0x1ee`). They contain
`0xBAADF00D` in a fresh save.

**Correction (2026-10-26b): the "second block" is the cumulative block.**
`0x00585240` is version-gated at `0x0058560b` (`cmp [esp+0x18],0x2e`). For
version >= 46 it reads 180 bytes into `+0xbc..+0x16c`, but `0x00585780`
writes those bytes from `+0x170..+0x220`. It also reads only 5 of the 13
first-block total dwords before starting the second block, so the retail
loader is 32 bytes out of phase. The parser follows the writer layout and
exposes `yesterday_*` and `cumulative_*` separately; the loader bug is not
reproduced in the port. The remaining 0x49 bytes of the park block are the
schedule-manager tail (0x3c bytes), a 12-byte field and one trailing byte.
`sav_graph._read_park_stream` and `tools/formats/economy.py` use this order;
see `docs/formats/sav.md`.

### Park header and schedule tail (2026-10-26c)

`FUN_005874a0` (`spPark` ctor) and `FUN_0058b200` (`spPark::Save`, called by
`spSimuManager::Save` at `0x00586b30`) give the runtime offsets that the block
header mirrors:

| Block | Bytes | Manual | Auto | Notes |
| --- | --- | --- | --- | --- |
| `+0x00` | u32 | 2 | 2 | class id (`spPark`) |
| `+0x04` | u32 | 0 | 3 | unknown; differs between manual and autosave |
| `+0x08` | u32 | 100 | 100 | unknown, constant in both |
| `+0x0c` | f32 | 10.0 | 0.0 | unknown |
| `+0x10` | 32 B | 0 | 0 | reserved/zero |

The schedule tail starts at `block + 0x24a`. `0x00594cb0` (called from
`spPark::Save`) writes it in two parts:

- `0x0058c260` writes the time manager's `+4/+8/+0xc` dwords (12 bytes). In
  the file these are day `0`, elapsed `23.214` and current `23.214` in the
  manual save; all zero in the autosave.
- `0x0058c6b0` writes the task vector's `+0x18` field (4 bytes) and then one
  `u32` per task (`task + 0x70`). The vector length is not serialised; the
  reader `0x0058c620` reuses its constructor length. Retail saves have 11
  zero slots.

The block then ends with `0x0058b200`'s final write: 12 bytes from
`spPark+0x148`. The ctor zeroes bytes `+0x148/+0x149` and dwords
`+0x14c/+0x150`, which leaves `+0x14a/+0x14b` = `0xBAAD` debug fill; the file
shows `00 00 ad ba 00 00 00 00 00 00 00 00` at `block + 0x286`. One byte
(`0x01`) remains at `block + 0x292`. No writer for it has been identified:
the decompiled `0x0058b200` ends with the 12-byte write, so it is documented
as an unexplained trailing byte rather than guessed.

Verified offsets (both retail saves): economy stream `0x30..0x249`, schedule
`0x24a..0x285`, park field `0x286..0x291`, trailing byte `0x292`, next dword
(`0x293`) = dino store count `20` followed by a 22-byte UTF-16 name
(`Allosaurus`), which pins the block size.

