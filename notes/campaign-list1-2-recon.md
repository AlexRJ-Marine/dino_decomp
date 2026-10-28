# Campaign simulation `list1`/`list2` — recon

Status: **solved** (Ghidra `DinoIsland.exe-ebea17` + the 53,338-byte sample
`build/campaign-autosave.sav`). `list1` is `saFoodSource` and `list2` is the
employee list; both have fixed record sizes and no length prefixes. The park
block after the remaining lists is **not yet located** (open).

## 0. Correction: records start with a registry id, not a class id

Every simulation-list record is `u32 registry_id` + `<class>::Save(stream)`.
`FUN_0058aed0` reads the dword and resolves it through the `saIDProvider` map
`0x0067570c` (`FUN_00572f40`, value at node+`0x14`), then calls the object's
vtable slot. The earlier "list0 class id 1" was a misread of that call: the
pushed argument is the just-read id, not a constant.

## 1. List map in the campaign save

| list | manager vector | object | vtable | ctor | load | save | record | sample |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| list0 | `spPark+0x68` | `saDinosaur` 0x52c | `0x00643124` | `0x56fed0` | `0x005706c0` (+0x10) | `0x00570c90` (+0x14) | `0x19+name+0x302+0x16*N` | 20 rec, `0x2636..0x6942` |
| **list1** | **`spPark+0xcc`** | **`saFoodSource` 0x50 B** | **`0x006431d0`** | **`0x00576f10`** | **`0x00576f70`** (+0x10) | **`0x00576fd0`** (+0x14) | **0x2C** | **count 2 @0x6942, recs 0x6946/0x6972, ends 0x699E** |
| **list2** | **`spPark+0xe4`** | **employees 0x60 B** | **`0x006431a0`** | **`0x005765e0`** | **`0x00576660`** (+0x10) | **`0x005766b0`** (+0x14) | **0x2B** | **count 6 @0x699E, recs 0x69A2+0x2B*i, ends 0x6AA4** |
| visitor | `spPark+0x80` | 0x50 B | `0x00643184` | `0x005761b0` | `0x00576270` | `0x005762d0` | 0x50 | count 0 @0x6AA4 |
| next | `spPark+0x90` | attraction family (save `+0x8`, load `+0x4`) | — | — | — | — | — | count 57 @0x6AA8 |

The campaign file matches the **7-list writer** in `0x0058b200`; the free-game
`.sav` references only show one count (`1`) between the visitor list and the
0x293 park block. Do not use the free-game saves as the campaign layout oracle.

## 2. Serializers

All object lists first call `saActor::Save` (`0x00577d60`), which writes
`+0x08` x4, `+0x0c` x4, `+0x10..13` x1, `+0x14`/`+0x16` x2 (16 bytes).

```c
/* 0x00576fd0  list1, after the 4-byte id */
saActor_Save(d, s);                 /* 16 B */
s->write(d + 0x28, 4, 2);           /* f32 x, f32 y */
s->write(d + 0x30, 4, 2);           /* f32 dirx, f32 diry */
s->write(d + 0x4c, 4, 1);
s->write(d + 0x44, 4, 1);           /* = 0x2C total */

/* 0x005766b0  list2, after the 4-byte id */
saActor_Save(d, s);                 /* 16 B */
/* base 0x005793d0 */
s->write(d + 0x28, 4, 1);           /* state */
s->write(d + 0x2c, 4, 2);           /* f32 x, f32 y */
s->write(d + 0x34, 4, 2);           /* f32 dirx, f32 diry */
s->write(d + 0x44, 1, 1);
s->write(d + 0x45, 1, 1);
s->write(d + 0x4c, 1, 1);           /* = 0x2B total */
```

Loads mirror the order; `0x576f70` gates on version > 3, `0x576660` on > 3 /
> 0x10.

## 3. Record field tables (file offsets from record start)

### list1 (`saFoodSource`, 0x2C)

| off | size | field |
| ---: | ---: | --- |
| `+0x00` | 4 | registry id (`u32`) |
| `+0x04` | 4 | cell x (`u32`) |
| `+0x08` | 4 | cell y (`u32`) |
| `+0x0c` | 4 | state bytes (`0x00010001`) |
| `+0x10` | 2 | runtime `+0x14` (`u16`) |
| `+0x12` | 2 | runtime `+0x16` (`u16`) |
| `+0x14` | 8 | f32 x, f32 y |
| `+0x1c` | 8 | f32 dirx, f32 diry |
| `+0x24` | 4 | runtime `+0x4C` |
| `+0x28` | 4 | runtime `+0x44` (rec 1 = `0x00080104`, uninitialised-looking) |

### list2 (employee, 0x2B)

| off | size | field |
| ---: | ---: | --- |
| `+0x00` | 4 | registry id (`u32`) |
| `+0x04` | 4 | cell x (`u32`) |
| `+0x08` | 4 | cell y (`u32`) |
| `+0x0c` | 4 | state bytes |
| `+0x10` | 2 | `u16` |
| `+0x12` | 2 | `u16` |
| `+0x14` | 4 | `+0x28` state (`u32`) |
| `+0x18` | 8 | f32 x, f32 y (`+0x2c`/`+0x30`) |
| `+0x20` | 8 | f32 dirx, f32 diry (`+0x34`/`+0x38`) |
| `+0x28` | 1 | `+0x44` |
| `+0x29` | 1 | `+0x45` |
| `+0x2a` | 1 | `+0x4C` (all records `00 01 00`) |

## 4. Sample walk

```text
0x6942  count = 2          list1 (saFoodSource)
0x6946  id 264 @ (461,461)
0x6972  id 267 @ (461,461)
0x699E  count = 6          list2 (employees)
0x69A2  ids 13, 15, 337, 375, 411, 63785  (0x2B each)
0x6AA4  count = 0          visitor list
0x6AA8  count = 57         next attraction-family list
```

## 5. Port contract

- `list1`/`list2` are **self-delimiting**: fixed 0x2C/0x2B records, no strings
  or vectors, so a writer can keep `[count, count × (u32 id + payload)]` raw
  and re-emit it byte-safely. Only the counts are needed to walk them.
- The current Python `sav_campaign_scan.py`/C++ reader stop after `list0`
  and keep the rest raw; walking list1/list2 is safe and gives food-source and
  employee counts without changing the round-trip.
- The campaign park block/economy is still open: the 57-element list at
  `0x6AA8` and any following lists must be walked first.

## 6. Open / uncertain

- list1's source-file name (`saFoodSource`) rests on `.cpp` link-order
  adjacency (`saFoodSource.cpp` string `0x665dc0` referenced at `0x576da9`,
  next file `saDinoIDConverter.cpp` at `0x577f60`); the vtable/save/load
  addresses are proven.
- The 0x293 park block was **not** located in the campaign sample: no
  `100 + 32-zero-byte` header pattern exists after `0x6AA8`, so the block/
  economy offset is not established.
- The 57-record list at `0x6AA8` is identified only as the attraction family
  (`spPark+0x90`, save slot `+0x8` / load `+0x4`); its records are not decoded.

## 7. Evidence

- Ghidra: `0x0058aed0` list loop and `FUN_00572f40` registry resolution;
  `0x0058b200` 7-list writer; save/load slots `0x00576fd0`/`0x00576f70`,
  `0x005766b0`/`0x00576660`; `saActor::Save` `0x00577d60`;
  creation sites `FUN_00588860` (list1) and `FUN_00588600` (list2).
- Data: `build/campaign-autosave.sav` offsets above; list1 records
  `264 @461,461` and `267 @461,461`, list2 ids 13/15/337/375/411/63785.

## 8. Park-block search status (after list1/list2)

A scan of the whole 53,338-byte file for a strict 0x293 park header
(`class=2`, `header_b=0x64`, `header_c` 0.0 or 10.0, 32 zero reserved bytes)
finds **no hit**. The candidate at `0x6AAC` has the class/header dwords but its
reserved block is nonzero and the parsed budget/sellback are 0, so it is the
first record of the 57-element list, not the park block. The campaign park /
economy offset therefore remains open until the 57-list (and any following
lists) are walked.
