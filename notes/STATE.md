# State

Updated: 2026-10-25

## Environment

- Python: `.venv/` (pyghidra-mcp, pefile, capstone, construct, pytest, pillow, cmake, ninja)
- Ghidra: `C:\Tools\ghidra_12.2_PUBLIC` + JDK 25 at `C:\Tools\jdk25\jdk-25.0.4.1+1`
- Ghidra MCP: `http://127.0.0.1:8000/mcp` (headless, project `analysis/ghidra`)
- C++: portable w64devkit MinGW-w64 in `C:\Tools\w64devkit`
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
  `dino_tools.exe`, and all 20 tests pass.
- FMT-4 complete: **`.skel` is solved.**
  - 64-byte bone record is a row-major 4×4 float matrix (3ds Max row-vector
    convention): rotation/scale in the upper 3×3, translation in row 3.
  - Layout: header to `0x0D`, `N × 64` matrices from `0x11`, child lists,
    explicit parent table, names, flags.
  - The reader is `0x477890` (vtable `0x625ee8`, slot `+0x10`), reached via
    the generic loader `0x4714b0`. It reads matrices, child lists, parents,
    names, flags in exactly the file's order. `0x4663a0` is the 64-byte block
    reader; `0x477610` is the writer/export counterpart.
  - Compose `world = local @ parent_world`. Root `Scene_Root` is `0.01 × I`,
    so allosaurus bounds are `(1.814, 10.189, 5.317)` metres.
  - 64/64 files parse, 2,113 bones; 20 dinosaurs × 80 bones. Use the parent
    array, not the child lists: 44 prop files store child indices one lower.
  - Evidence: `tools/formats/skel.py`, `docs/formats/skel.md`,
    `tests/test_skel.py`, `notes/skel-allo-views.png`.
- FMT-3 complete: **`.skin` is solved.**
  - 35 per-bone proxy blocks + one full-mesh block. The full-mesh tail carries
    a material list, a per-triangle `u16` material array, and 35 skin records.
  - Each skin record = bone name + vertex-index list + weight list + 64-byte
    inverse-bind matrix. All 60 dinosaur skins (20 × 3 LOD) partition every
    full-mesh vertex across the 35 bones; every weight is `1.0`.
  - Rigid skinning in centimetres:
    `v_cm @ inverse_bind @ W_raw_anim(bone)`, then `× 0.01` for metres.
    `inverse_bind` is the inverse of the raw world matrix (root scale removed).
  - `.skin` full-mesh vertices are in centimetres; the `.mesh` is in metres.
  - Verified on `allo_lod1`: max bind error `5.7e-5` cm; a 35° head rotation
    deforms only the head/neck. 60/60 dinosaur skins bind correctly at LOD1.
  - LOD2/LOD3 mostly bind correctly; `pachy` LOD2/LOD3 store a different
    per-LOD bind pose, so animation retargeting for those remains open.
  - Evidence: `tools/formats/skin.py`, `docs/formats/skin.md`,
    `tests/test_skin.py`, `notes/allo-skin-bind.png`,
    `notes/allo-skin-pose.png`.
- Allosaurus retail mesh rendered by our own parser/rasterizer:
  `notes/allo-3q-vc-flip.png`, `notes/allo-side-vc-flip.png`,
  `notes/allo-head-flip.png`. Uses `allo_lod1.mesh` (317 v / 382 tri),
  `Data/Graphics/Textures/actors/dinos/allo.tga`, per-vertex RGBA colors from
  the mesh tail, and UV `v_tex = 1 + v` (engine UVs are bottom-left origin).
- Save failure fixed: `run\save` and `run\save\auto` did not exist; the game
  never creates directories. After creating them, saving writes
  `run\save\slot001.sav` and autosaves. `run/` is gitignored.
- No-CD startup patch found and verified: drive scan `C:`..`Z:` calls
  `GetDriveTypeA`/`GetVolumeInformationA`; branch at `0x401BFC` patched from
  `jne 0x401D4C` to `jmp 0x401D4C`. Documented in `docs/patches/no-cd.md`
  with IPS at `patches/dinoisland-nocd.ips` and `tools/patch_nocd.py`.
- Patched `run/` copy passes the CD check under x32dbg + dgVoodoo2, reaches
  `Begin3D Success`, and loads menu assets.
- Game is runnable for dynamic testing:
  - `run/DinoIsland.exe` patched with the no-CD startup patch.
  - dgVoodoo2 x86 D3D8/DDraw wrappers in `run/`.
  - `run/Data/Config/Dino.ini` has `UseIntroVideo=0`.
  - Free Mode -> IGUANO -> easy -> Start reaches a playable park.
  - Screenshots: `notes/menu-main.png`, `notes/menu-freegame-selected.png`,
    `notes/park-after2.png`, `notes/save-panel.png`.
- x32dbg harness works headless: `scripts/x32dbg/play-freegame.ps1`
  launches, clicks Free Mode/IGUANO/easy/Start, opens Options -> Save, types a
  name, and clicks the green check.
- Live trace confirms asset loading:
  - `entrance_lod1.skel`, `.skin`, `.anim`, `.mesh`
  - customer/person `.mesh` LODs
  - dinosaur animation `.anim` files (LowQuadruped, etc.)

## In progress

- FMT-5: `.anim` animation. 299 files. Next task: find the animation reader
  vtable/loader, decode track/keyframe layout, and apply a clip to the 80-bone
  `allo` skeleton. The skinning path is ready: rigid, one bone per vertex.
- FMT-3b follow-up (not blocking): retarget skeleton animation onto LOD2/LOD3
  bind poses that differ from the `.skel` bind (notably `pachy`).

## Next action

1. FMT-5: decompile the `.anim` loader (same 5-byte magic; search for the
   animation class vtable and the reader hit by `FUN_004714b0`).
2. Parse one `allo` clip; identify bone name, keyframe count, times, and
   transform values. Validate all 299 files.
3. Apply the clip to `allo.skel`, drive the 35 skin records, and render a
   few frames with the existing software rasterizer.
4. VIEW-1/2: SDL3 + OpenGL viewer with the allosaurus mesh + texture; then
   skinned animation playback.

## Blockers

- None.

## Decisions

- Recommended path is a **reimplementation** with original assets, not a full
  binary-to-source decompilation. Decompilation is used for targeted algorithms.
- Full decompilation remains the long-term goal; a playable vertical slice
  comes first. First playable target: one island, camera, path/fence, one
  animated dinosaur, 10-minute session.
- Use headless `pyghidra-mcp` over HTTP rather than GUI GhidraMCP; the agent
  environment must be reproducible and unattended.
