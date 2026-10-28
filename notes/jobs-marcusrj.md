# Contributor work queue — marcusrj

Owner: marcusrj (`portable-setup`). Claim a row in `notes/lanes.md` before
starting; build in `build/marcus`; run `scripts/verify.ps1 -Fast` before every
commit; branch/merge flow is in `notes/lanes.md`.

**Off-limits (primary lane):** `src/hook/**`, `notes/background-input-recon.md`,
TOOL-4, the P3 chooser/provider work, `notes/harness/*` probes. Never edit
`run/`, `run-test/`, `run-window/`.

## Already done by marcusrj (do not re-queue)

- Portable toolchain (`scripts/tools.ps1`, `setup-tools.ps1`, `DINO_TOOLS`,
  Ghidra `-TakeOwnership`, build-cache reconfiguration) — commit `1c6a03b`.
- x32dbg 2026.05.27 harness support — `66cae9a`.
- ROAD-1 retail road tool: `OPEN_TRANSPORT -> Road`, 2-click runs, 10/cell from
  `GeneralParams` to CONSTRUCTION, BUSY/INSIDE_PERIMETER/TOO_SHORT warnings,
  retail preview tint/mesh, 4 tests — `a92bc41..bb241e8`.
- `GeneralParams.bgd` structure extraction (`src/formats/general_params.*`).

His work aligns with the project priorities and does not overlap the hook/AI
lane. None of it touched `notes/lanes.md` or added a contributor queue, so this
file is the single place to claim work.

## Queue (pick top-down)

| # | Job | Scope / files | Done when |
| --- | --- | --- | --- |
| R-1 | Skybox seam | `src/viewer/main.cpp` sky faces, `docs/platform/environment.md` | Bottom/side seam gone at pitch ±60; side-by-side shot in `notes/`; A/B test |
| R-2 | Frustum culling + LOD2 | `src/viewer/main.cpp` terrain draw | Off-screen chunks skipped, LOD2 at distance, `test_cpp_*` for both |
| R-3 | UI-2 Phase C rest | `src/ui/*`, `src/viewer/main.cpp` list widgets | Slider + checkbox selection on store/lab lists; minimap water colour + partition lines; snapshot test |
| R-4 | SIM-4 wages | `src/world/economy.*`, `docs/decomp/economy.md` | `0x585d80` wages + `0x585d50` EFFECTIFS_SECU wired once staff actors exist; ledger test |
| R-5 | Skinned mesh export | `tools/formats/`, `docs/formats/` | One retail skinned mesh round-trips to glTF/OBJ, renders in Blender, pytest |
| R-6 | Campaign leftovers | `src/world/campaign_*` | Scarcity `+0x1e8` filled; show verbs evaluated when judging sources exist |
| R-7 | Viewer audio | `src/viewer/main.cpp` (SDL3 audio) | Music streams/loops and SFX play; `--no-audio` A/B test |
| R-8 | ROAD-2 | `src/world/build.*`, `src/viewer/main.cpp` | Road selling (`REQUEST_SELL_ROAD`), Farm/Metro buttons, road slope/pen validation; tests kept green |
| R-9 | FMT-26 numerics | `src/formats/general_params.*`, `docs/formats/general-params.md` | Time/rate/distance/ramp meanings named (started in `a92bc41`) |
| R-10 | Laboratory gameplay (stretch) | `src/world/`, `src/ui/park.cpp` | Breeding/cloning/fridge actions run on real `.laboratory`/`simu_trans` data with tests |
| R-11 | Path preview + rules | `src/viewer/main.cpp` path tool, `src/world/build.*` | Preview is now one stretched box with depth; remaining: match the retail dark-blue/red shade and alpha from the harness capture |
| R-12 | Camera parity | `src/viewer/main.cpp` camera | Mouse orbit/pivot/zoom/limits match a retail side-by-side trace |
| R-13 | Building preview texture | `src/viewer/main.cpp` placement render | Placed/preview building uses its real texture, not a white model |
| R-14 | Enclosure placement | `src/ui/park.cpp`, `src/viewer/main.cpp` | Done: two-click rectangle + capacity paddock; red/blue disc; size/inside/busy/16 checks. Remaining: perimeter price, warning text, LunchBox/observation feature mode |
| R-15 | Campaign world population | `src/viewer/main.cpp` campaign start | Done: `.wld` simulation lists parsed; 6 employees + 2 food sources spawn/render; 51/71 show waypoint features render; Cpg04 tests assert them |
| R-16 | Menu: Load Game page | `src/ui/menu.*`, `src/viewer/main.cpp` menu list/loading | Done (`9f58349`): page 4 lists the `run/save` port `.psav` slots and `LOAD_LOAD_BUTTON` loads the selected one; test `test_cpp_retail_menu_load_game_page`. |
| R-16b | Menu: retail `.sav` slots | `src/formats/sav.*`, `src/ui/menu.*`, `src/viewer/main.cpp` | List `.sav` beside `.psav`. The island is in the header (`p=0x0c+name_len`: mode @p+0, island index @p+8; mode 1 = campaign id), so load `IslandList.ini[island]` then the existing `.sav` import; no island picker. Evidence `notes/sav-menu-load-recon.md`. |
| R-17 | Menu: Cinematics list | `src/ui/menu.*`, `src/viewer/main.cpp` menu list/loading | Done: rows are the campaigns' `EndingVideoName` (basket/tennis/trampoline/Trappe/mixe; Cpg06/Cpg08 empty), START plays `Data/Video/<EndingVideo>`; the "seven clips" assumption was wrong (the list is campaign-driven, not the folder). Evidence `notes/cinematics-recon.md`; test `test_cpp_retail_menu_cinematics_page`. |
| R-18 | Menu: Options page | `src/ui/menu.*`, `src/viewer/main.cpp` | The page is static (icons + empty sliders/OK). Decide which retail settings are playtest-relevant (music/SFX/resolution) and wire them to the existing config/audio; otherwise document it as deferred. |

## Coordination

- Rebase `portable-setup` on `main` before hand-off; never force-push `main`.
- Announce completions with `scripts/state.ps1 -Done "..."`.
- Keep `docs/platform/feature-status.md` rows current with each shipped feature.
