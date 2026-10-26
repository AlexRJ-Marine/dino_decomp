# `.skin` record reader `0x47d880` (Ghidra)

Status: **ABI solved; hook implemented and runtime-verified.** This is the
per-bone skin table reader used by the `.skin` loader. It reads the full table
for one 0xB8-byte mesh block (35 records for the full-mesh block, 0 for proxy
blocks). On the free-game park the hook read **2221 bone records** and the
park rendered normally (`notes/hyb4-counter.txt`, `notes/park-hook-all.png`).
Per-call logging is disabled because the reader can run under the loader
lock; the counters print from `hook_4663a0` every 10,000 calls.

## Contract

```text
0047d880  SUB ESP,0x94
0047d886  PUSH EBX / EBP / ESI
0047d889  MOV ESI,[ESP+0xa4]   ; stream
0047d890  MOV EBX,ECX          ; this
0047d892  PUSH EDI
...
0047da93  MOV AL,1 / RET 4     ; true
```

`__thiscall bool f(SkinBoneVector* this /*ECX*/, mcStream* s /*[esp+4]*/)`,
`RET 4`, return in AL (1 = ok, 0 = fatal read error). Call site: `0x47251a`
at the end of `FUN_00472240`, `this = block+0xa8`, called once per 0xB8 block
from `FUN_00477f30` after reading the block count.

Stream interface: `s->vptr[2]` (`+0x8`),
`read(void* dst, u32 elem_size, u32 count) -> u32 got`.

## Read order

| # | Read | Size | Mode |
| --- | --- | --- | --- |
| 0 | record count | 4 | one shot; resize outer vector |
| 1 | bone name length + bytes | 1 × len | bulk; `reserve(len+1)`, then `strlen` |
| 2 | index count + u32 elements | 4 | per element; resize `vector<u32>` |
| 3 | weight count + f32 elements | 4 | per element; resize `vector<float>` |
| 4 | inverse-bind matrix | 64 | `0x4663a0(dst, stream)` |

No flags and no counts after the matrix. Name/index/weight read failures are
tolerated (the inner loop breaks and parsing continues); a matrix failure
returns 0.

## Runtime record (0x6C)

```text
+0x00 char* name      ; mcStringBasic: buf, capacity, length
+0x04 u32   capacity
+0x08 u32   length
+0x0C vptr  = 0x6256cc   ; vector<u32> indices
+0x10 u32* first / +0x14 last / +0x18 end
+0x1C vptr  = 0x625eac   ; vector<float> weights
+0x20 f32* first / +0x24 last / +0x28 end
+0x2C f32 inverse_bind[16]   ; 64 bytes
```

The outer container at `this` is the usual `{vptr, first, last, end}`; the
record dtor/copy is `FUN_0047c210`, which frees `[+0x20]` and `[+0x10]` and
copies 16 dwords from `src+0x2c`. Both inner containers must keep their
vptrs or the generated dtor crashes.

Read order matches `tools/formats/skin.py` / `src/formats/skin.cpp` exactly:
count, per-record name, indices, weights, matrix. Proxy blocks legally carry
a zero bone table (verified in `allo_lod1.skin` block 0).

## Replacement sketch

```cpp
struct SkinBoneRecord {
    McString name;          // buf/capacity/length at +0..+8
    IVec<uint32_t> indices; // vptr 0x6256cc + first/last/end
    IVec<float> weights;    // vptr 0x625eac + first/last/end
    float inverse_bind[16];
};
struct SkinBoneVector { void* vptr; SkinBoneRecord* first; SkinBoneRecord* last;
                        SkinBoneRecord* end; };

bool __thiscall hook_47d880(SkinBoneVector* self, mcStream* s) {
    uint32_t n;
    if (s->read(&n, 4, 1) != 1) return false;
    resize_skin_bones(self, n);        // 0x47c210 shrink / 0x47f8c0 grow
    for (uint32_t i = 0; i < n; ++i) {
        // name, indices, weights (helpers 0x4058b0/0x4056c0, 0x47a7c0,
        // 0x47daa0), then hook_4663a0(r->inverse_bind, s)
    }
    return true;
}
```

If hooking in place, reuse `0x47c210`/`0x47f8c0`, `0x47a7c0`/`0x47daa0`,
`0x4058b0`/`0x4056c0` and `0x4663a0` so the container vptrs and mcMemSTL
allocations stay valid.

## Confidence

- High: prologue/ABI, read order, record size/offsets, call site, zero-bone
  proxy blocks.
- Medium: the semantic identity of the two vectors (u32 indices vs f32
  weights) comes from the file/port, not the code; they are structurally
  identical.
- Open: the outer container's vtable and the block fields `+0x94..+0xa7`.

## Evidence

- `0x47d880` disassembly; `0x47c210` dtor/copy; `0x47f8c0` grow;
  `0x47a7c0`/`0x47daa0` vector resize; `0x4058b0`/`0x4056c0` string helpers.
- `0x47251a` call site; `FUN_00472240`, `FUN_00477f30`.
- `allo_lod1.skin` block bytes (read-only).
- `docs/decomp/hybrid.md`, `notes/hybrid-vector-recon.md`.
