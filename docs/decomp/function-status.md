# Function status

Hybrid-decompilation tracker. Statuses:

- **identified** — address and purpose known.
- **decompiled** — Ghidra pseudo-C recovered and understood.
- **verified** — a parser/test or runtime trace confirms the behavior against
  retail data.

Addresses are virtual addresses in the unpatched `DinoIsland.exe` (image base
`0x00400000`). Names are Ghidra `FUN_*` unless a better name is known.

## Loaders / formats

| Address | Purpose | Status | Evidence |
| --- | --- | --- | --- |
| `0x004714b0` | generic `mgXFileImport` loader; calls vtable `+0x10` reader | decompiled, verified | used by `.skel`/`.skin`; `docs/formats/*.md` |
| `0x00477b50` | `mgVMeshXMdl` header reader (mesh/skin; magic/hash/sentinel/name) | decompiled | vtable `0x626aac`; `.skin` doc |
| `0x00477f30` | mesh/skin block-count + block-array reader | decompiled | `.skin` doc |
| `0x00472240` | per-block reader: name, vertices, indices, UVs, normals, colors, materials | decompiled | `.skin` doc |
| `0x0047d460` | `u16` per-triangle array reader | decompiled | `.skin` doc |
| `0x0047d880` | skin bone-record reader: name, indices, weights, matrix | decompiled, verified | 60 dinosaur skins |
| `0x004663a0` | 64-byte (16-float) matrix reader | decompiled, verified | `.skel` + `.skin` |
| `0x0048b870` | `.skin` loader wrapper (`0x02468ace`) | identified | `.skin` doc |
| `0x00474320` | animation constructor (`0xdef01234`); vtable `0x625ec4` | identified | `.anim` doc |
| `0x00475990` | animation reader: duration/start, bone count, 0x2c records | decompiled, verified | 299/299 `.anim` files |
| `0x004839e0` | animation bone reader: name, `u64` key times, matrices | decompiled, verified | `.anim` doc |
| `0x00475e30` | skeleton constructor (`0x56789abc`) | decompiled | `.skel` doc |
| `0x00430fb0` | `mgFontDx8::LoadFile` (`.fnt` header/tables/glyphs) | decompiled, verified | 5/5 fonts + reference render |
| `0x00430ea0` | `mgFontDx8::CreateTexture` | identified | `.fnt` doc |
| `0x00477890` | skeleton reader: matrices, children, parents, names, flags | decompiled, verified | 64/64 `.skel` files |
| `0x00477610` | skeleton writer/export counterpart | decompiled (partial) | `.skel` doc |

## Runtime / save / startup

| Address | Purpose | Status | Evidence |
| --- | --- | --- | --- |
| `0x00401bfc` | CD-check branch patched for no-CD | verified | `docs/patches/no-cd.md` |
| `0x00598bad` | `SetCurrentDirectoryA` helper | decompiled | save-path investigation |
| `0x004160c0` | change working directory | decompiled | save-path investigation |

## C++ reimplementations

These are readable ports used by the portable viewer. They are not yet hooked
into the original exe; that is Phase 3 (`HYB-1`).

| Original | C++ | Status |
| --- | --- | --- |
| `0x477890` skeleton reader | `src/formats/skel.cpp` | reimplemented, tested on 80-bone allo |
| `0x47d880` skin bone records | `src/formats/skin.cpp` | reimplemented, 317 assignments |
| `0x475990` / `0x4839e0` animation reader | `src/formats/anim.cpp` | reimplemented, quaternion sampling |
| `.mesh` body reader | `src/formats/mesh.cpp` | reimplemented, `--validate` |
| `.tga` reader | `src/formats/tga.cpp` | reimplemented, 24/32/8-bit + RLE |
| `.skel`/`.skin`/`.anim` CPU skinning | `src/viewer/main.cpp` | reimplemented, VIEW-3 |

## Open items

- `.fnt`, `.loc`, `.lnd`, audio loaders — FMT-6/FMT-7.
- Map the simulation scheduler (`lpScheduler`) and AI update order — DEC-2.
- `.wld` save format — DEC-3.
