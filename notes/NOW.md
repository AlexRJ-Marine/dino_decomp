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
- WORLD-3/2 polish: water animation (opposite scroll), skybox seam, frustum
  culling/LOD2.
- TOOL-4: background DirectInput polling/clicks are verified solved via the
  proxy-driven `FUN_00524610`; the deactivated main loop still skips the
  UI/message update, so the menu ignores them.

## Next actions

1. P3 chooser target term: resolve the policy vtable `+0x14` target
   provider (GetTarget x/y/radius) and feed `threat_score`; this also
   unblocks the visitor/obstacle reaction.
2. TOOL-4: input delivery in the background is solved (proxy drives
   `FUN_00524610`; `state_calls` ~64/s, a down/up pair per click). Remaining:
   the deactivated main loop also skips UI/message processing. Patch that
   branch or call the view-manager dispatcher `0x5960c0` on the game thread,
   or get one `-PhysicalInput` approval for the P3 provider trace.
3. WORLD-3: animate the two water layers (retail opposite scroll) and fix the
   skybox seam.
4. SIM-4: runtime-verify the budget panel rows against a populated save.
5. STAFF rest: decode job kind `0xb`.

## Recent

- 2026-10-27 - WORLD-3: second Modulate2x water stage (water2 + coordinate set 1)
- 2026-10-27 - TOOL-4: proxy-driven FUN_00524610 keeps background DirectInput polling and delivers every click; retail UI gate remains
- 2026-10-27 - VIDEO-2: windowed MPEG playback + CPGEND ending-video handoff (tennis.mpg)
- 2026-10-27 - Store tail: MESSAGE_PANEL toast keeps the panel open; mission/free-game store catalogs
- 2026-10-27 - VIDEO-1: PL_MPEG decoder, --video-info/--video-frame, tennis 640x480@24fps
- 2026-10-27 - Campaign phase wrap-up: full suite 426 passed; roadmap/coverage updated
- 2026-10-27 - CAMPAIGN-3 franckstrangedinos: carnivore/biped/size/intelligence predicate; Cpg06_Mission4 completes
- 2026-10-27 - Store 3D preview: selected Market dino rendered in DINO_ACHAT_SAVE_RENDER_VIEW
  (`DinoAttributes`, 17/20 Cpg04 rows exact).

## Blockers

- None.

## Pointers

- `docs/formats/` — proven format specs.
- `docs/decomp/` — function status, scheduler, economy, show score, hybrid.
- `notes/history/` — dated completion log (`scripts/state.ps1 -Done`).
- `notes/evidence-index.md` — claim → evidence → test index.
