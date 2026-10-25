# Save format recon (`.sav`) — partial

Date: 2026-10-25
Scope: read-only analysis of saves produced by the patched retail game.

## Files

| File | Size | Notes |
| --- | ---: | --- |
| `run/save/slot001.sav` | 7,376 | manual save; name `agenttest` |
| `run/save/auto/auto.sav` | 7,116 | autosave; empty name |
| `run/save/auto/slot001.sav` | 7,116 | identical to `auto.sav` |
| `run/save/dino.dat` | 4 | `01 00 00 00` |

The save is a serialized world: a header, a **show catalog** (39 records),
then an opaque park-state object graph.

## Header (observed)

```text
0x00  u32  53              unknown; same in manual and auto saves
0x04  u32  0               slot/index (auto also 0)
0x08  u32  20              save-name byte length
0x0c  ...  UTF-16LE        "agenttest\0" (20 bytes)
0x20  ...  variable        zeros, 0xffffffff sentinels, bitsets of 0x01
...    u32  show count     (39)
```

The first 64 bytes of `slot001.sav`:

```text
35 00 00 00 00 00 00 00 14 00 00 00 61 00 67 00
65 00 6e 00 74 00 74 00 65 00 73 00 74 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 ff ff ff ff
ff ff ff ff 06 00 00 00 04 00 00 00 1e 00 00 00
64 00 00 00 00 00 00 00 1e 00 00 00 64 00 00 00
```

The header block between the name and the show count is variable (auto save
count offset 303 vs manual 369), so a reader must locate the first
`DATA\design\scenario\show\` path and read the count 8 bytes before it. This
is what `tools/formats/sav.py` does.

## Show catalog

Count: **39** in both retail saves. Record layout:

```text
u32   path byte length (NUL included)
...   ASCII path, e.g. DATA\design\scenario\show\Cpg1_Race_1.shw
u32   name byte length (NUL included, UTF-16)
...   UTF-16LE display name, e.g. The Crazy Course
6 x u32 parameters
```

First record:

```text
path   DATA\design\scenario\show\Cpg1_Race_1.shw
name   The Crazy Course
params 1800, 4, 1, 19, 39, 1
```

Last record:

```text
path   DATA\design\scenario\show\Cpg7_obst_2.shw
name   The Ultimate Race
params 42000, 4, 256, 51, 39, 2
```

The 24-byte parameter block is not decoded. It likely holds show duration,
type, state, min/max dinos, difficulty/team (values look like 1800/42000 ms,
4, 1/256 flags, 19/51, 39, 1/2).

The catalog is produced by the world manager: `FUN_0053a400` creates a show
manager with `FUN_00557a30("DATA\\design\\scenario\\show\\")`. So the save
stores the manager's list rather than parsing `.shw` files at load time.

## Park state

Starts immediately after the last show record:

| File | State offset | State bytes |
| --- | ---: | ---: |
| `slot001.sav` | 0x1101 (4,353) | 3,023 |
| `auto/auto.sav` | 0x10BF (4,287) | 2,829 |

It is a variable-length object graph, not an aligned struct. First dwords:

```text
manual: 1, 524, 392, 524, 452, 0, 0, 0, 2, 4, 524, 433, ...
auto:   1, 524, 392, 524, 452, 0, 0, 0, 0, 1, 2, 3, 100, ...
```

Plain u32 searches for 75050 (budget), 584 (09:44), 9 and 44 did not match;
money/time may be floats or derived. `0xBAADF00D` (MSVC uninitialized fill)
appears in the state, so some fields are unset/padding.

## Reader / writer candidates

- `FUN_0053a400` — `pwdWorldManager` constructor; creates the show manager
  (`FUN_00557a30("DATA\\design\\scenario\\show\\")`) and
  `FUN_0053d4d0(param_2)` (show/world data).
- `FUN_0053a140` — registers world classes with IDs `0x12`–`0x18`; likely the
  serializer class table.
- `FUN_0053a730` — `pwdWorldManager` destructor.
- Save paths: `save/`, `save/auto/`, `save/auto/auto.sav` at
  `0x0064bc48`–`0x0064bc68`, xrefs in `FUN_00402bc0`, `FUN_00402f10`,
  `FUN_00403340` (autosave/path logic).
- `*.sav` filter at `0x0065ee20`, xrefs in the options/save panel
  (`FUN_0050d2c0`, handlers around `0x0050ca..`).
- `dino.dat` at `0x006654d4`, xrefs `FUN_005612d0`, `FUN_005617a0`.

The writer/reader of the state graph has **not** been decompiled yet; the
class table registration is the place to start.

## Tooling

`tools/formats/sav.py` parses the header + catalog and exposes the state blob:

```text
python tools/formats/sav.py run/save/slot001.sav --shows
```

Tests: `tests/test_sav.py` (retail files when present, plus a synthetic save).

## Open questions

- Meaning of header `version=53`, slot, and the variable header block.
- Show parameter meanings.
- Park-state object graph (class IDs, field order, variable-length arrays).
- Whether the port needs to read retail saves or defines a new format
  (SAVE-1). Reading the catalog is already possible; the state needs the
  serializers.
