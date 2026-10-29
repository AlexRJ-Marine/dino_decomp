# NOW — Dino Island

Updated: 2026-10-29

This is the injected dashboard. Keep it under ~80 lines. Full history and
evidence live in `notes/STATE.md` (read on demand); open tasks are in
`notes/OPEN-TASKS.md` (regenerate with `scripts/tasks.ps1 -Update`).

## Rules (short)

- Never write `original/` or `game/`; never copy retail assets into `src/`.
- Every format/gameplay claim needs evidence: hex dump, decompiled function,
  trace, or a parser/test against a real sample. Note first, doc second.
- Small verified steps; commit each completed task; hand non-trivial changes to
  `reviewer`.
- Claim a lane and a build dir before editing (see `notes/lanes.md`).

## Environment

- Python `.venv/`; Ghidra 12.2 + MCP `http://127.0.0.1:8000/mcp`; w64devkit;
  SDL3 3.4.18.
- Build: `scripts/build.ps1 -BuildDir build/<lane>` (default `build/mingw`).
- Tests: `scripts/test.ps1 -Exe build/<lane>/dino_viewer.exe -Fast` or
  `scripts/run-tests.ps1` for the full suite.
- Retail play: `scripts/play-windowed-game.ps1` (`run-window/`, bordered
  800x600, starts behind your work; click inside to capture the mouse, click
  outside to release).
  x32dbg: `scripts/x32dbg/play-freegame.ps1` / `start-campaign.ps1`
  (`run-test/`, virtual input; screenshots background-safe, but retail stops
  polling DirectInput when inactive, so clicks need the game active).
  Harness screenshots go to `notes/harness/`; tracked `notes/*.png` are
  evidence, do not overwrite.
- UI click map: `docs/platform/ui-coordinates.md` + `.json`; query with
  `.venv\Scripts\python.exe tools\formats\ui_map.py find <NAME>`.
- Verify everything: `scripts/verify.ps1` (`-Fast` for the quick loop).

## Lanes and owners

See `notes/lanes.md`. Default split:

- `src/hook`, `docs/decomp`, `notes/hybrid-*` — hybrid/hooks.
- `src/viewer`, `src/world`, `src/ui`, `src/formats`, `tests/test_viewer_cpp.py`
  — port/viewer.
- `tools/formats`, `docs/formats`, `tests/test_*.py` — formats.
- `notes/*`, `docs/decomp` — recon.
- `scripts/`, `run-test/`, `run-window/` — harness.

## Focus

- MAINT-1: **readable app shell — done, current focus.** `src/app/main.cpp` is
  10 lines and `Game::run()` 35 (was 15,266 / 11,145); the app is `dino_port`
  with one `src/app/game_*.cpp` per domain, 21 boot phases and 14 frame
  phases. Naming/layering rules: `docs/platform/naming.md`; file map and
  phase tables: `docs/platform/app-shell.md`. Next: independent review, then
  MAINT-2 (split `draw_park_hud` 758, `handle_input` 638,
  `update_placement` 638, `draw_menu` 428) and MAINT-3 (scene-pixel and
  SDL-free guard tests). **All other port work is paused** until the code
  base is readable; `--selftest` 194 PASS, fast suite 350 passed.
- PLACE-1: **exact object placement (paused behind MAINT-2/3).** Road cursor now
  snaps to the nearest 4-unit anchor like retail (`0x5faaa0`, half-down), so
  the horizontal/vertical flip at 45 degrees matches; the signed-length cost
  quirk is disproven (retail +x/-x 100, +y 100, -y 190, all commit). Marcus's
  `portable-setup` road network (segments, merge `0x58d890`, T-snap
  `0x58d9d0`, per-segment paint `0x602380`), Entrance start road, detail map
  and vegetation LOD are merged into `main`. Occupancy grid foundation added
  with the retail classes (walls 7, buildings 1, roads `DAT_00675a60`); wall
  raster is a 1-unit line, validators use the high nibble plus a 0.01
  height-slope test (`notes/wall-raster-recon.md`). Next: wire the occupancy
  grid into roads/buildings/fences, then the exact fence flow (margin scan,
  `4*margin-1`, cost, wall cells, sell gate/refund), then building/food/show
  placement — no placeholders.
- MENU-1: **main-menu playtest.** Load Game is fixed (`9f58349`): page 4
  lists the port `.psav` slots and loads the selected one. Retail `.sav`
  slots are now unblocked: the header stores mode + island index
  (`p=0x0c+name_len`, mode @p+0, island @p+8; mode 1 = campaign id;
  `notes/sav-menu-load-recon.md`), so the next slice is `SavHeader` fields
  and listing `.sav` (R-16b). Cinematics is fixed: page 6 lists the
  campaigns' ending videos and START plays them (R-17,
  `notes/cinematics-recon.md`). List layout/scrollbars are fixed: rows use
  `LB_LINE_HEIGHT`, the viewport is 8 islands, and the exact retail VSCROLL
  (arrows ±1, track ±3, absolute drag, wheel) reaches islands 9-10
  (`notes/ui-scrollbar-recon.md`). Options is fixed: sliders apply live
  (music/SFX/game speed), indications cycle quality/depth/screen, and
  cinematics skip on any mouse click (`notes/menu-options-recon.md`,
  `notes/video-exit-recon.md`). Next: R-16b and the Campaign Info mission
  list. Campaign Info has no retail mission list; use
  `campaign-progress.txt` (or a port-only override) to pick a mission.
- R-1 (marcusrj): skybox seam fixed — retail `0x00600df0` centres a ±100 cube on the camera; `notes/skybox-seam-recon.md`, `test_cpp_skybox_follows_camera`.
- R-20 (marcusrj): roads are retail `spRoadNetwork` segments (merge `0x58d890`, T-snap `0x58d9d0`, per-segment paint `0x602380`); terrain uses `detailmap.ini` (AddSigned detail, world/16, unlit); `notes/road-network-recon.md`.
- R-19 (marcusrj): Entrance dock + start road — `.mesh` chunk_count is real (dock = chunks 3/4 of entrance_lod1), free game lays the `FUN_0051dad0` road (524,392)-(524,452); `notes/entrance-dock-road-recon.md`.
- R-2 (marcusrj): vegetation LOD from retail gpkLandscape — 15x15 sectors, LOD1 <= 200 / LOD2 <= 400 x LODQuality, else hidden; port frustum cull on top; `notes/landscape-lod-recon.md`.
- CAM-1: **retail park camera solved and now the interactive default.**
  Verified from `0x5e5ff0` plus a live object probe (`DINO_CAMERA_PROBE`):
  start p=0.1/yaw 0, pan `lerp(1,20,p)` **per update** on a fixed 60 Hz
  tick (`--camera-hz`), yaw 90 deg/s
  with a **5/s** low-pass (16.8 deg lag, not the old 10/s), zoom
  `lerp(0.05,0.2,p)`/s, target/eye XY `lerp(6,2,p)` with instant Z, no roll,
  mouse edge scroll (2 px; Shift disables; `--edge-scroll`). The pan moves
  the eye and target goals by the same delta (rigid; no yaw lean, confirmed
  by the live probe).
  `--camera-orbit` keeps the legacy view; `--camera-selftest`
  asserts the measured equations. `notes/camera-mouse-tilt-recon.md`,
  `notes/camera-controls-binary-recon.md` §12. Re-test the in-hand feel.
- SAVE-2: **complete and accepted by retail.** The C++ writer re-serializes
  the whole `.sav` byte-identically; a `--sav-write` output (`cppwrite`) is
  listed by the retail LOAD GAME panel, selects and loads a park
  (`notes/sav-retail-accept.png`). `--sav-name/--sav-slot/--sav-budget`
  mutate header/park fields for testing. The saved camera tail is applied by
  `--sav-park` as a port convenience; retail **manual LOAD** ignores it
  (`notes/sav-camera-load-ignored.png`). Autosave flag confirmed: patching
  park `header_a` 3→0 and `header_c` 0→10 makes an autosave manually
  loadable (`notes/sav-autosave-manualized.png`); `tools/formats/sav_manual.py`
  does the patch, and `--sav-manual` is the C++ follow-up once `main.cpp`
  is free.
- SAVE-2 campaign class: `list0` is `saDinosaur` (`0x570c90`/`0x5706c0`);
  tail `0x2D1 + threshold + tex_bytes + 0x16*N` (N at `path+0x1c6`), no
  length prefix. `tools/formats/sav_campaign.py` models every field and
  re-encodes all 20 records byte-identically; the C++ writer validates each
  record length and keeps list0 + list1/tail raw, so `--sav-roundtrip`/
  `--sav-write` are byte-identical on the 53 KB sample (no test skip).
  list1/list2 decoded (2 food sources, 6 employees). Campaign saves have
  **no park block** (`FUN_0058b200` gates it out); remaining: 57-record
  attraction list, store/lab tail, typed C++ list0.
- SHOW-4: mission `[SHOWS]` catalog, `--place-show`, the 10-slot hourly
  `ShowRuntime`, the interactive construction panel (`PREDEFINED_SHOW_LIST`
  selection/scroll + VALIDATE placement) and the ten attraction rows
  (lock/iteration/permutation/selection) are ported; remaining: live `.bfi`
  arena/judge actors and per-show weight authoring.
- P3-CACHE: writers ported. GetCourage/ObstAttackSize1 completion sets
  `DinoNeeds::threat`/`threat_radius` from the exact
  `combined_attack`/`obstacle_radius`; DINOPSAV persists both caches and
  sources. The chooser's per-action target provider (`+0x14`) is ported
  (`ActionTarget` + `ActionThreatProvider`); `FUN_00569640`/`FUN_0056a9a0`
  attach one target per action list (owner skip at `+0x0c`; one-fill lag).
- P3-PERCEPTION: the visitor/obstacle reaction lists (`beh+0xf4`/`+0xb8`/
  `+0xc8`) have **no writer in this build** (whole-image store census), so the
  list-driven starters are inert. Target policy `+0x14` is ported; the live
  entry points are the direct `start_visitor`/`start_dino(slot, block,
  instance)` calls used by the show/attraction controller. A Cpg04 trace
  (`notes/target-vtable-runtime.md`) saw `0x521d80` fire 20 times with actor
  targets, `0x521d10` 0, and box only gated for `Eat`; Free Game has 0 dinos.
- P3 rest: element bone/event overlay source and the visitor/obstacle
  perception reaction; the cache writers are done.
- UI-2 Phase C rest: list snapshots, slider/checkbox selection, minimap
  water/partition polish.
- WORLD-3/2 polish: vegetation sector LOD + cull done (R-2); terrain
  patch LOD and vegetation slope tilt remain.
- TOOL-4: **park load solved.** The background stall was a hidden mcLib
  "Unable to restore previous video mode" dialog: dgVoodoo returned
  `D3DERR_DEVICELOST` for the park's 800x600 R5G6B5 Reset. The proxy now
  forces 16-bit park Resets to X8R8G8B8 and activates the game window on the
  Reset thread; the free-game park loads and renders
  (`notes/park-retail-windowed.png`). `notes/park-load-static.md` has the
  call map; `notes/background-input-recon.md` §4b the resolution.

## Next actions

1. MAINT-2: split the four remaining long bodies (`draw_park_hud` 758,
   `handle_input` 638, `update_placement` 638, `draw_menu` 428) into named
   handlers, one commit each, verbatim moves; then MAINT-3 (scene-pixel guard
   in `tests/test_app_cpp.py`, SDL-free layer guard). Keep `--selftest` (194)
   and the fast suite green after every slice.
2. MENU-1: Options (R-18) is done — sliders apply live and indication
   buttons cycle quality/depth/screen; cinematics skip on any mouse click.
   Next: retail `.sav` slots (R-16b, island in the header) and the Campaign
   Info mission list. Campaign Info has no retail mission list; use
   `campaign-progress.txt` or a port-only override.
3. CAM-1 feel retest: arrows pan, Shift+arrows rotate (Del/PgDn/Home/End are
   the retail alternates), Shift+Up/Down zoom. Pan is frame-rate dependent in
   retail; the port applies one step per rendered frame (vsync 60). If the
   user reports a speed mismatch, measure their display rate before changing
   the step, not the formula.
4. P3 perception: wire the direct `start_visitor`/`start_dino` entry points
   in the show/attraction controller (Cpg04 is the test park; Free Game has 0
   dinos). Do not invent a list producer; the lists stay empty.
5. CAMPAIGN population: `WldWorld.records` now retains campaign shows/
   animals/employees; wire them into `load_campaign_park` and assert
   shows=1/animals=2/employees=6 on Cpg04 (`notes/playtest-2026-10-27.md`).
6. SHOW-4 rest: live `.bfi` arena/judge actors and per-show weight
   authoring; the construction panel and attraction-list actions are in.
7. SAVE-2 HUD: cash is `economy+0x08` = park `+0x30`. Manual/autosave `.sav`
   LOAD has `BL=0` and restores it; the `0x58b1b0` skip is only the campaign
   `.wld` path (`BL=1`, no park block). The port matches; the 26000 mystery
   needs a runtime watch (`notes/hud-budget-restore-flag-recon.md` §8).
8. WORLD-3: skybox seam fixed (R-1); capture a final retail side-by-side.

## Recent

- 2026-10-29 - MAINT-1: readable app shell + naming complete - main.cpp 10 lines, Game::run() 35, per-domain game_*.cpp, 21 boot / 14 frame phases; selftest 194 PASS, fast suite 350 passed
- 2026-10-28 - UI: paddock info panel on enclosure selection - EnclosureN name, dino count, occupation %, per-diet food sufficiency snapshot (0x5269e0/0x57e720); sell-mode cursor observation and DINO-STATS-1/P3-MOTION-1 audits recorded; full suite 500
- 2026-10-28 - ECONOMY: fence sell flow - HUD Sell state, REQUEST dialog, inset-perimeter GetSellValue x SellBackFactor (live 870->630, port 2130->1890), non-empty gate, fence/ground/wall removal; removed the disproven 16-paddock gate; full suite 497
- 2026-10-28 - VIDEO: cinematic MP2 audio wired - PL_MPEG audio decode, SDL float stereo stream, frame-PTS pacing, menu/island music paused; all retail movies probe 44.1 kHz/1 stream; full suite 497
- 2026-10-28 - PLACE-1: Esc now clears the pending mode-2 enclosure grid (was leaving the blue dots); loaded/campaign paddocks build their gatAttraction sandy floors at load (rebuild after the paddock list); full suite 496
- 2026-10-28 - INPUT: retail Escape behavior - default park no-op, construction strip stays, Esc closes OPTIONS_PANEL, placement/selection states leave; park Esc never quits; full suite 494
- 2026-10-28 - PLACE-1: Esc leaves the active placement state (retail fence key handler 0x52be60 -> leave 0x5269c0, no close button); fence/road/building/show/store tools release and a selected paddock deselects, so placed enclosures can be selected; --inject-key test hook; full suite 493
- 2026-10-28 - PLACE-1: fence palette pulse ported from the binary - bank 4 additive laser sin4 (1.571 s), bank 1 Dino_SelectedWall sin10 (0.628 s, 1.0-2.0x); click-select enclosure with right-click deselect; HUD/minimap tint reset; full suite 493

## Blockers

- None.

## Pointers

- `docs/formats/` — proven format specs.
- `docs/decomp/` — function status, scheduler, economy, show score, hybrid.
- `notes/history/` — dated completion log (`scripts/state.ps1 -Done`).
- `notes/evidence-index.md` — claim → evidence → test index.

