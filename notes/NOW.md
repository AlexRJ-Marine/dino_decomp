# NOW — Dino Island

Updated: 2026-10-26

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

- P3-CACHE: `GetCourage`/`ObstAttackSize1` writers decoded
  (`0x571190`/`0x5727b0`); part-record fields `+0x44/+0x78/+0x80` and the
  chooser threat port remain
- P3-PERCEPTION: visitor/obstacle reaction (recon lane; `0x51fa10`/
  `0x51f8d0`/`0x521d10`/`0x521d80` decoded, target policy `+0x14` open)
- P3 rest: element bone/event overlay source, GetCourage/ObstAttackSize1 cache
  writers, visitor/obstacle perception reaction.
- UI-2 Phase C rest: list snapshots, slider/checkbox selection, minimap
  water/partition polish.
- WORLD-3/2 polish: second `Modulate2x` water stage, skybox seam, frustum
  culling/LOD2.

## Next actions

- CAMPAIGN-2: objective evaluator (simple verbs + trait pattern + MISSION_PANEL tri-state)
1. UI-2 Phase D tail: store visitor travel state and the dino clip phase.
2. Store/lab attribute gauges: `DinoAttributes` is ported; wire the
   `PROGRESS` widgets (ConstructionInterface) and the 3D preview.
3. WORLD-3: capture the remaining side-by-side and fix the water stage.
4. SIM-4: runtime-verify the budget panel rows against a populated save.
5. STAFF rest: decode job kind `0xb`.

## Recent

- 2026-10-26 - CAMPAIGN-3 live phenotype traits: cleverdinos objective succeeds on Cpg04
- 2026-10-26 - SIM-5 employees: .bei profiles, hire/wages, hourly feeding queue, visible nutritionist
- 2026-10-26 - CAMPAIGN-3 mission runtime: --start-mission, MISSION_PANEL tri-state/lights, CPGEND on result
- 2026-10-26 - CAMPAIGN-2 evaluator core (tri-state; simple + trait verbs; 30 selftest verdicts)
- 2026-10-26 - UI-2 Phase D tail: visitor travel state/route + dino clip phase in DINOPSAV
- 2026-10-26 - CAMPAIGN-1 C++ campaign/mission reader (7/39/89, 33 verb ids; --campaign-info/--campaign-mission)
- 2026-10-26 - DOCS-4 decoded feature port contracts (shows/employees/animals/lab/objectives)
- 2026-10-26 - CAM-1 rest: camera.ini Config1 parser, data-driven speeds/limits, FUN_00529d00 height smoothing
  (`DinoAttributes`, 17/20 Cpg04 rows exact).

## Blockers

- None.

## Pointers

- `docs/formats/` — proven format specs.
- `docs/decomp/` — function status, scheduler, economy, show score, hybrid.
- `notes/history/` — dated completion log (`scripts/state.ps1 -Done`).
- `notes/evidence-index.md` — claim → evidence → test index.
