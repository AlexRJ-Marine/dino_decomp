# `.fnt` loader `0x430fb0` (Ghidra)

Status: **implemented and runtime-verified.** `mgFontDx8::LoadFile` reads the
header, code table and metrics and builds the glyph map; the pixel atlas is
fetched lazily by `0x4316e0` (`LoadChar`), which reopens the stored path.

## Contract

```text
00430fb0  SUB ESP,0x240
00430fb6  PUSH EBX / EBP / ESI / EDI
00430fba  MOV ESI,ECX      ; this
...
```

`void __thiscall LoadFile(mgFontDx8* this, const mcString* path)`, `RET 4`,
no return value. Calls: `0x4228bd` in `FUN_004227d0` (`GetFont`) and
`0x413888` in `mcLibBegin` (`Data/Fonte/Font.fnt`). Not virtual; font vtable
`0x6256dc` does not contain it. Failure only asserts.

## Reads

`Read(dst, size, count)` is the file vtable `+0x8` (`0x417db0`); opened via
`0x417d10(path, "rb")`.

| # | Size | Destination | arial12 |
| --- | --- | --- | --- |
| 1 | u32 version | local (must be 1) | 1 |
| 2 | u32 atlas width | local (dead) | 256 |
| 3 | u32 atlas height | local (dead) | 256 |
| 4 | u32 font size | `this+0x10` | 12 |
| 5 | u32 cell height | `this+0x20` | 10 |
| 6 | 2×u32 | `this+0x18` (max), `+0x1c` (if zero) | 9, 12 |
| 7 | u32 count + count×u16 | `vector<u16>` codes | 247 |
| 8 | u32 count + count×8 | `vector<24-byte>` metrics | 247 |
| 9 | count×8 (4×u16 x0/y0/x1/y1) | metric records | — |

**Quirk:** the 9th header dword (hash) is never skipped. Reads #1–#6 consume
28 bytes (`0x00..0x1b`), read #7 takes the count at `0x1c`, and the code
table starts at `0x20` — the hash dword. The runtime codes are therefore
`[hash_lo, hash_hi, code[0]..code[244]]`, which exactly explains the port's
`metrics[i+2]` mapping and the two bogus leading code points. The last two
padding code words (`247, 0`) are consumed as the next metric count.

## Runtime object

```text
+0x00 vtable 0x6256dc
+0x04 mcString name
+0x10 font size        +0x14 int 0
+0x18 cell pitch X     +0x1c cell pitch Y
+0x20 cell height
+0x24 current mgTexture* +0x28 glyphs used +0x2c capacity
+0x2c capacity = (texH/pitchY)*(texW/pitchX) = 21*28 = 588
+0x30/+0x34 texture W/H (caller-supplied 256, not the .fnt fields)
+0x38 float 1.0        +0x3c max textures (3)
+0x40 hash map u16 -> glyph node (0x38: code, 24-byte metric, mcString path)
+0x58 vector<mgTexture*>
```

The ctor `0x42fd30` zeroes `+0x30/+0x34`; the caller's `0x4301d0` sets both to
the square atlas size (256) and `+0x3c` to 3. `LoadFile` only reads them to
compute `+0x2c`. No code/metric arrays are kept; they are converted into the
map. Helpers: `0x432920` (u16 vector resize), `0x432b10` (24-byte vector
resize), `0x432010` (read metric), `0x432fd0` (map insert), `0x432d70` (glyph
ctor), `0x4316b0` (glyph zero), `0x430ea0` (texture create).
`0x4316e0` (`LoadChar`) reopens the path stored in the glyph record, creates
the texture when full and blits 16-bit rows; the atlas is not read here.

Map at `font+0x40` (`0x14` bytes): 3 seeded bytes, bucket vector at
`+0x44/+0x48/+0x4c`, count at `+0x50`, texture-array vtable at `+0x54`.
Node (`0x38`): `+0x00 next`, `+0x08 u16 key`, `+0x10` value (24-byte metric,
mcString at `+0x28`, texture index at `+0x34`). `0x432fd0` takes a pointer to
a `0x30` insertion record (`u16 key`, pad, 24-byte metric at `+0x8`, mcString
at `+0x20`, u32 at `+0x2c`) and returns `node+8`; it never compares keys, so a
duplicate key only adds an unreachable node.

## Hook

Target: the 12-byte entry (`SUB ESP,0x240`; push EBX/EBP/ESI/EDI;
`MOV ESI,ECX`); no vtable slot. `hook_430fb0` in
`src/hook/dinput8_proxy.cpp` reads the file directly (`CreateFileA`), parses
the header and tables, writes `+0x10/+0x18/+0x1c/+0x20`, computes `+0x2c`,
and inserts every code/metric with `0x432d70` + `0x432fd0`.

The hooked code table is read from file `0x20`, i.e.
`[hash_lo, hash_hi, code[0]..code[count-3]]`, paired with `metrics[0..]`;
that reproduces both the port's `codes[i] -> metrics[i+2]` mapping and the two
bogus leading code points. `metric_count` is the u32 at
`0x20 + count*2` and must equal `count`; anything else is rejected (the font
stays empty rather than reading out of bounds). The glyph value's `+0x10/+0x14`
dwords come from a device query in retail and are left 0; the HUD renders.

## Verification

Free-game park (`notes/hyb7-counter.txt`):

```text
counters: mesh=121 skin=2221 anim=8911 skel=21 vec16=2221 vec32=1680 fnt=5
```

Five font files (`Font.fnt`, `Font12`, `Font14`, `Font19`, and the menu
reloads) each insert 247 glyphs, and the park HUD clock, budget, dino count
and panel labels render (`notes/park-hook-fnt.png`). Installed by default.

## Confidence

- High: signature/reads/call sites, entry detour, hash shift (independently
  predicts the port's `metrics[i+2]` mapping), 588-cell grid.
- Lower: the exact pixel seek base in `0x4316e0` (unchanged by this hook, since
  the lazy path stays retail code and the stored path points at the same file).

## Evidence

- `0x430fb0` disassembly; `0x4228bd`, `0x413888` calls; `0x417d10`/`0x417db0`
  file read; `0x432920`, `0x432b10`, `0x432010`, `0x432fd0`, `0x432d70`,
  `0x4316b0`, `0x430ea0`, `0x4316e0`.
- `Data/Fonte/arial12.fnt` bytes (read-only).
- `docs/formats/fnt.md`, `src/formats/fnt.cpp`.
