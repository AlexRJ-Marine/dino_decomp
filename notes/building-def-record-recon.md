# `.bbi` → 500-byte building definition record (Ghidra)

Status: **solved**; 39/39 files consume exactly (exact EOF) in the recovered
read order. This corrects the old `30 × u32 + flag` header hypothesis.

## Loader

`FUN_0053fb00` is the `.bbi` loader, called as `FUN_0053fb00(param_2[3])` from
`FUN_0053d4d0` at `0x53d4d0`. `param_2[3]` is the
`DATA\design\buildings\` directory set at `0x51d1e1` → `world_manager+0x8c`.
The design-database family is walked by `FUN_0053a400` from `FUN_0051d1d0`:

| index | loader | domain |
| ---: | --- | --- |
| 0 | `0x53d9a0` | walls |
| 1 | `0x53e060` | actions |
| 2 | `0x53ed30` | features |
| 3 | `0x53fb00` | buildings |

For each `BuildingList.ini` name it opens `<dir><name>.bbi` (format string at
`0x665084`) and writes the 500-byte record to `*(world_mgr+0x8c) + i*500`,
starting at **element+0x0c**.

`FUN_0053d1a0` passes the definition base `def = element+0x0c` onward:
`FUN_00589100` reads `param_4+0x6c` = element+0x78 = category, and
`FUN_00586b80` reads `*param_4` = footprint.

## Header layout (file bytes)

```text
0    u32 footprint width   (world units)
4    u32 footprint height  (world units)
8    f32 scale
12   u8  flag              (1 in all 39 files)
13   27 x unaligned u32 at 13, 17, 21, ... 117
121  ten length-prefixed paths
```

The `u8` at 12 shifts every later dword by one byte; a 30-u32 view from offset
0 is wrong from word 3 onward. `data[120]` (the old "flag") is 0.

## Runtime record fields

`D = element+0x0c` is the definition base. `M = element+0x7c` is the model
record base.

| Element | `D+` | File | Type | Entrance |
| ---: | ---: | ---: | --- | ---: |
| +0x0c | +0x00 | 0 | u32 | footprint W 24 |
| +0x10 | +0x04 | 4 | u32 | footprint H 12 |
| +0x14 | +0x08 | 8 | f32 | 1.0 |
| +0x18 | +0x0c | 12 | u8 | 1 |
| +0x1c | +0x10 | 13 | u32 | 100 |
| +0x20 | +0x14 | 17 | u32 | 10 |
| +0x24 | +0x18 | 21 | u32 | 30 |
| +0x28 | +0x1c | 25 | u32 | **50 = price** |
| +0x2c | +0x20 | 29 | u32 | 0 |
| +0x30 | +0x24 | 33 | f32 | 10.0 |
| +0x34 | +0x28 | 37 | f32 | **10.0 = visit duration** |
| +0x38 | +0x2c | 41 | u32 | 10 |
| +0x3c | +0x30 | 45 | u32 | 0 |
| +0x40..0x50 | +0x34..0x44 | 53,57,61,65,69 | f32 | 0, 0, 0, 0, 0 (appeal weights) |
| +0x54 | +0x48 | 73 | f32 | **4.0 = appeal scale** |
| +0x58 | +0x4c | 77 | f32 | 100.0 |
| +0x5c..0x68 | +0x50..0x5c | 81,85,89,93 | f32 | 0, 0, 24.0, 7.0 |
| +0x6c..0x74 | +0x60..0x68 | 97,101,105 | f32/u32 | 24.0, 6.0, **1024 = radius²** |
| +0x78 | +0x6c | 49 | u32 | **10 = category** |
| +0x7c | +0x70 | 109 | u32 | `model[0]` = 0 (yaw only) |
| +0x80 | +0x74 | 113 | f32 | `model[1]` = 0.0 (Z offset) |
| +0x84 | +0x78 | 117 | u32 | 2 = model count |

`model[0]` is an int flag in `FUN_0061ffc0` (0 → yaw basis, 1 →
`FUN_0061c510` slope basis). Only `LunchDoor.bbi` has 1. `model[1]` is a
float bit pattern (`0x00620227: MOV EDX,[ESP+0x88]`, no conversion) and is
`0.0f` in all 39 files. `Entrance.bbi` bytes 105..121:

```text
00 04 00 00 | 00 00 00 00 | 00 00 00 00 | 02 00 00 00
1024          model0=0       model1=0.0    count=2
```

No `7.18`, `718`, `7180` or `0x44338000` occurs in `Entrance.bbi`. The ~7.18 m
hull depth is purely the primary `.skin` asset; retail places the pivot on the
heightfield with no offset.

## Model record paths

`FUN_005fecf0` → `FUN_00603e30` loads from `M`:

| `M+` | Element | Asset | Entrance |
| ---: | ---: | --- | --- |
| +0x0c | +0x88 | primary `.skin` | `entrance_lod1.skin` |
| +0x6c | +0xe8 | `.skel` | `entrance_lod1.skel` |
| +0x90 | +0x10c | `.anim` | `entrance_lod1.anim` |
| +0xb4 | +0x130 | texture directory | `Data\Graphics\TEXTURES\buildings` |
| +0x54 | +0xd0 | second model `.mesh` | `entrance_lod1.mesh` |

## Attraction block (`building+0x74`)

The visitor list stores the sim building (0xc4 bytes, vtable `0x64355c`,
`+0x40` = def, `+0x34` = road-connected byte, `+0xa4` = cell). Three
functions build the block:

- `FUN_00576720(block, *(def+0x54))` from the ctor `0x00586bb2`:
  `block+0x14 = &PTR_FUN_00626ad4`, `block+0x18/1c/20` = empty float vector
  with capacity 6, `block+0x24 = *(def+0x54)` (appeal scale).
- `FUN_00576860(block, count of (*(park+0x78)+0x34) vector)` resizes the
  appeal vector to the park need count.
- `FUN_005872c0` (spBuilding vtable slot +0x10) copies:

```text
005872cd  MOV EDX,[ESI+0x40]          ; def
005872d0  ADD EAX,4 / 005872d9  MOV EDX,[EDX+EAX-4]  ; def+0x34,38,3c,40,44
005872dd  MOV [ECX-4],EDX             ; block+0x00..0x10
005872ea  MOV ECX,[EAX+0x50] / 005872ed MOV [ESI+0x9c],ECX  ; block+0x28
005872f6  MOV EAX,[EDX+0x54] / 005872f9 MOV [ESI+0xa0],EAX  ; block+0x2c
00587317  MOV EBP,[ESI+0x8c]          ; block+0x18 float array
00587328  CALL 0x00576550             ; appeal = clamp(sum, -100, +100)
0058732d  FSTP [EBP+EDI*4]            ; appeal[need]
```

`block+0x18` is not a file field: it is
`clamp(Σ_{k=0..4} (def+0x34+4k) × (need[k]+0x18), −100, +100)` via
`FUN_00576550`. The Entrance weights are all 0, so every appeal value is 0.

Corrections to `docs/decomp/visitor-ai.md`:

- The scorer's price is `descriptor(+0x40)+0x1c` (`0x522af2`/`0x522bb2`),
  i.e. definition file offset 25 (Entrance 50), **not** block+0x1c.
- The duration is `descriptor+0x28` (file offset 37, Entrance 10.0).

## Uncertainties

- `block+0x28/+0x2c` are copied from float fields (files 69/73) but read with
  `FILD` at `0x00522b63`/`0x00522b8c`. The copy is proven; the intended
  meaning is a retail quirk to runtime-verify.
- The park need table `*(spPark+0x78)+0x34` was not dumped, so the actual
  per-need appeal numbers for each building are unknown.
