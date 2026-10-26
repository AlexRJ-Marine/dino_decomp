# Runtime calibration batch (2026-10-26)

Headless x32dbg trace, retail `run/DinoIsland.exe` (no-CD patched), Free Game
→ island → easy → Start. Script: `scripts/x32dbg/trace-calibration.ps1`.
Logs: `notes/x32dbg-calibration-isl01b.out.log`,
`notes/x32dbg-calibration-isl02.out.log`.

## Visitor initial needs (ctor `FUN_005761b0`)

Disassembly `0x005761b0..0x00576228`:

```text
005761f8  LEA EAX,[ESI+0x58]
005761fb  MOV ECX,4
00576200  MOV EDX,0x63
00576205  MOV [EAX-0x10],EDX      ; +0x48..+0x64 = 99
00576208  MOV [EAX],EDX
0057620a  ADD EAX,4
0057620d  DEC ECX
0057620e  JNZ 0x00576205
00576210  FILD [ESI+0x48]         ; 99.0
00576213  FMUL float [0x00643180] ; 0.4
00576219  CALL 0x005982d4         ; __ftol
0057621e  MOV [ESI+0x48],EAX      ; need0 = 39
```

`0x00643180` = `0x3ECCCCCD` = `0.4f`; `(int)(99 * 0.4) = 39`.

The ctor also sets `+0x4c/+0x50/+0x54 = 99`, `+0x58..+0x64 = 99`,
`+0x68 = profile index`, `+0x28 = 0`, `+0x74 = 0`, `+0x34 = 1.0`,
`+0x38 = 0`, `+0x78 = 0`.

Runtime first `VNEED` hits for both spawned visitors:

```text
VNEED ecx=0C8D81E8 st=0 n0=39 n1=99 n2=99 prof=0 leave=0
VNEED ecx=0C738370 st=0 n0=39 n1=99 n2=99 prof=4 leave=0
```

So **initial needs are `(39, 99, 99)`**, not `(99, 99, 99)`.

## Need1 decay gate (correction)

`FUN_00576320` decrements need1 only when:

```text
if (tick1 && (state != 2 || *(target + 0x28) == 0)) need1 -= 1;
```

`target+0x28` is the attraction's visit duration (`.bbi` file offset 37).
The pause is therefore **state 2 + nonzero attraction duration**, not a
"need delta" or a feeding flag.

## Free-game Entrance cell and orientation

Runtime `FUN_00586b80` (`param_2` = cell pair, `param_3` = state):

```text
isl01  BUILD cellx=0x208 (520) celly=0x178 (376) state=1
isl02  BUILD cellx=0x138 (312) celly=0x1F8 (504) state=2
```

These are exactly the island `.lnd` fields:

| Island | `.lnd` w/h | `.lnd` 0/1/2 | Entrance state |
| --- | --- | ---: | ---: |
| isl01 | 520×376 | 1 | 1 |
| isl02 | 312×504 | 2 | 2 |
| isl03 | 704×448 | 2 | 2 (campaign) |
| isl04 | 440×200 | 1 | 1 (campaign) |
| isl05 | 480×200 | 2 | 2 (campaign) |
| isl06 | 628×172 | 1 | 1 (campaign) |
| isl07 | 120×492 | 0 | 0 (campaign Cpg08) |

All seven campaign `Cpg0*_Save.wld` Entrance records match. The port now
places the free-game Entrance at `(lnd.width, lnd.height)` with
`state = lnd.unknown_int`.

## Water surface height

Runtime `FUN_00602cf0` (`[ecx+0x30]`):

```text
isl01  0x41800000 = 16.0
isl02  0x41A00000 = 20.0
```

These match the `.lnd` `unknown_float` (16.0/20.0). The earlier claim that the
float is dead data is too strong: the landscape/water path uses it. Width,
height and the 0/1/2 value are also consumed as the free-game Entrance
cell/orientation. They are **not** merely editor metadata.

## SIM-2b review note

The ported `src/world/visitors.cpp` (another agent) is structurally faithful,
but two values differ from this evidence:

1. `Visitor::need0` is initialized to `99`; retail uses **39**
   (`static_cast<int>(99 * 0.4)`).
2. The need1 decay pause checks `need_delta1 != 0`; retail pauses when
   `state == Use && attraction.duration != 0`.

Apply both in `visitors.cpp` / `visitors.h` and add a regression check.
