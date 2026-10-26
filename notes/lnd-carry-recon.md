# `.lnd` header carry path (Ghidra)

Status: **solved** — the four header values fill a stack landscape descriptor
and reach the water/Entrance objects with no transformation.

## Descriptor

`FUN_0051dad0` stores into the stack struct based at `[esp+0x6c]`
(frame base `B` at `0x51e0b7`):

| Field | Offset | Read instruction |
| --- | ---: | --- |
| water level (f32) | `+0x54` | `0x51e0bd` LEA, call at `0x51e0cb` |
| Entrance cell x | `+0x9c` | `0x51e0d4` LEA, call at `0x51e0e2` |
| Entrance cell y | `+0xa0` | `0x51e0eb` LEA, call at `0x51e0f9` |
| orientation | `+0xa4` | `0x51e102` read, `0x51e110` move, `0x51e125` store |

The following 4-byte read at `0x51e11a` is the music string length, not a
fifth header number. The descriptor also carries the 7 strings, vegetation
vector, detail-map params (`+0x90..0x98`, overwritten to 20000/1.0/0.05), the
music string `+0xa8` and a gate byte `+0xbc`.

`0x51e24a` passes `&struct` to `FUN_005f3f10`, and `0x5f3ffe` to
`FUN_005fb8b0`.

## Water

`FUN_005fb8b0`:

```text
0x5fbde0  MOV ECX,[ESI+0x54]     ; raw f32
0x5fbde6  PUSH ECX               ; 5th arg of water ctor 0x602b80
```

`0x602b80` stores it at `this+0x30`; `FUN_00602cf0` reads `+0x30` and uses it
as the water quad Z. `0x5fbe0a` also copies the raw float to the terrain
object `+0x38`. No scale or unit conversion.

## Entrance

At `0x51e29b` the loader checks the descriptor gate, then loops the park's
500-byte building definitions (`park+0x8c`, category `[def+0x78] == 10`):

```text
0x51e2f5  MOV EAX,[ESP+0x110]    ; orientation
0x51e2fc  LEA ECX,[ESP+0x108]    ; &(cell x, cell y)
0x51e309  CALL 0x53b330(index, &cell, state)
```

`FUN_0053b330` stores:

```text
ESI+0x14 = cell.x
ESI+0x18 = cell.y
ESI+0x1c = building index
ESI+0x30 = orientation
```

and registers the object (`0x53b37d CALL 0x5e7310`). The creation listener
`FUN_0053d1a0` (invoked at `0x53c145`) pushes `&obj+0x14` and `[obj+0x30]`
(`0x53d1d0..0x53d1e3`) into `0x586950 -> FUN_00589100`; the building ctor
`FUN_00586b80` stores `this[2] = cell_x`, `this[3] = cell_y`,
`this[0x12] = state` (byte `+0x48`). This matches the runtime log
`(520,376) state 1`.

Only the spawned entry road applies a transform (`(v>>2)*4` on the footprint
centre plus per-state offsets), not the `.lnd` values themselves.

## Confidence

- High: descriptor offsets, water path, cell/state ctor stores, no transforms.
- Medium: the listener hop `FUN_005e7310 -> 0x53c145 -> FUN_0053d1a0` is
  inferred from the registration call and matching signature/args.
- Open: the writer of the descriptor `+0xbc` gate byte was not located inside
  `FUN_0051dad0` (likely set through a callee/pointer).

## Evidence

- `0x51e0bd..0x51e125` reads and stores; `0x51e24a` call.
- `0x5fbde0/0x5fbde6` water arg; `0x602b80`, `0x602cf0`, `0x5fbe0a`.
- `0x51e2f5..0x51e309`; `0x53b330`, `0x53d1a0`, `0x586950`,
  `0x589100`, `0x586b80`.
- `docs/formats/lnd.md`, `notes/calibration-2026-10-26.md`.
