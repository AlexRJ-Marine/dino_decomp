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

- P3-CACHE: writers ported. GetCourage/ObstAttackSize1 completion sets
  `DinoNeeds::threat`/`threat_radius` from the exact
  `combined_attack`/`obstacle_radius`; DINOPSAV persists both caches and
  sources. The chooser's per-action target provider (policy vtable `+0x14`)
  remains.
- P3-PERCEPTION: visitor/obstacle reaction (recon lane; `0x51fa10`/
  `0x51f8d0`/`0x521d10`/`0x521d80` decoded, target policy `+0x14` open)
- P3 rest: element bone/event overlay source, GetCourage/ObstAttackSize1 cache
  writers, visitor/obstacle perception reaction.
- UI-2 Phase C rest: list snapshots, slider/checkbox selection, minimap
  water/partition polish.
- WORLD-3/2 polish: second `Modulate2x` water stage, skybox seam, frustum
  culling/LOD2.

## Next actions

1. P3 chooser target term: resolve the policy vtable `+0x14` target
   provider (GetTarget x/y/radius) and feed `threat_score`; this also
   unblocks the visitor/obstacle reaction.
2. Store/lab attribute gauges: all eleven sources are exact (20/20 Cpg04);
   wire the `PROGRESS` widgets (ConstructionInterface) and the 3D preview.
3. WORLD-3: capture the remaining side-by-side and fix the water stage.
4. SIM-4: runtime-verify the budget panel rows against a populated save.
5. STAFF rest: decode job kind `0xb`.

## Recent

- 2026-10-26 - CAMPAIGN-3 dino.dat unlock gating: locked campaign rows disabled, Cpg04 needs mask 0F
- 2026-10-26 - Port the GetCourage/ObstAttackSize1 cache writers into DinoNeeds, import/buy and DINOPSAV
- 2026-10-26 - CAMPAIGN-3 live employees: Cpg01_Mission6 employees objective succeeds with two hires
- 2026-10-26 - CAMPAIGN-3 menu campaign flow: select/briefing/start imports Cpg04 park and runs Mission1
- 2026-10-26 - Port the phenotype dummy/skin/carapace accumulator (Parts 4-9); all 20 Cpg04 attribute rows and the obstacle radii now match
- 2026-10-26 - CAMPAIGN-3 live phenotype traits: cleverdinos objective succeeds on Cpg04
- 2026-10-26 - SIM-5 employees: .bei profiles, hire/wages, hourly feeding queue, visible nutritionist
- 2026-10-26 - CAMPAIGN-3 mission runtime: --start-mission, MISSION_PANEL tri-state/lights, CPGEND on result
  (`DinoAttributes`, 17/20 Cpg04 rows exact).

## Blockers

- None.

## Pointers

- `docs/formats/` — proven format specs.
- `docs/decomp/` — function status, scheduler, economy, show score, hybrid.
- `notes/history/` — dated completion log (`scripts/state.ps1 -Done`).
- `notes/evidence-index.md` — claim → evidence → test index.
