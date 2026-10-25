# State

Updated: 2026-10-25

## Environment

- Python: `.venv/` (pyghidra-mcp, pefile, capstone, construct, pytest, pillow, cmake, ninja)
- Ghidra: `C:\Tools\ghidra_12.2_PUBLIC` (download/extract in progress)
- Ghidra MCP: `http://127.0.0.1:8000/mcp` (headless, project `analysis/ghidra`)
- C++: portable w64devkit MinGW-w64 in `C:\Tools\w64devkit` (install in progress)
- Build: CMake + Ninja from the venv
- Extracted game: `game/%MAINDIR%/` (2,891 files, 397.5 MiB)
- Original media: `original/` (read-only)

## Done

- ISO inventoried (50 files, 297.7 MiB).
- Wise installer extracted with REWise (2,891 files).
- `DinoIsland.exe` triaged: MSVC 6.0, D3D8 + DirectDraw + DInput8 + DirectSound8,
  no DRM, no PDB, no RTTI.
- Engine module map recovered from assert strings.
- Asset inventory and sample headers recorded in `notes/FINDINGS.md`.
- Agent harness created: `opencode.jsonc`, `.opencode/agents/`, `TASKS.md`.
- FMT-1 complete: `.mesh` header parser scans all 536 files with 0 errors
  (`notes/mesh-scan.json`, `docs/formats/mesh.md`).
- FMT-2 complete: full `.mesh` geometry decoded and OBJ exporter written.
  536/536 files parse, 47,171 vertices, 35,190 triangles, 0 errors
  (`tools/formats/mesh.py`, `docs/formats/mesh.md`, `tests/test_mesh.py`).
- Ghidra 12.2 + JDK 25 + pyghidra-mcp running on port 8000 with
  `DinoIsland.exe` imported (`/DinoIsland.exe-ebea17`).
- Environment verified end-to-end: `scripts/check-env.ps1` passes,
  `opencode mcp list` shows `✓ ghidra connected`, `scripts/build.ps1` builds
  `dino_tools.exe`, and all 9 tests pass.

## In progress

- FMT-3: `.skin` reader complete (`tools/formats/skin.py`). 104 files parse;
  2,244 bone blocks; 73 full meshes match their `.mesh`. `allo_lod1` has
  35 bone blocks (1,105 verts / 614 tris) + full mesh `dino_lod1`
  (317 verts / 382 tris, exact `.mesh` match).
- FMT-4: `.skel` reader complete (`tools/formats/skel.py`). 64 files parse,
  2,113 bones; `allo` = 80 bones / 0.01 global scale.
- Transform experiment: chain-fit best is 3x3 record[0:9] + translation
  record[16:19] at 0.01 scale, but the assembled render is a cross, so the
  layout is not yet correct. Full-mesh tail is 6,514 bytes and likely holds
  the skin mapping.
- No-CD startup patch found and verified: drive scan `C:`..`Z:` calls
  `GetDriveTypeA`/`GetVolumeInformationA`; branch at `0x401BFC` patched from
  `jne 0x401D4C` to `jmp 0x401D4C`. Documented in `docs/patches/no-cd.md`
  with IPS at `patches/dinoisland-nocd.ips` and `tools/patch_nocd.py`.
- Patched `run/` copy passes the CD check under x32dbg + dgVoodoo2, reaches
  `Begin3D Success`, and loads menu assets. `run/` is gitignored.

- Game is now runnable for dynamic testing:
  - `run/DinoIsland.exe` patched with the no-CD startup patch.
  - dgVoodoo2 x86 D3D8/DDraw wrappers in `run/`.
  - `run/Data/Config/Dino.ini` has `UseIntroVideo=0`.
  - Free Mode -> IGUANO -> easy -> Start reaches a playable park.
  - Screenshots: `notes/menu-main.png`, `notes/menu-freegame-selected.png`,
    `notes/park-after2.png`.
- x32dbg harness works headless: `scripts/x32dbg/play-freegame.ps1`
  launches, clicks Free Mode/IGUANO/easy/Start, and logs loader hits.
- Live trace confirms asset loading:
  - `entrance_lod1.skel`, `.skin`, `.anim`, `.mesh`
  - customer/person `.mesh` LODs
  - dinosaur animation `.anim` files (LowQuadruped, etc.)
- Skeleton transform remains open. The `.skel` body is 80 × 64-byte blocks
  plus more sections, but the 16 floats are not a standard 4×4 matrix; the
  reader at `0x477610` is not hit in the `.skel` path, and vtable probing only
  hit `0x43b8f0`. The next target is the generic loader/64-byte reader or a
  live object dump after a dinosaur is spawned.

## Next action

1. Spawn a dinosaur in the running park (lab or campaign) so `allo.skel`/
   `.skin` and a full 80-bone skeleton are live.
2. Break on `0x4714b0` / `0x466380` and dump the live skeleton object after
   the load; compare its 64-byte blocks with the file.
3. Decode the 6,514-byte full-mesh tail as the skin map.
4. Then FMT-5 (`.anim`) and VIEW-1 (SDL3 viewer).

## Blockers

- None yet.

## Decisions

- Recommended path is a **reimplementation** with original assets, not a full
  binary-to-source decompilation. Decompilation is used for targeted algorithms.
- First playable target is a vertical slice (one island, camera, path/fence,
  one animated dinosaur, 10-minute session), not full parity.
- Use headless `pyghidra-mcp` over HTTP rather than GUI GhidraMCP; the agent
  environment must be reproducible and unattended.
