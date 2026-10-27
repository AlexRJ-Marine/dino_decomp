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

- P3-CACHE: writers ported. GetCourage/ObstAttackSize1 completion sets
  `DinoNeeds::threat`/`threat_radius` from the exact
  `combined_attack`/`obstacle_radius`; DINOPSAV persists both caches and
  sources. The chooser's per-action target provider (policy vtable `+0x14`)
  remains.
- P3-PERCEPTION: visitor/obstacle reaction (recon lane; `0x51fa10`/
  `0x51f8d0`/`0x521d10`/`0x521d80` decoded, target policy `+0x14` open)
- P3 rest: element bone/event overlay source and the visitor/obstacle
  perception reaction; the cache writers are done.
- UI-2 Phase C rest: list snapshots, slider/checkbox selection, minimap
  water/partition polish.
- WORLD-3/2 polish: skybox bottom/side seam, frustum culling/LOD2.
- TOOL-4: background menu interaction is solved (app-loop gate `0x4024db`
  -> `EB 0A`; cursor ring + frame offset). Level start resets to exclusive
  fullscreen; forcing `Windowed=1` removes the `Present` hang. Remaining: the
  loading loop still never opens the world files (`FUN_00402460` never
  returns from the park-start state) and retail still takes the foreground.

## Next actions

1. P3 chooser target term: resolve the policy vtable `+0x14` target
   provider (GetTarget x/y/radius) and feed `threat_score`; this also
   unblocks the visitor/obstacle reaction.
2. TOOL-4: the background menu path works end to end and level-start
   fullscreen resets are forced windowed; decode the park-start state that
   never returns from the loading screen loop (`app_data+0x20`, `0x29a` =
   movie state). Until then, a `-PhysicalInput` approval is still the way to
   run the P3 provider trace.
3. WORLD-3: fix the skybox bottom/side seam and capture the final side-by-side.

## Recent

- 2026-10-27 - TOOL-4: reset probe shows park start asks for exclusive fullscreen; forced Windowed=1 removes the Present hang, loading loop still stalls before the world files
- 2026-10-27 - TOOL-4: app-loop scheduler gate 0x4024db patched (menu clicks work in the background), virtual cursor ring + frame offset fixed; park load now blocks in D3D8 Present
- 2026-10-27 - SIM-4: budget panel imports the save's yesterday block
  (`SavEconomy`, `notes/budget-sav-rows.png`)
- 2026-10-27 - WORLD-3: water layers animate from FUN_00602fd0 (opposite 0.03/0.01 UV/s)
- 2026-10-27 - WORLD-3: second Modulate2x water stage (water2 + coordinate set 1)
- 2026-10-27 - TOOL-4: proxy-driven FUN_00524610 keeps background DirectInput polling and delivers every click; retail UI gate remains
- 2026-10-27 - VIDEO-2: windowed MPEG playback + CPGEND ending-video handoff (tennis.mpg)
- 2026-10-27 - Store tail: MESSAGE_PANEL toast keeps the panel open; mission/free-game store catalogs

## Blockers

- None.

## Pointers

- `docs/formats/` — proven format specs.
- `docs/decomp/` — function status, scheduler, economy, show score, hybrid.
- `notes/history/` — dated completion log (`scripts/state.ps1 -Done`).
- `notes/evidence-index.md` — claim → evidence → test index.
