# Campaign `list0` (saDinosaur) simulation record — tail recon

Status: **solved**. The populated `list0` records of a campaign autosave are
`saDinosaur` objects serialized by `0x00570c90` / read by `0x005706c0`, invoked
from the simulation manager through the class vtable `0x00643124`
(`vtable+0x14` = save, `vtable+0x10` = load). The tail is **not** opaque: every
field is schema-driven, and the only variable parts are the record name, a
second (usually empty) ASCII string and a `u32`-counted path-node vector.

Sample: `build/campaign-autosave.sav` (version 53, 20 records, 53,338 bytes).

## 1. Manager chain and addresses

| Role | Address | Notes |
| --- | --- | --- |
| Sim manager save | `0x00586b30` | `FUN_0058b200` (roads+lists), `0x00593e10` (park), `0x005920f0` (store/lab) |
| Roads + lists save | `0x0058b200` | roads `0x0058cd60`; `list0` vector at `this+0x68..0x6c` |
| Sim manager load | `0x00586ad0` | `0x0058aed0` when version > 1, `0x0058ccc0` for version 1 |
| Roads + lists load | `0x0058aed0` | roads first (`0x0058ccc0`), then `list0` |
| `list0` count + id | `0x0058b235`..`0x0058b26a` | write `u32 count`; per object write `[obj+0x04]` then `[obj][0x14](stream)` |
| `list0` record save | `0x00570c90` | `saDinosaur::Save` (Ghidra had no function; reached only from `vtable+0x14`) |
| `list0` record base save | `0x00577d60` | `saActor` part: `+0x08`, `+0x0c`, `+0x10..0x13`, `+0x14`, `+0x16` |
| `list0` record load | `0x005706c0` | `saDinosaur::Load` |
| `list0` record base load | `0x00577cc0` | mirror of `0x00577d60` |
| mcString writer | `0x00404840` | `u32 (chars*2 + 2)` + UTF-16LE incl NUL (reader `0x004047d0`) |
| Path sub-object save | `0x00573930` | `saDinosaur+0x2c8`, vtable `0x00643170`, slot `+4` |
| Path sub-object load | `0x00573590` | vtable `0x00643170`, slot `+0` (takes `(stream, version)`) |
| Path sub-object ctor | `0x00573370` | also sets the second vtable `0x0064313c` at `+0x1d8` |
| Class id | `1` | reader `0x0058aed0`: `FUN_00572f40(1)` then `vtable+0x10` |
| `saDinosaur` vtable | `0x00643124` | `+0x10`=`0x005706c0`, `+0x14`=`0x00570c90` |

The record object id in the file is **`[saDinosaur+0x04]`**, written by the list
wrapper, not by the record serializer. The base serializer then writes
`+0x08` (x), `+0x0c` (y).

Pseudo-C of the writer (`write(ptr, elem_size, count)`; stream slot `+4`):

```c
void saDinosaur_save(saDinosaur *d, Stream *s)
{
    saActor_save(d, s);                       /* 0x00577d60 */
    s->write(d + 0x4c, 1, 1);                 /* flag: 0 = thresholds present */
    mcString_write(&d->name, s);              /* 0x00404840: u32 bytes, UTF-16 */
    s->write(&d->f38, 4, 1);                  /* always 0 in retail */
    if (*(u8 *)(d + 0x4c) == 0)               /* 4 need-threshold arrays x 3 int */
        for (i = 0; i < 4; i++)
            s->write(*(int **)(d + 0x3c + 4*i), 4, 3);   /* 80,60,30 */
    s->write(d + 0x214, 0xb0, 1);             /* needs/gauges/action cache block */
    s->write(d + 0x208, 0x0c, 1);             /* 3 dwords */
    s->write(d + 0x514, 1, 1);
    s->write(d + 0x515, 1, 1);
    s->write(d + 0x516, 1, 1);
    u32 n = d->tex_name ? d->tex_len + 1 : 1; /* +0x518 / +0x520, ASCII */
    if (s->write(&n, 4, 1) == 1) {
        if (d->tex_name) s->write(d->tex_name, 1, n);
        else { u8 z = 0; s->write(&z, 1, 1); }
    }
    s->write(&d->f524, 4, 1);
    s->write(d + 0x517, 1, 1);
    s->write(&d->f1d0, 4, 1);
    ((Vtbl **)(d + 0x2c8))[0][1](d + 0x2c8, s);   /* 0x00573930, path object */
}
```

The reader `0x005706c0` mirrors it, with version gates
(`0x12` = flag/name, `0x13` = mcString reader, `0x1e` = `+0x514..0x516` and
`+0x518` string, `0x1f` = `+0x524`, `0x20` = `+0x517`, `0x2e` = `+0x1d0`).
Version 53 takes every path.

## 2. Record layout (file offsets)

`record + 0x00` is the object id written by the list wrapper; the rest is
`saDinosaur` (`obj+off` shown in the last column).

| File offset | Size | Field | Runtime |
| --- | ---: | --- | --- |
| `+0x00` | 4 | object id (63823..63932 in the sample) | `+0x04` |
| `+0x04` | 4 | tile x (517) | `+0x08` |
| `+0x08` | 4 | tile y (450) | `+0x0c` |
| `+0x0c` | 4 | 4 state bytes (values 0..4) | `+0x10..0x13` |
| `+0x10` | 2 | u16 (425 in rec0; `[+0x68]/10` on the fresh-ctor path) | `+0x14` |
| `+0x12` | 2 | u16 (200; `+0x1d4` copy on the fresh-ctor path) | `+0x16` |
| `+0x14` | 1 | flag; **0 = the 48-byte threshold block is present**, non-zero = absent | `+0x4c` |
| `+0x15` | 4+ | mcString: `u32 byte_len` (`chars*2+2`) + UTF-16LE incl NUL | `+0x2c` |
| `...` | | **tail** (starts at `+0x19 + name_len`) | |

### Tail (offsets from `name_end`)

| Tail offset | Size | Field |
| --- | ---: | --- |
| `+0x000` | 4 | `+0x38`, always 0 in retail |
| `+0x004` | 48 | 4 need-threshold arrays (`+0x3c/+0x40/+0x44/+0x48`), 3 ints each, default `{80,60,30}` — present only when `+0x4c == 0` |
| `+0x034` | 176 | `+0x214..+0x2c3` gauge/action block (raw memory copy, see table below) |
| `+0x0E4` | 12 | `+0x208..+0x213`, 3 dwords (state, often 0) |
| `+0x0F0` | 1 | `+0x514` (animation effect flag, set by `ApplyActionEffect`) |
| `+0x0F1` | 1 | `+0x515` |
| `+0x0F2` | 1 | `+0x516` |
| `+0x0F3` | 4+L | ASCII string `+0x518` (texture/`.tga` name), `u32 len+1` + bytes; always 5 bytes (empty) in this save |
| `+0x0F8` (+L) | 4 | `+0x524`, 0 |
| `+0x0FC` | 1 | `+0x517`, 0 |
| `+0x0FD` | 4 | `+0x1d0`: 0x1c (28) in 17 records, 0x12f098 / 0x671be4 in two (unidentified) |
| `+0x101` | 513 | embedded path object (`saDinosaur+0x2c8`), see below |
| `+0x302` | 22N | path node vector (N = 0..5 here) |

Fixed tail length for this save: **`0x302 + 0x16*N`**. General form:

```text
tail = 0x04 + (flag==0 ? 0x30 : 0) + 0xB0 + 0x0C + 3 + (4 + tex_len + 1)
     + 4 + 1 + 4 + 0x201 + 0x16*N
     = 0x302 + 0x16*N        (flag 0, empty texture name)
record = 0x19 + name_len + tail
```

### `+0x214` gauge block (176 bytes, 44 dwords)

| Offset | Meaning | Init (`0x00570fd0`) |
| --- | --- | --- |
| `+0x214..+0x224` | 5 phenotype/value stats | 100 or 0 (gated on `+0x200`) |
| `+0x228..+0x240` | 7 ints | 0 or 100 (same gate) |
| `+0x244` | **FOOD** need 0..99 | 99 |
| `+0x248` | **VIOLENCE** need | 99 |
| `+0x24c` | **SECU** need | 99 |
| `+0x250` | **REPOS** need | 99 |
| `+0x254` | int gauge | 100 |
| `+0x258` | f32 rest gauge, max `+0x1d8` | `[+0x1d8]` |
| `+0x25c` | f32 food gauge, max `+0x1dc` | `[+0x1dc] * 0.5` |
| `+0x260` | action countdown | — |
| `+0x264..+0x2a8` | action-effect caches (Smell/Speed/HeadCharge/ObstAttack/DinoAttack/...) | — |
| `+0x2ac..+0x2b8` | FOOD/VIOLENCE/SECU/REPOS **state** 0..3 | — |
| `+0x2bc` | activity/need type 0..4 | — |
| `+0x2c0` | current action id, `0x1f` = idle | `0x1f` |

`+0x2ac..+0x2b8` are exactly the thresholds of `0x005714b0` applied to
`+0x244..+0x250`; rec0 validates (`FOOD 41 -> 1`, `VIOLENCE 80 -> 0`,
`SECU 99 -> 3`, `REPOS 95 -> 3`).

### Embedded path object (`saDinosaur+0x2c8`, vtable `0x00643170`)

Its save starts at runtime `+0x04` (the vtable pointer at `+0x00` is not
serialized), which is file tail `+0x101`.

| Tail offset | Size | Runtime field |
| --- | ---: | --- |
| `+0x101` | 8 | `+0x04`: two f32 (position) |
| `+0x109` | 8 | `+0x0c`: two f32 (target position) |
| `+0x111` | 8 | `+0x14`: two u32 (cells) |
| `+0x119` | 8 | `+0x1c`: two f32 (1.0, 0.0) |
| `+0x121` | 40 | `+0x24..+0x48`: 9 f32 + 1 u32 sentinel `0x7f7fc99e` (`+0x48`) |
| `+0x149` | 2 | `+0x4c` u16 |
| `+0x14B` | 2 | `+0x4e` u16 |
| `+0x14D` | 1 | `+0x50` u8 |
| `+0x14E` | 8 | `+0x54`: two f32 (1.0, 0.0) |
| `+0x156` | 4 | `+0x5c` cell x (517) |
| `+0x15A` | 4 | `+0x60` cell y (450) |
| `+0x15E` | 4 | `+0x64..+0x67`: four bytes |
| `+0x162` | 352 | `+0x68`: **44 fixed `(u32 x, u32 y)` waypoint cells** = `0x2c` records |
| `+0x2C2` | 4 | `+0x1c8` u32 (18 in rec0, likely the current waypoint index) |
| `+0x2C6` | 1 | `+0x1cc` u8 (1) |
| `+0x2C7` | 4 | **`u32` path node count N** (0..5) |
| `+0x2CB` | 22N | N nodes: `f32 x, f32 y, f32 metric, u8, u8, u32, u32` |
| `+0x2CB+22N` | 4 | `+0x1ec` u32 |
| `+0x2CF+22N` | 4 | `+0x1f0` u32 |
| `+0x2D3+22N` | 4 | `+0x1f4` u32 |
| `+0x2D7+22N` | 4 | `+0x1f8` u32 |
| `+0x2DB+22N` | 1 | `+0x208` u8 |
| `+0x2DC+22N` | 4 | `+0x20c` u32 (cell x) |
| `+0x2E0+22N` | 4 | `+0x210` u32 (cell y) |
| `+0x2E4+22N` | 8 | `+0x214` two f32 (0.9999999, 0.0) |
| `+0x2EC+22N` | 1 | `+0x21c` u8 |
| `+0x2ED+22N` | 4 | `+0x220` u32 (cell x) |
| `+0x2F1+22N` | 4 | `+0x224` u32 (cell y) |
| `+0x2F5+22N` | 8 | `+0x228` two f32 (0.9999999, 0.0) |
| `+0x2FD+22N` | 4 | `+0x1d0` u32 |
| `+0x301+22N` | 1 | `+0x1d4` u8 (last byte of the record) |

The 44-slot waypoint array carries **no count**: the loader reuses the
constructor capacity (`0x2c` elements) exactly like the park schedule vector.
Only the node vector is counted.

## 3. Sample validation

All 20 records satisfy `tail_len == 0x302 + 0x16*N` with `N` read at
`tail+0x2c7` (19/19 checkable; the last record ends at `0x6942`, followed by
the next list count `2`).

| Record | Name | id | x,y | tail | N |
| --- | --- | ---: | --- | ---: | ---: |
| 0 | `Ankylosaur1` | 63823 | 517,450 | `0x344` | 3 |
| 1 | `Allosaurus1` | 63830 | 432,520 | `0x318` | 1 |
| 2 | `Allosaurus2` | 63835 | 479,374 | `0x302` | 0 |
| 17 | `Deinonychu5` | 63922 | 463,330 | `0x370` | 5 |

Records 0 and 1 differ **only** by `N` (`3` vs `1`,
`0x344 - 0x318 = 0x2c = 2*0x16`). The count field itself is the divergence at
`tail+0x2c7` (`03 00 00 00` vs `01 00 00 00`); every byte before `+0x302` has
the same width, only different values (rec0 `FOOD 41` vs rec1 `FOOD 80`, etc.).

## 4. Port contract

* **No explicit record length exists.** The length is a function of three
  fields inside the record: the name length, the `+0x518` string length and the
  path node count `N` at `tail+0x2c7`. The reader never scans or skips: it
  consumes the schema in order, so a corrupted `N` desynchronizes the whole
  list.
* **Opaque retention is not a general solution.** You cannot store the tail as
  a blob and still write a new save unless you also keep `N` (and the two
  string lengths) alongside the blob; the blob is not self-delimiting at the
  API level.
* Recommended model (`SavCampaignDino`):
  * prefix: id, x, y, four state bytes, two u16, flag;
  * `name` (UTF-16 with terminator);
  * 48-byte threshold block (retain as 12 ints, or omit when flag != 0);
  * `gauges[44]` (raw 176 bytes is fine — it is a memory image; only the
    documented members above need decoding);
  * `state3[3]`, three bytes;
  * `texture_name` (ASCII, usually empty);
  * `f524`, `f517`, `f1d0`;
  * path: 4 pairs, 10 floats/ids, 2 u16, u8, 2 floats, 2 cell u32, 4 bytes,
    44 waypoints, `+0x1c8`, `+0x1cc`, node count, nodes, 14 trailing fields.
* Writer order is exactly the table order (never reorder); the four-triple block
  must be skipped when `+0x4c != 0` or the reader will misparse.
* `--sav-*` writers that do not model campaign dinos should keep the byte range
  `[list0_start, list0_end)` verbatim and record `N` from `tail+0x2c7` if they
  ever need to re-emit it after a length change.

## 5. Open / uncertain

* Semantics of `+0x1d0` (28 vs 0x12f098 vs 0x671be4) — likely a timer/seed, not
  proven.
* `+0x208..+0x213` (3 dwords) is usually 0, but records 5/11/12/13/14/16/17 hold
  float-looking or negative garbage (e.g. rec5 `0x04a6c858`), so it is a raw
  memory image whose fields are not per-record initialized.
* Path node `+0x10`/`+0x14` dwords: 1, 0x20, 0x200 observed; meaning unknown.
* The `+0x214..+0x240` 12-int block is only partly documented (5 "phenotype
  stats" + 7 ints, both gated on `+0x200`).
* Record 19's end (`0x6942`) is inferred from the formula and the following
  count `2`; it is not name-validated like the other 19.

## 6. Evidence

* Ghidra (binary `/DinoIsland.exe-ebea17`): `0x0058b200`, `0x0058aed0`,
  `0x0058ccc0`, `0x00586b30`, `0x00586ad0`, `0x00577d60`, `0x00404840`,
  `0x00577600`, `0x00573370`, `0x00570fd0`, `0x00570fa0`; disassembly of the
  Ghidra-missed functions `0x00570c90`, `0x005706c0`, `0x00573930`, `0x00573590`
  (vtable slots `0x00643124+0x10/+0x14` and `0x00643170+0x0/+0x4`).
* Data: `build/campaign-autosave.sav` — `tail_len = 0x302 + 0x16*N`, count at
  `tail+0x2c7`, path vector at `tail+0x2cb`, last record ends `0x6942`.
* Byte-equality of the reconstructed watcher offsets for rec0 (`0x344` exactly
  consumed) and all 19 checkable records.

## 7. Implementation (2026-10-27)

* `tools/formats/sav_campaign.py` implements the full typed model
  (`CampaignDino`, `CampaignPath`, `CampaignPathNode`) and a writer that
  re-encodes all 20 sample records byte-identically (`--check`).
* `tools/formats/sav_write.py --campaign` rebuilds header/world/catalog/
  roads/list0 from models and keeps list1+ verbatim; the 53,338-byte sample
  is byte-identical.
* `src/formats/wld.cpp` validates each record length with the same rule and
  keeps list0 + list1/tail raw, so the C++ `--sav-roundtrip`/`--sav-write`
  are byte-identical too. `test_cpp_sav_park_roundtrip` includes the campaign
  sample. The typed C++ struct and list1/list2 decode are still open.
