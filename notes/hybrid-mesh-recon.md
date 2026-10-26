# `.mesh` block reader `0x477f30` (Ghidra)

Status: **ABI solved; hook implemented and runtime-verified.** This is the
whole-file `.mesh`/`.skin` block reader; one call reads the block count and
every 0xB8 block. Free-game park: **121 `.mesh` files** with normal
rendering (`notes/hyb6-counter.txt`, `notes/park-hook-mesh-default.png`).

## Contract

```text
00477f30  81 EC C8 00 00 00   sub esp,0xC8
00477f36  53 56 8B F1 57      push ebx; push esi; mov esi,ecx; push edi
...
004780e4  B0 01 ... C2 08 00  mov al,1; add esp,0xC8; ret 8
```

`char __thiscall mgVMeshXMdl::ReadBlocks(mcFile* f /*[esp+4]*/, mcString* name
/*[esp+8]*/)`; ECX = this, `RET 8`, return in AL. Failure returns 0 with the
block vector partly filled.

Dispatch: vtable `0x626aac` slot `+0x10`, called from `0x4714b0` at
`0x471ac6` (`CALL [EDX+0x10]`). The class is shared by `.mesh` and `.skin`;
`this+4` is the magic: `0x12345678` mesh, `0x02468ACE` skin
(`0x48b800`/`0x48b870`; ctor `0x477b50`).

## Read order

1. `u32 count` (`f->v[+8](&count,4,1)`), resize the `0xB8`-block vector.
2. Per block, `FUN_00472240(block, f, name)` (stride 0xB8, no count cap).

`FUN_00472240` (`ret 8`, arg2 unused) reads:

| # | Field | Element |
| --- | --- | --- |
| 1 | name length + bytes | 1 |
| 2 | vertex count + positions | 12 (3 f32) |
| 3 | index count + indices | 4 u32 |
| 4 | UV flag | 1 byte |
| 5 | UV count + UVs | 8 (2 f32) |
| 6 | normal flag | 1 byte |
| 7 | normal count + normals | 12, per element `0x4809a0` |
| 8 | color flag | 1 byte |
| 9 | color count + RGBA | 4, per element `0x472530` |
| 10 | material count + 0x30 records | `0x472580` (21 bytes + 2 names) |
| 11 | triangle count + u16 | `0x47d460` (data at +0x7c) |
| 12 | 3 × u32 | `+0x88/+0x8c/+0x90` (1, -1, 35) |
| 13 | `.skin` bone table | `0x47d880` at `+0xa8` |

The u16 triangle array is the penultimate section, after materials; the first
field is the block name, not normals.

## Runtime layout

`mgVMeshXMdl`: `+0x00` vtable `0x626aac`, `+0x04` magic, `+0x08` mcString type
name, `+0x14` pointer to the block container. Container triple at `+0x04/
+0x08/+0x0c`, element `0xB8`.

0xB8 block (ctor `0x471bd0`):

```text
+0x00 mcString name (len +0x08)
+0x0C vector<Vec3>   positions, data +0x10, 12 B
+0x1C vector<u32>    indices, data +0x20, 4 B
+0x2C u8             uv flag
+0x30 vector<Vec2>   uvs, data +0x34, 8 B
+0x40 u8             normal flag
+0x44 vector<Vec3>   normals, data +0x48, 12 B
+0x54 u8             color flag
+0x58 vector<u32>    colors, data +0x5C, 4 B (RGBA)
+0x68 vector<Material> materials, data +0x6C, 0x30 B
+0x78 vector<u16>    triangles, data +0x7C
+0x88/+0x8C/+0x90    three u32 (1, -1, 35)
+0x98                unproven container
+0xA8 vector<SkinBone> data +0xAC, 0x6C B
```

Helpers: block grow/shrink `0x47dd70`/`0x47e560` (default `0x471bd0`);
array resize `0x47b060` (12 B), `0x47a7c0` (4 B), `0x47b9e0` (8 B),
`0x47f1d0` (4 B colors), `0x479a80` (0x30 B), plus the hooked `0x47d460`
and `0x47d880`.

## Hook target

Three options:

1. **Vtable patch** at `0x626abc` (slot `+0x10`) — only reference to
   `0x477f30`. Needs `VirtualProtect` on `.rdata`; wrapper checks
   `this+4 == 0x12345678` and forwards `.skin` to the original.
2. **6-byte entry patch** `81 EC C8 00 00 00` — `E9 rel32` + NOP. The
   epilogue is `add esp,0xC8; ret 8`, so the replacement must restore ESP
   itself (or use a trampoline starting at `sub esp,0xC8; jmp 0x477f36`).
3. **Per-block patch** `0x472240`, prologue `83 EC 0C 55 56` = 5 bytes,
   `E9 rel32` clean. The hook must reproduce the count read and block-vector
   resize.

Because `.mesh` and `.skin` share the class, either path must branch on the
magic and forward `.skin` to the original (the `.skin` readers are already
hooked, so forwarding the block loop is fine).

## Confidence

- High: signature, prologue, dispatch, block count, `0x472240` order/element
  sizes, `0x47d460` position, 0xB8 field map.
- Medium: `arg1` is an `mcString*` that `0x472240` never uses; the
  `+0x88/+0x8C/+0x90` semantics and `+0x98` container are unproven; the
  stream type name "mcFile" is inferred from `v[+8]` returning counts.

## Verification

The vtable slot `0x626abc` was repointed to `hook_477f30` (the `0x477f30`
code entry is untouched, so `.skin` is forwarded to the original). On the
free-game park:

```text
counters: mesh=121 skin=2221 anim=8911 skel=21 vec16=2221 vec32=1680
```

All 121 mesh files loaded and the park rendered normally
(`notes/park-hook-mesh-default.png`). Installed by default.

## Evidence

- `0x477f30` disassembly/epilogue; `0x471ac6` dispatch; `0x477b50` ctor;
  `0x48b800`/`0x48b870` magic selection.
- `0x472240` read order; `0x471bd0` block ctor; `0x47dd70`/`0x47e560`,
  `0x47b060`, `0x47a7c0`, `0x47b9e0`, `0x47f1d0`, `0x479a80`.
- `docs/formats/mesh.md`, `docs/formats/skin.md`,
  `notes/hybrid-skin-recon.md`, `notes/hybrid-vector-recon.md`.
