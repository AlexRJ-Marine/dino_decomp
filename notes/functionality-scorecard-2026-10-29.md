# Functionality scorecard — 2026-10-29

Status: **assessment after MAINT-1..6.** This page answers two questions:
how much of the retail game the port actually runs, and how much of the
refactor's surface is covered by evidence. It is a snapshot, not a roadmap;
the task view is `OPEN-TASKS.md` and the feature index is
`docs/platform/feature-status.md`.

## Method

- `docs/platform/feature-status.md` row counts (36 rows, one per feature):
  19 **ported**, 14 **partial**, 3 **decoded** (not ported), 0 **not ported**.
- Test evidence: 518 tests (403 in the fast gate, 115 window/screenshot),
  plus `--selftest` 194 checks.
- Fresh vertical-slice capture:
  `notes/harness/review-isl01-dino.png` (isl01 terrain, vegetation, water,
  HUD, minimap, skinned allo) from the refactored `build/refactor` binary.
- Fresh stability run: 60,000 frames on isl01 with the skinned allo,
  `160x120`, no vsync, exit 0, no errors or warnings
  (`notes/harness/review-soak.log`, 98.6 s wall clock).
- Domain gaps cross-checked against `notes/OPEN-TASKS.md` (49 items).

## Definition of a playable port (AGENTS.md)

| Criterion | State | Evidence |
| --- | --- | --- |
| Loads one island | **yes** | `--lnd isl01` in the capture; `test_cpp_lnd_validate` |
| Terrain + placed objects with textures | **yes** | capture; terrain/vegetation/sky/water/building shot tests |
| Skinned dinosaur animation | **yes** | capture (allo); `test_cpp_skinned_validate`, `test_cpp_skinned_window` |
| Player moves the camera | **yes** | retail camera equations (`park_camera.cpp`), `test_cpp_camera_retail_selftest` |
| Place a path/fence | **yes** | road tool + fence placement tests; occupancy-grid rejection |
| 10 minutes without crashing | **partial evidence** | 60,000 frames / 98.6 s fresh, exit 0; the last 10-minute wall-clock soak predates the refactor (`notes/stability3*`) |

The vertical slice is functionally met; the only unrefreshed item is a
10-minute **wall-clock** soak (this run is 60k frames at ~600 FPS).

## Feature matrix by area

| Area | Ported | Partial | Decoded | Reading |
| --- | ---: | ---: | ---: | --- |
| Assets/formats | 6 | 3 | 0 | All main retail formats parse; a few semantic fields and video polish remain |
| Menus | 3 | 2 | 0 | Main menu, Free Game, cinematics, dialogs work; campaign briefing and retail `.sav` slots are partial |
| Park | 2 | 5 | 0 | HUD/clock/camera/minimap and the road tool are complete; construction, store, lab, food and list panels are partial |
| Simulation | 4 | 3 | 2 | Visitors, dino chooser/needs, danger grid and campaign objectives work; economy, shows, employees partial; `.bni` actors and lab gameplay are decoded-only |
| Presentation/runtime | 3 | 0 | 1 | Camera/keymap/config/harness ported; global-parameter rates decoded-only |

Row counts understate the working surface: a "partial" row is a running main
flow with enumerated gaps, not an absent system. They overstate it too: format
rows are cheap compared with simulation rows.

## What works end to end

- **Assets**: `.mesh` (536/536), `.skin`, `.skel`, `.anim`, `.lnd`/`.raw`,
  vegetation `.imp`/`.blf`, `.tga`, `.fnt`/`.loc`, `.wav`, `.sav`/`.wld`,
  `.bbi`/`.bvi`/`.bsi`/`.bei`/`.bfi`/`.shw`, `.bai`/`.bad`, translators.
- **Rendering**: SDL3 + OpenGL 3.3 scene, detail-mapped terrain, vegetation
  sector LOD + frustum cull, skybox/water, skinned dinos, roads/fences/
  buildings, visitors/employees, HUD, native menu overlays, minimap.
- **Park**: island load, Free Game start (Entrance dock + access road), retail
  road tool, fences/enclosures with occupancy validation, buildings, store
  purchase, budget/economy, daily settlement, park clock, camera.
- **Simulation**: visitor profiles/spawn/state machine/attractions/spending;
  nutritionist feeding queue + wages; dino import, needs, action chooser,
  food sources, health/SECU danger grid, show scoring; 33 campaign objectives.
- **Persistence**: `.psav` round-trip; retail park `.sav` re-serialized
  byte-identically and accepted by retail; campaign lists byte-identical.
- **Media**: WAV/music/SFX, MPEG playback with MP2 audio; ending videos.

## Main functional gaps (largest first)

1. **Dinosaur mover/action execution** (`P3-MOTION-1`): the chooser, needs and
   scoring run; the path mover, action-element events and the live
   perception/reaction entry points are the largest simulation gap.
2. **Shows**: `.bfi` arena/judge actors and the live performance loop are not
   acted out; scoring, scheduling, placement and the construction UI work.
3. **Laboratory gameplay** (`LAB`): panels render; breeding, cloning and the
   fridge logic are decoded but not ported.
4. **Retail `.sav` slots in Load Game** (`MENU-1/R-16b`) and the campaign
   mission list (no retail list; use `campaign-progress.txt`).
5. **Economy depth**: shop sales, lab actions, `EFFECTIFS_SECU`, exact
   attraction consumer weights.
6. **Construction/placement exactness**: slope/pen validation, costs/limits,
   per-triangle fence materials, food-panel placement buttons.
7. **UI polish**: slider/checkbox selection, list snapshots, minimap water,
   store 3D preview + gauges.
8. **Sell flows**: fences sell; roads/buildings/dinos/features/shows do not.
9. **Dino info/stats**: panel, per-tick health, dino sell, growth (the last is
   a proven negative in this build).
10. **Media/world polish**: intro movie, A/V sync, full SFX triggers,
    vegetation slope tilt, terrain patch LOD.

## Distance estimate

- **Vertical slice**: complete; every AGENTS.md criterion is evidenced except
  a fresh 10-minute wall-clock soak.
- **Broad playable port**: all domains have a working main flow (19/36 rows
  fully ported, 33/36 at least partial); the missing 14 partial rows are
  depth and edge cases, not absent screens.
- **Full parity**: not close. The remaining long tail is simulation semantics
  (mover, perception, live shows, lab, exact economy), which is the same
  bottleneck the roadmap identifies; format work is mostly done.

A single percentage is misleading because the feature rows are not equal
weight. The defensible summary is: **the playable vertical slice is done; the
broad port's main flows exist across every domain; full parity is still a
simulation-rule effort, not a rendering/format effort.**

## Refactor evidence (MAINT-1..6)

- `src/app/main.cpp` 15,266 -> 10 lines; `Game::run()` 35; 21 boot / 14 frame
  phases; 172 `Game` methods; 32.7k lines in the first-party app layers.
- Four long bodies + four lifecycle bodies split (MAINT-2/MAINT-5); policy and
  static guards (MAINT-6).
- 518 tests, 403 fast; 60,000-frame soak exit 0; full suite 4:06.

## Code-quality distance

Three axes, because one percentage hides the difference:

1. **File/function readability — essentially done.** `main.cpp` is 10 lines,
   `Game::run()` 35, the 12 identified long bodies are split, and the largest
   remaining method is 263 lines (8 methods over 200). The policy page and
   static guards exist.
2. **Object/architecture — not done.** `Game` still owns 172 methods across
   ~10.5k lines of `game_*.cpp`. There is no `ParkSession`, `BuildController`,
   `Scheduler` or `Animator` type; the retail module map
   (`docs/decomp/scheduler.md`) is represented by phase order and comments,
   not by objects. This is the remaining half of the original review's Phase 2.
3. **Correctness debt — two small P1s.** Simulation `frame_dt` is unclamped
   (`game.cpp:223`) although retail caps the first delta at 0.2 s
   (`scheduler.md:45`); save writers are not atomic (`port_save.cpp:25`,
   `campaign.cpp:280/422`). Both are independent of the refactor and predate
   it.
