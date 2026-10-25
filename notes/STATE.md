# State

Updated: 2026-10-25

## Environment

- Python: `.venv/` (pyghidra-mcp, pefile, capstone, construct, pytest, pillow, cmake, ninja)
- Ghidra: `C:\Tools\ghidra_12.2_PUBLIC` + JDK 25 at `C:\Tools\jdk25\jdk-25.0.4.1+1`
- Ghidra MCP: `http://127.0.0.1:8000/mcp` (headless, project `analysis/ghidra`)
- C++: portable w64devkit MinGW-w64 in `C:\Tools\w64devkit`
- SDL3: 3.4.18 MinGW under `C:\Tools\SDL3\SDL3-3.4.18\x86_64-w64-mingw32`
  (`scripts/setup-sdl3.ps1`)
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
  `dino_tools.exe`, and all 63 tests pass.
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
- FMT-5 complete: **`.anim` is solved.**
  - Header: magic/hash/sentinel `0xdef01234`, duration, start, bone count.
  - Per bone: name, key count, `u64` times, matrix count, 64-byte local 4×4
    matrices (row-vector: rotation in the 3×3, translation in row 3).
  - 299/299 files parse: 9,129 tracks, 285,507 keys, monotonic times.
  - 255 clips are 35 bones × 31 keys over 0..4,800; six are empty 25-byte clips.
  - `HeavyBiped_Armor` applied to `allo.skel` + `allo_lod1.skin` produces a
    rearing loop; `t=0` and `t=4800` match within `1e-6` m.
  - Evidence: `tools/formats/anim.py`, `docs/formats/anim.md`,
    `tests/test_anim.py`, `notes/allo-anim.gif`.
- FMT-6 complete: **TGA + WAV.**
  - 649 TGA files (405 × 32-bit, 240 × 24-bit, 4 × 8-bit grayscale), 0 errors.
    Decoded pixels match PIL exactly on 32/24/8-bit samples.
  - 231 WAV files (all PCM 16-bit; 225 mono 22,050, 4 stereo 22,050, 2 mono
    44,100), 0 errors. Durations/samples match Python's `wave`.
  - Evidence: `tools/formats/tga.py`, `tools/formats/wav.py`,
    `docs/formats/tga.md`, `docs/formats/wav.md`, `tests/test_tga.py`,
    `tests/test_wav.py`.
- VIEW-0 complete: Phase 1 asset viewer.
  - `tools/viewer/asset_viewer.py` renders a `.mesh` + `.tga` to PNG, writes
    texture previews, and prints/plays WAV info.
  - Software renderer supports side/three-quarter/front/top views, vertex
    colors, bilinear texture sampling, supersampling.
  - Evidence: `tools/viewer/soft_render.py`, `tests/test_viewer.py`,
    `notes/view-allo.png`, `notes/view-cursor.png`.
- VIEW-1 complete: portable SDL3 + OpenGL window.
  - SDL3 3.4.18 MinGW installed via `scripts/setup-sdl3.ps1`; CMake target
    `dino_viewer` links SDL3 + `opengl32`.
  - Resizable 1280×720 window, OpenGL context, event loop, delta time, clean
    shutdown, BMP screenshot.
  - Evidence: `src/viewer/main.cpp`, `docs/platform/sdl3-viewer.md`,
    `tests/test_viewer_cpp.py`, `notes/viewer-window.png`.
- VIEW-2 complete: C++ mesh + texture renderer.
  - `src/formats/mesh.cpp` and `tga.cpp` port the validated readers to C++.
  - GLSL 3.3 lit pipeline with texture modulation, vertex colors, orbit camera,
    resize handling and BMP screenshots.
  - `--validate` confirms `allo_lod1.mesh` = 317 vertices / 382 triangles /
    317 UVs / normals / colors; `allo.tga` = 256×256 RGBA.
  - Evidence: `src/formats/mesh.cpp`, `src/formats/tga.cpp`,
    `src/viewer/main.cpp`, `tests/test_viewer_cpp.py`,
    `notes/viewer-allo.png`.
- VIEW-3 complete: skinned animation in the C++ viewer.
  - `src/formats/skel.cpp` (80 bones), `skin.cpp` (35 rigid records,
    317 assignments), `anim.cpp` (35 bones, 1,085 keys, quaternion slerp).
  - CPU rigid skinning: `v_cm @ inverse_bind @ W_raw_anim`, then `× 0.01`;
    normals transformed and renormalised; VBO updated per frame.
  - Fixed-time poses and deterministic 25-frame capture work.
  - Evidence: `src/formats/{skel,skin,anim}.cpp`, `src/viewer/main.cpp`,
    `tests/test_viewer_cpp.py`, `notes/viewer-anim-1600.png`,
    `notes/viewer-anim-sheet.png`, `notes/viewer-anim-cpp.gif`.
- QOL-1 complete: window modes + persistence.
  - Windowed/fullscreen/borderless, F10/F11 toggles.
  - `dino_viewer.cfg` persists size/mode/vsync; CLI overrides;
    `--config`, `--no-save-config`.
  - Widescreen framing uses actual drawable size and fixed vertical FOV.
  - Evidence: `docs/platform/qol.md`, `tests/test_viewer_cpp.py`,
    `notes/qol-16x9.png`, `notes/qol-4x3.png`.
- QOL-3 complete: portable key rebinding.
  - `dino_keys.cfg` for orbit/zoom/reset/window/quit; `--rebind`,
    `--list-keys`, `--keys`, `--no-save-keys`.
  - Rebinds persist and are loaded before the window starts.
  - Evidence: `src/viewer/keymap.cpp`, `docs/platform/qol.md`,
    `tests/test_viewer_cpp.py`.
- AUDIO-1 complete: SDL3 WAV playback.
  - C++ RIFF/WAVE PCM reader (8/16-bit, mono/stereo), SDL3 audio device
    stream, loop and mute (`M`).
  - `--wav`, `--once`, `--mute`; graceful if no audio device.
  - Evidence: `src/formats/wav.cpp`, `docs/platform/audio.md`,
    `tests/test_viewer_cpp.py`, `notes/audio-smoke.txt`.
- HYB-1/2 complete: two functions replaced in the original exe.
  - 32-bit `DINPUT8.dll` proxy in `run/`; forwards the single imported
    `DirectInput8Create` to the real system DLL.
  - Inline detours on `0x4663a0` (64-byte/16-float matrix reader, 281k calls)
    and `0x4809a0` (12-byte/3-float normal reader, 95k calls).
  - Game reaches a playable park with both replacements active.
  - Evidence: `src/hook/dinput8_proxy.cpp`, `scripts/build-hook.ps1`,
    `scripts/play-freegame-direct.ps1`, `docs/decomp/hybrid.md`,
    `notes/park-hook.png`, `notes/hook-4663a0-4809a0.log`.
- WORLD-1 complete: **isl01 terrain + 1,536 placed trees.**
  - `.lnd` + 512×512 8-bit heightmap → 1024-unit terrain (522,242 triangles),
    basemap TGA, bilinear heights.
  - `.imp` solved: `u32 path, model path, u32 count, count × (x, y, scale,
    angle)`; 78/78 files, 17,059 instances, all coordinates 0..1024.
  - `.blf` solved: version/LOD/texture counts, 2 mesh paths + texture dir;
    24/24 files, all 48 LOD paths exist.
  - C++ bakes every instance per entry (9 groups, 52,291 triangles) using the
    material texture from the mesh tail + `.blf` texture dir.
  - Evidence: `src/world/{terrain,vegetation}.cpp`, `src/formats/{imp,blf}.cpp`,
    `docs/platform/terrain.md`, `docs/formats/{imp,blf}.md`,
    `tests/test_imp.py`, `tests/test_blf.py`, `tests/test_viewer_cpp.py`,
    `notes/isl01-vegetation.png`.
- SLICE-1 complete: island + dinosaur in one scene.
  - `--lnd` can now be combined with `--mesh/--skel/--skin/--anim`;
    `--dino-pos`/`--dino-angle`/`--dino-texture` place the model on the
    heightfield with a soft contact shadow.
  - Important: the `.skin` full mesh is a crouched bind pose. The viewer uses
    the `.mesh` standing rest pose with the same rigid weights
    (`--skin-bind mesh`, default); `.skin` bind selectable for comparison.
  - `HeavyBiped_Walk` at t=1200 renders a clean walk cycle on the island.
  - Free-fly camera (`--free-camera` / `C`): WASD + mouse look, Q/E up/down,
    Shift sprint, terrain clamp; movement actions are in `dino_keys.cfg`.
  - Evidence: `docs/platform/vertical-slice.md`,
    `notes/isl01-dino-meshbind.png`, `notes/isl01-dino-walk.png`,
    `tests/test_viewer_cpp.py`.
- SIM-1 complete: grid path/fence placement.
  - 4-unit grid, canonical south/east fence edges, undo stack.
  - Terrain ray-pick + click placement (`1`/`2`/`0`/`U`), retail paddock
    fence mesh and `ground.tga` path tiles.
  - Placement save/load text format; `--place-path`/`--place-fence`.
  - Evidence: `docs/platform/build.md`, `src/world/build.cpp`,
    `notes/build-preview.png`, `notes/build-preview2.png`,
    `tests/test_viewer_cpp.py`.
- WORLD-3 implemented (side-by-side comparison pending): skybox + water.
  - Six 128×128 PNG skybox faces via vendored `stb_image`; unlit cube drawn
    before the terrain (`src/third_party/stb_image.h`,
    `src/formats/image.cpp`).
  - Tiled `Water.tga` plane at the `.lnd` float level (16.0), alpha 0.78,
    repeat wrap, 6144-unit half-extent.
  - Evidence: `docs/platform/environment.md`, `notes/world3-sky2.png`,
    `notes/world3-hero.png`, `notes/world3-up.png`,
    `tests/test_viewer_cpp.py::test_cpp_skybox_and_water`.
- Stability (initial slice): **60,000 frames (~10 min) at ~100 FPS**, working
  set 134.1 MB flat, exit 0.
  - Evidence: `notes/stability.out.log`, `notes/stability-memory.csv`,
    `notes/stability-final.png`.
- SIM-2 (walking part) complete: visitors on the path network.
  - People are not skinned; the walk is a two-pose flip book
    (`woman_walk1`/`woman_walk2`, 72 verts / 52 tris).
  - Random-neighbour cell walk at 1.6 u/s, fixed seed, one combined mesh.
  - Evidence: `docs/platform/visitors.md`, `src/world/visitors.cpp`,
    `notes/visitors.png`, `tests/test_viewer_cpp.py`.
- Reviewer fixes applied: free-fly strafe direction, unsigned visitor RNG seed,
  water drawn after opaque actors, unified grid origin for non-divisor cell
  sizes, initial pitch/distance clamps, shader leak on failed compile,
  high-DPI picking uses logical window size, exact pick-cell and dino-visible
  test assertions.
- `--selftest` added; it caught an undo bug (undo re-applied the new value
  instead of the previous one) which is fixed. 18 logic checks now run in
  `tests/test_viewer_cpp.py::test_cpp_selftest`.
- UI-0 complete: C++ `.fnt` reader + on-screen HUD.
  - Same layout as the Python parser (9-u32 header, `codes[i] -> metrics[i+2]`,
    16-bit atlas); atlas uploaded as R8 from the high byte.
  - Screen-space glyph quads, orthographic MVP, `u_unlit = 2` uses coverage as
    alpha; F1 toggles the HUD, `--no-hud`/`--hud-font`.
  - Evidence: `docs/platform/hud.md`, `src/formats/fnt.cpp`, `notes/hud.png`,
    `tests/test_viewer_cpp.py::test_cpp_hud`.
- Stability (full scene): **60,000 frames (~10 min) at ~100 FPS**, 12
  visitors + 24 path cells + sky + water + dinosaur, working set 135.2 MB
  flat, exit 0.
  - Evidence: `notes/stability2.out.log`, `notes/stability2-memory.csv`,
    `notes/stability2-final.png`.
- UI-1 (partial): start screen and controls panel.
  - Start screen over the island (`--menu`), ENTER/SPACE begins, ESC quits;
    Tab toggles the controls panel (`--help-panel` for screenshots).
  - Shared `draw_2d` path for translucent rectangles + text.
  - Evidence: `docs/platform/hud.md`, `notes/menu.png`,
    `notes/menu-help.png`, `tests/test_viewer_cpp.py::test_cpp_start_menu`,
    `test_cpp_help_panel`.
- UI-2 Phase A complete: **retail menu rendered from data.**
  - `src/ui/ini.cpp` (INI/#INCLUDE), `overlay.cpp` (114 widgets, 9 pages,
    53 graphics, absolute rects), `menu.cpp` (page state machine, hit-test,
    transitions), `src/formats/loc.cpp` (`MenuEng.loc`).
  - Viewer: 800×600 design space centred in the window, `BACKGROUND_*`,
    compact button frame strips (`NB_OBJECTS`/`SKIP_LIST`), `.fnt` text with
    `TEXT_HEIGHT`/`TEXT_JUSTIFY`.
  - Free Game page: 10-island list, island picture, difficulty, Start/Cancel.
    Pages 2–8 render their retail backgrounds/widgets; Start closes the menu.
  - CLI: `--menu`, `--menu-page N`, `--menu-click X,Y`,
    `--placeholder-menu`, `--no-menu`.
  - Cross-checked against the Python parser/preview (`tools/formats/overlay.py`,
    `notes/menu-preview.png`) and the 800×600 retail reference.
  - Evidence: `docs/platform/ui-original-menu.md`, `notes/menu-retail.png`,
    `notes/menu-free.png`, `notes/menu-pages-sheet.png`,
    `tests/test_viewer_cpp.py::test_cpp_retail_menu_*`.
- 100 tests pass; working tree committed.
- UI-RECON complete: **retail overlay/menu research delivered.**
  - The UI is data-driven at 800×600: `MenuInterface.Ini` (114 widgets,
    9 pages), `ParkInterface.Ini` (137 + 9 includes), `MenuGraphicDB.ini`
    (53) and `ParkGraphicDB.ini` (605 unique), `MenuEng.loc`/`ParkEng.loc`.
  - Widget semantics recovered: `moWidget::Load` `0x5bca20` (flags,
    `TEXT_JUSTIFY` bits), `moWidget::OnResize` `0x5bc000` (anchor/stretch/
    center), event setters `0x5c5f00`.
  - Menu state machine: nine pages, state index 0..8, button handlers and
    next states (`mvwMenuStateManager` `0x517c60`; page ctors
    `0x51bbf0`, `0x51b120`, …).
  - 1024×768 retail reference proves the UI is re-anchored, not scaled
    (MAD 1.0 vs 40.3).
  - Follow-up: frame strips (`SKIP_LIST` is a bitmask; skipped frames are not
    stored), observed normal-state frames per control family,
    `ALIGN_USER_DEFINED`, `SIZE`, `TEXTURE_FRAME`.
  - Python oracle: `tools/formats/overlay.py` + `tools/viewer/overlay_preview.py`
    with `tests/test_overlay.py`, `tests/test_overlay_preview.py` (13 tests).
    Previews: `notes/menu-preview.png`, `notes/menu-freegame-preview.png`,
    `notes/hud-preview.png`; HUD matches the retail 800×600 screenshot
    (right panel MAD 8.8, gauge 12.0).
  - Evidence: `notes/ui-recon.md`, `notes/ui-frame-handoff.md`,
    `docs/formats/overlay-ini.md`,
    `docs/platform/ui-original-menu.md` (the seven UI-2 questions answered).

- FMT-7 complete: **`.loc` / `.lnd` / `.fnt`.**
  - `.loc`: UTF-16LE records (`FIELD_ID`, `TEXT`, `LAST_MODIFIED`); 2 files,
    758 records.
  - `.lnd`: 8 resource strings + float/dimensions/theme + music + 25
    vegetation `.imp`/`None` slots; 10/10 islands.
  - `.fnt`: 9-u32 header, 247 code words, 247 metrics, 16-bit atlas;
    `codes[i] -> metrics[i+2]`, 245 real glyphs; 5/5 fonts.
  - `arial12` renders `"Hello, Dino Island!"` correctly.
  - Evidence: `tools/formats/loc.py`, `lnd.py`, `fnt.py`,
    `docs/formats/{loc,lnd,fnt}.md`, `tests/test_loc.py`, `test_lnd.py`,
    `test_fnt.py`, `notes/font-arial12-sample.png`.
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

- **UI-2 Phase B rest:** `FREE_START_BUTTON` should show `LOADING_PAGE`
  (`loading_800x600.tga`) and build the selected island's scene, then hand
  control to the park.
- **UI-2 Phase C:** park HUD from `ParkInterface.Ini` + `ParkGraphicDB.ini`
  (always-on panels; live budget/visitor/time fields); then Phase D panels,
  save/load and menu sounds.
- Phase 3 vertical slice otherwise works: island, vegetation, dinosaur, free
  camera, path/fence, sky/water, visitors, HUD.
- WORLD-3 polish remains: skybox orientation vs the original, water tuning.
- WORLD-2 remains: culling, LOD2, scenario/building placements.
- QOL-2 (UI scaling) builds on the `.fnt` renderer.
- FMT-3b follow-up (not blocking): LOD2/LOD3 bind poses (notably `pachy`).

## Next action

1. UI-2 Phase B: Start → page 8 → build `isl01` from the `.lnd`/heightmap/
  basemap/vegetation loaders already in the viewer; then set the park page.
2. UI-2 Phase C: parse `ParkInterface.Ini`/`ParkGraphicDB.ini`, render the
  always-on HUD panels at 800×600 with `ALIGN_*` anchoring, and bind
  `&BudgetAmount`/`&VisitorAmount`/`&CurrentTime` to live state.
3. SIM-2b: visitor needs/attraction targets after the menu work.
4. Capture a side-by-side comparison against the original game for WORLD-3.
5. Continue hybrid detours (`0x47d460`, `0x47cc90`) when contracts are ported.
6. User: publish `docs/community/xentax-post.md` and record any replies.

## Blockers

- None. The menu research handoff is complete
  (`docs/platform/ui-original-menu.md`); UI-2 can start.

## Decisions

- Four-phase roadmap in `docs/ROADMAP.md`: (1) formats/assets, (2) platform
  layer, (3) hybrid decompilation, (4) remove the original exe.
- Long-term path is **hybrid decompilation**: hook the original exe and replace
  functions one at a time with recovered C++, keeping the game runnable
  throughout. The portable SDL/OpenGL layer is built alongside it. Function
  status is tracked in `docs/decomp/function-status.md`.
- A playable vertical slice still comes first: one island, camera, path/fence,
  one animated dinosaur, 10-minute session.
- Phase 1 also includes a simple asset viewer and a XeNTaX/ZenHax post
  (`docs/community/xentax-post.md`, drafted; the user publishes it).
- Use headless `pyghidra-mcp` over HTTP rather than GUI GhidraMCP; the agent
  environment must be reproducible and unattended.
