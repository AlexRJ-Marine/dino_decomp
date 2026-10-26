# `.skel` reader `0x477890` (Ghidra)

Status: **ABI solved; hook implemented and runtime-verified.** The generic
loader consumes the 13-byte file header; this reader reads the five sections
in one call. Free-game park: **21 skeletons**, normal rendering
(`notes/hyb5-counter.txt`, `notes/park-hook-skel-default.png`).

## Contract

```text
00477890  SUB ESP,0x5C
00477893  PUSH EBX / EBP / ESI
00477896  MOV ESI,[ESP+0x6C]   ; stream
0047789A  MOV EBX,ECX          ; this
0047789C  PUSH EDI
...
00477AE6  MOV AL,1
00477AE8  POP EBX
00477AE9  ADD ESP,0x5C
00477AEC  RET 8
```

`bool __thiscall Skeleton::Read(Stream* s, void* ctx)` — ECX = skeleton,
two stack args, `RET 8`, return in AL. `ctx` is not dereferenced. Call site:
`0x471AC6` (`CALL [EDX+0x10]`) inside `FUN_004714b0`; the loader already
consumed the 13-byte header (4-byte hash, 4-byte sentinel, magic/CRC).

## Read order (offset 0x0D onward)

| # | Read | Element |
| --- | --- | --- |
| 1 | `u32 n` matrices | `n × 64` via `0x4663a0`, stride 0x40 |
| 2 | `u32 n` child lists | `n × 0x10`; each via `0x47cc90` |
| 3 | `u32 n` parents | `n × u32` inline |
| 4 | `u32 n` names | `n × 0x0C`; per name `u32 len`, `reserve(len+1)`, bytes, `strlen` |
| 5 | `u32 n` flags | `n × u32` inline |

All five counts equal `bone_count` in retail (allo.skel 80/80/80/80/80), but
the reader does not enforce it. A failed early read falls through to the next
section; only the final read failure returns 0.

## Runtime object (0x64)

```text
+0x00 vptr 0x625ee8   (dtor 0x456a70, clear 0x476080, load 0x4714b0,
+0x04 u32 id 0x56789ABC   save 0x477610, read 0x477890, +0x14 0x4762d0,
+0x08 mcString name 0x08/0x0c/0x10   id 0x43b8f0, name 0x453040)
+0x14 vector<Matrix4>   vptr 0x6256c4, data +0x18/+0x1c/+0x20, stride 0x40
+0x24 vector<vector<u32>> vptr 0x625f08, data +0x28/+0x2c/+0x30, stride 0x10
+0x34 vector<u32> parents vptr 0x6256cc, data +0x38/+0x3c/+0x40
+0x44 vector<mcString> names vptr 0x62530c, data +0x48/+0x4c/+0x50
+0x54 vector<u32> flags vptr 0x6256cc, data +0x58/+0x5c/+0x60
```

**The `.skel` sentinel is `0x56789ABC`**, not `0xDEF01234` (that is the
`.skin`/`.anim` marker). `allo.skel` bytes 0x09..0x0c = `BC 9A 78 56`;
`0x4743a0` stores it at `+0x04`.

## Resize helpers

Called with `&first` (`+0x18`, `+0x28`, `+0x38`, `+0x48`, `+0x58`):

| Address | Contract |
| --- | --- |
| `0x425140` | `__thiscall resize(MatrixData*, u32 n, const void* fill)`, stride 0x40 |
| `0x47b7d0` | `__thiscall resize(ChildData*, u32 n, const ChildVec* fill)`, stride 0x10 |
| `0x47a7c0` | `__thiscall resize(U32Data*, u32 n, const u32* fill)`, stride 4 |
| `0x40a270` | `__thiscall resize(NameData*, u32 n, const McString* fill)`, stride 0x0C |
| `0x4058b0` | `mcString::reserve` (buf/cap) |
| `0x4056c0` | `strlen` |

## Replacement

```cpp
bool __thiscall hook_477890(Skeleton* self, Stream* s, void* ctx) {
    // 1 matrices: resize 0x425140, read each via hook_4663a0
    // 2 children: resize 0x47b7d0 with {vptr=0x6256cc}, each via hook_47cc90
    // 3 parents:  resize 0x47a7c0, read u32
    // 4 names:    resize 0x40a270, per name reserve/read/strlen
    // 5 flags:    resize 0x47a7c0, read u32
}
```

6-byte prologue `83 EC 5C 53 55 56` (`E9 rel32` + NOP). Installed by
default; per-call logging is disabled because the reader can run under the
loader lock.

## Verification

- Free-game park with the hook active: **21 `.skel` calls**, 2221 skin bones,
  8911+ animation keys, normal rendering.
- Counter excerpt: `notes/hyb5-counter.txt`.
- Screenshot: `notes/park-hook-skel-default.png`.

## Evidence

- `0x477890` disassembly; `0x471ac6` call; `0x4743a0`/`0x475e30` ctor.
- `0x425140`, `0x47b7d0`, `0x47a7c0`, `0x40a270`, `0x4058b0`, `0x4056c0`,
  `0x4663a0`, `0x47cc90`.
- `allo.skel` header bytes (read-only).
- `docs/formats/skel.md`, `src/formats/skel.cpp`.
