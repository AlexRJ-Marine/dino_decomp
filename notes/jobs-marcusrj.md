# Contributor work queue — marcusrj

Owner: marcusrj (`portable-setup` branch). Do not start a job here without
claiming it in `notes/lanes.md`. Build in `build/marcus`; run
`scripts/verify.ps1 -Fast` before every commit.

**Off-limits (owned by the primary lane):** `src/hook/**`,
`notes/background-input-recon.md`, TOOL-4 items, the P3 chooser/provider work,
and `notes/harness/*` probes. Do not edit `run/`, `run-test/`, `run-window/`.

## Queue (pick top-down)

| # | Job | Scope / files | Done when |
| --- | --- | --- | --- |
| R-1 | Skybox seam | `src/viewer/main.cpp` sky faces, `docs/platform/environment.md` | Bottom/side seam gone at pitch ±60; side-by-side screenshot in `notes/`; A/B test |
| R-2 | Frustum culling + LOD2 | `src/viewer/main.cpp` terrain draw | Off-screen chunks skipped; LOD2 visible at distance; `test_cpp_*` for both |
| R-3 | UI-2 Phase C rest | `src/ui/*`, `src/viewer/main.cpp` list widgets | Slider + checkbox selection works on store/lab lists; minimap water colour + partition lines; snapshot test |
| R-4 | SIM-4 wages | `src/world/economy.*`, `docs/decomp/economy.md` | `0x585d80` wages + `0x585d50` EFFECTIFS_SECU wired once staff actors exist; ledger test |
| R-5 | Skinned mesh export | `tools/formats/`, `docs/formats/` | One retail skinned mesh round-trips to glTF/OBJ; renders in Blender; pytest |
| R-6 | Campaign leftovers | `src/world/campaign_*` | Scarcity `+0x1e8` filled; show verbs evaluated when judging sources exist |
| R-7 | Viewer audio | `src/viewer/main.cpp` (SDL3 audio) | Music streams/loops and SFX play; `--no-audio` A/B test |
| R-8 | ROAD-2 | `src/world/build.*`, `src/viewer/main.cpp` | Road tool follow-ups you already own (curves/refund/delete) with tests |

## Coordination

- Claim the row in `notes/lanes.md` (add your build dir) before editing.
- Rebase your `portable-setup` branch on `main` before opening/merging; never
  force-push `main`.
- Announce completions with `scripts/state.ps1 -Done "..."` so the dashboard
  shows both lanes.
