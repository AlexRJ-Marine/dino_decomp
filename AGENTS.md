# Dino Island — reverse engineering / port project

## Goal

Recover the data formats and enough of the original logic to build a native,
modern-platform port of **Dino Island** (Monte Cristo, 2002), reusing the
locally extracted retail assets. Secondary goal: a readable C++ reconstruction
of the original architecture.

## Ground truth

- `original/` — read-only copies from the ISO (`SETUP.EXE`, `AUTORUN.EXE`, docs).
  Never edit, rename, or delete anything here.
- `game/%MAINDIR%/` — extracted retail tree (2,891 files, 397.5 MiB).
  Treat as read-only reference data. Do not "clean up" filenames.
- `notes/` — evidence and recon logs. Append findings; do not rewrite history.
- `tools/` — extraction, triage, and format scripts.
- `docs/formats/` — format specs as they are proven.
- `src/` — the port. Never copy extracted assets into the repo.

## Rules

1. Every format claim needs evidence: hex dump, decompiled function, or a parser
   that round-trips a real sample. Write it to `notes/` first, then `docs/formats/`.
2. Prefer small, verifiable steps: identify a header, parse one file, dump it,
   view it, then generalize.
3. Never touch `original/` or `game/` with write operations.
4. Keep the original binary as the oracle. When behavior is unclear, trace it
   (Process Monitor, API Monitor, x32dbg, Wine relay) instead of guessing.
5. Do not redistribute game assets or verbatim decompiled code. This is a
   personal preservation/interoperability project. Keep `game/` and `original/`
   out of version control.
6. Runtime patches live in `patches/`, are documented in `docs/patches/`, and
   are applied only to a copy under `run/`. Never patch `game/` or `original/`.
   The no-CD startup patch is `docs/patches/no-cd.md`.
7. Build portable C++20 for the port (CMake + SDL3 + bgfx/OpenGL), not MSVC 6.

## Known architecture (from assert strings)

- Platform: `mcLib` (core, graph `mg*`, input `mi*`, overlay `mo*`, sound `ms*`,
  math `mm*`/`lk*`) and `mcWorld` (`rs*`, `wd*`).
- Game: `APPLI`, `CORE` (`lpScheduler`), `DISPLAY` (`gsc*`), `VIEW` (`pvw*`,
  `ivw*`, `mvw*`), `AI` (`ap*`), `ANIM` (`aac*`, `anm*`), `SOUND` (`sdm*`,
  `sdp*`, `sdt*`), `SIMU` (`sp*`).
- Binary: PE32, MSVC 6.0, D3D8 + DirectDraw + DInput8 + DirectSound8, no DRM,
  no PDB, no RTTI.

## Immediate task order

1. Document `.mesh` / `.skin` / `.skel` / `.anim` headers; write a CLI dumper.
   (`.mesh` header done; see `docs/formats/mesh.md`.)
2. Export a skinned mesh to OBJ/glTF; validate in Blender/Noesis.
3. Map `.tga` / `.fnt` / `.loc` / `.lnd`; these are simple.
4. Build a minimal SDL3 + bgfx viewer for one mesh + texture.
5. Trace startup file I/O to confirm loader order.
6. Only then choose: reimplementation (recommended) vs. function-by-function
   Ghidra decompilation.

## Environment

- Python venv: `.venv/` (pyghidra-mcp, pefile, capstone, construct, pytest,
  pillow, cmake, ninja).
- Ghidra: `C:\Tools\ghidra_12.2_PUBLIC`; MCP at `http://127.0.0.1:8000/mcp`.
  Ghidra 12.2 requires JDK 25: `C:\Tools\jdk25\jdk-25.0.4.1+1`.
  OpenCode reaches it through `mcp-proxy` (stdio → HTTP); `opencode mcp list`
  should say `✓ ghidra connected`.
- C++: portable w64devkit in `C:\Tools\w64devkit`; CMake + Ninja from the venv.
- Verify: `scripts/check-env.ps1`; build: `scripts/build.ps1`;
  tests: `scripts/run-tests.ps1`; Ghidra: `scripts/start-ghidra.ps1`.

## Agent protocol

1. Read `notes/STATE.md` and `TASKS.md`.
2. Claim one task; write it under "In progress" in `notes/STATE.md`.
3. Do the smallest experiment that resolves the unknown.
4. Run the relevant tests/build.
5. Record evidence in `notes/` and `docs/formats/`.
6. Mark the task done in `TASKS.md`; update `notes/STATE.md` with the next action.
7. Hand off to `reviewer` for any non-trivial change.

## Definition of done

- Tests pass.
- The result works against real retail data, not a synthetic sample.
- The evidence path is recorded.
- No writes to `original/` or `game/`; no retail assets copied into `src/`.
- `notes/STATE.md` tells the next agent exactly what to do.
