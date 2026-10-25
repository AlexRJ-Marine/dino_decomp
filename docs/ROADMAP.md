# Roadmap

Four phases. The goal is a playable, portable game and a readable hybrid
decompilation that keeps the original runnable while functions are replaced.

## Phase 1 — Formats and assets (current)

- Use Ghidra to locate the archive/file loaders and document every format with
  evidence: hex dumps, decompiled functions, or a parser that round-trips retail
  data.
- Write extractors and tests against real files.
- Build a simple viewer for models, textures and sounds so decoded data can be
  checked by eye.
- Post findings to XeNTaX / ZenHax and ask whether anyone has prior work.

Exit criteria:

- `.mesh`, `.skin`, `.skel`, `.anim`, `.tga`, `.fnt`, `.loc`, `.lnd`, audio.
- All parsers scan their full retail sets with zero errors.
- A model/texture/sound viewer opens real assets.
- Community post published (or drafted for the user to publish).

### Task mapping

| Task | Status |
| --- | --- |
| FMT-1 `.mesh` header | done |
| FMT-2 `.mesh` geometry | done |
| FMT-3 `.skin` | done |
| FMT-4 `.skel` | done |
| FMT-5 `.anim` | **next** |
| FMT-6 simple asset loaders | todo |
| FMT-7 text/config formats | todo |
| VIEW-0 simple asset viewer | todo |
| OUTREACH-1 XeNTaX/ZenHax post | todo |

## Phase 2 — Platform layer (weeks)

- Replace the original renderer, audio and input with SDL3 plus OpenGL or
  Vulkan.
- Start by drawing the extracted assets; then the world, then animation.
- Add quality-of-life work while it is cheap: resolution/widescreen, borderless
  window, UI scaling, key rebinding.

Exit criteria:

- Portable window + input + audio + renderer.
- One mesh + texture renders; one skinned dinosaur animates.
- QoL features documented and configurable.

### Task mapping

| Task | Status |
| --- | --- |
| VIEW-1 SDL3 window | todo |
| VIEW-2 mesh + texture | todo |
| VIEW-3 skinned animation | todo |
| QOL-1 resolution/widescreen/borderless | todo |
| QOL-2 UI scaling | todo |
| QOL-3 key rebinding | todo |

## Phase 3 — Hybrid decompilation (months)

- Hook the original `DinoIsland.exe` and replace functions one at a time with
  recovered C++ source, keeping the game runnable throughout.
- Work from the outside in: loaders and utility code first, then simulation
  subsystems.
- Track progress as a function list with status per function:
  `identified`, `decompiled`, `verified`.

Exit criteria:

- Every replaced function is exercised by the running game.
- The function list records evidence and status.
- Original and replacement can be toggled for comparison.

### Task mapping

| Task | Status |
| --- | --- |
| DEC-1 loader map | in progress (mesh/skin/skel done) |
| DEC-6 function status list | todo |
| HYB-1 hook + replace first loader | todo |
| WORLD/SIM/UI/SAVE tasks | later verification targets |

## Phase 4 — Remove the original exe and expand

- Once nothing calls the original code, drop the exe and retarget other
  platforms.
- Optional: matching decompilation for a byte-identical rebuild.

Exit criteria:

- No runtime dependency on `DinoIsland.exe`.
- Portable builds for other platforms.
- Optional matching-decomp milestone tracked separately.
