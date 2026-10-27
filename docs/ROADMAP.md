# Roadmap

Four phases. The goal is a playable, portable game and a readable hybrid
decompilation that keeps the original runnable while functions are replaced.

Last refreshed: 2026-10-27. The **task** view is `notes/OPEN-TASKS.md`
(generated from `TASKS.md`). The **feature** view is
[`platform/feature-status.md`](platform/feature-status.md) — status, retail
evidence, port entry points, tests and gaps per feature. This page only keeps
the phase-level picture.

## Phase 1 — Formats and assets (largely complete)

Every shipped format in `game/` is mapped to a spec in `docs/formats/` and,
where it is a parser, to `tools/formats/` with a test against retail files.

- Mesh/skin/skeleton/animation, TGA, WAV, FNT, LOC, LND, RAW: done.
- `FMT-8..31`: shows, genotypes, visitor/building/food/animal/employee
  profiles, sequence events, campaign data, translator tables, render states,
  lab/global parameters, market/free-game config: done.
- Remaining work is semantic, not structural: see the open items in
  `notes/OPEN-TASKS.md` and the per-feature gaps in
  `platform/feature-status.md`.
- Community post is drafted (`community/xentax-post.md`); the user publishes
  it and records replies.

Exit criteria: all parsers scan their full retail sets with zero errors (met);
model/texture/sound viewer (met); community post published (pending).

## Phase 2 — Platform layer (working vertical slice)

- SDL3 + OpenGL window, mesh/texture renderer, skinned animation: done
  (`VIEW-1/2/3`).
- Island terrain, vegetation, skybox/water/fog, buildings, paths/fences:
  working (`WORLD-1/1b/1c`, `SLICE-1`).
- Visitor agents, dinosaur action loop, danger grid/SECU, feeding: working
  (`SIM-1`, `SIM-2`, `SIM-3` partial).
- Campaign menu, reader, evaluator, runtime, progression and completion:
  working (`CAMPAIGN-1/2/3`); 81/89 objective lines implemented, the 8 show
  lines and ending videos remain.
- Retail menu, park HUD, panels, store, save/load, dialogs, minimap, clock:
  working (`UI-0/1/2` partial, `SAVE-1`).
- Audio: WAV, menu/park clicks and streamed island music (`AUDIO-1/2/3`).
- Ending-video MPEG-1 decode, frame export and windowed playback (`VIDEO-1`);
  MP2 audio and the intro movie remain.
- Windowed/background test harness and the retail UI click map: done
  (`TOOL-1/2/3`); background input activation shim still open (`TOOL-4`).

Exit criteria: portable window/input/audio/renderer (met); one skinned dinosaur
on one island (met); QoL documented and configurable (mostly met; UI scaling
`QOL-2` remains).

## Phase 3 — Hybrid decompilation (active)

- Hooked the original exe through the `DINPUT8.dll` proxy and replaced the
  matrix, normal, vector, `.skin`, `.anim`, `.skel`, whole-`.mesh` and `.fnt`
  readers (`HYB-1..7`). The game still reaches a playable park.
- Function-level status is tracked in `decomp/function-status.md`; remaining
  small candidates include the `0x4809e0` UV reader.
- Verification targets (world, simulation, presentation) continue from the
  matrix rows marked **partial** or **decoded**.

Exit criteria: every replaced function exercised by the running game; function
list current; original/replacement toggle (partly met).

## Phase 4 — Remove the original exe and expand

Not started. Depends on Phase 3 reaching zero remaining original-code calls.
The reference-comparison and packaging tasks (`PAR-1`, `PAR-2`) are the first
steps.

## Keeping this current

- When a task completes: `scripts/state.ps1 -Done "..."` and
  `scripts/tasks.ps1 -Update`.
- When a feature changes status, evidence or tests: edit the corresponding
  row in [`platform/feature-status.md`](platform/feature-status.md); its test
  (`tests/test_feature_matrix.py`) checks links, screen ids, control names and
  test references.
- Do not duplicate per-feature detail here; link to the matrix and its
  evidence.
