# `.sav` save format (partial)

Status: **header + show catalog solved; park-state graph open.**
The retail save is a serialized world, not a fixed struct.

## Inventory

- `run/save/slot001.sav` — 7,376 bytes, manual save.
- `run/save/auto/auto.sav` — 7,116 bytes, autosave.
- `run/save/dino.dat` — 4 bytes, `01 00 00 00`.

## Layout

```text
0x00  u32   version / flags (53 in both retail saves)
0x04  u32   slot/index (0)
0x08  u32   save-name byte length (NUL included)
0x0c  ...   UTF-16LE save name
...         variable header block (zeros, 0xffffffff sentinels, 0x01 bitsets)
...   u32   show count (39)
...         show_count x show record
...         park-state object graph (variable length)
```

The header block after the name is variable (manual 369-byte count offset vs
auto 303), so locate the first `DATA\design\scenario\show\` path and read the
count **8 bytes before it**.

## Show record

```text
u32   path byte length (NUL included)
...   ASCII path
u32   name byte length (NUL included, UTF-16)
...   UTF-16LE display name
6 x u32 parameters
```

Example:

```text
DATA\design\scenario\show\Cpg1_Race_1.shw
The Crazy Course
1800, 4, 1, 19, 39, 1
```

Last of 39:

```text
DATA\design\scenario\show\Cpg7_obst_2.shw
The Ultimate Race
42000, 4, 256, 51, 39, 2
```

Parameter meanings are open. The world manager produces this list via
`FUN_00557a30("DATA\\design\\scenario\\show\\")` in `FUN_0053a400`.

## Park state

Begins after the last show:

| File | Offset | Bytes |
| --- | ---: | ---: |
| `slot001.sav` | 0x1101 | 3,023 |
| `auto/auto.sav` | 0x10BF | 2,829 |

It is a variable-length object graph (first dwords `1, 524, 392, 524, 452, …`),
with `0xBAADF00D` uninitialized markers. Budget/time were not found as plain
u32s. The `pwdWorldManager` class registrations at `FUN_0053a140`
(IDs `0x12`–`0x18`) are the likely serializer table; the reader/writer is not
decompiled yet.

## Reader

`tools/formats/sav.py`:

```text
python tools/formats/sav.py run/save/slot001.sav --shows
```

`read_sav()` returns version, slot, name, `shows` (path/name/params), and the
state blob. Tests: `tests/test_sav.py`.

## Evidence

- `notes/save-recon.md` — raw header, catalog parse, state offsets, function
  candidates.
- `FUN_0053a400` — `pwdWorldManager` ctor; creates the show manager.
- `FUN_0053a140` / `FUN_0053a730` — class registration / destructor.
- Save paths `save/`, `save/auto/`, `save/auto/auto.sav` at
  `0x0064bc48`–`0x0064bc68`.

## Open

- Header field meanings, variable header block.
- Show parameter semantics.
- Park-state graph (class IDs, arrays, money/time encoding).
- Read retail saves in the port or define a new format (SAVE-1).
