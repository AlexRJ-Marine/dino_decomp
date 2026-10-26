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
  - LOD2/LOD3 bind rule solved: `inv(inverse_bind)` equals the skeleton rest
    for 58/60 dinosaur skins; only `pachy` LOD2/LOD3 store a bent-neck bind
    pose. Use `inv(skeleton rest)` for the `.mesh` bind, `inverse_bind` for
    the `.skin` bind.
  - Evidence: `tools/formats/skin.py`, `docs/formats/skin.md`,
    `tests/test_skin.py`, `notes/lod-bind-recon.md`,
    `notes/allo-skin-bind.png`, `notes/allo-skin-pose.png`.
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
  - Free Game page: 10-island list with row sprites, island picture, difficulty,
    Start/Cancel. Start shows `LOADING_PAGE` for `--menu-loading-time` seconds,
    then closes the menu and resumes the park. Pages 2–8 render their retail
    backgrounds/widgets.
  - CLI: `--menu`, `--menu-page N`, `--menu-click X,Y`,
    `--menu-loading-time N`, `--placeholder-menu`, `--no-menu`.
  - Cross-checked against the Python parser/preview (`tools/formats/overlay.py`,
    `notes/menu-preview.png`) and the 800×600 retail reference.
  - Evidence: `docs/platform/ui-original-menu.md`, `notes/menu-retail.png`,
    `notes/menu-free.png`, `notes/menu-loading.png`,
    `notes/menu-pages-sheet.png`,
    `tests/test_viewer_cpp.py::test_cpp_retail_menu_*`.
- UI-2 Phase C complete at 800×600: **retail park HUD from data.**
  - `ParkInterface.Ini` + `ParkGraphicDB.Ini` (includes expanded):
    2,694 widgets, 38 roots, 53 visible at park start.
  - Always-on roots rendered over the scene (`RIGHT_PANEL`,
    `NAVIGATION_PANEL`, `GAUGE_PANEL`, `MAP_PANEL`, `MESSAGE_PANEL`);
    `RENDERVIEW` skipped. Runtime budget/visitor/dino/show/time/day fields.
  - Frame model aligned with the research: normal=4 (frame 0 disabled),
    hover=5, pressed=6, selected=7; `ISLAND_LIST_BUTTON` rows normal 4,
    selected 7; compact horizontal/vertical strips.
  - Python-oracle parity at 800×600: right panel MAD 10.8, gauge 4.0,
    map 2.1 (oracle vs retail 8.8 / 3.65).
  - Evidence: `docs/platform/hud.md`, `notes/park-hud.png`,
    `notes/hud-preview.png`, `tests/test_viewer_cpp.py::test_cpp_park_hud_*`.
- UI-2 Phase C bindings complete: **park panel switching.**
  - `src/ui/park.cpp` maps the 12 right-panel/gauge buttons, the 3
    construction sub-buttons, the kit list/stats icons, the info-controller
    pages, the 5 lab centres and each panel's close/cancel/validate/put-back
    button. Exactly one state panel is visible at a time, matching
    `pvwParkStateManager`; `OPEN_PADDOCK_LIST_PANEL` opens `FENCE_LIST_PANEL`.
  - `DIALOG_BACKGROUND` is only used by warning/request/campaign-end dialogs,
    so park panels do not dim the scene. Lab centres show their own 800×600
    backdrop and hide the park HUD.
  - Hover (frame 5) and pressed (frame 6) art; `--park-click X,Y`,
    `--park-hover X,Y`.
  - Evidence: `notes/ui-recon.md` ("Park HUD panel switching"),
    `notes/park-budget.png`, `notes/park-construction-kit.png`,
    `notes/park-lab-breed.png`,
    `tests/test_viewer_cpp.py::test_cpp_park_panel*`,
    `test_cpp_park_lab_shot`, `test_cpp_park_hover_shot`.
- UI-2 Phase B rest complete: **Free Game start loads the selected island.**
  - `FREE_START_BUTTON` (0x51b370) → state 8; `LOADING_PAGE` only sets the
    start request; `daMainApplication` (0x402460) tears down the menu and
    calls `FUN_00403e10` → `FUN_0051dad0`.
  - The viewer reads `IslandList.ini` (`0x506530`, isl01..isl10) and rebuilds
    `Data/Design/Landscape/<name>/<name>.lnd` at runtime: terrain, basemap,
    vegetation, skybox, water and GPU resources (`load_island` in
    `src/viewer/main.cpp`).
  - Free-game budgets from `FreeGame.ini` `[CONTEXT]`: 75,000 / 50,000 /
    25,000 for easy/middle/hard; the HUD budget reflects the selection.
  - Evidence: `notes/ui-recon.md` ("Free Game start -> park handoff"),
    `notes/menu-start-isl01.png`, `notes/menu-start-isl03.png`,
    `notes/menu-start-isl10.png`,
    `tests/test_viewer_cpp.py::test_cpp_menu_start_loads_selected_island`,
    `test_cpp_island_list_and_free_game_budgets`.
- UI-2 Phase C minimap complete: **rendered detail map in `MAP_RENDERVIEW`.**
  - 256×256 FBO rendered top-down, centred on the camera target and rotated
    with the camera heading (`FUN_005fb120`); visible width
    `2 * world_size * scale`.
  - Retail zoom model: scale default 0.25, clamp 0.1..2.0, held buttons
    ±0.15/s (`FUN_005fb7e0`); `--park-click` steps 0.05 and `--minimap-scale`
    sets the start value.
  - Red/yellow camera marker; FBO refresh only when target/heading/scale
    change.
  - Evidence: `notes/ui-recon.md` ("Minimap implementation"),
    `notes/minimap-isl01.png`, `notes/minimap-isl01-detail.png`,
    `notes/minimap-isl01-zoom-detail.png`, `notes/minimap-isl10-detail.png`,
    `tests/test_viewer_cpp.py::test_cpp_minimap_detail_view`.
- UI-2 Phase C anchoring complete: **park HUD re-anchors at 1024×768.**
  - `Overlay::layout_for_size` implements `moWidget::OnResize` with the base
    800×600 rects; right panel 931,168, gauge 293,737, map 840,584,
    `MAP_RENDERVIEW` 876,619.
  - Menu pages remain centred; the minimap quad follows the anchored rect.
  - Evidence: `notes/ui-recon.md` ("1024x768 park HUD anchoring"),
    `notes/park-hud-1024.png`, `notes/hud-1024.png`,
    `tests/test_viewer_cpp.py::test_cpp_park_hud_1024_anchor`.
- UI-2 Phase C clock complete: **live park time/day in the HUD.**
  - `ParkClock` mirrors `0x0058c1c0`/`0x0058c2f0`/`0x0058c350`: base hour 9,
    time scale 60 (the schedule-manager float, not `GameSpeed`),
    hours/sec = scale/3600, 12-hour day ending at hour 11, day rollover,
    HUD `%02d:%02d` (24-hour) and `day+1`.
  - `--park-time HH:MM` seeds the clock; `--park-time-scale N` overrides
    60.0.
  - Evidence: `notes/ui-recon.md` ("Park clock"),
    `notes/clock-0944.png`, `notes/clock-0944-detail.png`,
    `tests/test_viewer_cpp.py::test_cpp_park_clock`.
- UI-2 Phase C budget ledger complete: **category/total numbers populated.**
  - The 30 `BUDGET_*_NUMBER` widgets are refreshed by `0x510430`; a fresh
    park has no spending, so the port shows zeros for every category and both
    totals. The gauge shows the difficulty budget.
  - Evidence: `notes/park-budget.png`, `notes/park-budget-detail.png`,
    `tests/test_viewer_cpp.py::test_cpp_park_panel_shot`.
- FMT-3b C++ handoff complete: **LOD bind-matrix choice.**
  - `build_skinned_mesh` uses `inv(skeleton rest raw)` for the `.mesh` bind
    and the skin's `inverse_bind` for `--skin-bind skin`.
  - `--validate` prints `skin rest max error`: pachy LOD2 mesh bind
    0.0001 cm (was ~4.3 cm), skin bind 7.8464 cm; allo LOD1 0.0001 cm.
  - Evidence: `notes/lod-bind-recon.md`, `docs/formats/skin.md`,
    `tests/test_viewer_cpp.py::test_cpp_pachy_lod2_mesh_bind`.
- SLICE-1 menu path complete: **animated dinosaur from Free Game Start.**
  - `--start-dino` preloads the allo mesh/skel/skin/anim/texture; the menu
    Start handoff loads the selected island and places the allosaurus at the
    centre with its contact shadow and walk animation.
  - Evidence: `notes/menu-start-dino.png`, `docs/platform/vertical-slice.md`,
    `tests/test_viewer_cpp.py::test_cpp_menu_start_with_dino`.
- 232 tests pass; working tree committed (UI-2 Phase B rest plus
  FMT-9/10/11 and DEC-2 research).
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
    (right panel MAD 8.8, gauge 3.65) and the Free Game page (2.93).
  - Evidence: `notes/ui-recon.md`, `notes/ui-frame-handoff.md`,
    `docs/formats/overlay-ini.md`,
    `docs/platform/ui-original-menu.md` (the seven UI-2 questions answered).
- SAVE-RECON complete: **header, world object graph, show catalog and
  simulation-state prefix.**
  - Header: version 53, slot, length-prefixed UTF-16 save name, 12 reserved
    zero bytes; world block starts at `0x18 + name_len` (`0x2c` / `0x1a`).
  - World object graph: class IDs `0x12`–`0x18` = `drsDinosaur`, `drsVisitor`,
    `drsEmployee`, `drsAnimal`, `drsShow`, `drsFence`, `drsBuilding`; every
    per-list record decoded. Save games add the campaign/mission ids, six
    objective records, the visitor list and 62/39/10/6 unlock bytes.
  - `.wld` world files use the same block with campaign flag 1, then the
    simulation road rectangles and camera; 7/7 version 50–53 worlds parse.
  - Show records: ASCII `.shw` path + UTF-16 name + 6 u32 params; 39 records
    in both manual and auto saves.
  - Manager order: campaign → world (objects + show manager) → simulation →
    AI → scenario (`0x51e6c0` load, `0x51e7e0` save).
  - Simulation prefix decoded: one road rectangle, 0x50-byte visitor records
    (2/0), 0x293-byte park block with **budget 75,125/75,000**, 20 dinosaur
    store records, five visitor-model values, 8 empty lab flags.
  - AI/camera tail decoded: 5 AI-list counts (fifth 2/0), 17-byte AI entries,
    and 10 camera floats (`camSwitcher`).
  - Dinosaur world objects store the 18-gene genotype (same order as `.gen`).
  - **Lab genotype store solved:** `u8 flag` (1 = empty), then for flag 0:
    18 × `(u32, u32)` = 0x90-byte genotype, `u32 name_len` (NUL included) and
    an ASCII name. Writer `0x592b20` → genotype writer `0x585100` (18 pairs);
    reader `0x592a00` → `0x584ed0`. Runtime record is 0xe8 bytes; the file
    stores only flag/genotype/name. Retail saves have 8 empty slots.
  - Remaining: a few field semantics (park header, store_id meaning).
  - Evidence: `notes/save-recon.md`, `notes/save-graph-recon.md`,
    `notes/world-objects-recon.md`, `docs/formats/sav.md`,
    `docs/formats/wld.md`, `tools/formats/sav.py`, `sav_graph.py`,
    `wld.py`, `tests/test_sav.py`, `test_sav_graph.py`, `test_wld.py`.
- FMT-8 complete: **`.shw` show scenarios.**
  - Plain text `//` sections: name, cost, judge kind, cage size, dimensions,
    wall type, 4-int feature grid, cage→team list.
  - 43/43 files parse, 1,679 features, 174 team slots, 0 errors.
  - Save catalog: cost and dimensions match all 39 records.
  - Evidence: `tools/formats/shw.py`, `docs/formats/shw.md`,
    `tests/test_shw.py`.
- FMT-9 complete: **`.gen` genotypes.**
  - 57 design files (18 genes) + 20 editor files (27 genes); 77/77 parse,
    1,566 genes, 0 errors.
  - 18-gene table decoded; expression/dominance from `simu_trans.dic`
    (`min(a,b)` for dominant genes, average for skin-sub/carapace-sub/
    Agre/Stam/Cour), plus rarity weights.
  - Evidence: `tools/formats/gen.py`, `gene_trans.py`,
    `docs/formats/gen.md`, `tests/test_gen.py`, `test_gene_trans.py`,
    `notes/gen-recon.md`.
- FMT-10 complete: **`.bvi` visitor profiles.**
  - 24/24 parse, 10 personalities (`VisitorList.ini`), 624 mesh paths all
    exist.
  - Header: personality id, height 1.7, walk speed 1.2–2.5, five preference
    weights; 26 meshes + texture per profile.
  - Evidence: `tools/formats/bvi.py`, `docs/formats/bvi.md`,
    `tests/test_bvi.py`, `notes/bvi-recon.md`.
- FMT-11 complete: **`.bbi` building definitions.**
  - 39/39 parse, one per `BuildingList.ini`; 390 mesh/skel/anim paths all
    exist; 213 `CONSTRUC_*` UI names (212 resolve).
  - Header hypothesis: footprint `w0×w1`, cost `w5`; tail carries texture
    dir, explosions, sounds and build-panel widget names.
  - Evidence: `tools/formats/bbi.py`, `docs/formats/bbi.md`,
    `tests/test_bbi.py`, `notes/bbi-recon.md`.
- FMT-12 complete: **`.bsi` food sources.**
  - 10/10 parse; nutrition/quantity/price/kind id, four assets, feeder mesh
    and `FOOD_DISTRIBUTION_*` build button.
  - All 40 asset paths + 10 feeder meshes exist; all 10 buttons resolve.
  - Evidence: `tools/formats/bsi.py`, `docs/formats/bsi.md`,
    `tests/test_bsi.py`, `notes/bsi-recon.md`.
- FMT-13 complete: **`.bni` animal profiles.**
  - 5/5 parse: variant, size, limit, food value, quantity, food kind,
    5–12 meshes + texture dir; food kinds match `.bsi`; 33 meshes exist.
  - Evidence: `tools/formats/actors.py`, `docs/formats/bni.md`,
    `tests/test_actors.py`, `notes/actors-recon.md`.
- FMT-14 complete: **`.bei` employee profiles.**
  - 2/2 parse: kind, height 1.7, walk/action speeds, meshes, textures,
    sounds; 9 meshes exist.
  - Evidence: `tools/formats/actors.py`, `docs/formats/bei.md`,
    `tests/test_actors.py`, `notes/actors-recon.md`.
- FMT-15 complete: **`.seq` animation events.**
  - 10/10 parse (5 FX + 5 Sound), 1,304 events, exact byte sizes.
  - Common 42–47-string table (sounds, particles, bones); FX 6-byte events
    `(frame/30, type, string)`; Sound 16-byte events `(type|string, flags,
    time, value)`.
  - Evidence: `tools/formats/seq.py`, `docs/formats/seq.md`,
    `tests/test_seq.py`, `notes/seq-recon.md`.
- FMT-16 complete: **campaign/mission data.**
  - 7 campaigns, 39 missions, 89 objective lines / 28 verbs; all 206 loc IDs
    resolve; every `WorldFile` `.wld` exists.
  - Campaign table (island, budget, missions) and the `CampaignBase`/
    `MissionBase` schema documented; `Cpg07` is unlisted cut content.
  - Objective dispatch solved: 33 verbs, ids 0–32, evaluator addresses,
    tri-state verdict (0 pending / 1 success / 2 failure). Simple evaluators
    (treasury, marchandizing, dyingdinos, employees, visitors, happyvisitors,
    buildings, camera altitude, delay limit, buy-all-dinos, bankrupt) decoded.
  - Dinosaur-trait verbs use a 10-slot pattern over actor fields `+0x90`
    (cuteness), `+0xbc` (aggression), `+0x1e4` (hybrid), `+0x1e8` (scarcity),
    `+0x244` (food), `+0x98` (size), `+0x7c` (speed), `+0xc0` (stamina),
    `+0x1e0` (agility), `+0x8c` (intelligence). Show query `0x567190` maps
    race/obstacle/fight/destruction/team/capture/fence and per-profile scores.
    `carnivordinos` (`+0x1b0/+0x70`) and `ornateddinos` (23-slot `+0xE8`
    array) are decoded; `scientificshow` requires show score > arg1 and a dino
    `+0x1e4` > arg2, and `dinoexclusiveshow` a dinosaur unique to one show.
    Difficulty selection (`FUN_00561bb0`): `mission+0x50` 0/1/2 picks
    `Budget`/`SellBackFactor`/`VisProfiles` and their `Medium`/`Hard`
    variants; other content lists are difficulty-independent.
    All 33 evaluator predicates are decoded; field names, objective weighting
    and where `mission+0x50` is set remain.
  - Evidence: `tools/formats/campaign.py`, `docs/formats/campaign.md`,
    `tests/test_campaign.py`, `notes/campaign-recon.md`,
    `notes/objectives-recon.md`.
- FMT-17 complete: **`anim_trans.dic` / `graphic_trans.dic` / `proportion.dat`.**
  - 20 dinosaurs, 5 families, 51 clip names; four 20×20 merge tables.
  - All 51 clip names exist in every family folder (255 `.anim`).
  - ArmLeg is a directional 10/10 table (A→B=1, B→A=99) with two
    exceptions; Head/Body/Tail are identity+50.
  - `proportion.dat`: 20 × 5 part scales (head/arm/body/leg/tail, leg = 1.0;
    T-rex arm 0.235, Diplodocus tail 2.093).
  - Evidence: `tools/formats/anim_trans.py`,
    `docs/formats/anim-trans.md`, `tests/test_anim_trans.py`,
    `notes/anim-trans-recon.md`.
- FMT-18 partial: **`.bai` action header decoded.**
  - 64/64 files parse; packed header `name[20]`, fields at `+0x14..+0x32`,
    `params[8]` at `+0x36`, variable tail from `+0x56`.
  - `field_20` high byte = special-action group (exploration 1, runaway 2,
    sleep 3, dino attack 4, obstacle attack 5, victory 6); long names are
    truncated to 19 chars in the file itself.
  - Tail arrays and field semantics remain open.
  - Evidence: `tools/formats/bai.py`, `docs/formats/bai.md`,
    `tests/test_bai.py`.
- FMT-19 complete: **`TranslatorConstants.ini` + genotype translator.**
  - 41 `[SIMU_CONSTANTS]` keys map to translator `+0x228..+0x2dc`.
  - Five dinosaur-need formulas (`0x618020`): beauty, danger, spectacular
    (cube root of a 3-dimension volume × 60 / 1346), conventional, taming.
  - Phenotype accumulator fields `+0x18..+0x194` and the add/normalize part
    builders (`0x619c70`/`0x619d90`) mapped; gene→part record open.
  - Evidence: `tools/formats/translator_constants.py`,
    `docs/formats/translator-constants.md`,
    `tests/test_translator_constants.py`, `notes/translator-recon.md`.
- FMT-21 complete: **island `.raw` heightmap + occupation mask.**
  - Heightmap 512×512 u8; occupation 1024×1024 u8 with values {0,255}.
  - 0 = buildable land, 255 = blocked/water; 0 cells are higher and never
    height 0 on all 10 islands.
  - Design/Graphics occupation copies differ by <1%; `.lnd` uses Design.
  - Evidence: `tools/formats/raw.py`, `docs/formats/raw.md`,
    `tests/test_raw.py`, `notes/raw-recon.md`, `notes/occupation-masks.png`.
- FMT-20 complete: **`simu_trans.dic` all 10 parts structurally parsed.**
  - Part 2's four 20×20 merge tables are byte-identical to `anim_trans.dic`
    (1,600 cells).
  - Part 3: 11 attributes × 5 parts = 55 tables (20 diagonal + 20×20).
  - Parts 4–6: 3 × 48 disposition/type tables; two 50×18 influence tables;
    five reference widths = `graphic_trans.dic`.
  - Parts 7–9: skin `8×3` + two `22×18`, carapace `6×3` + two `16×18`,
    20 min / 20 max attributes.
  - Part 10: 20 default family indices (3/4/3/7/3); pachy = LightBiped.
  - Semantics of the influence values and the Part 7 `582` remain open.
  - Evidence: `tools/formats/simu_trans.py`,
    `docs/formats/simu-trans.md`, `tests/test_simu_trans.py`,
    `notes/simu-trans-recon.md`.
- PARK-TIME research: **clock and show scheduler decompiled.**
  - `spScheduleManager` (`0x594b80`) owns a time manager and a 12-entry task
    vector. Time manager: base hour `+0x00`, elapsed `+0x08`, new-hour
    `+0x28`, end-of-day `+0x29`, hours/sec `+0x20`, minutes/sec `+0x18`.
  - `0x58c2f0` advances the clock; the day ends at hour 11. `0x58c350`
    returns H:M:S; `0x526010` formats `%.2d:%.2dAM/PM` in the HUD.
  - `0x58c760` scans the 12 show slots on each hour boundary, checks show
    flags `+0xd3`/`+0xe0`/`+0xe4`, and starts via `0x57b110`/`0x57b130`;
    `0x58c390` computes the show window.
  - `0x58dfc0` is the show render-record builder, not the clock.
  - Day rollover: `0x524630` checks the end-of-day flag, calls `0x58ae60`
    (daily actor reset), `0x58c2d0` (day++ / clock reset) and `0x58ad00`
    (daily economy settlement). The day counter is `time+0x04`.
  - Economy labels recovered from the budget panel `0x50fd30`: FOOD,
    EFFECTIFS_FOOD, EFFECTIFS_SECU, DINOACHAT, LABOACTION, CONSTRUCTION,
    ENTREES, RESTOS, BARAQUES, SOUVENIRS, BOITNOIRE, POINTDEVUE, GRADINS,
    VENTES. The `spPark+0x1c` index-to-label order is still open.
  - Evidence: `notes/park-time-recon.md`, `docs/decomp/park-time.md`,
    `docs/decomp/economy.md`, `docs/decomp/function-status.md`.
- MEDIA-1 recon complete: **sound DB, music and DirectShow movies.**
  - `MenuSoundDB.ini` (5 entries) and `ParkSoundDB.ini` (224); all filenames
    resolve except the documented `[bell]` retail typo.
  - `dsound.dll`/`winmm.dll` are loaded dynamically (`0x5ca960`); the engine
    is `msSoundEngine`/`msSoundDatabase`. Island `.lnd` music names map to
    five streaming/looping park themes plus `musLabo`.
  - Movies use DirectShow (`mvVideo`/`mvMovie`, `0x5c7ed0`), with
    `intro.mpg`/`generic.mpg` startup paths and a menu wrapper `0x517a40`.
  - Evidence: `docs/formats/sounddb.md`, `docs/decomp/media.md`,
    `notes/media-recon.md`, `tools/formats/sounddb.py`,
    `tests/test_sounddb.py`.
- SIM-3 needs complete: **dinosaur action IDs, need gauges and idle sentinel.**
  - Corrected the old dword-index offsets: the two raw gauges are `+0x258`
    (rest, max `+0x1d8`) and `+0x25c` (food, max `+0x1dc`), not `+0x96`/
    `+0x97`; the action id is `+0x2c0`, not `+0xb0`.
  - Four UI needs: `+0x244` FOOD, `+0x248` VIOLENCE, `+0x24c` SECU,
    `+0x250` REPOS; state indices `+0x2ac..+0x2b8` with thresholds
    `[80,60,30]` and GOOD/MID/BAD widgets in `0x511d80`/`0x5138b0`.
  - `0x1f` is the idle/finished sentinel: `0x571540` skips completion and
    `0x572826` checks it before `0x571190`, which sets `+0x2c0 = 0x1f`.
  - Eat (4) / Sleep (2) are real actions; the AI chooser that maps BAD
    FOOD/REPOS to them remains open.
  - Evidence: `docs/decomp/dinosaur-actions.md`,
    `notes/dinosaur-actions-recon.md`, `docs/decomp/function-status.md`.
- DEC-2 complete: **simulation scheduler + AI update order decompiled.**
  - `lpScheduler` (`0x506ec0` ctor, `0x507050` AddTask, `0x507490`
    SetTaskEnabled, `0x507530` Update) runs a fixed-order task list with
    variable dt, no priorities.
  - Park frame order (`0x51d050`): ParkInterfaceProcess, ParkInterfaceUpdate,
    SimulationManager, BehaviorManager, WorldManager, CampaignManager,
    AnimationManager, ParkScene, AudioPark, ParkInterfaceDraw.
  - Menu order (`0x596500`): MenuInterfaceProcess, MenuInterfaceUpdate,
    MenuInterfaceDraw.
  - `spSimuManager::Update` (`0x5869e0`) ticks actors, then the park-wide
    step (`0x589cd0`); `apBehaviorManager::Update` (`0x51fab0`) runs
    perception → actor update → reactions → visitor/employee AI.
  - `saDinosaur::Update` (`0x571540`) decays two need slots; 5 s and 10 s
    passes at `0x5725e0`/`0x571370`/`0x571410`.
  - Action IDs 0..63 now map to `actionlist.ini`/`.bai`; `ApplyActionEffect`
    (`0x5727b0`) and completion (`0x571190`) decoded. Two need gauges
    (`+0x96`/`+0x97`) and the `0x1f` idle sentinel remain unnamed.
  - Evidence: `notes/scheduler-recon.md`, `docs/decomp/scheduler.md`,
    `notes/dinosaur-actions-recon.md`, `docs/decomp/dinosaur-actions.md`,
    `docs/decomp/function-status.md`.
- SIM-2b research: **visitor attraction AI decompiled; scoring weights solved.**
  - `apVisitorBehavior::Update` (`0x522820`) + state transitions (`0x522940`):
    choose → travel → use attraction → leave.
  - Attraction scoring (`0x522a10`) uses a constant seven-float block from
    `.bvi` file `0x38..0x50` (`[1, 1, 1, 500, 300, −3000, 3000]`), not the
    five per-profile weights: `rand()%6000 − 3000 + Σ(attr·(99−need)²) −
    300·distance − 500·price`. Runtime-verified under x32dbg.
  - Show availability (`0x522ff0`), path following (`0x521490`) and obstacle
    rerouting (`0x522cb0`) mapped.
  - Remaining: consumer of the five variable profile weights (record
    `+0x24..+0x34`), attraction field names, need decay.
  - Evidence: `notes/visitor-ai-recon.md`, `notes/visitor-weights-runtime.md`,
    `docs/decomp/visitor-ai.md`, `docs/formats/bvi.md`,
    `tools/recon/dump-visitor-weights.ps1`,
    `docs/decomp/function-status.md`.
- STAFF research: **employee task AI decompiled.**
  - `apEmployeeBehavior::Update` (`0x5220e0`) uses the same choose → travel →
    work loop as visitors, but jobs come from a 0xc-byte task queue
    (`0x5224c0`).
  - Kind-0xb work animation (`0x522390`) uses the target duration, moves to
    the target cell at 51%, and sets target flags at 75%.
  - `spScheduleManager` (`0x594b80`) has a 9-slot timing table and a 12-entry
    task vector; `0x594d00` updates it each frame.
  - Roles: Nutritionist (0), Guard (1). Job kind/role assignment still open.
  - Evidence: `notes/employee-ai-recon.md`, `docs/decomp/employee-ai.md`,
    `docs/decomp/function-status.md`.
- SIM-4 research: **park budget and daily economy decompiled.**
  - Budget at `spPark+0x08`; eight spend/income setters (`0x585c00`..
    `0x585de0`) and the `0x585e10` roll-up/bankruptcy pass.
  - Daily settlement `0x58ad00`: employee wages (role buckets `+0x1c` /
    `+0x20`), EFFECTIFS_SECU pass (`+0x14` / `+0x18`), visitor ticket income,
    counter reset.
  - Park save block is a 0x30-byte header + sequential writer-order stream
    (`0x585780` write, `0x585240` read), not a raw object dump; `+0x1c` is a
    two-entry staff bucket array, not a 14-row array.
  - Save read fix: `sav_graph._read_park_stream` parses the stream; the manual
    save's `125/2` is the ticket pair (stream `+0x18/+0x1c`), not `+0x20/+0x24`.
  - **Budget panel mapping solved** (`0x50fd30` ctor, `0x510430` refresh):
    FOOD `+0xc4/+0xc8`; EFFECTIFS_FOOD `+0xbc` + role-0 employee count;
    EFFECTIFS_SECU `+0xcc/+0xd0`; DINOACHAT `+0x154/+0x150`; LABOACTION
    `+0x164/+0x160`; CONSTRUCTION `+0x15c/+0x158`; ENTREES `+0x13c/+0x140`;
    RESTOS..GRADINS = shop revenue `+0xd8..+0xec` / counts `+0x10c..+0x120`
    (kinds 1..6); VENTES `+0x16c/+0x168`; totals `+0x144/+0x148/+0x14c`.
    `0x585db0` is the shop-sale setter; `0x585c90` the FOOD setter.
  - Employee roles solved: `employeelist.ini` + `.bei` `kind_id`
    (Nutritionist 0 / Guard 1); hire and wage = the `+0x50` descriptor's
    `[8]`. Guard bucket index still needs a runtime check.
  - Fresh saves contain the `0xBAADF00D` debug fill in the three computed
    totals (before the first day rollover);
    `tools/formats/economy.py` masks it and `tests/test_economy.py` checks
    the retail save.
  - **Save stream completed:** the 0x293 park block is a 0x30-byte header +
    0x21a-byte economy stream + 0x49-byte schedule tail. `sav_graph` decodes
    both the yesterday block (`+0xb2..+0x166`) and the cumulative block
    (`+0x166..+0x21a`) and exposes `stream_end`/`tail`. The retail reader
    `0x585240` reads the cumulative block into the yesterday fields and is
    32 bytes out of phase (a loader bug, not reproduced). The docs' old
    `+0xfa/+0x12a` offsets used 12-dword arrays; the real arrays are 13.
  - Evidence: `notes/park-economy-recon.md`, `docs/decomp/economy.md`,
    `docs/decomp/function-status.md`, `tools/formats/economy.py`,
    `tests/test_economy.py`.

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

- **UI-2 Phase C rest:** dynamic values inside sub-panels (budget ledger rows,
  list snapshots, slider/checkbox selection), minimap water colour/partition
  polish. Button bindings, hover/pressed states, the menu→island handoff,
  the minimap and 1024×768 anchoring are done.
- **UI-2 Phase B polish:** the original park camera's initial framing is not
  yet recovered; the port still auto-frames with `bounds.extent() * 1.7`.
- **UI-2 Phase D:** save/load panels, menu sounds, warning/request dialogs
  (`DIALOG_BACKGROUND` + `WARNING_PANEL`/`REQUEST_PANEL`/`CPGEND_PANEL`).
- Phase 3 vertical slice otherwise works: island, vegetation, dinosaur, free
  camera, path/fence, sky/water, visitors, HUD.
- WORLD-3 polish remains: skybox orientation vs the original, water tuning.
- WORLD-2 remains: culling, LOD2, scenario/building placements.
- QOL-2 (UI scaling) builds on the `.fnt` renderer.
- FMT-3b C++ handoff (not blocking): apply the `.mesh` bind-matrix choice in
  `build_skinned_mesh`; the rule and snippet are in `notes/lod-bind-recon.md`.

## Next action

1. UI-2 Phase C rest: dynamic values in sub-panels (budget ledger rows, list
   snapshots, sliders) and minimap water/partition polish.
2. SIM-2b: port the decompiled visitor choose → travel → use → leave flow
   (`docs/decomp/visitor-ai.md`) and bind visitor profiles/needs.
3. Capture a side-by-side comparison against the original game for WORLD-3.
4. Continue hybrid detours (`0x47d460`, `0x47cc90`) when contracts are ported.
5. DEC-2 follow-up: runtime-confirm the task order and `0x1f` idle sentinel,
   and name the two dinosaur need gauges (`+0x96`, `+0x97`); action IDs are
   solved in `docs/decomp/dinosaur-actions.md`.
6. DEC-3 rest: the world object graph, unlock arrays, AI/camera tail and lab
   genotype store are solved. Remaining: park block header, `store_id`
   semantics and a few small fields. See `docs/formats/sav.md` and
   `docs/formats/wld.md`.
7. SIM-4 rest: runtime-verify the panel rows against a populated save; the
   row mapping, category labels and daily settlement trigger are solved. See
   `docs/decomp/economy.md`.
8. STAFF rest: decode job kind `0xb`; roles (Nutritionist 0 / Guard 1) and
   wages are solved in `docs/decomp/economy.md`. See
   `docs/decomp/employee-ai.md`.
9. User: publish `docs/community/xentax-post.md` and record any replies.

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
