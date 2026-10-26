# `.anim` bone reader `0x4839e0` (Ghidra)

Status: **ABI solved; hook implemented and runtime-verified.** One call reads
one 0x2C-byte bone record; the caller `0x475990` reads the duration, bone
count and then loops. On the free-game park the hook read **8911+ animation
keys** and the park rendered normally (`notes/hyb4-counter.txt`,
`notes/park-hook-all.png`). Per-call logging is disabled because the reader
can run under the loader lock.

## Contract

```text
004839e0  SUB ESP,0x48
004839e3  PUSH EBX / EBP / ESI
004839e6  MOV ESI,[ESP+0x58]   ; stream
004839ea  MOV EBX,ECX          ; this = AnimBone
004839ec  PUSH EDI
...
00483b46  RET 4
```

`bool __thiscall f(AnimBone* this /*ECX*/, Stream* s /*[esp+4]*/)`, `RET 4`,
returns 1/0 in AL. Only code xref: `0x475a62` inside the `.anim` loader
`0x475990` (vtable `0x625ec4`, slot `+0x10`); the caller advances by `0x2C`
per bone.

## Read order

`Stream::Read(dst, elem_size, count)` is vtable `+0x8` (slot 2).

| # | Read | Size | Failure |
| --- | --- | --- | --- |
| 1 | name byte length **including NUL** | 4 × 1 | skip name, continue |
| 2 | name bytes | 1 × len | skip strlen, continue |
| 3 | key count | 4 × 1 | skip times, continue |
| 4 | `u64` time per key | 8 × 1 | break loop |
| 5 | matrix count | 4 × 1 | **return 0** |
| 6 | 64-byte matrices via `0x4663a0` | 4 × 16 | **return 0** |

No flags or trailing counts. `HeavyBiped_Armor.anim` @0x19:
`06 00 00 00` + `"Bip01\0"` + `1F 00 00 00` (31 keys); the writer `0x4838d0`
writes `strlen + 1`.

## Runtime bone record (0x2C)

```text
+0x00 char*  name_buf      ; mcString, NUL-terminated
+0x04 u32    name_cap      ; min 0x10 (0x4058b0)
+0x08 u32    name_len      ; strlen (0x4056c0)
+0x0C void*  times_vptr    = 0x625f34   ; vector wrapper
+0x10 u64*   times_first
+0x14 u64*   times_last
+0x18 u64*   times_end
+0x1C void*  mats_vptr     = 0x625f30
+0x20 Mat4*  mats_first
+0x24 Mat4*  mats_last
+0x28 Mat4*  mats_end
```

Helpers: `0x484d30` shrink times, `0x486a60` grow times (8-byte), `0x4855f0`
grow matrices (0x40-byte), `0x5900b0` shrink matrices, parent vector
`0x47b490`/`0x47f570`. The dtor `0x4744e0` frees `+0x00` and the two
containers; keep their vptrs.

## Parent `mgAnim` (0x30)

```text
+0x00 vptr 0x625ec4
+0x04 u32 id 0xdef01234
+0x08/0x0c/0x10 name string
+0x14 vptr 0x625f2c
+0x18/0x1c/0x20 bone vector first/last/end
+0x24 unwritten
+0x28 u64 duration
```

`0x484160` reads the duration from `+0x28`, divides by 160 and adds 1 (31
keys for 4,800 time units). `docs/formats/anim.md` calls the 8-byte header
"duration+start"; the runtime treats it as one `u64` (high dword 0 in
retail).

## Replacement sketch

```cpp
struct AnimBone {
    char* name; uint32_t cap; uint32_t len;
    McVector times;      // uint64
    McVector mats;       // 16 floats
};
bool __thiscall hook_4839e0(AnimBone* bone, Stream* s) {
    // read name length (incl NUL), name; read key count, u64 times;
    // read matrix count, matrices via 0x4663a0; return false on matrix fail.
}
```

Must allocate through mcLib (`0x4058b0`, vector helpers) and preserve the
vptrs `0x625f34`/`0x625f30`.

## Confidence

- High: ABI, read order, 0x2C layout, parent 0x30 layout, key count (31),
  writer's `strlen+1`.
- Medium: the exact stream provider for the binary `.anim` path (vtable-only
  xref).
- Open: parent `+0x24` is never written; the `flx` vs binary dispatch in
  `0x4745b0` is not traced.

## Evidence

- `0x4839e0` disassembly; `0x475990`, `0x475a62`, `0x4838d0`, `0x4744e0`,
  `0x484160`, `0x484d30`, `0x486a60`, `0x4855f0`, `0x5900b0`, `0x47b490`,
  `0x47f570`.
- `HeavyBiped_Armor.anim` bytes (read-only).
- `docs/formats/anim.md`, `notes/hybrid-skin-recon.md`.
