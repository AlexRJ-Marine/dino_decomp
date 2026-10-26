# State

Updated: 2026-10-26

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
- Live retail runs overwrite `run/save/auto/auto.sav`. Restore the test
  reference from `run/save/auto/slot001.sav` (7,116 bytes, 39 shows) before
  the save tests; a campaign autosave is ~53 KB (`notes/handoff.md`).
- Campaign progression is `run/save/dino.dat`, a 4-byte dword. `1` = first
  campaign only; `0F 00 00 00` unlocks campaigns 1-4 so Cpg04 ("Panic in the
  Savanna") can be entered for runtime tests. Restore `01 00 00 00` after.
  Reader `0x5612d0`, writer `0x5617a0`;
  harness `scripts/x32dbg/start-campaign.ps1`.

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
- WORLD-2a complete: **`.bbi` building loader + retail placement transform.**
  - `src/formats/bbi.cpp` reads the 121-byte header, ten asset paths and tail
    strings; `src/formats/wld.cpp` reads the shared `.wld`/`.sav` world block
    (versions 50–53) and exposes every `drsBuilding`.
  - Placement solved from `0x53d1a0`/`0x589100`/`0x586b80`/`0x5fecf0`/
    `0x61ffc0`: `cell_x/cell_y` is the footprint min corner, `state` is the
    90° orientation (odd states swap the footprint), the model sits at the
    footprint centre on the engine heightfield, and the primary `.skin` is
    the actor model. isl01 Entrance = centre `(525.5,387.5)`, Z `69.4`,
    yaw 90°. The terrain sampler now matches `FUN_005c0b80` (`world*0.5`,
    sample 0 at -512).
  - Cpg04 on isl04 places 57/57 buildings, including 12 skinned primaries
    (`load_skin_auto` in `src/formats/skin.cpp`).
  - Header correction from `FUN_0053fb00`: the `.bbi` header is
    `u32 w, u32 h, f32 scale, u8 flag, 27 unaligned u32`, not 30 aligned u32.
    `model[1]` is a float 0.0 at file 113 in all 39 files, so there is no Z
    offset; the Entrance hull sink is the asset pivot and matches retail.
    `model[0]` is 0 except `LunchDoor` (1 = slope-aligned). Category/price/
    duration/appeal-scale/radius fields are now exposed.
  - Open: `model[0] = 1` slope basis and construction-UI placement.
  - Evidence: `docs/platform/buildings.md`,
    `notes/building-placement-recon.md`,
    `notes/building-def-record-recon.md`, `notes/entrance-save.png`,
    `notes/entrance-center2.png`, `notes/cpg04-buildings.png`,
    `tests/test_viewer_cpp.py::test_cpp_building_bbi_place`,
    `test_cpp_building_placement_transform`,
    `test_cpp_wld_places_all_buildings`,
    `test_cpp_wld_reader_matches_retail`.
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
- Stability (retail menu path): **60,000 frames through Free Game Start**,
  ~540–818 FPS at 800x600 with no vsync, working set 167.1 MB flat; final
  frame at 10:30 day 1 with the dino, HUD and minimap.
  - Evidence: `notes/stability3.out.log`, `notes/stability3-memory.csv`,
    `notes/stability3-final.png`.
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
    `test_cpp_island_list_and_free_game_budgets`,
    `test_cpp_menu_start_all_islands` (all 10 islands).
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
- AUDIO-2 complete: **menu/park click sounds.**
  - `MenuSoundDB.ini` (5 entries) parsed at startup; menu buttons play
    `clicDefault`, Quit plays `exitClic`, park HUD actions play
    `clicDefault`. One short SDL3 stream per click (new click cuts the old).
  - `--ui-sounds`, `--no-sounds`, `--list-sounds`, `--play-sound NAME`.
  - Evidence: `docs/platform/audio.md`, `tests/test_viewer_cpp.py::test_cpp_menu_sounds`.
- AUDIO-3 complete: **menu and island music.**
  - `ParkSoundDB.ini` parsed; the `.lnd` music name starts a 26–28 MB theme
    streamed in 64 KB chunks and looped when the queue drains.
  - The menu plays the `MenuSoundDB` `music` entry (`musLabo`); lab centres
    switch to `musLabo` and leaving restores the island theme.
  - `--music`/`--no-music`/`--music-volume`; interactive default on.
  - Evidence: `docs/platform/audio.md`,
    `tests/test_viewer_cpp.py::test_cpp_park_music`,
    `test_cpp_menu_and_island_music_switch`.
- UI-2 Phase B camera complete: **free-game start framing.**
  - Target = island centre at terrain height, distance 39, elevation 52.7,
    yaw 0 (`camera.ini` progress 0.1); park FOV 80°.
  - Verified against `run/save/slot001.sav` eye (551,512,84.7), target
    (512,512,32); `--start-dino` keeps a close demo camera.
  - Evidence: `notes/ui-recon.md` ("Start camera solved"),
    `docs/platform/vertical-slice.md`, `notes/menu-start-dino.png`.
- UI-2 Phase D save/load complete: **Options Save/Load + `DINOPSAV 1`.**
  - `OPTIONS_MENU_SAVE/LOAD/OPTIONS_BUTTON` switch between
    `OPTIONS_SAVE_PANEL` and `OPTIONS_OPTIONS_PANEL`.
  - `src/formats/sav.cpp` reads the retail header (version, slot, UTF-16
    name); the list shows the real `run/save/*.sav` names (`agenttest`,
    `esfs`) plus `[port]` `.psav` saves.
  - `src/world/port_save.{h,cpp}` stores island, budget, clock, camera,
    path/fence grid and visitors. The list click selects a row, the green
    check saves/loads, the red X cancels. Loading enables the build/visitor
    assets on demand and can switch islands.
  - CLI `--save-port`/`--load-port`/`--save-name`; pre-frame
    `--park-click` save/load actions are queued.
  - Open: request/campaign-end triggers, `OPTIONS_NAME_SLOT` text entry,
    placed buildings/dinosaur state in the save.
  - Evidence: `docs/platform/save-load.md`, `notes/port-ui-save.png`,
    `notes/port-ui-load2.png`,
    `tests/test_viewer_cpp.py::test_cpp_options_save_panel`,
    `test_cpp_port_save_roundtrip`,
    `test_cpp_options_save_and_load_buttons`.
- UI-2 Phase D dialogs complete: **warning, request and campaign-end panels.**
  - `DIALOG_BACKGROUND` + one of `WARNING_PANEL`/`REQUEST_PANEL`/
    `CPGEND_PANEL`; `--warning`, `--request`, `--campaign-end`.
  - `WARNING_OUTCOME_ICON` closes, `REQUEST_VALIDATE_ICON` accepts,
    `REQUEST_CANCEL_ICON` cancels, `CPGEND_OUTCOME_ICON` closes; background
    clicks are consumed.
  - Text is word-wrapped into the active `*_TEXT_ZONE` and drawn above its
    back zone. Panels use `ALIGN_CENTER` (request/campaign end at
    `(258,204)`/`(258,125)` at 800×600).
  - Open: `OPTIONS_NAME_SLOT` text entry, placed buildings/dinosaur state in
    the port save.
  - Evidence: `docs/platform/hud.md`, `notes/request.png`,
    `notes/campaign-end.png`,
    `tests/test_viewer_cpp.py::test_cpp_request_dialog`,
    `test_cpp_campaign_end_dialog`.
- 291 tests pass; working tree committed (UI-2 Phase B rest plus
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
  - Remaining: a few field semantics (park header type ids).
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
- FMT-18 complete: **`.bai` action definitions fully solved.**
  - 64/64 files parse to an exact EOF: packed header `name[20]`, fields at
    `+0x14..+0x32`, `params[8]`, then `consume_flag` and five counted
    sections (preconditions, effect weights, need deltas, stat grants,
    effect class + 0x58-byte elements).
  - `field_20` high byte = special-action group (exploration 1, runaway 2,
    sleep 3, dino attack 4, obstacle attack 5, victory 6); long names are
    truncated to 19 chars in the file itself.
  - Loader `0x0053e060` rotates every pair: preconditions
    `(attribute, comparison, threshold)` -> `(threshold, attribute,
    comparison)`; weights `(attribute, unused, weight)` -> `(unused, weight,
    attribute)`; needs `(need, delta)` -> `(delta, need)`; grants
    `(attribute, value)` -> `(value, attribute)`.
  - `FUN_0056e0e0` gates on the preconditions and the `consume_flag`
    inventory check; `FUN_00569660` scores elements with
    `sum(attribute*weight) + rand()%field_14` and picks the first element
    whose `time >= score`; `FUN_0056e410`/`FUN_0056e5d0` apply need deltas;
    `FUN_00569a00` executes an element; `FUN_005727b0` applies events.
  - Event kinds are **effect ids**, not action ids, and the third field is a
    **percentage of the current value** (`value + current*percent/100`),
    proven by `GetQuad` (125..400%). Attribute ids were corrected from
    `FUN_00572620` + jump table `0x00572730` (4/5 = integer size class,
    7/8 = float body dimensions, 13 = weight total, 14 = second weight).
  - Retail totals: 46 preconditions/33 files, 43 weights/33, 134 need
    deltas, 24 grants, 204 elements/63 files, 171 events; effect classes
    0/1/2/3/5 = 44/1/1/1/17.
  - Header consumers solved: `field_32` is an additive behavior-node score
    (`FUN_0056e820`); `params[7]` and indexed `params[i]` are percent score
    multipliers (`FUN_0056cb30`/`FUN_0056cc20`, built by
    `apDinosaurBehavior`); `field_2e` is the action duration
    (`FUN_0056c0c0`/`FUN_0056d880`); `field_26`/`field_2a` form the
    end-of-action ramp pair consumed by `FUN_0056ddb0`; `flag_20` is copied
    by `FUN_0056ac80` but never read. Element `+0x28` is a duration in
    seconds, not a size.
  - Open: the renderer of the end-ramp vectors and the source object for the
    indexed `params[i]` multiplier; the stat-grant vector is stored and
    cloned but has no reader.
  - Evidence: `tools/formats/bai.py`, `docs/formats/bai.md`,
    `tests/test_bai.py`, `notes/bai-recon.md`,
    `notes/bai-tail-parse.json`.
- FMT-19 complete: **`TranslatorConstants.ini` + genotype translator.**
  - 41 `[SIMU_CONSTANTS]` keys map to translator `+0x228..+0x2dc`.
  - Five dinosaur-need formulas (`0x618020`): beauty, danger, spectacular
    (cube root of a 3-dimension volume × 60 / 1346), conventional, taming.
  - Phenotype accumulator fields `+0x18..+0x194` and the add/normalize part
    builders (`0x619c70`/`0x619d90`) mapped.
  - **`simu_trans.dic` -> translator map solved** (`0x6180e0`): Part 1 at
    `+0x1b8`, Part 2 at `+0x3bc`, the 11 Part 3 groups at `+0x308/+0x200/
    +0x31c/+0x214/+0x330/+0x344/+0x358/+0x36c/+0x2e0/+0x2f4/+0x380`,
    Parts 4-10 at `+0x394..+0x460`.
  - Runtime genotype = 18 genes x 3 ints (`[3g]`/`[3g+1]` alleles).
    `0x617250` maps part -> gene `[0,1,1,2,3]`, looks up
    `table[part][alleleB][alleleA]`, sums Perception/Intelligence/
    Beauty/Armor/Agility over the five parts and reads Attack per part;
    `0x619830` writes the five part lengths/widths/heights and scales.
  - Evidence: `tools/formats/translator_constants.py`,
    `docs/formats/translator-constants.md`,
    `tests/test_translator_constants.py`, `notes/translator-recon.md`.
- OPEN-1 complete: **remaining-unknowns tracker** (`notes/open-issues.md`)
  with status, attempts and next steps for `Actions.bad`, `.lnd` w/h,
  `.bfi` body, `.bwi` fields/switch keys, behavior-graph nodes and the demo
  `.shw` value.
- FMT-31 complete: **`Actions.bad` structure.**
  - 9,244 bytes = `u32 5` + 5 ? 462 floats. Each block ends in 12
    playback-scale values (0.8?2.0) matching the `.bai` `field_18` range;
    the 5 blocks match the 5 animation families.
  - First 450 floats per block remain unidentified (no loader yet).
  - Evidence: `tools/formats/actions_bad.py`,
    `docs/formats/actions-bad.md`, `tests/test_actions_bad.py`,
    `notes/actions-bad-recon.md`.
- AUDIT-1 complete: **asset coverage index** (`notes/asset-coverage.md`):
  all 2,891 files mapped to specs; remaining unknowns listed (`Actions.bad`,
  `.lnd` width/height, demo `.shw` judge value, `.bfi` body ints, `.bwi`
  field semantics, behavior-graph nodes).
- FORMAT-SEMANTICS batch complete:
  - ENV-1 re-verified: `scripts/check-env.ps1` passes (317 tests),
    `scripts/build.ps1` is up to date, Ghidra MCP lists the analysed
    `DinoIsland.exe-ebea17` (`notes/env-check.md`).
  - FMT-8: show-feature rotation is counter-clockwise 90 deg steps about the
    footprint centre (`FUN_00586b80` / `0x586d0c`, `FUN_0061ffc0`);
    `.shw` `(x,y)` remains the min-corner anchor. Wall type has only the
    retail value 1.
  - FMT-28: missing `DinoStore1` is an authoring gap; difficulty selects only
    budget/sellback/profiles, so it does not modify store prices.
  - FMT-29: profile triplet's second value is retail-constant 100; `Shows8/9`
    are empty because only campaigns 1-7 exist.
  - FMT-30: only `Config1` is opened by `FUN_005e5d10`; a new game and the
    saved camera both use progress 0.1; hardness min/max are separate fields
    and the 6/2 order is intentional.
  - Evidence: `docs/formats/shw.md`, `docs/formats/market.md`,
    `docs/formats/freegame.md`, `docs/formats/config.md`,
    `notes/env-check.md`.
- DEC-2b complete: **dinosaur behavior graph has no type enum.**
  - `apDinosaurBehavior` (`0x521b30`) owns the actor and two sub-behaviors;
    the graph is serialized by `0x56ce80` / `0x56d5a0`; `0x29` is the
    serialization version (0x25..0x2b), not a 41-node-type switch.
  - Base node `0x56cc90` (0x40 bytes): kind dword at +8, scalars at
    +0xc/+0x10/+0x14, int vectors at +0x18/+0x28, action object/vtable at
    +0x38/+0x3c.
  - Builders `0x56e190`/`0x56e300` attach nodes to usable action objects;
    `0x56e410` scores kind-1 nodes through `vtable+0x14` and the `.bai`
    effect weights; `0x56e0e0` applies the `.bai` preconditions.
  - The BAD FOOD/REPOS -> action 4/2 choice is the `.bai` score/precondition
    path, not a node-type constant. `notes/dinosaur-behavior-graph.md`.
- FMT-16b complete: **mission content groups.**
  - `BuildingsN`/`FeaturesN` indices are categories (farm, seats, entrance,
    barracks, souvenir, restaurant, ride, deco; lunchbox, cages, judges,
    bait, waypoint/smell, obstacle, power-up tiles, special tiles, vehicles).
  - `FoodSources0/1` plant/animal; `ShowsN` campaign page; `DinoStoreN`
    per-mission store (campaign prices far below Market.ini); `EggsN` pairs.
  - Quirks: `TileSmellBad` in 4 Cpg02 missions, mixed-case `.shw` names,
    Cpg07 `BarakSport`, repeated store entries.
  - Briefing: the 7 `IslandSnapshot` TGAs are not shipped (dead field); the
    5 ending videos exist (Cpg06/Cpg08 empty, Cpg07 `None`).
  - Edition demo `.shw` fourth value = wall type (`WallList.ini` index:
    2 MediumIron, 6 MediumWood); resolved, not judge kind.
  - Evidence: `docs/formats/campaign.md`, `tests/test_campaign.py`,
    `notes/campaign-recon.md`.
- FMT-30 complete: **`Data/Config` startup/input/camera.**
  - `camera.ini` gives 5 identical motion blocks (distance 10..300,
    elevation 3..500, rotation ?/2, move 1..20, progress 0.05..0.2); the
    initial framing is still not in the data.
  - `KeyMapper.ini` 17 bindings; `key/KeyFrancais.ini` 144 DIK/MIK rows,
    AZERTY; `Start.Ini` 800?600 HAL/SW and the key-table path.
  - Evidence: `tools/formats/config.py`, `docs/formats/config.md`,
    `tests/test_config.py`, `notes/config-recon.md`.
- FMT-29 complete: **`FreeGame.ini` free-game setup.**
  - Budgets 75000/50000/25000, sell-back 1.0/0.5/0.2; lab starts at level 3;
    interface all 7 enabled, dinosaur selling allowed.
  - Profiles: 3 lists ? 6 triplets `(profile, first, second)`; first sums
    180/110/60, second always 100.
  - Shows: 39 names on pages 0..7 (8/9 empty), all `.shw` exist.
  - Dino store: 20 entries, a subset of `Market.ini` with matching prices.
  - Evidence: `tools/formats/freegame.py`, `docs/formats/freegame.md`,
    `tests/test_freegame.py`, `notes/freegame-recon.md`.
- FMT-28 complete: **`Market.ini` dinosaur store.**
  - 54 entries in 6 pages (`DinoStore0,2..6`; page 1 absent), each
    `(display name, price, .gen)`; prices 3,200?42,000; all genotypes exist.
  - Unsold design genotypes: `anky2.gen`, `peleca3.gen`, `proto2.gen`.
  - Evidence: `tools/formats/market.py`, `docs/formats/market.md`,
    `tests/test_market.py`, `notes/market-recon.md`.
- FMT-8b complete: **`.shw` feature grid.**
  - Tuples are `(FeatureList index, rotation, x, y)`: all 1,679 tuples in
    range 0..61, rotation 0..3, positions inside dimensions.
  - Cage features (0..3) = 174 = team slots in every file.
  - `Judge Kind` 1..4 = JudgeRace/JudgeObstacle/JudgeFight/JudgeDestruction;
    cage-size flags equal the cage features used (43/43).
  - `read_feature_names()` exposes the 62-entry FeatureList order.
  - Footprints: `.bfi` w/h is the grid footprint, `(x,y)` the min corner,
    rotation 0/2=(w,h) 1/3=(h,w). All 43 shows tile with 0 overlaps over
    29,500 cells; 3 judges overhang the edge by 1.
  - Evidence: `tools/formats/shw.py`, `docs/formats/shw.md`,
    `tests/test_shw.py`, `notes/shw-recon.md`.
- FMT-27 complete: **`Data/States` render-state profiles.**
  - 34 `.ini` profiles + 18 `.txt` fragments; `mgStateInclude` expands
    recursively (`Standard` = `Misc` + `TextureParam` + `FogParam`).
  - Default 3D material: stage 0 `Modulate` (texture ? diffuse), `Wrap`,
    linear min/mag, mipmaps off. Fog 1500..2000, colour (255,220,165).
    Water: `SelectArg1` / `Modulate2x` / `Disable`.
  - Grammar reference is `Description.txt`; four state names drifted from
    it (`mgStateAlphablendOp`, `mgStatePointSizeMax`, `mgStateStencilFunc`,
    `mgStateTextureMaxAnisotropy`).
  - Park registry `0x600800`: 16 indexed states (0 FakeShadow ? 15 Clouds);
    menu list in `0x42da60`, WaterSurface in `0x602b80`.
  - Evidence: `tools/formats/render_states.py`,
    `docs/formats/render-states.md`, `tests/test_render_states.py`,
    `notes/render-states-recon.md`.
- FMT-25/26 complete: **`Laboratory.ini` + `GeneralParams.bgd`.**
  - Lab: 4 levels (reproduction 0/1/1/1, clone fail 100/40/20/0, beam cap
    0/1/2/3, fridge 4/8/8/8, mods 0/2/4/20), 3 beams, 8 costs.
  - `.bgd`: 170 bytes ? 4 floats, sounds `rocket/trumpet/startBlast/whitle`,
    5 scalars, a 4?4 float grid and 4 floats; all fields at known runtime
    offsets (`0x542f80`).
  - Evidence: `tools/formats/laboratory.py`, `general_params.py`,
    `docs/formats/laboratory.md`, `docs/formats/general-params.md`,
    `tests/test_laboratory.py`, `tests/test_general_params.py`,
    `notes/laboratory-params-recon.md`.
- FMT-24 complete: **skin/carapace textures + adornment models.**
  - `SkinCarapaceData.ini`: 22 skins (0 = `void`) and 16 carapaces ? 3
    textures (`nomina`/`hurt`/`blood`), all present.
  - `AdornmentData.ini`: 49 head/body/tail LOD pairs + 4 claw types ? 4
    sizes; all 114 models present under `Actors/elements_sup/`.
  - The 22/16 indices are exactly the unique values of the Part 7/8 maps.
  - Evidence: `tools/formats/genotype_assets.py`,
    `docs/formats/genotype-assets.md`, `tests/test_genotype_assets.py`,
    `notes/genotype-assets-recon.md`.
- FMT-23 complete: **`.bfi` show-feature definitions (full body).**
  - 62/62 files parse to an exact EOF. The body is a fixed sequential
    stream; the class tag does not change its shape.
  - 12-field header 0x00..0x2c, tag at 0x2d, then judge kind, show types,
    food kind, field pair, actions, anim flag/speed, state count, 8
    mesh/texture slots, texture dir, model scale, 7 effect INIs, 3 u32,
    billboard dir, attribute-weight lists, score steps, sounds, duration
    pair, category, switch key and tail flag.
  - Judge kinds 1..6; food kinds match `.bsi`/`.bni` 0..9; price is header
    `0x19`; waypoint team is header `0x1d`; display type header `0x21`;
    `state_count` is read by the mesh renderer `0x621770` (model *i* uses
    slots *i* and *i+3*); judge arena mesh is `effects[5]`.
  - Tags: Tile 26 (incl. `Danse` and trap/hole obstacles), empty 10 (large
    props), Judge 6, AnimalBait 5, StaticBait 5, Cage 4, Waypoint 4,
    LunchBox 1, Teleport 1. 57 `.mesh` + 49 `.tga`, all present;
    60/62 switch keys are real construction-kit sections.
  - Remaining: only field semantics (`field_3c`/`field_40`,
    `anim_flag`/`anim_speed`, `field_11`/`field_15`, `field_154..15c`,
    scoring arithmetic).
  - Evidence: `tools/formats/bfi.py`, `docs/formats/bfi.md`,
    `tests/test_bfi.py`, `notes/bfi-recon.md`.
- FMT-22b complete: **`.bwi` field semantics closed.**
  - `0x0c` is the construction cost (proven via `0x52b2dd` ->
    `FUN_00585d20`). `0x08`/`0x10` are hit points/build time (inferred).
  - The seven texture-scale floats are a material/tiling vector:
    `scales[5] = height/8` is the vertical tiling; the other six are
    constant defaults (1.0, 0.5, 0.25, 0.6, 2.0, 0.125) and are not read by
    the geometry builder `0x5ee970`. Cosmetic only.
  - Evidence: `docs/formats/bwi.md`, `notes/open-issues.md`.
- FMT-22 complete: **`.bwi` wall definitions.**
  - 24/24 files: version 4, height 2/4/6/8, cost class 10/15/30, a `u8`
    electrified flag, seven unaligned floats at 0x15, seven
    length-prefixed strings from 0x31, `u32 4` and two `SWITCH_*` keys.
  - `scales[5] == height/8`; the four `*Elect` files are the only electrified
    ones; every mesh/texture path exists.
  - Loader `0x53d9a0` builds 0xb4-byte records, matching the file layout.
  - Evidence: `tools/formats/bwi.py`, `docs/formats/bwi.md`,
    `tests/test_bwi.py`, `notes/bwi-recon.md`.
- FMT-21b corrected: **`.lnd` header semantics runtime-confirmed.**
  - `width`/`height` = free-game Entrance cell; the 0/1/2 value = Entrance
    orientation; the float = water surface Z.
  - Runtime `FUN_00586b80`: isl01 `(520,376) state 1`, isl02 `(312,504)
    state 2`, matching the `.lnd` fields; all seven campaign Entrance records
    match. Runtime `FUN_00602cf0`: water Z 16.0 (isl01) / 20.0 (isl02),
    matching the float.
  - `FUN_0051dad0` reads the four values into stack scratch; the exact carry
    path to the Entrance/landscape objects is still open (the earlier “dead
    metadata” conclusion was wrong).
  - The terrain builder `0x5fb8b0` `+0x24/+0x25/+0x26` fields are detail-map
    parameters (`FUN_005f3f10` sets 20000/1.0/0.05), not the `.lnd` trio.
  - Evidence: `docs/formats/lnd.md`, `notes/calibration-2026-10-26.md`,
    `notes/open-issues.md`.
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
- SIM-2b complete: **visitor choose → travel → use → leave ported.**
  - Six free-game easy `.bvi` profiles (weights/walk speed) and building
    attractions (category 1..9) from `.bbi`; exact score, BFS routes, timed
    need decay and the need0 snapshot rule; entrance exit and despawn.
  - `--visitor-stats`; 50-visitor run reached 47 alive / 24 using after 30 s.
  - Evidence: `src/world/visitors.cpp`, `src/formats/bvi.cpp`,
    `docs/platform/visitors.md`, `docs/decomp/visitor-ai.md`,
    `tests/test_viewer_cpp.py::test_cpp_sim2b_visitor_state_machine`.
  - **P0 review correction:** the actor ctor `FUN_005761b0` initializes
    need0 to `39` (`int(99*0.4)`), not `99`; and the need1 10 s decay pauses
    when `state == Use && target duration != 0`, not when `need_delta1 != 0`.
    `notes/calibration-2026-10-26.md` has the disassembly and runtime hit.
    The ported `visitors.cpp` now uses need0=39 and pauses need1 decay on
    nonzero-duration use.
  - `apVisitorBehavior::Update` (`0x522820`) + state transitions (`0x522940`):
    choose → travel → use attraction → leave.
  - Attraction scoring (`0x522a10`) uses a constant seven-float block from
    `.bvi` file `0x38..0x50` (`[1, 1, 1, 500, 300, −3000, 3000]`), not the
    five per-profile weights: `rand()%6000 − 3000 + Σ(attr·(99−need)²) −
    300·distance − 500·price`. Runtime-verified under x32dbg.
  - Show availability (`0x522ff0`), path following (`0x521490`) and obstacle
    rerouting (`0x522cb0`) mapped.
  - Needs solved except initial values: `spPark+0x78` is the visitor-profile
    table (6 in free game), `actor+0x68` is the profile index, and
    `+0x48/+0x4c/+0x50` are three int needs. Timed decay is inline in
    `FUN_00576320`/`FUN_005763f0`: need1 −1/10 s, need2 −1/7.5 s (state 1) or
    −3 (state 3), need0 −1/12.5 s (state 0/1) or −10 (state 3), with the
    state-2 appeal adjustment. Attraction use adds `def+0x50/+0x54` to needs
    1/2 and adjusts need 0 from the appeal. Hitting 0 sets the leave flag
    `+0x74`.
  - Remaining: consumer of the five variable profile weights, initial need
    values and the writer of `actor+0x68`.
  - Evidence: `notes/visitor-ai-recon.md`, `notes/visitor-needs-recon.md`,
    `notes/visitor-weights-runtime.md`, `docs/decomp/visitor-ai.md`,
    `docs/formats/bvi.md`,
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
- DEC-3 park header/tail complete: **`0x293` block mapped.**
  - Header: the leading `2` is consumed by `FUN_0058aed0` as a record count,
    not a proven class id; then `0/3` (manual/autosave type id), `100`,
    `10.0/0.0`, 32 zero bytes.
  - Tail: day `0`, elapsed/current seconds (23.214 manual, 0.0 auto), task
    field `+0x18`, 11 task slots, the 12-byte `spPark+0x148` field
    (`00 00 ad ba` + two zero dwords) and a trailing `0x01`.
  - Writers: `0x0058b200` (`spPark::Save`), `0x00594cb0` (schedule),
    `0x0058c260`/`0x0058c6b0`; reader `0x0058aed0` / `0x0058c210` /
    `0x0058c620`. The schedule vector length is not serialised.
  - `sav_graph.ParkState` exposes `class_id`, `header_a/b/c`,
    `header_reserved`, `schedule_*`, `trailer`, `tail_final`.
  - Evidence: `docs/formats/sav.md`, `notes/save-graph-recon.md`,
    `tests/test_sav_graph.py::test_park_header_and_schedule`.

- P2a/P2b complete: **retail `.wld`/`.sav` park import.**
  - `--wld-park FILE` imports road rectangles as path cells, fence
    rectangles as canonical south/east edges, and the existing `.bbi`
    building placement. Cpg04 on isl04: 57 buildings, 14 roads -> 301 cells,
    9 fences -> 268 edges; Cpg01: 1 road -> 16 cells.
  - `--sav-park FILE` skips the show catalog and reads `spSimuManager`:
    roads, visitors (0x50 records), budget (park block +0x30) and day/elapsed
    (tail +0x00/+0x04). slot001: budget 75,125, day 0, 23.214 s, 1 road ->
    16 paths, 2 visitors, 1 building.
  - Corrected the mapping: raw `.wld` world `wx` maps to `floor(wx/4)`, not
    `floor((wx+512)/4)` (the old note was off by half the world).
  - Dinosaurs spawn from the world genotype: gene 0's dominant allele
    (`min(A,B)`) is the head type 0..19, one-to-one with the 20 base
    `<species>.gen` files; the family is `simu_trans.dic` Part 10. Cpg04:
    20/20 on isl04 (allo 7, anky 2, coelo 4, deino 5, trood 2), distributed
    in the 9 fence rectangles because the file stores no position.
    `--focus-dino N`, `--dino-clip`, `--no-import-dinos`.
  - Evidence: `docs/platform/world-import.md`, `notes/cpg04-park-import.png`,
    `notes/p2-dinos.png`, `src/formats/dino_data.cpp`,
    `tests/test_viewer_cpp.py::test_cpp_wld_park_import`,
    `test_cpp_wld_park_import_single_road`, `test_cpp_sav_park_import`,
    `test_cpp_wld_park_dinosaurs`,
    `tests/test_anim_trans.py::test_base_species_head_types_are_unique`.
- P3a complete: **dinosaur action chooser decompiled.**
  - `FUN_0056e410` scores every available node from its `.bai` `need_deltas`
    via `FUN_0056e5d0(need, delta, actor+0x214)`; the weight tables are
    FOOD `[2,15,300,1]`, VIOLENCE `[1,10,60,1]`, SECU `[2,15,65,1]`,
    REPOS `[5,20,90,1]`, indexed by the need state 0..3 (thresholds
    `[80,60,30]` → >80 = 3, 60..80 = 0, 30..60 = 1, ≤30 = 2).
  - `FUN_0056e670` starts the highest-scoring node with score ≥ 1 after the
    `FUN_0056e0e0` precondition/consume gate; `FUN_0056d980` filters nodes by
    target distance. `.bai` `field_32` is the node base score.
  - Eat (`FOOD +30`, `VIOLENCE +10`) scores ~9009 at BAD food vs 70 at good;
    Sleep (`SECU −50`, `REPOS +100`) scores up to 7060 at BAD rest and is
    negative when full; all-full needs score 0 and the dinosaur stays idle.
  - Open: the action→animation clip link (`Eat`→`Chew` etc. is inferred, not
    verified) and the threat term details.
  - Evidence: `notes/dino-action-chooser-recon.md`,
    `docs/decomp/dinosaur-actions.md`, `docs/decomp/function-status.md`.
- P3b complete: **action loop drives the imported dinosaurs.**
  - `DinoActionLoop` (`src/world/dino_actions.*`) decays the four needs,
    decides every 0.5 s, counts the `.bai` `field_2e` duration and applies the
    need deltas on completion. The viewer runs one loop per imported dinosaur
    (`--dino-action-loop`, `--dino-needs F,V,S,R`, `--dino-action-speed N`)
    and switches clips through the inferred name mapping.
  - Cpg04: `--dino-needs 20,90,50,90` → 20 × Eat 9009;
    `70,90,90,20` → 20 × Sleep 7060; `99,99,99,99` stays idle.
  - Default needs are `50,99,99,99` (retail: `FUN_00570fd0` sets the food
    gauge to 50 % and the first 10 s tick rewrites UI FOOD to 50), which
    selects Eat/EatForSmallies at score 450.
  - Decay rates are port tuning; the raw gauge formulas are recorded in
    `notes/dino-action-chooser-recon.md`.
  - Open: the verified action→clip table, the SECU threat/visitor term and
    the feeding interaction.
  - Evidence: `notes/p3-dino-eat.png`, `docs/platform/world-import.md`,
    `docs/decomp/dinosaur-actions.md`,
    `tests/test_viewer_cpp.py::test_cpp_dino_action_loop`.
- P4a complete: **paddock-capacity model ported.**
  - `src/world/build.cpp`: `Paddock` (rectangle, capacity, used, occupants),
    `paddock_capacity = (w/cell)*(h/cell)`, `paddock_cost` 8/16/32/64 by
    index, duplicate rejection, `used`/`fill`, `paddock_can_add`/`add`.
  - Imported `.wld` dinosaurs fill their fence rectangles in record order
    under the retail rule instead of a modulo ring. Cpg04: 9 paddocks,
    total capacity 492, 20 dinosaurs used 384 (species-bbox size classes;
    retail individual classes give 336).
  - `--selftest` now has 30 checks (capacity, size class, costs, duplicate,
    fill, no-room); `test_cpp_wld_park_dinosaurs` asserts the summary.
  - Pending: the store purchase branch (`DINOACHAT` charge + store-row
    removal), the `WARNING_NO_ROOM_IN_FENCE` panel path and the runtime
    `dinosaur+0x1e0` cost index.
  - Evidence: `docs/platform/build.md`, `docs/decomp/construction-validation.md`,
    `notes/paddock-capacity-verification.md`,
    `tests/test_viewer_cpp.py::test_cpp_wld_park_dinosaurs`,
    `test_cpp_selftest`.
- P4b complete: **building construction through the retail panel.**
  - The construction panel now switches its six `CONSTRUC_*_GROUP` categories
    and exposes every `CONSTRUC_*_BUTTON` as a `construct <widget>` action.
    The viewer resolves the widget through the `.bbi` `tail_strings` and
    places the building on the next ground click.
  - Build cost is `.bbi` file offset 21 (`D+0x18`), proven at
    `pvwStateBuildingConstruction` `0x5306e1` (gate `FUN_0052bed0`) and
    `0x5307ac` (`FUN_00585d20`). File offset 25 is the visitor entry price.
  - Budget and footprint overlap are checked; the HUD `BUDGET_AMOUNT` follows.
    RestCirc costs 2,500. `--construct`, `--construct-pos` (repeatable),
    `--park-budget` cover the tests.
  - Evidence: `docs/platform/build.md`, `docs/formats/bbi.md`,
    `src/ui/park.cpp`, `tests/test_viewer_cpp.py::test_cpp_construction_placement`,
    `tests/test_bbi.py::test_construction_cost_is_file_offset_21`.
- P4c complete: **daily settlement on the park-clock rollover.**
  - Ticket income = visitor count × the Entrance `.bbi` visitor price (50);
    wages and EFFECTIFS_SECU are zero until staff/animal actors exist. The
    counters roll with `ParkClock::next_day()`. `--settle-day` seeds one
    settlement for tests; 8 visitors give income 400, budget 75,400.
  - Open: the exact per-visitor ticket value (manual save 125/2) and staff
    wages (`0x585d80`) / EFFECTIFS_SECU (`0x585d50`).
  - Evidence: `docs/decomp/economy.md`,
    `tests/test_viewer_cpp.py::test_cpp_daily_settlement`.
- P4 research complete (**capacity + building placement ported**; store
  purchase and the warning panel remain):
  **`FENCE_INSERTION` validator is a paddock-capacity test.**
  - `0x535be0` is `pvwStateFenceInsertion` (ctor `0x535b10`,
    `FUN_0052bd10(0x12, "FENCE_INSERTION")`, vtable `0x642d80`). It places a
    dinosaur (store purchase when `tool+0x3c`, otherwise a park dinosaur)
    into a paddock: validate → place → charge/remove the store row
    (`FUN_00585cc0`/`FUN_00594270`, only when `tool+0x3c`) →
    `stFence::AddActor` or park registration.
  - The `WARNING_NO_ROOM_IN_FENCE` gate is `stFence::CanAddActor` `0x57eb50`:
    reject a duplicate occupant, else `capacity(+0x7c) - used(+0x80) >=
    cost_table[dino+0x1e0]` with costs `8/16/32/64` at `+0x84..+0x90`.
    `stFence::AddActor` `0x57e8a0` updates `used` and `fill(+0x78)`. The
    occupant vector is a 4-byte pointer vector, not 16-byte records.
  - `stFence` ctor `0x57dc60`: `capacity = ((x1-x0+1)/cell) *
    ((y1-y0+1)/cell)` (float) from the `drsFence` rectangle. The capacity
    failure shows the warning through `FUN_005e3d70([tool+0x48])` and **stays
    in the insertion state**; it does not set state 5.
  - `stFence::PlaceActor` `0x57ec00` uses the caller's picked point on the
    purchase path. Its 50-iteration random loop is unreachable (`0x57ee44`
    sets the success flag to 1 and it is never cleared), and the function
    always returns 1. The port's mask-based random cell is an approximation.
  - The earlier "cell predicate" lead was a list/duplicate selection check,
    not a buildability rule. The `this+0x48` branch at `0x535c97` (64-byte
    records, `record+0x24 * 0.5 < record+0x20`) is still undecoded.
  - **Runtime-confirmed (Cpg04, `scripts/x32dbg/start-campaign.ps1`):**
    `stFence` ctor logs all nine fences with `cell = 4`; `[dino+0x1e0]` at
    `0x570fd0` is a size class `1,2,2,2,0,0,1,0,1,1,2,0,1,1,1,1,1,1,0,0`
    (coelo/trood 0, anky/deino 1, allo mixed 1/2). Cpg04 `used` is therefore
    **336**. The port now derives a species baseline from the rest-mesh bbox
    (`paddock_size_class`), giving **384**; exact parity needs the individual
    phenotype (Allosaurus splits four class 2 / three class 1).
  - Campaign progression is `run/save/dino.dat` (u32; `0xF` unlocks
    campaigns 1-4, enabling Cpg04 runtime tests). The mission briefing is
    modal; dismiss it before expecting sim hits.
  - Evidence: `notes/fence-insertion-recon.md`,
    `docs/decomp/construction-validation.md`,
    `notes/construction-validation.md`, `notes/campaign-unlock-runtime.md`,
    `notes/campaign-park.png`, `notes/campaign-sim.png`.
  - Reviewed by the reviewer agent; corrections applied: warning path stays
    in-state, 4-byte occupant vector, unreachable 50-loop, softened
    `+0x1e0`, `stWall.cpp` rasteriser, `this+0x48` branch recorded as open.

## In progress

- **P3 action loop (remaining):** the chooser + `DinoActionLoop` are ported
  and drive the imported dinosaurs. The old "action→clip table" premise is
  resolved: `aacDinosaurAnimator` selects a locomotion/activity slot from
  `saDinosaur+0x2bc` plus an event animation handle (`+0x518/+0x520`), never
  the action id. See `notes/action-clip-lead.md`. Remaining: wire the event
  animation handle source, the SECU threat/visitor reaction term and the
  feeding interaction (Eat currently assumes an item).
- **UI-2 Phase C rest:** dynamic values inside sub-panels (budget ledger rows,
  list snapshots, slider/checkbox selection), minimap water colour/partition
  polish. Button bindings, hover/pressed states, the menu→island handoff,
  the minimap and 1024×768 anchoring are done.
- **UI-2 Phase D:** `OPTIONS_NAME_SLOT` text entry, the dialog family,
  Save/Load, building placement, the imported dinos' actor state
  (needs/action id/remaining seconds) and the visitors' profile/needs are
  stored/restored. Visitor travel state and the clip playback phase are not
  stored.
- Phase 3 vertical slice otherwise works: island, vegetation, dinosaur, free
  camera, path/fence, sky/water, visitors, HUD.
- WORLD-3: the skybox face order/UVs match the retail builder
  (`front=-X, left=+Y, back=+X, right=-Y, top=+Z, bottom=-Z`), the water uses
  the retail additive blend (`WaterSurface.ini`: One/One/Add), and the scene
  and water fog are applied (`FogParam.txt` 1500..2000 warm,
  `FogParam_WaterSurface.txt` 100..700 black; both linear). Remaining: the
  second `Modulate2x` water texture stage (coord set 1, animation) and the
  skybox bottom/side seam.
- WORLD-2 remains: frustum culling and LOD2. The `model[0] = 1` slope basis
  is ported (`dino::slope_basis`, three-radius terrain normal; selftest
  covers flat and tilted cases). `model[1]` is 0.0 in all files. Building
  placement from `.wld`/`.sav` is solved (57/57 on Cpg04/isl04), and
  construction-UI placement is now live.
- QOL-2 (UI scaling) builds on the `.fnt` renderer.

## Next action

1. UI-2 Phase D is closed except visitor travel state and the dino clip
   playback phase; add those if the save fidelity matters.
2. UI-2 Phase C rest: dynamic values in sub-panels (list snapshots, sliders)
   and minimap water/partition polish.
3. P3 rest: the chooser, probe and `DinoActionLoop` viewer integration are
   done; the "action→clip table" premise is resolved (locomotion slot +
   event animation handle, `notes/action-clip-lead.md`). Remaining: wire the
   event animation handle source, the SECU threat/visitor term, and model the
   feeding item so Eat requires a real food source. See
   `notes/dino-action-chooser-recon.md`.
4. Capture a side-by-side comparison against the original game for WORLD-3.
5. WORLD-2 rest: frustum culling and LOD2; the `model[0] = 1` slope basis
   and construction-UI placement are done.
6. Continue hybrid detours (`0x47d460`, `0x47cc90`) when contracts are ported.
7. DEC-2 follow-up: runtime-confirm the task order and `0x1f` idle sentinel,
   and name the two dinosaur need gauges (`+0x96`, `+0x97`); action IDs are
   solved in `docs/decomp/dinosaur-actions.md`.
8. DEC-3 rest: the world object graph, unlock arrays, AI/camera tail, lab
   genotype store, park tail and the dinosaur-store price are solved.
   Remaining: the park record type ids (`0/3`), the `100`/`10.0/0.0` fields
   and a few small fields. See `docs/formats/sav.md` and
   `docs/formats/wld.md`.
9. SIM-4 rest: runtime-verify the panel rows against a populated save; the
   row mapping, category labels and daily settlement trigger are solved. See
   `docs/decomp/economy.md`.
10. STAFF rest: decode job kind `0xb`; roles (Nutritionist 0 / Guard 1) and
   wages are solved in `docs/decomp/economy.md`. See
   `docs/decomp/employee-ai.md`.
11. User: publish `docs/community/xentax-post.md` and record any replies.
12. SHOW research: the `.bfi` attribute-weight lists and 200..1000 score
   steps are decoded, but their consumer is not. Next lead: the six judge
   behaviors share `0x582d70` and differ only by vtable
   (`0x64350c`/`0x6434e8`/...); map the score path from there.
13. WORLD-3: water height **confirmed** (`FUN_00602cf0` = the `.lnd` float:
   isl01 16.0, isl02 20.0), the skybox face order matches the retail builder
   (`0x00600f80`/`0x00600df0`), and scene/water fog plus the additive blend
   are applied. Remaining: the second `Modulate2x` water stage, the skybox
   bottom seam, a direct side-by-side still, and the exact `.lnd` carry path
   from the reader scratch to the landscape.
14. CONS-1 rest: refine the imported dinosaurs' paddock cost class from the
   species baseline (port `used` = 384) to the individual phenotype (retail
   Cpg04 `used` = 336; Allosaurus is four class 2 / three class 1); port the
   store purchase branch (charge `DINOACHAT`, remove the store row) and the
   `WARNING_NO_ROOM_IN_FENCE` panel path with the insertion state staying
   active; decode the `0x535c97` branch. The capacity model and both runtime
   fields are solved (`cell = 4`, size class 0..2). See
   `docs/decomp/construction-validation.md`.

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
