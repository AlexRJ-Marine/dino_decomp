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

## Next action

1. Decode the `.skel`/`.skin` virtual read methods in Ghidra:
   vtables `0x625f0c` (skel) and `0x625e8c` (skin); types `0x56789abc`
   and `0x02468ace`.
2. Decode the 6,514-byte full-mesh tail as the skin map.
3. Re-fit the skeleton transform after the layout is known.
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
