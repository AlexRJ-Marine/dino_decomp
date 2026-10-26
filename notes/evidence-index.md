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
| Daily economy settlement + per-visitor ticket | `docs/decomp/economy.md` | `tools/formats/economy.py`, `tests/test_economy.py` |
| Save world graph + park stream | `docs/formats/sav.md` | `tools/formats/sav_graph.py`, `tests/test_sav_graph.py` |
| `.wld` park import (roads/fences/buildings/dinos) | `docs/platform/world-import.md` | `tests/test_viewer_cpp.py::test_cpp_wld_park_import*` |
| Terrain raw files are bottom-up | `docs/platform/terrain.md`, `notes/terrain-orientation-recon.md` | `tests/test_raw.py`, viewer entrance screenshot |
| Phenotype dimensions/attributes/quotient | `docs/formats/translator-constants.md`, `notes/phenotype-attributes-runtime.md` | `tests/test_viewer_cpp.py::test_cpp_cpg04_phenotype_attributes` |
| Part-record accumulator (23 slots) | `docs/formats/translator-constants.md`, `notes/part-record-accumulator-recon.md` | (port pending) |
| `.fnt` loader hook `0x430fb0` | `notes/hybrid-fnt-recon.md`, `notes/hyb7-counter.txt` | `notes/park-hook-fnt.png` |
| `.mesh`/`.skin`/`.anim`/`.skel` hooks | `docs/decomp/hybrid.md` | `notes/hyb6-counter.txt`, `notes/park-hook-mesh-default.png` |
| Windowed/background test runtime | `docs/platform/windowed-testing.md` | `notes/windowed-park.png`, `scripts/setup-test-runtime.ps1` |
| Retail UI click map (800x600, all screens) | `docs/platform/ui-coordinates.md`, `tools/formats/ui_map.py` | `tests/test_ui_map.py`, `docs/platform/ui-coordinates.json` |
| Feature status matrix (retail feature/screen -> status/evidence/tests) | `docs/platform/feature-status.md` | `tests/test_feature_matrix.py` |
| Park panel contract + verification recipes | `docs/platform/park-panels.md` | `tests/test_feature_matrix.py`, `tests/test_viewer_cpp.py::test_cpp_park_panels` |
