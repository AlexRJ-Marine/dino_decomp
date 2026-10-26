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
- [x] Solve the LOD2/LOD3 bind rule: `inv(inverse_bind)` equals the skeleton
  rest for 58/60 dinosaur skins; only pachy LOD2/LOD3 store a bent-neck pose.
  Use `inv(skeleton rest)` as the `.mesh` bind, `inverse_bind` for the
  `.skin` bind.
- [ ] Apply the bind-matrix choice in `build_skinned_mesh` (snippet in
  `notes/lod-bind-recon.md`).
- **Evidence:** `notes/lod-bind-recon.md`, `tools/recon/analyze_lod_bind.py`,
  `tests/test_skin.py::test_pachy_lod2_bind_pose_retarget`,
  `test_only_pachy_lod23_bind_pose_differs`, `docs/formats/skin.md`.
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

### FMT-8: `.shw` show scenarios
- [x] Parse all 43 text files: 11 `//` sections, feature grid and team slots.
- [x] Validate every feature/team count; correlate with the save catalog
  (cost and dimensions match all 39 records).
- **Acceptance:** 43/43 parse, 0 errors; 1,679 features, 174 team slots.
- **Depends on:** RUN-2.
- **Evidence:** `tools/formats/shw.py`, `docs/formats/shw.md`,
  `tests/test_shw.py`, `notes/save-recon.md`.

### FMT-9: `.gen` genotypes
- [x] Parse 57 design genotypes (18 genes) and 20 editor genotypes (27 genes).
- [x] Decode the 18-gene table and observed allele ranges.
- [x] Decode expression/dominance and rarity weights from `simu_trans.dic`
  (`min` for dominant genes, average for skin-sub/carapace-sub/Agre/Stam/Cour).
- **Acceptance:** 77/77 parse, 1,566 genes; 18/18 gene blocks valid.
- **Depends on:** RUN-2.
- **Evidence:** `tools/formats/gen.py`, `gene_trans.py`,
  `docs/formats/gen.md`, `tests/test_gen.py`, `test_gene_trans.py`,
  `notes/gen-recon.md`.

### FMT-10: `.bvi` visitor profiles
- [x] Parse 24 profiles: 96-byte header, 28 strings, personality ids.
- [x] Map the 10 personalities and their walk speeds/preference vectors via
  `VisitorList.ini`.
- [ ] Decode the five preference weights and the 91–113-byte tail.
- **Acceptance:** 24/24 parse, 624 mesh paths exist, 0 errors.
- **Depends on:** RUN-2, DEC-2.
- **Evidence:** `tools/formats/bvi.py`, `docs/formats/bvi.md`,
  `tests/test_bvi.py`, `notes/bvi-recon.md`.

### FMT-11: `.bbi` building definitions
- [x] Parse 39 files: 121-byte header, 10 mesh/skel/anim paths, tail strings.
- [x] Footprint/cost hypothesis from the header; 390 asset paths exist;
  212/213 `CONSTRUC_*` UI names resolve.
- [ ] Exact header/tail numeric semantics.
- **Acceptance:** 39/39 parse, 0 errors, BuildingList maps 1:1.
- **Depends on:** RUN-2, FMT-7.
- **Evidence:** `tools/formats/bbi.py`, `docs/formats/bbi.md`,
  `tests/test_bbi.py`, `notes/bbi-recon.md`.

### FMT-12: `.bsi` food sources
- [x] Parse 10 files: 29-byte header (nutrition, quantity, price, kind id),
  four asset strings, feeder mesh and build button in the tail.
- [x] Validate all 40 asset paths and 10 feeder meshes; all 10
  `FOOD_DISTRIBUTION_*` buttons resolve.
- [ ] Exact `quantity`/tail numeric semantics.
- **Acceptance:** 10/10 parse, kind ids match FoodSourceList order.
- **Depends on:** RUN-2, FMT-7.
- **Evidence:** `tools/formats/bsi.py`, `docs/formats/bsi.md`,
  `tests/test_bsi.py`, `notes/bsi-recon.md`.

### FMT-13: `.bni` animal profiles
- [x] Parse 5 files: 48-byte header, size/food value/quantity/food kind,
  5–12 meshes + texture directory.
- [x] Food kind ids match `.bsi`; all 33 mesh paths exist.
- [ ] Full numeric field semantics.
- **Acceptance:** 5/5 parse, 0 errors.
- **Depends on:** FMT-12.
- **Evidence:** `tools/formats/actors.py`, `docs/formats/bni.md`,
  `tests/test_actors.py`, `notes/actors-recon.md`.

### FMT-14: `.bei` employee profiles
- [x] Parse 2 files: 60-byte header, kind/height/walk/action speeds,
  meshes, texture, sounds (`tweet`, `singing`).
- [x] All 9 mesh paths exist.
- **Acceptance:** 2/2 parse, 0 errors.
- **Depends on:** FMT-7.
- **Evidence:** `tools/formats/actors.py`, `docs/formats/bei.md`,
  `tests/test_actors.py`, `notes/actors-recon.md`.

### FMT-15: `.seq` animation events
- [x] Parse 10 files (5 FX + 5 Sound): counts, shared 42–47-string table,
  per-sequence records.
- [x] FX: 41×8 + 96×6 exact; events are `(frame, type, string)` at 30 fps.
- [x] Sound: 41×8 + 91×16 exact; events are `(type|string, flags, time,
  value)` with seconds.
- [ ] Type codes, Sound flags/value and the family `lead` float.
- **Acceptance:** 10/10 parse, 1,304 events, 0 errors.
- **Depends on:** FMT-5, FMT-9.
- **Evidence:** `tools/formats/seq.py`, `docs/formats/seq.md`,
  `tests/test_seq.py`, `notes/seq-recon.md`.

### FMT-16: Campaign and mission data
- [x] Parse `CampaignList.ini`, `CampaignBase.ini`, `CpgNN.ini`,
  `MissionList.ini`, `MissionBase.ini`, mission files.
- [x] 7 campaigns, 39 missions, 89 objective lines (28 verbs); 206/206 loc IDs
  resolve; all `WorldFile` `.wld` files exist.
- [x] Decode the objective dispatch: 33 verbs, ids 0–32, evaluator addresses,
  tri-state verdict (0 pending / 1 success / 2 failure).
- [x] Simple evaluators (treasury, marchandizing, dyingdinos, employees,
  visitors, happyvisitors, connected buildings, camera altitude, delay limit,
  buy-all-dinos, bankrupt) decoded.
- [x] Dinosaur-trait verbs: 10-slot pattern mapped to actor fields
  (`+0x90` cuteness, `+0xbc` aggression, `+0x1e4` hybrid, `+0x1e8` scarcity,
  `+0x244` food, `+0x98` size, `+0x7c` speed, `+0xc0` stamina, `+0x1e0`
  agility, `+0x8c` intelligence).
- [x] Show query (`0x567190`): show types race/obstacle/fight/destruction/
  team/capture/fence, per-profile score array, participant minimum.
- [x] `scientificshow` (show score > arg1, dino `+0x1e4` > arg2) and
  `dinoexclusiveshow` (a dinosaur unique to one show).
- [x] Difficulty selection: `mission+0x50` 0/1/2 picks `Budget`/
  `SellBackFactor`/`VisProfiles` and their `Medium`/`Hard` variants.
- [ ] Human-readable names for the trait/show fields, objective weighting and
  where `mission+0x50` is set.
- **Acceptance:** 7/7 campaigns and 39/39 missions parse; every mission verb
  resolves to the 33-verb table.
- **Depends on:** FMT-7, FMT-8, FMT-9.
- **Evidence:** `tools/formats/campaign.py`, `docs/formats/campaign.md`,
  `tests/test_campaign.py`, `notes/campaign-recon.md`,
  `notes/objectives-recon.md`.

### FMT-17: `anim_trans.dic` / `graphic_trans.dic` / `proportion.dat`
- [x] Parse both text dictionaries: 20 dinosaurs, 5 families, 51 animation
  names, four 20×20 merge tables, five reference widths.
- [x] Parse `proportion.dat`: 20 × 5 part scale factors (`head, arm, body,
  leg, tail`; leg column = 1.0).
- [x] All 51 clip names exist in every family folder (51 × 5 = 255 `.anim`).
- [x] Pin the ArmLeg directional 10/10 split and its two exceptions.
- [ ] Meaning of merge codes 1/50/99 and the consumer in the hybrid lab.
- **Acceptance:** all three files parse; matrix dimensions/value sets and the
  20 × 5 table are checked.
- **Depends on:** FMT-5, FMT-9.
- **Evidence:** `tools/formats/anim_trans.py`,
  `docs/formats/anim-trans.md`, `tests/test_anim_trans.py`,
  `notes/anim-trans-recon.md`.

### FMT-18: `.bai` action definitions
- [x] Decode the packed fixed header (name[20] + fields + `params[8]`),
  64/64 files parse; names match `ActionList.ini` order.
- [x] Decode the special-action group in the high byte of `field_20`
  (exploration/runaway/sleep/attack/obstacle/victory).
- [ ] Tail arrays (12-byte and 8-byte records pushed into the 0xac-byte
  action record) and the semantics of `field_14/18/1c/2a/2e/32` plus the
  eight params.
- **Acceptance:** all 64 headers parse; the group map and name truncation are
  checked against retail files.
- **Depends on:** FMT-15.
- **Evidence:** `tools/formats/bai.py`, `docs/formats/bai.md`,
  `tests/test_bai.py`.

### FMT-19: `TranslatorConstants.ini` and the genotype translator
- [x] Parse all 41 `[SIMU_CONSTANTS]` keys and map them to translator-object
  offsets `+0x228..+0x2dc`.
- [x] Decode the five dinosaur-need formulas (`FUN_00618020`): beauty, danger,
  spectacular (cube root of a volume × 60 / 1346), conventional, taming.
- [x] Map the phenotype accumulator fields (`+0x18..+0x194`) and the additive
  (`FUN_00619c70`) / normalizing (`FUN_00619d90`) part-record builders.
- [ ] Map the 18 gene translation values onto the part records
  (`+0x128..+0x19c`) and name the remaining phenotype fields.
- **Acceptance:** all 41 keys parse; the need formulas match the assembly.
- **Depends on:** FMT-9.
- **Evidence:** `tools/formats/translator_constants.py`,
  `docs/formats/translator-constants.md`,
  `tests/test_translator_constants.py`, `notes/translator-recon.md`.

### FMT-20: `simu_trans.dic` parts 2/3/10
- [x] Parse all ten `// Part` headers; Part 1 genes reuse `gene_trans.py`.
- [x] Part 2: four 20×20 merge tables, byte-identical to `anim_trans.dic`
  (1,600 cells).
- [x] Part 3: 11 attributes × 5 parts = 55 tables (20 diagonal + 20×20).
- [x] Part 10: 20 default family indices, counts 3/4/3/7/3; pachy = LightBiped.
- [ ] Parts 4–9 (dummy/skin/carapace) semantics and the gene→Part 3 lookup.
- **Acceptance:** 10 parts found, 55 attribute tables, Part 2 == anim_trans.
- **Depends on:** FMT-9, FMT-17.
- **Evidence:** `tools/formats/simu_trans.py`,
  `docs/formats/simu-trans.md`, `tests/test_simu_trans.py`,
  `notes/simu-trans-recon.md`.

### SIM-2b-W: visitor attraction scoring weights
- [x] Prove `DAT_00675638` is set once from the profile record `+0xD0`.
- [x] Identify the block as `.bvi` file `0x38..0x50`: constant
  `[1, 1, 1, 500, 300, −3000, 3000]` in all 24 profiles.
- [x] Decode the real score: `rand()%6000 − 3000 + Σ(attr·(99−need)²) −
  300·distance − 500·price`; `FUN_0059aef0` is `pow(x, 2.0)`.
- [x] Locate the five variable profile weights at record `+0x24..+0x34`
  (file `0x1C..0x2C`), runtime-verified with SurferMan.
- [ ] Consumer of the five variable weights (no read in an empty or
  two-visitor park within 15 s).
- **Acceptance:** live dump matches the file block; all 24 files share it.
- **Depends on:** FMT-10, SIM-2b.
- **Evidence:** `notes/visitor-weights-runtime.md`,
  `tools/recon/dump-visitor-weights.ps1`, `docs/formats/bvi.md`,
  `docs/decomp/visitor-ai.md`, `tests/test_bvi.py`.

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

### DEC-2: Simulation scheduler and AI update order
- [x] Decompile `lpScheduler` (`0x506ec0`, `0x507050`, `0x507490`,
  `0x507530`) and the park/menu task registration (`0x51d050`, `0x596500`).
- [x] Recover the ten-task park frame order and the three-task menu order.
- [x] Decompile `spSimuManager::Update` (`0x5869e0`), the park-wide step
  (`0x589cd0`), and `apBehaviorManager::Update` (`0x51fab0`).
- [x] Recover the `apDinosaurBehavior` reaction state machine
  (`0x521b30`, `0x521d80`, `0x521d10`, `0x521d60`).
- [x] Decode `saDinosaur::Update` need decay (`0x571540`) and the 5 s/10 s
  passes (`0x5725e0`, `0x571370`, `0x571410`).
- [x] Map the 64 action IDs to `ActionList.ini` / `.bai` and decode
  `ApplyActionEffect` (`0x5727b0`) and completion (`0x571190`).
- [ ] Runtime-confirm the task order, the `0x1f` idle sentinel, and name the
  two need gauges (`+0x96`, `+0x97`).
- **Acceptance:** a documented frame order and the scheduler/AI functions are
  in `docs/decomp/function-status.md`.
- **Depends on:** DEC-6.
- **Evidence:** `notes/scheduler-recon.md`, `docs/decomp/scheduler.md`,
  `docs/decomp/function-status.md`.

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
- [x] Visitors spawn on placed path cells and walk cell-to-cell (random
  neighbour, no immediate backtrack), at 1.6 units/s.
- [x] Retail two-pose walk flip book (`woman_walk1` / `woman_walk2`), one
  combined draw call, fixed seed for reproducibility.
- [x] Decompile visitor attraction scoring (`0x522a10`), travel (`0x521490`),
  attraction use (`0x5219f0`) and leave flow (`0x522940`).
- [ ] Port the full enter → choose → use → leave flow with visitor profiles.
- **Acceptance:** 50 visitors enter, use one attraction, and leave.
- **Depends on:** SIM-1.
- **Evidence:** `docs/platform/visitors.md`, `src/world/visitors.cpp`,
  `notes/visitors.png`, `tests/test_viewer_cpp.py::test_cpp_visitors_validate`,
  `notes/visitor-ai-recon.md`, `docs/decomp/visitor-ai.md`.

### SIM-3: Dinosaurs
- [x] Map all 64 action IDs, `ApplyActionEffect`, completion and the `0x1f`
  idle sentinel.
- [x] Decode the two raw gauges (`+0x258` rest, `+0x25c` food), the four UI
  needs (`+0x244` FOOD, `+0x248` VIOLENCE, `+0x24c` SECU, `+0x250` REPOS),
  their state indices and the `[80,60,30]` thresholds.
- [x] Cross-check the need-to-widget mapping against the info panel
  (`0x511d80`, `0x5138b0`, `0x5df900`).
- [ ] Decode the dinosaur-AI action chooser that turns a BAD FOOD/REPOS state
  into `Eat` (4) / `Sleep` (2), and the visitor reaction.
- **Acceptance:** one dinosaur eats, sleeps, and reacts to a visitor.
- **Depends on:** SIM-2.
- **Evidence:** `docs/decomp/dinosaur-actions.md`,
  `notes/dinosaur-actions-recon.md`, `docs/decomp/function-status.md`.

### SIM-4: Economy and attractions
- [x] Decompile the park budget API, ticket income, expense categories and
  daily settlement (`0x585c00`..`0x585e10`, `0x58ad00`).
- [x] Decompile employee AI: job queue, travel, kind-0xb work action and
  `spScheduleManager` (`0x5220e0`, `0x5224c0`, `0x522390`, `0x594b80`).
- [x] Name the fixed expense categories, day trigger, staff costs/role
  assignment and UI budget-ledger rows.
- [x] Decode the park clock and show scheduler: `spScheduleManager` time
  manager (`+0x00` base hour, `+0x08` elapsed, `+0x28` new-hour, `+0x29`
  end-of-day), 12-entry task vector, `0x58c2f0` advance, `0x58c350` H:M:S,
  `0x58c760` show scan and `0x58c390` show window.
- [x] Locate the day rollover: `0x524630` -> `0x58ae60` (actor reset),
  `0x58c2d0` (day++ / clock reset), `0x58ad00` (daily settlement).
- [x] Recover the fixed economy-category labels from the budget panel:
  FOOD, EFFECTIFS_FOOD, EFFECTIFS_SECU, DINOACHAT, LABOACTION, CONSTRUCTION,
  ENTREES, RESTOS, BARAQUES, SOUVENIRS, BOITNOIRE, POINTDEVUE, GRADINS,
  VENTES.
- [x] Confirm the `spPark+0x1c` index-to-label order. Corrected: `+0x1c` is a
  two-entry staff bucket array; the 14-row panel mapping is decoded from the
  refresh `0x510430` (`docs/decomp/economy.md`).
- [x] Employee roles/costs: `employeelist.ini` + `.bei` `kind_id`
  (Nutritionist 0 / Guard 1); hire and daily wage use `def[8]`.
- **Acceptance:** a 10-minute session is playable without crashes.
- **Depends on:** SIM-2.
- **Evidence:** `notes/park-economy-recon.md`,
  `docs/decomp/economy.md`, `notes/park-time-recon.md`,
  `docs/decomp/park-time.md`, `notes/employee-ai-recon.md`,
  `docs/decomp/employee-ai.md`, `docs/decomp/function-status.md`.

## Phase 3 verification targets — presentation

### UI-0: Bitmap font + HUD
- [x] C++ `.fnt` reader (9-u32 header, 247 codes, +2 metrics, 16-bit atlas).
- [x] R8 atlas upload, screen-space glyph quads, coverage alpha via `u_unlit=2`.
- [x] Live HUD (FPS/visitors/paths/fences), F1 toggle, `--no-hud`, `--hud-font`.
- **Acceptance:** `arial12.fnt` renders readable text over the island.
- **Depends on:** FMT-7, VIEW-1.
- **Evidence:** `docs/platform/hud.md`, `src/formats/fnt.cpp`,
  `notes/hud.png`, `tests/test_viewer_cpp.py::test_cpp_hud`.

### UI-RECON: Retail overlay and menu research
- [x] Inventory the overlay INIs: `MenuInterface.Ini` (114 widgets, 9 pages),
  `ParkInterface.Ini` (137 + 9 includes), the lab/list/construction panels.
- [x] Recover widget semantics in `moWidget::Load` (`0x5bca20`) and the
  anchoring pass (`0x5bc000`), including `TEXT_JUSTIFY` bits and
  `ALIGN_*`/`ALIGN_CENTER`.
- [x] Recover the menu state machine: nine pages, state indices, button
  handlers and next states (`mvwMenuStateManager` at `0x517c60`).
- [x] Map `TEXTURE` names through `MenuGraphicDB.ini`/`ParkGraphicDB.ini` to
  TGA atlas rectangles; verify all menu textures exist.
- [x] Prove the 800×600 UI is re-anchored, not scaled, at 1024×768 using the
  retail reference screenshots (MAD 1.0 vs 40.3).
- [x] Answer the seven UI-2 questions in
  `docs/platform/ui-original-menu.md`.
- **Acceptance:** the port can build the retail main menu and park HUD from
  data alone, with no guessed layout.
- **Depends on:** FMT-7, UI-0.
- **Evidence:** `notes/ui-recon.md`, `docs/formats/overlay-ini.md`,
  `docs/platform/ui-original-menu.md`.

### UI-1: Menus and HUD
- [x] Live HUD (FPS/visitors/paths/fences) and `.fnt` text.
- [x] Start screen (ENTER to begin) and Tab controls panel.
- [ ] New game/load/save flows and mouse-driven info panels.
- **Acceptance:** start a new game, open/close panels, save.
- **Depends on:** SIM-4.
- **Evidence:** `docs/platform/hud.md`, `notes/menu.png`,
  `notes/menu-help.png`, `tests/test_viewer_cpp.py::test_cpp_start_menu`,
  `test_cpp_help_panel`.

### UI-2: Original menu parity
- [x] Phase A: parse `MenuInterface.Ini` + `MenuGraphicDB.ini` into
  `src/ui/` (INI/tree/graphics), render the retail main menu at 800×600,
  wire pages 0..8 and the recovered transitions.
- [x] Phase B (partial): Free Game island list (10 names), island picture,
  difficulty buttons, Start/Cancel; Start shows the loading page, then closes
  the menu.
- [x] Phase B rest: load the selected island at runtime (`IslandList.ini` →
  `Data/Design/Landscape/<name>/<name>.lnd`), including terrain, basemap,
  vegetation, sky, water and GPU rebuild; free-game budgets 75k/50k/25k.
- [x] Phase C (800×600): park HUD from `ParkInterface.Ini` + `ParkGraphicDB.ini`
  (2,694 widgets, 53 visible), runtime budget/visitor/dino/show/time/day.
- [x] Phase C rest (bindings): `src/ui/park.cpp` maps the 12 HUD buttons, the
  construction sub-buttons, info-controller pages, lab centres and close
  buttons; hover/pressed frames, `--park-click` / `--park-hover`.
- [x] Phase C rest (minimap): `MAP_RENDERVIEW` renders a 256² top-down detail
  map into an FBO, centred on the camera target, rotated with the camera
  heading; held zoom buttons change the 0.1..2.0 scale; red/yellow camera
  marker.
- [x] Phase C rest (anchoring): `Overlay::layout_for_size` implements
  `moWidget::OnResize`; at 1024×768 the right panel is at 931,168, the gauge
  at 293,737 and the map at 840,584 (matching the measured retail layout);
  the minimap quad follows `MAP_RENDERVIEW`'s anchored rect.
- [x] Phase C rest (clock): `ParkClock` mirrors the decompiled time manager
  (base hour 9, time scale 60, 12-hour day ending at hour 11, day rollover);
  the HUD `PARK_CURRENT_TIME`/`PARK_CURRENT_DAY` are live, with
  `--park-time`/`--park-time-scale`.
- [x] Phase C rest (budget ledger): `BUDGET_*_NUMBER` category/total widgets
  show the fresh-park zeros (refresh mapping from `0x510430`); gauge budget
  follows the difficulty.
- [ ] Phase C rest: list snapshots/sliders, minimap water colour/partition
  polish.
- [ ] Phase D: save/load panels, menu sounds.
- **Acceptance:** startup shows the original main menu; Free Mode → island →
  Start reaches a park through the real screens.
- **Depends on:** UI-1, UI-0, `docs/platform/ui-original-menu.md`.
- **Evidence:** `docs/platform/ui-original-menu.md`,
  `notes/menu-retail.png`, `notes/menu-free.png`, `notes/menu-loading.png`,
  `notes/menu-pages-sheet.png`, `notes/menu-start-isl01.png`,
  `notes/menu-start-isl03.png`, `notes/menu-start-isl10.png`,
  `notes/minimap-isl01.png`, `notes/minimap-isl01-zoom-detail.png`,
  `notes/minimap-isl10-detail.png`, `notes/park-hud-1024.png`,
  `notes/hud-1024.png`, `notes/clock-0944.png`,
  `notes/park-hud.png`, `notes/park-budget.png`,
  `notes/park-construction-kit.png`, `notes/park-lab-breed.png`,
  `tests/test_viewer_cpp.py` (`test_cpp_retail_menu_*`,
  `test_cpp_park_hud_*`, `test_cpp_park_panel*`, `test_cpp_park_lab_shot`,
  `test_cpp_park_hover_shot`, `test_cpp_menu_start_loads_selected_island`,
  `test_cpp_island_list_and_free_game_budgets`,
  `test_cpp_minimap_detail_view`, `test_cpp_park_hud_1024_anchor`,
  `test_cpp_park_clock`), `tools/formats/overlay.py`,
  `tests/test_overlay.py`.

### MEDIA-1: Audio and video
- [x] Decode `ParkSoundDB.ini` / `MenuSoundDB.ini`, the dynamic
  `dsound.dll`/`winmm.dll` loader and the `msSoundEngine`/`msSoundDatabase`
  path.
- [x] Decode music selection: the `.lnd` music name maps to a streaming,
  looping sound-DB entry; five park themes plus the `musLabo` menu loop.
- [x] Decode the DirectShow movie pipeline (`mvVideo`/`mvMovie`), the
  `intro.mpg`/`generic.mpg` startup paths and the menu wrapper.
- [ ] Port music streaming/looping and SFX playback into the SDL3 viewer;
  implement the MPEG intro with a platform decoder.
- **Acceptance:** music loops, SFX trigger, intro plays.
- **Depends on:** VIEW-1.
- **Evidence:** `docs/formats/sounddb.md`, `docs/decomp/media.md`,
  `notes/media-recon.md`, `tools/formats/sounddb.py`,
  `tests/test_sounddb.py`.

### SAVE-RECON: Retail `.sav` research
- [x] Parse the header and the 39-record show catalog in `slot001.sav` and
  `auto.sav`; name, `.shw` paths and 6-u32 params recovered.
- [x] Recover the manager order: campaign → world (objects + show manager) →
  simulation → AI → scenario (`0x51e6c0` load / `0x51e7e0` save).
- [x] Recover the world serializer class table `0x12`–`0x18` (`0x53a140`).
- [x] Decode the simulation-state prefix: road rectangles, visitor records,
  park block with the real budget, 20-genotype dinosaur store, lab flags,
  AI lists and the 10-float camera state.
- [x] Decode the world object graph: class table `0x12`–`0x18`, every list
  record, the objective list, the 62/39/10/6 unlock arrays and the `.sav`
  world-start offset (`0x18 + name_len`).
- [x] Decode the `.wld` header, world objects, simulation roads and camera;
  7/7 version 50–53 campaign worlds parse.
- [ ] Populated lab records (the empty-flag path is solved).
- **Depends on:** RUN-2.
- **Evidence:** `notes/save-recon.md`, `notes/save-graph-recon.md`,
  `notes/world-objects-recon.md`, `docs/formats/sav.md`,
  `docs/formats/wld.md`, `tools/formats/sav.py`,
  `tools/formats/sav_graph.py`, `tools/formats/wld.py`,
  `tests/test_sav.py`, `tests/test_sav_graph.py`, `tests/test_wld.py`.

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
  **Done:** `docs/decomp/scheduler.md`, `notes/scheduler-recon.md`,
  `docs/decomp/dinosaur-actions.md` (64 action IDs mapped); runtime
  confirmation, the idle sentinel and need-gauge names remain.
- DEC-3: Map the `.wld` save format. **Solved for versions 50–53:** header,
  world object graph, objective list, unlock arrays, 39-show catalog,
  simulation-state prefix and the AI/camera tail. Populated lab records and
  a few field semantics remain. See `docs/formats/sav.md`,
  `docs/formats/wld.md`, `notes/save-graph-recon.md`,
  `notes/world-objects-recon.md`, `tools/formats/sav_graph.py`,
  `tools/formats/wld.py`.
- DEC-4: Recover the original assert-string source map into `docs/architecture.md`.
- DEC-5: Determine whether `Pathmap.bin` is generated at runtime.
  **Resolved:** 0 bytes in retail and after Free Mode; no exe string.
