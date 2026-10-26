# Evidence index

One line per load-bearing claim: where the proof lives. `docs/` and `NOW.md`
link here instead of restating results. This is a seed list, not exhaustive;
add a row when a claim becomes something later work depends on.

| Claim | Evidence | Test / artifact |
| --- | --- | --- |
| `.mesh` header and geometry | `docs/formats/mesh.md` | `tools/formats/mesh.py`, `tests/test_mesh.py` |
| `.skin` rigid bind + inverse-bind matrices | `docs/formats/skin.md` | `tests/test_skin.py`, `notes/allo-skin-bind.png` |
| `.skel` 4x4 row-major bone matrices | `docs/formats/skel.md` | `tests/test_skel.py` |
| `.anim` tracks/keys | `docs/formats/anim.md` | `tests/test_anim.py`, `notes/allo-anim.gif` |
| `.fnt` hash-shifted code table (`codes[i] -> metrics[i+2]`) | `docs/formats/fnt.md`, `notes/hybrid-fnt-recon.md` | `tests/test_fnt.py`, `tests/test_viewer_cpp.py::test_cpp_hud` |
| `.lnd` carries water Z + Entrance cell/orientation | `docs/formats/lnd.md`, `notes/lnd-carry-recon.md` | `tests/test_lnd.py` |
| `.bbi` construction cost is file offset 21 | `docs/formats/bbi.md`, `notes/building-def-record-recon.md` | `tests/test_bbi.py::test_construction_cost_is_file_offset_21` |
| `.bfi` weight lists / score steps are dead editor data | `docs/decomp/show-score.md` | `notes/show-score-recon.md` |
| Show judge score = 5-float weighted sum | `docs/decomp/show-score.md` | `tests/test_viewer_cpp.py` (show tests) |
| Visitor attraction score + needs decay | `docs/decomp/visitor-ai.md` | `tests/test_viewer_cpp.py::test_cpp_sim2b_visitor_state_machine` |
| Employee tasks are the hourly feeding queue | `docs/decomp/employee-ai.md` | `notes/employee-ai-recon.md` |
| Dinosaur chooser / `.bai` scoring | `docs/decomp/dinosaur-actions.md`, `notes/dino-action-chooser-recon.md` | `tests/test_viewer_cpp.py::test_cpp_dino_action_loop` |
| SECU danger grid (8-unit cells, once per second) | `docs/platform/danger-grid.md` | `tests/test_viewer_cpp.py::test_cpp_danger_grid_secu` |
| GetCourage/ObstAttackSize1 cache writers + DINOPSAV tail | `docs/decomp/dinosaur-actions.md`, `docs/platform/danger-grid.md`, `notes/dino-cache-writers-recon.md` | `test_cpp_dino_action_chooser`, `test_cpp_port_save_danger_cache` |
| Store attribute gauges (11 PROGRESS/INDICS) | `notes/store-panel-recon.md`, `notes/phenotype-attributes-runtime.md`, `notes/store-gauges.png` | `test_cpp_store_attribute_gauges` |
| Daily economy settlement + per-visitor ticket | `docs/decomp/economy.md` | `tools/formats/economy.py`, `tests/test_economy.py` |
| Save world graph + park stream | `docs/formats/sav.md` | `tools/formats/sav_graph.py`, `tests/test_sav_graph.py` |
| `.wld` park import (roads/fences/buildings/dinos) | `docs/platform/world-import.md` | `tests/test_viewer_cpp.py::test_cpp_wld_park_import*` |
| Terrain raw files are bottom-up | `docs/platform/terrain.md`, `notes/terrain-orientation-recon.md` | `tests/test_raw.py`, viewer entrance screenshot |
| Phenotype dimensions/attributes/quotient (20/20 Cpg04 rows) | `docs/formats/translator-constants.md`, `notes/phenotype-attributes-runtime.md` | `tests/test_viewer_cpp.py::test_cpp_cpg04_phenotype_attributes` |
| Part-record accumulator (23 slots + skin/carapace, `+0x78/+0x7c/+0x80`) | `docs/formats/translator-constants.md`, `notes/part-record-accumulator-recon.md`, `notes/phenotype-attributes-runtime.md` | `tests/test_viewer_cpp.py::test_cpp_cpg04_phenotype_attributes` |
| `.fnt` loader hook `0x430fb0` | `notes/hybrid-fnt-recon.md`, `notes/hyb7-counter.txt` | `notes/park-hook-fnt.png` |
| `.mesh`/`.skin`/`.anim`/`.skel` hooks | `docs/decomp/hybrid.md` | `notes/hyb6-counter.txt`, `notes/park-hook-mesh-default.png` |
| Windowed/background test runtime | `docs/platform/windowed-testing.md` | `notes/windowed-park.png`, `scripts/setup-test-runtime.ps1` |
| Employee profiles + hire/wage/feeding | `docs/platform/employees.md`, `docs/decomp/employee-ai.md` | `tests/test_viewer_cpp.py::test_cpp_employee_info`, `test_cpp_employee_feeding_and_wages`, `notes/employees.png` |
| `.shw` + `.bfi` show grid (C++ tiling) | `docs/formats/shw.md`, `docs/formats/bfi.md` | `tests/test_viewer_cpp.py::test_cpp_show_info` |
| Retail UI click map (800x600, all screens) | `docs/platform/ui-coordinates.md`, `tools/formats/ui_map.py` | `tests/test_ui_map.py`, `docs/platform/ui-coordinates.json` |
| Feature status matrix (retail feature/screen -> status/evidence/tests) | `docs/platform/feature-status.md` | `tests/test_feature_matrix.py` |
| Park panel contract + verification recipes | `docs/platform/park-panels.md` | `tests/test_feature_matrix.py`, `tests/test_viewer_cpp.py::test_cpp_park_panels` |
| Campaign flow: menu -> briefing -> park, unlock, difficulty | `docs/platform/campaign-flow.md` | `tests/test_feature_matrix.py`, `tests/test_viewer_cpp.py::test_cpp_retail_menu_pages` |
| C++ campaign reader (7 campaigns / 39 missions / 89 objectives / 33 verbs) | `src/formats/campaign.h`, `src/formats/campaign.cpp`, `docs/platform/campaign-flow.md` | `tests/test_campaign_cpp.py` |
| Campaign objective evaluator core (simple + trait verbs, tri-state) | `src/world/campaign_objectives.h`, `src/world/campaign_objectives.cpp`, `docs/platform/decoded-features.md` | `tests/test_campaign_cpp.py::test_cpp_campaign_objective_selftest` |
| Campaign mission runtime + MISSION_PANEL tri-state | `src/world/campaign_runtime.h`, `src/world/campaign_runtime.cpp`, `docs/platform/campaign-flow.md` | `tests/test_campaign_cpp.py::test_cpp_campaign_mission_panel`, `test_cpp_campaign_mission_success` |
| Campaign live dinosaur traits (phenotype snapshots) | `src/viewer/main.cpp` (`ImportedDino::attributes`), `docs/platform/campaign-flow.md` | `tests/test_campaign_cpp.py::test_cpp_campaign_live_traits` |
| Campaign menu flow (select -> briefing -> park) | `src/ui/menu.{h,cpp}`, `src/viewer/main.cpp` (`load_campaign_park`), `notes/campaign-menu-select.png`, `notes/campaign-menu-briefing.png`, `notes/campaign-menu-start.png` | `tests/test_campaign_cpp.py::test_cpp_menu_campaign_select`, `test_cpp_menu_campaign_mission_start` |
| Campaign live employee roles (`.bei` names) | `src/viewer/main.cpp` (`CampaignState::employee_roles` fill), `docs/platform/campaign-flow.md` | `tests/test_campaign_cpp.py::test_cpp_campaign_live_employees` |
| Campaign unlock mask (`run/save/dino.dat`) | `src/formats/campaign.{h,cpp}` (`read_campaign_unlock`), `src/ui/menu.{h,cpp}`, `notes/campaign-menu-unlock.png`, `docs/platform/campaign-flow.md` | `tests/test_campaign_cpp.py::test_cpp_campaign_unlock_mask`, `test_cpp_menu_campaign_locked` |
| Campaign mission id in `DINOPSAV` | `src/world/port_save.{h,cpp}` (`mission` record), `src/viewer/main.cpp` (`make_port_state`/`apply_port_state`), `docs/platform/campaign-flow.md` | `tests/test_campaign_cpp.py::test_cpp_campaign_mission_port_save` |
| Decoded feature port contracts (shows/employees/animals/lab/objectives) | `docs/platform/decoded-features.md` | `tests/test_feature_matrix.py` |
