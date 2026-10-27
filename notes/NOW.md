# NOW — Dino Island

Updated: 2026-10-27

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

- SAVE-2: **complete and accepted by retail.** The C++ writer re-serializes
  the whole `.sav` byte-identically; a `--sav-write` output (`cppwrite`) is
  listed by the retail LOAD GAME panel, selects and loads a park
  (`notes/harness/load-cppwrite.png`). `--sav-name/--sav-slot/--sav-budget`
  mutate header/park fields for testing.
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
- WORLD-3/2 polish: skybox bottom/side seam, frustum culling/LOD2.
- TOOL-4: **park load solved.** The background stall was a hidden mcLib
  "Unable to restore previous video mode" dialog: dgVoodoo returned
  `D3DERR_DEVICELOST` for the park's 800x600 R5G6B5 Reset. The proxy now
  forces 16-bit park Resets to X8R8G8B8 and activates the game window on the
  Reset thread; the free-game park loads and renders
  (`notes/park-retail-windowed.png`). `notes/park-load-static.md` has the
  call map; `notes/background-input-recon.md` §4b the resolution.

## Next actions

1. P3 perception: wire the direct `start_visitor`/`start_dino` entry points
   in the show/attraction controller (Cpg04 is the test park; Free Game has 0
   dinos). Do not invent a list producer; the lists stay empty.
2. SHOW-4: show construction UI and live judge/schedule loop (the score
   pipeline and objective verbs are done).
3. SAVE-2 follow-up: the HUD `BUDGET_AMOUNT` cash is not park `+0x30` and is
   absent from the save (scenario/FreeGame config). The port currently
   overrides `park_budget` from the save; verify before changing the HUD or
   `buy_budget` import.
4. WORLD-3: fix the skybox bottom/side seam and capture the final side-by-side.

## Recent

- 2026-10-27 - SAVE-2: --sav-park applies the saved camera tail (eye/target -> orbit camera)
- 2026-10-27 - P3: action target policy (node+0x3c +0x14) ported; chooser threat term + viewer Eat target
- 2026-10-27 - SHOW-3: all four show verbs ported (happyshow/scientificshow/happydinoshow/dinoexclusiveshow) with the FUN_00618020 five-need + .bvi weight pipeline and a --show test hook; coverage 89/89 objectives
- 2026-10-27 - SAVE-2: C++ writer re-serializes the whole .sav byte-identically (world records, catalog, simulation); --sav-write emits it
- 2026-10-27 - ROAD-1: retail road tool (OPEN_TRANSPORT -> Road): straight 2-click runs, 10/cell from GeneralParams to CONSTRUCTION, BUSY/INSIDE_PERIMETER/TOO_SHORT warnings, preview
- 2026-10-27 - TOOL-4: reset probe shows park start asks for exclusive fullscreen; forced Windowed=1 removes the Present hang, loading loop still stalls before the world files
- 2026-10-27 - TOOL-4: app-loop scheduler gate 0x4024db patched (menu clicks work in the background), virtual cursor ring + frame offset fixed; park load now blocks in D3D8 Present
- 2026-10-27 - SIM-4: budget panel imports the save's yesterday block
  (`SavEconomy`, `notes/budget-sav-rows.png`)

## Blockers

- None.

## Pointers

- `docs/formats/` — proven format specs.
- `docs/decomp/` — function status, scheduler, economy, show score, hybrid.
- `notes/history/` — dated completion log (`scripts/state.ps1 -Done`).
- `notes/evidence-index.md` — claim → evidence → test index.
