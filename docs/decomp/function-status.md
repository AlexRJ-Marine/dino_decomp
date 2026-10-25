# Function status

Hybrid-decompilation tracker. Statuses:

- **identified** — address and purpose known.
- **decompiled** — Ghidra pseudo-C recovered and understood.
- **verified** — a parser/test or runtime trace confirms the behavior against
  retail data.
- **replaced** — a C++ replacement is live in the original exe and the running
  game exercises it.

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
| `0x004663a0` | 64-byte (16-float) matrix reader | **replaced** | `src/hook/dinput8_proxy.cpp`; 281k calls, park reached |
| `0x004809a0` | 12-byte (3-float) normal reader | **replaced** | same proxy; 95k calls, park reached |
| `0x0048b870` | `.skin` loader wrapper (`0x02468ace`) | identified | `.skin` doc |
| `0x00474320` | animation constructor (`0xdef01234`); vtable `0x625ec4` | identified | `.anim` doc |
| `0x00475990` | animation reader: duration/start, bone count, 0x2c records | decompiled, verified | 299/299 `.anim` files |
| `0x004839e0` | animation bone reader: name, `u64` key times, matrices | decompiled, verified | `.anim` doc |
| `0x00475e30` | skeleton constructor (`0x56789abc`) | decompiled | `.skel` doc |
| `0x00430fb0` | `mgFontDx8::LoadFile` (`.fnt` header/tables/glyphs) | decompiled, verified | 5/5 fonts + reference render |
| `0x00430ea0` | `mgFontDx8::CreateTexture` | identified | `.fnt` doc |
| `0x00477890` | skeleton reader: matrices, children, parents, names, flags | decompiled, verified | 64/64 `.skel` files |
| `0x00477610` | skeleton writer/export counterpart | decompiled (partial) | `.skel` doc |

## Overlay / menu (UI-2)

| Address | Purpose | Status | Evidence |
| --- | --- | --- | --- |
| `0x005bca20` | `moWidget::Load` — reads all INI properties, text/font fields and align bits | decompiled | `notes/ui-recon.md`, `docs/formats/overlay-ini.md` |
| `0x005bc000` | `moWidget::OnResize` — fixed/right/down/stretch/center anchoring pass | decompiled | same; 800→1024 MAD check |
| `0x005c5f00` | `moWidget::OnEvent` — property opcodes and flag setters (bits 9–15) | decompiled | same |
| `0x004227d0` | `moFontCtrl` font cache/loader; calls `mgFontDx8::LoadFile` | decompiled | same |
| `0x00517c60` | `mvwMenuStateManager` ctor — registers the nine menu pages in order | decompiled | same |
| `0x0051c510` | menu page base ctor (page index + display name) | decompiled | same |
| `0x0051bbf0` | `MAIN_MENU_PAGE` page ctor; binds seven buttons by name | decompiled | same |
| `0x0051b120` | `FREE_GAME_PAGE` page ctor; island list, difficulty, start | decompiled | same |
| `0x0051aa90` / `0x0051a560` / `0x00519ba0` | campaign select/info, load-game page ctors | decompiled (strings) | same |
| `0x00518830` / `0x00518210` / `0x00517fe0` / `0x00517e90` | options, cinematics, credits, loading page ctors | decompiled (strings) | same |
| `0x0051be70` … | button handlers; set `page + 0x18 = next state` | identified | transition table in `notes/ui-recon.md` |
| `0x00421630` | `moGraphicCtrl::Load` — GraphicDB entries, `NB_OBJECTS`, `SKIP_LIST` bitmask | decompiled | frame-strip section in `notes/ui-recon.md` |
| `0x004219c0` | graphic lookup by name (returns atlas index) | decompiled | same |
| `0x005c3a20` / `0x005c3a60` / `0x005c3a40` | widget texture index / `TEXTURE_FRAME` / `TEXTURE_MOSAIC` setters | decompiled | same |

## Runtime / save / startup

| Address | Purpose | Status | Evidence |
| --- | --- | --- | --- |
| `0x00401bfc` | CD-check branch patched for no-CD | verified | `docs/patches/no-cd.md` |
| `0x00598bad` | `SetCurrentDirectoryA` helper | decompiled | save-path investigation |
| `0x004160c0` | change working directory | decompiled | save-path investigation |

## C++ reimplementations

These are readable ports used by the portable viewer. The first one is now
hooked into the original exe (Phase 3 `HYB-1`).

| Original | C++ | Status |
| --- | --- | --- |
| `0x4663a0` matrix reader | `src/hook/dinput8_proxy.cpp` | **replaced live**, 281k calls |
| `0x4809a0` normal reader | `src/hook/dinput8_proxy.cpp` | **replaced live**, 95k calls |
| `0x477890` skeleton reader | `src/formats/skel.cpp` | reimplemented, tested on 80-bone allo |
| `0x47d880` skin bone records | `src/formats/skin.cpp` | reimplemented, 317 assignments |
| `0x475990` / `0x4839e0` animation reader | `src/formats/anim.cpp` | reimplemented, quaternion sampling |
| `.mesh` body reader | `src/formats/mesh.cpp` | reimplemented, `--validate` |
| `.tga` reader | `src/formats/tga.cpp` | reimplemented, 24/32/8-bit + RLE |
| `.skel`/`.skin`/`.anim` CPU skinning | `src/viewer/main.cpp` | reimplemented, VIEW-3 |

## Open items

- `.fnt`, `.loc`, `.lnd`, audio loaders — FMT-6/FMT-7.
- Overlay draw order (`PRIORITY`/`ZPRIORITY`) and button frame selection
  (`NB_OBJECTS`/`SKIP_LIST`) — UI-2 Phase A.
- Page-background creation helper and menu-page centring code path — UI-2.
- Map the simulation scheduler (`lpScheduler`) and AI update order — DEC-2.
- `.wld` save format — DEC-3.
