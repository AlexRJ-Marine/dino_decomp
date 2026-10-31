# Park panels — verification dossier

Status: **working reference** for the park HUD and its state panels. This is
the companion to `hud.md` (implementation) and `ui-coordinates.md` (retail
click map). It answers: *which panel opens from where, what the port
populates, and how to reproduce it*.

The port follows the retail model: exactly one state panel at a time, the
always-on HUD stays visible for non-lab panels, and every opener toggles its
panel. Lab centres hide the HUD and show their own 800x600 backdrop.

## How to reproduce

Normal play (non-intrusive, behind your window):

```powershell
.\scripts\play-windowed-game.ps1
```

Scripted viewer on the docked Cpg04 park:

```powershell
$game = "game\%MAINDIR%"
.\build\port\dino_port.exe --frames 2 --no-menu --no-hud `
  --width 800 --height 600 --no-vsync --no-save-config `
  --lnd "$game\Data\Design\landscape\isl04\isl04.lnd" --asset-root $game `
  --no-vegetation --no-water `
  --wld-park "$game\Data\Design\Scenario\Cpg04\Cpg04_Save.wld" `
  --park-click 97,584 --park-click 429,482
```

- `--park-click X,Y` is applied before the first frame and is repeatable; it
  prints `park click X,Y -> <action>`.
- Use `--park-hover X,Y` for hover art and `--warning`, `--request`,
  `--campaign-end` for the dialog family.
- Coordinates below are the retail 800x600 client space. Query a control
  instead of guessing:

```powershell
.venv\Scripts\python.exe tools\formats\ui_map.py find --screen park-budget --active
.venv\Scripts\python.exe tools\formats\ui_map.py find OPEN_BUDGET_PANEL
```

- Retail scripted path: `scripts\x32dbg\start-campaign.ps1 -UnlockCampaigns`
  reaches Cpg04; keep the game active for clicks (DirectInput is
  `DISCL_FOREGROUND`). Screenshots default to `notes/harness/`.

## Panel matrix

`Open` and `Close` are control names with their click point. `-` means no
opener/closer in the port; the panel is reached by a world pick or is legacy.
`Populated` says what the port draws from live state.

| Screen id | Open | Close | Populated in port | Status | Test |
| --- | --- | --- | --- | --- | --- |
| `park-hud` | always on | - | clock/day, budget, visitor/dino/paddock counts, minimap | ported | `test_cpp_park_hud_load`, `test_cpp_park_clock`, `test_cpp_minimap_detail_view` |
| `park-budget` | `OPEN_BUDGET_PANEL` (97,584) | `BUDGET_OUTCOME_OK` (429,482) | 30 `BUDGET_*_NUMBER` fields, categories and totals | ported partial | `test_cpp_budget_panel_ledger` |
| `park-visitors` | `OPEN_VISITOR_PANEL` (207,584) | `VISITEURS_STATS_OUTCOME_OK` (195,400) | static art; no visitor-stat fields | partial | `test_cpp_park_panels` |
| `park-dino-list` | `OPEN_DINO_LIST_PANEL` (302,584) | same opener | five name slots, `DINO_AMOUNT` | partial | `test_cpp_park_fence_and_dino_lists` |
| `park-paddock-list` | `OPEN_PADDOCK_LIST_PANEL` (492,584) | same opener | five rows: snapshot, name, dino quantity, occupation percent, herbivore/carnivore food sufficiency, `PADDOCK_AMOUNT`; port fills name/count/percent | partial | `test_cpp_park_fence_and_dino_lists` |
| `park-show-list` | `OPEN_SHOW_LIST_PANEL` (397,584) | `ATTRACTION_LIST_OUTCOME_CANCEL` (543,381) | ten hour rows: show name, iteration count, lock icon, permutation/selection; lock/unlock, iteration up/down and selection act on the placed show | ported partial | `test_cpp_campaign_show_panel_ui`, `test_cpp_campaign_attraction_list_actions` |
| `park-dino-purchase` | `OPEN_PURCHASE` (765,298) | `DINO_ACHAT_SAVE_OUTCOME_CANCEL` (502,521) | six-row store viewport, selection, scroll, charge and paddock placement | ported partial | `test_cpp_store_panel_purchase`, `test_cpp_store_panel_no_money`, `test_cpp_store_panel_scroll` |
| `park-options` | `OPEN_OPTIONS` (765,348) | `OPTIONS_OPTIONS_VALIDATE_BUTTON` (355,432) / `OPTIONS_UNVALIDATE_BUTTON` (364,436) | save/load tabs, real save names, name entry | ported partial | `test_cpp_options_save_panel`, `test_cpp_options_save_and_load_buttons`, `test_cpp_options_name_entry` |
| `park-construction` | `OPEN_CONSTRUCTION` (764,135) | `OPEN_CONSTRUCTION` again | three sub-buttons | ported | `test_cpp_park_panels` |
| `park-construction-kit` | `OPEN_CONSTRUCTION_KIT` (648,145) | `OPEN_CONSTRUCTION` (back to the bar) | six category groups, list/stats icons, 73 controls | partial | `test_cpp_park_panels` |
| `park-construction-fence` | `OPEN_CONSTRUCTION_FENCE` (618,145) | `OPEN_CONSTRUCTION` | switch buttons only; fence tool via key/CLI | partial | - |
| `park-construction-show` | `OPEN_CONSTRUCTION_ATTRACTION_PREDEF` (678,145) | `PREDEFINED_SHOW_CANCEL` (384,474) | mission `[SHOWS]` catalog in a seven-column list, selection + scroll, VALIDATE arms the placement tool | ported partial | `test_cpp_campaign_show_panel_ui`, `test_cpp_campaign_show_list_scroll` |
| `park-construction-road` | `OPEN_TRANSPORT` (765,248) | `PUTBACK_PANEL_BUTTON` (679,278) | static art | partial | - |
| `park-construction-buildings` | `OPEN_BUILDING` (765,195) | `CONSTRUCTION_BUILDINGS_PUTBACK_BUTTON` (632,397) | six category tabs; `.bbi` buttons feed `construct <widget>`; placement checks | ported partial | `test_cpp_construction_placement` |
| `park-lab-main` | `OPEN_LABORATORY` (757,46) | `CANCEL_LAB_MENU` (532,513) | backdrop/controls only | partial | `test_cpp_park_panels`, `test_cpp_park_lab_shot` |
| `park-lab-breed` | `SELECT_LAB_BREED` (261,513) | `BREED_BACK` (439,565) | static gene/part controls | partial | `test_cpp_park_panels` |
| `park-lab-modify` | `SELECT_LAB_MODIFY` (311,513) | `MODIFY_BACK` (439,507) | static | partial | - |
| `park-lab-fridge` | `SELECT_LAB_FRIDGE` (453,514) | `FRIDGE_BACK` (439,525) | static | partial | - |
| `park-lab-clone` | `SELECT_LAB_CLONE` (361,513) | `CLONE_BACK` (439,507) | static | partial | - |
| `park-attraction-stats` | `CONSTRUCTION_KIT_STATS_ICON` (84,527) | `ATTRACTION_STATS_OUTCOME_CANCEL` (488,375) | static | partial | `test_cpp_park_panels` |
| `park-mission` | `OBJECTIF_PANEL_LIGHTS_GROUP` (12,50, inactive until a mission) | `MISSION_PANEL_OUTCOME_OK` (320,407) | briefing/tips, five objective lines, ON/OFF lights when `--start-mission` runs | partial | `test_campaign_cpp.py` |
| `park-food` | no HUD opener | - | static | partial | `test_cpp_food_source_is_visitor_only` (CLI only) |
| `park-dino-info` | world pick / legacy | - | `--dino-info NAME` headless report: the 11 live gauges (`0x5e2940`), four needs + states, food gauge/kind/source, rest, health/state, market value/refund; the retail widgets are not drawn | partial | `test_cpp_dino_info` |
| info panels: `park-cage-info`, `park-building-info`, `park-staff-security`, `park-staff-food`, `park-cage-list`, `park-dino-placement` | world pick / legacy | - | static or not shown | not ported | - |
| `dialog-warning` | event or `--warning` | `WARNING_OUTCOME_ICON` | text zone | ported | `test_cpp_warning_dialog` |
| `dialog-request` | event or `--request` | `REQUEST_VALIDATE_ICON` / `REQUEST_CANCEL_ICON` | text zone | ported | `test_cpp_request_dialog` |
| `dialog-campaign-end` | event or `--campaign-end` | `CPGEND_OUTCOME_ICON` | text zone | ported | `test_cpp_campaign_end_dialog` |

## What the port fills

- **HUD:** `PARK_CURRENT_TIME`, `PARK_CURRENT_DAY`, `BUDGET_AMOUNT`,
  `VISITOR_AMOUNT`, `DINO_AMOUNT`, `PADDOCK_AMOUNT`, `SHOW_AMOUNT` (placed
  show arenas), plus the minimap FBO and camera marker.
- **Budget:** `BUDGET_<CATEGORY>_QUANTITY_NUMBER` and
  `..._COST_NUMBER` for the fourteen rows, `BUDGET_TOTAL_LEFT_COST_NUMBER`
  (expenses), `BUDGET_TOTAL_RIGHT_COST_NUMBER` (income),
  `BUDGET_TOTAL_PRICE_PANEL_PRICE_ZONE` (net). Fresh parks show zeros; the
  test purchases a dinosaur and a RestCirc to drive three rows.
- **Dino list:** `DINO_NAME_SLOT1..5` from the imported dinosaurs (species
  names) and `DINO_AMOUNT`. The slot frames and need/size indicators are
  retail art; live portraits are not rendered.
- **Paddock list:** `FENCE_NAME_SLOT1..5`, `FENCE_LIST_QUANTITY_SLOT1..5`,
  `FENCE_LIST_POURCENT_SLOT1..5`, `PADDOCK_AMOUNT`. Only the first five
  paddocks are shown; scrolling is not wired yet. Retail (`FUN_00511270`,
  `FUN_005e3240(5, ...)`) also fills `FENCE_SNAPSHOT_SLOT1..5`, the
  `FENCE_LIST_DINO_INDICATEURS_PANEL_SLOT` and the
  `FENCE_LIST_HERBIQUANTITY_SLOT`/`FENCE_LIST_CARNIQUANTITY_SLOT` pair.
  The `...QUANTITY...` names are misleading: the INI hints are
  `&HerbiFoodSufficiency` / `&CarniFoodSufficiency`, and `0x57e720` fills
  them with the summed per-dino food factor (`scale * [dino+0x6c]`), 100%
  when the bucket is empty. Clicking a snapshot selects that paddock and moves the camera
  (`notes/road-enclosure-preview-capture.md`, step 7). The snapshot is a
  static 64x64 button graphic, not a live thumbnail: `FENCE_SNAPSHOT_SLOT1`
  has `TEXTURE = DINO_SNAPSHOT_SLOT1`, mapped by `ParkGraphicDB.ini` to
  `pann_list4.tga` (0,176, 64x64, 8 objects). The indicator panel and
  herbivore/carnivore split are also not ported.
- **Store:** six visible rows (`DINO_ACHAT_SAVE_NAMES_LIST`), selection,
  scroll strip (`COMBOBOX1_LB_VS_BTN_UP/DOWN`), SAVE charges through the
  budget and places the dinosaur into the clicked paddock; no money shows
  `WARNING_NOT_ENOUGH_MONEY` and stays in the panel.
- **Options:** `OPTIONS_MENU_SAVE_BUTTON` / `OPTIONS_MENU_LOAD_BUTTON`
  switch to `OPTIONS_SAVE_PANEL`; `OPTIONS_SAVE_NAMES_LIST` lists the real
  `run/save/*.sav` names plus `[port]` `.psav` files; `OPTIONS_NAME_SLOT`
  edits a name; `OPTIONS_VALIDATE_BUTTON` saves/loads the port park state.
- **Construction:** the three sub-buttons replace the bar; the buildings
  panel switches six `CONSTRUC_*_GROUP`s and every `.bbi` button emits
  `construct <widget>`; the kit's stats/list icons open the info panels.
- **Show construction:** `OPEN_CONSTRUCTION_ATTRACTION_PREDEF` fills
  `PREDEFINED_SHOW_LIST` from the active mission's `[SHOWS]` catalog (seven
  columns: name, judge, price, cage 1..4). Selecting a row and pressing
  `PREDEFINED_SHOW_VALIDATE` arms the show placement tool; the next ground
  click places the arena. `--show-status` prints the ten attraction rows.
- **Attraction list:** `ATTRACTION_LIST_NAME<n>` shows the scheduled show
  and is blank on an empty slot (retail clears it; the hour labels are the
  separate `ATTRACTION_LIST_TIME<n>` widgets), `ATTRACTION_LIST_ITERATION_INDIC<n>`
  the performances-per-day, and `LOCK`/`UNLOCK`, `ITERATION_UP/DOWN`,
  `PERMUTATION` and `SELECTION_LIGHT` act on the placed show (`show+0x8c`,
  `show+0xd3`, hour slot `show+0x100`).
- **Mission panel:** with `--start-mission FILE` the briefing description,
  tips, five objective descriptions and the ON/OFF lights are populated;
  verdicts update every second and `CPGEND_PANEL` appears on success/failure.
  The live traits come from the imported phenotype snapshots; hired staff fill
  the `employees` objective, and hybrid/scarcity/strangeness/ornaments are not
  filled yet. Campaign pages start missions from the menu.

## Recipes

### Budget

```powershell
.\build\port\dino_port.exe --frames 2 --no-menu --no-hud --width 800 --height 600 `
  --no-vsync --no-save-config --lnd "...\isl04.lnd" --asset-root game\%MAINDIR% `
  --no-vegetation --no-water --wld-park "...\Cpg04_Save.wld" `
  --park-click 97,584 --park-click 429,482 --budget-info
```

Expect `park click 97,584 -> open BUDGET_PANEL`, `park click 429,482 ->
close BUDGET_PANEL`, `budget yesterday: dinoachat=0/0 construction=0/0
tickets=0/0`, `budget totals: expenses=0 income=0 net=0`. Add `--buy-dino
Zarbosaurus --construct RestCirc --construct-pos 516,288` to see
`dinoachat=1/4900 construction=1/2500` and `expenses=7400 net=-7400`.

### Store

```powershell
... --park-click 765,298 --park-click 160,88 --park-click 470,520 `
    --ground-click 400,300
```

Expect `open DINO_ACHAT_SAVE_PANEL`, `store select 0`, `store buy 0`,
`store: Allosaurus price=9000 selected, click a paddock`, then
`store: bought Allosaurus price=9000 budget=66000 paddock=7 dinos=21
dinoachat=1/9000 used=32 total=368`. With `--park-budget 100` the same
sequence prints `WARNING_NOT_ENOUGH_MONEY` and no purchase.

### Lists

```powershell
... --park-click 302,584   # dinosaur list
... --park-click 492,584   # paddock list
```

Expect `park list: dinos=20 paddocks=9` on Cpg04; the first name slot and
the two counter strips must render at (176,541)-(240,557) and (172,490)/(172,525).

### Construction / buildings

```powershell
... --park-click 748,177 --park-click 582,28 --park-click 577,243 `
    --construct-pos 500,500 --construct-pos 500,500
```

Expect `open CONSTRUCTION_BUILDINGS_PANEL`, the restaurant category, then
`construct CONSTRUC_RESTAURANT3_BUTTON`, `construction: RestCirc selected,
cost=2500`, `... budget=72500 cell=500,500`, and the second click
`RestCirc overlaps another building`.

### Lab

```powershell
... --park-click 757,46 --park-click 261,513 --park-click 489,565
```

Expect `open DINO_LAB_MAIN_CENTER`, `open LAB_BREED_CENTER`, `close lab`.
Lab centres hide the HUD and show their own backdrop; no lab simulation is
wired yet.

### Options / save

```powershell
... --park-click 765,348 --park-click 348,73 --park-click 334,436
```

Expect `open OPTIONS_PANEL`, `options save tab`, `save port`, and a
`port save:` line. The load tab uses `378,73`, selects a row at
`300,{131 + row*12}`, then `334,436` (`load port`).

`--park-click 765,348 --park-click 447,74` opens the panel and clicks
`OPTIONS_MENU_EXIT_ICON` (parent `OPTIONS_MENU_EXIT_GAME`,
`ConstructionInterface.Ini`, hint `&QuitGame`). Retail raises the **REQUEST**
panel with `&REQUEST_RETURN_MENU` — "Do you really want to quit and return to
the menu?" — not an immediate exit:

- validate (`REQUEST_VALIDATE_ICON`, 384,370): returns to the **main menu**
  (page 0). No save prompt; the park session ends. The retail harness
  snapshotted `run-test/save/` before the dialog, while it was open and 12 s
  after confirming: every file's size/mtime/SHA-256 is unchanged, so there is
  no autosave on this path.
- cancel (`REQUEST_CANCEL_ICON`, 414,370): closes the dialog and stays in the
  park.

Port: `ParkHud::return_to_menu_requested()` + `activate_main_menu()` (which
also calls `ParkHud::end_session()` and restores the menu music).
Evidence: `tests/test_app_cpp.py::test_cpp_park_quit_confirmation_and_menu`,
retail captures `notes/harness/quit-confirm.png`,
`quit-after-confirm.png`, `quit-cancel.png` and the timelines
`quit-confirm-timeline.txt` / `quit-cancel-timeline.txt`.
Re-run against retail with
`scripts\x32dbg\test-quit-flow.ps1 -Action confirm` (or `-Action cancel`:
windowed harness, `run-test/`, virtual mouse, background-safe).

The main-menu `QUIT_BUTTON` is different: it exits the application directly
(`Menu::quit`).

### Dialogs

```powershell
... --warning "No room in this paddock" --park-click 401,368
... --request "A sponsor wants a photo" --park-click 384,370
... --campaign-end "Campaign complete" --park-click 401,447
```

The icons close/accept/cancel; clicks behind the dim are consumed.

## Gaps / next work

- Visitor statistics, show list rows, attraction stats, mission/objective
  text and the food-distribution panel are static retail art.
- The info controllers (dino, cage, building, staff, placement) have no
  world-pick opener in the port; their roots exist but are never shown.
- Dino/paddock list scrolling, live 3D thumbnails, need/size indicator
  frames and slider/checkbox selection remain.
- Store: 3D preview actor and the eleven attribute gauges.
- Lab: breeding/cloning/fridge state machine, gene/part editing, attribute
  gauges.
- Construction: slope/pen validation, costs/limits, store/lab gauges and
  per-triangle fence materials.
- Minimap water colour/partition and HUD batching.

See the **partial** rows in `feature-status.md` for the same gaps with
evidence/test links.

## Evidence

- `hud.md` — HUD rendering, draw order, anchoring, clock, minimap.
- `ui-coordinates.md` — every control and click point.
- `../decomp/economy.md` — budget row mapping and daily settlement.
- `../decomp/construction-validation.md` — FENCE_INSERTION capacity model.
- `../decomp/park-time.md` — clock and show scheduler.
- `../../notes/store-panel-recon.md` — store panel research.
- `../../notes/park-budget.png`, `park-fence-list.png`, `park-dino-list.png`.
- Tests: `tests/test_app_cpp.py::test_cpp_park_panels`,
  `test_cpp_park_panel_shot`, `test_cpp_park_lab_shot`,
  `test_cpp_park_fence_and_dino_lists`, `test_cpp_budget_panel_ledger`,
  `test_cpp_store_panel_purchase`, `test_cpp_options_save_panel`.
