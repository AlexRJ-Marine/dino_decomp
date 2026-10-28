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

## Working philosophy (recon loop)

The project advances by **small, evidence-first steps**, not by guesses or
big rewrites. The loop that works:

1. **Frame one question.** Example: "does the park camera roll?", "which
   field holds the fence perimeter price?". One question per pass.
2. **Find the oracle in the binary and data.** Search strings and xrefs, read
   the vtable, decompile or disassemble the exact function, and correlate
   with the retail files (`.ini`, `.wld`, `.bfi`, `.shw`, ...). UI labels,
   names and memory of the game are not evidence.
3. **Write raw evidence first.** Append addresses, offsets, raw bytes,
   decompiled snippets and confidence to `notes/<topic>.md`. Then distil the
   proven part into `docs/formats/` or `docs/decomp/`, with an explicit
   "Open" list for what is still uncertain.
4. **Name semantics from data.** Use the INI `HINT`/localisation ids, the
   serialized values and runtime captures to name fields (for example
   `&HerbiFoodSufficiency` is food sufficiency, not a count; `0x17` is the
   wall element type). If a name cannot be proven, keep the offset.
5. **Prefer a negative result to a guess.** "No roll; the up vector is
   constant +Z" and "no wheel binding" are useful, testable conclusions.
   Record how they were tested (store census, xrefs, probe).
6. **Verify on real retail data.** A parser must round-trip a real sample; a
   rendering claim needs a retail capture or a probe; a gameplay claim needs
   a test on a real park. Synthetic samples are scaffolding only.
7. **Leave the next agent a map.** Separate proven facts, port implications
   and open questions. Put every load-bearing claim in
   `notes/evidence-index.md` with its test or artifact.
8. **Protect the shared tree.** Edit only your lane's files, stage explicit
   paths, commit immediately after a verified slice, and never sweep another
   agent's dirty files into your commit. If a file is being edited elsewhere,
   work on the adjacent module or docs and record the collision.

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

Many contributors on different machines work in this repo. Tracked files must
not contain a user name, home directory or fixed tool path.

- New machine: `scripts/setup-tools.ps1` installs the pinned toolchain under
  `$env:DINO_TOOLS` (default `C:\Tools`). Scripts find tools only through
  `scripts/tools.ps1` (`Find-W64Devkit`, `Find-Sdl3`, `Find-Jdk25`,
  `Find-Ghidra`, `Get-X32dbgHeadless`); use it in any new script.
- Python venv: `.venv/` (pyghidra-mcp, pefile, capstone, construct, pytest,
  pillow, cmake, ninja).
- Ghidra 12.2 + JDK 25 under the tools root; MCP at `http://127.0.0.1:8000/mcp`.
  OpenCode reaches it through `mcp-proxy` (stdio → HTTP); `opencode mcp list`
  should say `✓ ghidra connected`. A project copied from another user needs
  `scripts/start-ghidra.ps1 -TakeOwnership`.
- C++: w64devkit under the tools root; CMake + Ninja from the venv.
- Verify: `scripts/check-env.ps1`; build: `scripts/build.ps1`;
  tests: `scripts/run-tests.ps1`; Ghidra: `scripts/start-ghidra.ps1`.

## Runtime testing (default: windowed, non-intrusive)

Normal play:

```powershell
.\scripts\play-windowed-game.ps1
```

This uses `run-window/` (Data junction, own `save/`), a normal bordered
800x600 window, and `DINO_MOUSE_CLICK_CAPTURE=1`: mouse input is suppressed
until you click inside the game; click outside to release. The game starts
behind the current window and does not take focus.

The x32dbg harness now defaults to **windowed + virtual input**:

```powershell
.\scripts\x32dbg\play-freegame.ps1 -Seconds 30
.\scripts\x32dbg\start-campaign.ps1 -UnlockCampaigns
```

It uses `run-test/`, sends the game to the back, and restores the pre-run
foreground window. Screenshots and window placement are background-safe, and
clicks go through `DINPUT8.dll` + `run-test/dino_input.txt` without
`SetCursorPos`/`mouse_event`. **Input caveat:** retail sets DirectInput to
`DISCL_FOREGROUND`; when the game is fully inactive it logs
`virtual mouse: state calls=0` and ignores virtual clicks. Keep the game
active for a click test until the proxy shims activation.

- Rebuild the hook with `scripts\build-hook.ps1` after proxy changes.
- First run or new checkout: `scripts\setup-test-runtime.ps1` (and
  `-Target run-window` for normal play).
- Screenshots go to the gitignored `notes/harness/` by default
  (`-ShotDir` override). Do not overwrite tracked `notes/*.png` evidence.
- `-PhysicalInput` is the legacy fullscreen + physical-mouse path. Use it only
  when a test genuinely cannot work with virtual input.
- Do not add new `SetCursorPos`/`mouse_event`/foreground calls to harness
  scripts.
- **UI click map:** `docs/platform/ui-coordinates.md` lists every interactive
  control on every screen; `docs/platform/ui-coordinates.json` is the
  machine-readable form. Query it with
  `.venv\Scripts\python.exe tools\formats\ui_map.py find <NAME>` (add
  `--screen <id>` or `--active`). Do not hand-guess coordinates.
- Full details: `docs/platform/windowed-testing.md`.

## Agent protocol

1. Read `notes/NOW.md` (the compact injected dashboard), then
   `notes/OPEN-TASKS.md`. Open `TASKS.md` / `notes/STATE.md` only for the task
   you claim; they are the long archive.
2. Claim a lane and a build directory in `notes/lanes.md` (for example
   `build/port`); write the task under "Focus" in `notes/NOW.md`.
3. Do the smallest experiment that resolves the unknown.
4. Run the relevant tests/build: `scripts/build.ps1 -BuildDir build/<lane>`
   and `scripts/test.ps1 -Exe build/<lane>/dino_viewer.exe -Fast`.
5. Record evidence in `notes/` and `docs/formats/`; add a row to
   `notes/evidence-index.md` when the claim becomes load-bearing.
6. Mark the task done in `TASKS.md`; run
   `scripts/state.ps1 -Done "one line"` and `scripts/tasks.ps1 -Update` so
   `NOW.md` / `OPEN-TASKS.md` stay current.
7. Run `scripts/verify.ps1 -Fast` before committing; hand off to `reviewer`
   for any non-trivial change.
8. Use the project skills for repeatable loops (`dino-hook`, `dino-format`,
   `dino-verify`, `ghidra-query`, `dino-retail-run`) and the `/verify`,
   `/handoff`, `/next` commands where they fit.
9. Batch independent tool calls in one message (build/test, greps, log reads)
   and run independent research in a background subagent while the main lane
   continues. Use `triage` for failure logs and `archivist` for state
   compaction instead of doing either in the primary session.

## Definition of done

- Tests pass.
- The result works against real retail data, not a synthetic sample.
- The evidence path is recorded.
- No writes to `original/` or `game/`; no retail assets copied into `src/`.
- `notes/STATE.md` tells the next agent exactly what to do.
