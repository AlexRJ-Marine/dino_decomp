# Task queue

One task at a time. A task is done only when its acceptance criteria are met,
tests pass, and `notes/STATE.md` records the evidence path.

## Roadmap alignment

Four phases, tracked in `docs/ROADMAP.md`:

1. **Formats and assets** — FMT-1..7, VIEW-0, OUTREACH-1.
2. **Platform layer** — VIEW-1..3, QOL-1..3.
3. **Hybrid decompilation** — DEC-6 function list, HYB-1 hook/replace, then the
   WORLD/SIM/UI/SAVE tasks as verification targets.
4. **Remove the original exe** — PAR-1/PAR-2 and portability.

Current focus: **Phase 1, FMT-5 `.anim`**.

## Phase 0 — environment

### ENV-1: Verify the agent environment
- [ ] `scripts/check-env.ps1` passes.
- [ ] `scripts/start-ghidra.ps1` starts the Ghidra MCP server.
- [ ] `ghidra` MCP tools list `DinoIsland.exe`.
- [ ] `scripts/build.ps1` builds the C++ skeleton.
- **Evidence:** `notes/env-check.md` with command output.

## Phase 0.5 — runtime

### RUN-1: Run the retail game under dgVoodoo2
- [x] Patch the startup CD check in `run/DinoIsland.exe`.
- [x] Add dgVoodoo2 x86 wrappers.
- [x] Disable the intro video.
- [x] Reach Free Mode -> IGUANO -> easy -> Start.
- **Evidence:** `notes/park-after2.png`, `notes/menu-freegame-selected.png`,
  `scripts/x32dbg/play-freegame.ps1`.

### TEST-1: Live asset-load trace
- [x] Log `CreateFileA/W` and the mesh/skin/skeleton loaders under x32dbg.
- [x] Confirm entrance `.skel/.skin/.anim/.mesh`, people meshes, and dino
  animation files are loaded.
- **Evidence:** `notes/x32dbg-freegame.out.log`.

### FMT-4b: Decode the skeleton transform
- [x] Decode the 64-byte bone record and hierarchy from reader `0x477890`
  (live object dump no longer needed).
- [x] Validate on all 64 files and by composing the 80-bone allosaurus.
- **Evidence:** `docs/formats/skel.md`, `tools/formats/skel.py`,
  `tests/test_skel.py`, `notes/skel-allo-views.png`.
- **Depends on:** RUN-1, TEST-1.

### RUN-2: Save/load smoke test
- [x] Diagnose the silent save failure: `run\save` and `run\save\auto` were
  missing and the game does not create directories.
- [x] Create the folders; confirm `save/slot001.sav` and autosaves are written.
- **Evidence:** `notes/STATE.md`, `run/save/slot001.sav` (gitignored).

### PATCH-1: No-CD startup patch
- [x] Locate the drive scan (`GetDriveTypeA` / `GetVolumeInformationA`).
- [x] Patch the `CD found` branch at `0x401BFC`.
- [x] Document the patch and add an IPS + reversible Python patcher.
- **Acceptance:** patched `run/DinoIsland.exe` passes the CD check under
  x32dbg + dgVoodoo2 and reaches `Begin3D Success`.
- **Evidence:** `docs/patches/no-cd.md`, `patches/dinoisland-nocd.ips`,
  `notes/x32dbg-nocd.out.log`.

## Phase 1 — formats and assets

### FMT-1: `.mesh` header and file inventory
- [x] Parse the shared header across all 536 `.mesh` files.
- [x] Record version bytes, hash, sentinel, chunk count, name, payload count.
- [x] Reject malformed files with byte offsets.
- **Acceptance:** `pytest` passes; `notes/mesh-scan.json` has 536 valid entries.
- **Depends on:** ENV-1.
- **Evidence:** `notes/mesh-scan.json`, `docs/formats/mesh.md`, `tests/test_mesh_header.py`.

### FMT-2: `.mesh` geometry layout
- [x] Determine vertex stride, position/normal/UV offsets, index format, submeshes.
- [x] Cross-check against a known mesh (`cow_lod1.mesh`, `allo_lod1.mesh`) in Ghidra.
- **Acceptance:** parser reads a mesh and exports a valid OBJ with correct
  triangle count and bounding box; Blender opens it.
- **Depends on:** FMT-1.
- **Evidence:** `tools/formats/mesh.py`, `docs/formats/mesh.md`,
  `tests/test_mesh.py`; 536/536 meshes, 47,171 vertices, 35,190 triangles.

### FMT-3: `.skin` skinning data
- [x] Parse per-bone blocks: bone name + mini-mesh + material.
- [x] Parse the full-mesh block and match it to the `.mesh`.
- [x] Confirm every `.skin` parses (104 files, 2,244 bone blocks).
- [x] Assemble the 35 `allo` bone blocks with the solved `.skel` world
  matrices; the result is a coherent bind-pose proxy.
- [x] Decode the full-mesh tail: material, per-triangle material array,
  and 35 rigid skin records (vertex partition, weight 1.0, inverse bind).
- [x] Assemble the exact `.skin` bind mesh and validate a posed deformation.
- **Acceptance:** `allo_lod1` bone blocks transform into a coherent bind pose
  that matches the render mesh; all 104 `.skin` files parse.
- **Depends on:** FMT-2.
- **Evidence:** `docs/formats/skin.md`, `tests/test_skin.py`,
  `notes/allo-skin-bind.png`, `notes/allo-skin-pose.png`; 73 full meshes,
  60 dinosaur skin tables.

### FMT-3b: `.skin` bind matrices and weights
- [x] Decode the per-block tail (normals/colors/material) and the full-mesh
  tail (triangle materials + skin records).
- [x] Confirm rigid skinning: every full-mesh vertex in exactly one bone,
  weight `1.0`.
- [x] Verify `v_cm @ inverse_bind @ W_raw_bind == v_cm` on `allo_lod1`
  (max error `5.7e-5` cm).
- [ ] Retarget skeleton animation onto LOD2/LOD3 per-LOD bind poses.
- **Depends on:** FMT-3, FMT-4.

### FMT-4: `.skel` skeleton
- [x] Parse 64-byte 4×4 bone matrices, child lists, parent table, names, flags.
- [x] Compose `world = local @ parent_world`; validate all 64 files.
- [x] Confirm the allosaurus transform by rendering the skeleton.
- **Acceptance:** all 64 `.skel` files parse; hierarchy is acyclic.
- **Depends on:** FMT-1.
- **Evidence:** `docs/formats/skel.md`, `tools/formats/skel.py`,
  `tests/test_skel.py`, 64/64 files, 2,113 bones.

### FMT-5: `.anim` animation
- [x] Parse the 8-byte duration/start header, bone blocks, `u64` key times,
  and local 4×4 matrices.
- [x] Validate all 299 files: 9,129 tracks, 285,507 keys, monotonic times.
- [x] Apply `HeavyBiped_Armor` to `allo.skel` + `allo_lod1.skin`; render frames.
- **Acceptance:** all 299 `.anim` files parse; times are monotonic; a clip
  applied to a skeleton produces plausible motion.
- **Depends on:** FMT-3, FMT-4.
- **Evidence:** `tools/formats/anim.py`, `docs/formats/anim.md`,
  `tests/test_anim.py`, `notes/allo-anim.gif`.

### FMT-6: Simple asset loaders
- [x] TGA: 24/32-bit true-color, 8-bit grayscale, 16-bit and RLE support.
- [x] WAV: RIFF/PCM 16-bit, mono/stereo, header-only and sample loading.
- [x] Batch-validate the full retail sets against PIL and Python `wave`.
- **Acceptance:** batch-convert 10 samples of each; dimensions/duration match.
- **Depends on:** ENV-1.
- **Evidence:** `tools/formats/tga.py`, `tools/formats/wav.py`,
  `docs/formats/tga.md`, `docs/formats/wav.md`, `tests/test_tga.py`,
  `tests/test_wav.py`; 649 TGA + 231 WAV, 0 errors.

### FMT-7: Text/config formats
- [x] `.loc` (UTF-16LE records: FIELD_ID / TEXT / LAST_MODIFIED).
- [x] `.lnd` (8 resource strings, dimensions, music, 25 vegetation slots).
- [x] `.fnt` (16-bit atlas, 245 glyphs, +2 metric offset).
- [x] Render a reference string from `arial12.fnt`.
- **Acceptance:** parser round-trips a sample; font renders a reference string.
- **Depends on:** ENV-1.
- **Evidence:** `tools/formats/loc.py`, `lnd.py`, `fnt.py`,
  `docs/formats/loc.md`, `lnd.md`, `fnt.md`, `tests/test_loc.py`,
  `test_lnd.py`, `test_fnt.py`, `notes/font-arial12-sample.png`; 2 `.loc`
  (758 records), 10 `.lnd`, 5 `.fnt` (1,225 glyphs).

### VIEW-0: Simple asset viewer
- [x] Software renderer for one `.mesh` + `.tga` (views, supersampling,
  vertex colors, UV `v_tex = 1 + v`).
- [x] Texture preview and WAV info/play CLI.
- **Acceptance:** opens `allo_lod1.mesh` + `allo.tga`, a standalone TGA, and a
  WAV; writes a PNG screenshot/preview.
- **Depends on:** FMT-2, FMT-6.
- **Evidence:** `tools/viewer/asset_viewer.py`,
  `tools/viewer/soft_render.py`, `tests/test_viewer.py`,
  `notes/view-allo.png`, `notes/view-cursor.png`.

### OUTREACH-1: XeNTaX / ZenHax post
- [x] Draft the format summary and questions in
  `docs/community/xentax-post.md` (the user publishes it; the agent has no
  forum account).
- [ ] Publish and record any replies.
- **Depends on:** FMT-2, FMT-3, FMT-4.

## Phase 2 — platform layer

### VIEW-1: CMake + SDL3 + OpenGL window
- [x] SDL3 3.4.18 MinGW package + `scripts/setup-sdl3.ps1`.
- [x] `dino_viewer` target: 1280×720 resizable window, OpenGL context,
  input, delta time, clean shutdown.
- [x] BMP screenshot path and 2-frame smoke test.
- **Acceptance:** `scripts/build.ps1` builds; window opens for 5 seconds in a
  headless screenshot test or manual run.
- **Depends on:** ENV-1.
- **Evidence:** `src/viewer/main.cpp`, `docs/platform/sdl3-viewer.md`,
  `tests/test_viewer_cpp.py`, `notes/viewer-window.png`.

### VIEW-2: Load one mesh + texture
- [x] C++ `.mesh` reader (positions, indices, UVs, normals, colors).
- [x] C++ `.tga` reader (24/32-bit, 8-bit, RLE).
- [x] GLSL 3.3 lit pipeline, texture modulation, orbit camera, `--validate`.
- [x] `allo_lod1.mesh` + `allo.tga` renders; screenshot in `notes/`.
- **Acceptance:** `cow_lod1.mesh` + its TGA renders; screenshot in `notes/`.
- **Depends on:** FMT-2, FMT-6, VIEW-1.
- **Evidence:** `src/formats/mesh.cpp`, `src/formats/tga.cpp`,
  `src/viewer/main.cpp`, `tests/test_viewer_cpp.py`,
  `notes/viewer-allo.png`; 317 vertices / 382 triangles, 256×256 RGBA.

### VIEW-3: Skinned animation playback
- [x] C++ `.skel` (80 bones), `.skin` (35 rigid records, 317 assignments),
  `.anim` (35 bones, 1,085 keys) readers.
- [x] CPU rigid skinning per frame, quaternion-slerp sampling, VBO update.
- [x] Playable loop plus deterministic 25-frame capture.
- **Acceptance:** `allo` walk/idle plays smoothly; screenshot/gif in `notes/`.
- **Depends on:** FMT-3, FMT-4, FMT-5, VIEW-2.
- **Evidence:** `src/formats/{skel,skin,anim}.cpp`, `src/viewer/main.cpp`,
  `tests/test_viewer_cpp.py`, `notes/viewer-anim-1600.png`,
  `notes/viewer-anim-sheet.png`, `notes/viewer-anim-cpp.gif`.

### AUDIO-1: SDL3 WAV playback
- [x] C++ RIFF/WAVE PCM reader; SDL3 audio device stream; loop/mute.
- [x] `--wav`, `--once`, `--mute`; key-map action `mute=M`.
- **Acceptance:** a retail WAV plays and the viewer keeps running if no audio
  device is available.
- **Depends on:** FMT-6, VIEW-1.
- **Evidence:** `src/formats/wav.cpp`, `docs/platform/audio.md`,
  `tests/test_viewer_cpp.py`, `notes/audio-smoke.txt`.

### QOL-1: Resolution, widescreen, borderless
- [x] Windowed / fullscreen / borderless modes; F10/F11 toggles.
- [x] Persisted `dino_viewer.cfg` (size, mode, vsync); CLI overrides.
- [x] Widescreen framing via actual drawable size + fixed vertical FOV.
- **Acceptance:** config persists; 16:9 and 4:3 screenshots match expected framing.
- **Depends on:** VIEW-1.
- **Evidence:** `docs/platform/qol.md`, `tests/test_viewer_cpp.py`,
  `notes/qol-16x9.png`, `notes/qol-4x3.png`.

### QOL-2: UI scaling
- Scale the original overlay layout for high-DPI and widescreen.
- **Acceptance:** menus remain readable at 1080p/1440p/4K.
- **Depends on:** VIEW-2 + the Phase 3 overlay/UI layer.

### QOL-3: Key rebinding
- [x] Config-backed key map (`dino_keys.cfg`) for orbit/zoom/reset/window/quit.
- [x] `--rebind`, `--list-keys`, `--keys`, `--no-save-keys`; persists.
- **Acceptance:** rebind one camera and one reset action; persists.
- **Depends on:** VIEW-1.
- **Evidence:** `src/viewer/keymap.cpp`, `docs/platform/qol.md`,
  `tests/test_viewer_cpp.py`.

## Phase 3 — hybrid decompilation

### DEC-6: Function status list
- [x] Create `docs/decomp/function-status.md` with `identified` /
  `decompiled` / `verified` states.
- [ ] Keep it current as functions are replaced.
- **Depends on:** Ghidra import.

### HYB-1: Hook and replace the first loader
- [x] 32-bit `DINPUT8.dll` proxy; forwards `DirectInput8Create` to the real
  system DLL and installs an inline detour.
- [x] Replace `0x4663a0` (64-byte matrix reader) with C++.
- [x] Run Free Mode → IGUANO → easy → Start with the hook active; reach a
  playable park; 281,000 calls logged.
- **Acceptance:** original game loads a dinosaur with the replacement loader
  and the function-status entry is `verified`.
- **Depends on:** DEC-6, FMT-4.
- **Evidence:** `src/hook/dinput8_proxy.cpp`, `scripts/build-hook.ps1`,
  `scripts/play-freegame-direct.ps1`, `docs/decomp/hybrid.md`,
  `notes/park-hook.png`, `notes/hook-4663a0.log`.

### HYB-2: Replace the normal reader
- [x] Detour `0x4809a0` (12-byte/3-float normal reader) with C++.
- [x] Verify with the same park run; 95,000 calls, no crash.
- **Acceptance:** the game still loads with the replacement active.
- **Depends on:** HYB-1.
- **Evidence:** `src/hook/dinput8_proxy.cpp`, `docs/decomp/hybrid.md`,
  `notes/hook-4663a0-4809a0.log`.

## Phase 3 verification targets - world

### WORLD-1: Island terrain + placed vegetation
- [x] `.lnd` + 512×512 8-bit `.raw` heightmap → 1024-unit terrain mesh.
- [x] Basemap TGA, bilinear height sampling, 522,242 triangles.
- [x] `.imp` placements + `.blf` descriptors + lod1 meshes baked per entry.
- [x] isl01: 9 groups, 1,536 instances, 52,291 triangles, textures from the
  `.blf` texture directory.
- **Acceptance:** island `isl01` loads with correct bounds and textures.
- **Depends on:** FMT-6, FMT-7, VIEW-2.
- **Evidence:** `docs/platform/terrain.md`, `docs/formats/imp.md`,
  `docs/formats/blf.md`, `src/world/{terrain,vegetation}.cpp`,
  `notes/isl01-terrain.png`, `notes/isl01-vegetation.png`,
  `tests/test_viewer_cpp.py`, `tests/test_imp.py`, `tests/test_blf.py`.

### WORLD-2: Object placement and frustum culling
- Read scenario/landscape placement data; instance meshes; cull.
- **Acceptance:** 1,000+ instances render at interactive FPS.
- **Depends on:** WORLD-1.
- **Status:** placement and batching done (1,536 instances in 9 draws);
  frustum culling and the scenario/building placements remain.

### WORLD-3: Skybox, water, lighting
- [x] PNG skybox faces (`front/back/left/right/top/bottom`) via stb_image;
  unlit cube drawn before the terrain.
- [x] Tiled water plane at the `.lnd` float level, alpha-blended after the
  terrain; `--no-water`, `--water-level`, `--skybox`.
- [ ] Match the original look closely enough for comparison screenshots
  (cube orientation and water colour still open).
- **Acceptance:** side-by-side screenshot against the original.
- **Depends on:** WORLD-1.
- **Evidence:** `docs/platform/environment.md`,
  `src/formats/image.cpp`, `src/third_party/stb_image.h`,
  `notes/world3-sky2.png`, `notes/world3-hero.png`,
  `tests/test_viewer_cpp.py::test_cpp_skybox_and_water`.

### SLICE-1: Island + dinosaur scene
- [x] Combine `--lnd` terrain/vegetation with `--mesh/--skel/--skin/--anim`.
- [x] Spawn the skinned allosaurus on the heightfield (`--dino-pos`,
  `--dino-angle`, `--dino-texture`).
- [x] Use the `.mesh` standing rest pose with the `.skin` rigid weights
  (`--skin-bind mesh`), plus a soft contact shadow.
- [x] `HeavyBiped_Walk` / `Neutral` / `Run` deform the standing mesh cleanly.
- [x] Free-fly camera (`--free-camera` / `C`, WASD + mouse look, terrain clamp);
  all movement actions are rebindable.
- **Acceptance:** one scene shows island, trees, an animated dinosaur, and the
  player can move the camera.
- **Depends on:** WORLD-1, VIEW-3.
- **Evidence:** `docs/platform/vertical-slice.md`,
  `notes/isl01-dino-meshbind.png`, `notes/isl01-dino-walk.png`,
  `tests/test_viewer_cpp.py` (`test_cpp_island_dino_*`,
  `test_cpp_free_camera_start`).

## Phase 3 verification targets — simulation

### SIM-1: Path/fence placement
- [x] 4-unit grid over the island; paths own cells, fences own canonical
  south/east edges; undo stack.
- [x] Terrain ray-pick (march + bisect) and click placement
  (`1` path, `2` fence, `0` off, `U` undo).
- [x] Retail paddock fence mesh + `ground.tga` path tiles, baked per type.
- [x] Save/load placement files; CLI `--place-path` / `--place-fence`.
- **Acceptance:** place/remove a path and a fence; state survives save/load.
- **Depends on:** WORLD-1.
- **Evidence:** `docs/platform/build.md`, `src/world/build.cpp`,
  `notes/build-preview.png`, `notes/build-preview2.png`,
  `tests/test_viewer_cpp.py` (`test_cpp_build_save_load`,
  `test_cpp_pick_terrain`).

### SIM-2: Visitor agents
- Spawn visitors, pathfinding, needs, attraction use.
- **Acceptance:** 50 visitors enter, use one attraction, and leave.
- **Depends on:** SIM-1.

### SIM-3: Dinosaurs
- Load genotypes, needs, behaviors, animation state machine.
- **Acceptance:** one dinosaur eats, sleeps, and reacts to a visitor.
- **Depends on:** SIM-2.

### SIM-4: Economy and attractions
- Money, ticket price, staff, attraction income.
- **Acceptance:** a 10-minute session is playable without crashes.
- **Depends on:** SIM-2.

## Phase 3 verification targets — presentation

### UI-1: Menus and HUD
- Main menu, park HUD, info panels.
- **Acceptance:** start a new game, open/close panels, save.
- **Depends on:** SIM-4.

### MEDIA-1: Audio and video
- DirectSound-equivalent playback, music, MPEG intro.
- **Acceptance:** music loops, SFX trigger, intro plays.
- **Depends on:** VIEW-1.

### SAVE-1: Save/load
- Reproduce enough of `.wld`/save semantics or define a new format.
- **Acceptance:** save, restart, load, state matches.
- **Depends on:** SIM-4.

## Phase 4 — remove the original exe

### PAR-1: Reference comparison harness
- Run original under dgVoodoo2/DXVK; capture screenshots and traces.
- **Acceptance:** deterministic comparison scenes documented.
- **Depends on:** VIEW-2.

### PAR-2: Performance and packaging
- 60 FPS at 1080p on the target machine; reproducible builds.
- **Acceptance:** zip build runs on a clean Windows VM.
- **Depends on:** UI-1, SAVE-1.

## Backlog / research

- DEC-1: Map loader functions for `.mesh`, `.skin`, `.skel`, `.anim`, `.fnt`.
- DEC-2: Map the simulation scheduler (`lpScheduler`) and AI update order.
- DEC-3: Map the `.wld` save format.
- DEC-4: Recover the original assert-string source map into `docs/architecture.md`.
- DEC-5: Determine whether `Pathmap.bin` is generated at runtime.
