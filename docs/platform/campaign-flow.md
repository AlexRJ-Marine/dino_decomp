# Campaign flow — data, briefing and port status

Status: **menu campaign flow and the campaign runtime are ported.** The port
renders the campaign select/briefing pages from `CampaignList.ini`, reads the
`run/save/dino.dat` unlock mask, starts the selected unlocked campaign's first
mission from the menu, imports `CpgNN_Save.wld`, and evaluates objectives.
Mission progression and briefing pagination are still open.

This is the runtime companion to `../formats/campaign.md` (the INI contract)
and `../decomp/function-status.md` (the evaluator addresses).

## Retail flow

```text
Main menu
  CAMPAIGN_BUTTON (395,189)
    -> menu-campaign-select: eight row slots; CampaignList.ini lists 7
       campaigns (Cpg01..Cpg06, Cpg08; Cpg07 is cut/unlisted)
       row 3 = Cpg04 "PANIC IN THE SAVANNA"
       CAMPAIGN_START_BUTTON (364,530)
    -> menu-campaign-info: CAMPAIGN_INFO_DESCRIPTION rows (17 px apart),
       the mission list; row 0 at (266,150)
       CAMPAIGN_INFO_START_BUTTON (521,449)
    -> load CpgNN_Save.wld, open MISSION_PANEL over the park
       MISSION_PANEL_OUTCOME_OK (320,407) dismisses the briefing
    -> live objective evaluation; CPGEND_PANEL on success/failure
```

- Campaign progression is `run/save/dino.dat`, a 4-byte dword: `1` = first
  campaign only, `0x0F` = campaigns 1-4 unlocked. Reader `0x5612d0`, writer
  `0x5617a0`. The harness backs up/restores it.
- Difficulty is `mission+0x50` (0/1/2) and selects the `Budget`,
  `SellBackFactor` and `VisProfiles` variants (`FUN_00561bb0`). No retail
  mission overrides the `Medium`/`Hard` keys, so difficulty 1/2 would inherit
  the base values; the exact writer of `+0x50` remains open.
- The briefing and ending text come from loc IDs; `IslandSnapshot` is a dead
  field (none of the 7 TGAs ship). The five ending videos that exist are
  `basket/tennis/trampoline/Trappe/mixe.mpg`; Cpg06 and Cpg08 have no ending
  video and Cpg07 is `None`.

## Data contract

`Data/Design/Scenario/`:

```text
CampaignList.ini        7 folder names (count + lines)
CampaignBase.ini        campaign template: BRIEFING, LANDSCAPE, MISSION, CONTEXT
MissionBase.ini         mission template: BRIEFING, OBJECTIVES, CONTEXT,
                        PROFILES, BUILDINGS0..9, FOODSOURCES0..1,
                        FEATURES0..9, SHOWS0..9, DINOSTORE0..9, FRIDGE,
                        LABORATORY, INTERFACE
CpgNN/CpgNN.ini         includes CampaignBase.ini, overrides
CpgNN/CpgNN_MissionN.ini includes MissionBase.ini, overrides
CpgNN/CpgNN_MissionList.ini
CpgNN/CpgNN_Save.wld    initial park
```

- 7 campaigns, 39 missions, 89 objective lines / 28 verbs (33 evaluator ids
  0..32). Campaign budgets 20,000..60,000; missions can override the budget,
  sell-back and allowed profiles/content lists.
- Objectives are `verb arg...` strings; the evaluator returns 0 pending, 1
  success, 2 failure. The decoded evaluators, field offsets and trait verbs
  are in `../formats/campaign.md` and `notes/objectives-recon.md`.
- `BuildingsN`/`FeaturesN` indices are **categories**, not id ranges; see the
  content-group table in `../formats/campaign.md`.
- Mission content is selected by `mission+0x50`: budget, sell-back and
  profiles; the building/show/store/feature lists are difficulty-independent.

## Port status

- `src/ui/menu.cpp` maps page 2 = campaign select and page 3 = campaign
  info. `CAMPAIGN_LIST` selects rows in `CampaignList.ini` order and displays
  each campaign's `title_id`; `CAMPAIGN_START_BUTTON` opens page 3 with the
  campaign title, the wrapped `description_id` briefing and the matching
  `CAMPAIGN_INFO_ISLAND_PIC_islNN`; `CAMPAIGN_INFO_START_BUTTON` starts the
  campaign. `CAMPAIGN_CANCEL_BUTTON`/`CAMPAIGN_INFO_CANCEL_BUTTON` return to
  the main menu. Evidence: `notes/campaign-menu-select.png`,
  `notes/campaign-menu-briefing.png`.
- `src/formats/campaign.{h,cpp}` parses `CampaignList.ini`, the
  `CampaignBase.ini`/`MissionBase.ini` includes and every mission:
  **7 campaigns, 39 missions, 89 objectives, 33 known verbs**. CLI:
  `--campaign-info Data/Design/Scenario` prints the tree with resolved verb
  ids; `--campaign-mission FILE` prints one mission. `tests/test_campaign_cpp.py`
  pins the counts and Cpg04_Mission1's three objectives.
- `src/world/campaign_objectives.{h,cpp}` implements the evaluator core:
  tri-state verdicts, `treasury`/`marchandizing`/`dyingdinos`/`visitors`/
  `employees`/`cameraaltitude`/`delaylimit`/`buyalldinos`/`bankrupt`, the
  `happyvisitors` check and the 10-field trait verbs (`aggressivedinos`,
  `genotypedinos` species filter, `cutedinos`, `hybriddinos`, `speeddinos`,
  `staminadinos`, `cleverdinos`, `sizedagilitydinos`, `ornateddinos`,
  `prettystrangedino`). Show/building/isolation/hungry verbs stay pending.
  `--selftest` exercises 30 verdicts.
- `src/world/campaign_runtime.{h,cpp}` drives a started mission: objective
  evaluation once per second, tri-state aggregate, stop on success/failure.
- `--start-mission FILE` starts a mission in the loaded park, opens
  `MISSION_PANEL` with the briefing description, tips, five objective lines
  and ON/OFF lights, updates them live, and shows `CPGEND_PANEL` on
  success/failure. The live state fills treasury, visitor count/satisfaction,
  merchandise revenue, camera altitude, clock, store-empty, bankruptcy and
  the imported dinosaurs' phenotype traits (cuteness/beauty, aggression,
  size, speed, stamina, agility, intelligence, food). Hired staff fill the
  `employees` objective from their `.bei` role names (`Nutritionist`/`Guard`);
  hybrid, scarcity, strangeness and ornaments are still 0 and deaths are 0.
- `CAMPAIGN_INFO_START_BUTTON` loads the campaign's `[LANDSCAPE] Island` and
  `[CONTEXT] WorldFile` through the same building/road/fence/dinosaur import
  as `--wld-park`, sets the starting budget from the campaign plus the
  mission `Budget` delta, and opens `MISSION_PANEL` with the first mission.
  Example: Cpg04 -> `Isl04` + `Cpg04_Save.wld` (57 buildings, 14 roads,
  9 fences, 20 dinosaurs), budget 20,000, Cpg04_Mission1. Evidence:
  `notes/campaign-menu-start.png`, `tests/test_campaign_cpp.py`
  (`test_cpp_menu_campaign_mission_start`).
- The retail campaign list is gated by `run/save/dino.dat`; the port reads the
  4-byte mask (`1` = Cpg01 only, `0x0F` = Cpg01–Cpg04; missing/short defaults
  to 1) and renders locked rows disabled (frame 0 + grey text). Start refuses
  a locked row; `--dino-dat FILE` overrides the file for tests. The exact
  retail locked-row art is still unproven. Evidence:
  `notes/campaign-menu-unlock.png` (mask `0x0F`, row 3 selected; rows 5–7
  locked).
- The campaign always starts at the first mission (progression is not
  persisted), and the retail 4-page briefing (`1/4` + idea bulb) is collapsed
  into the current `MISSION_PANEL` description.
- `DINOPSAV` stores the active mission id (`mission Cpg04_Mission1`), so
  `--load-port`/the Load tab resumes the mission. Objective verdicts are
  recomputed by the first runtime tick, not stored. Mission progression, the
  exact actor trait fields, ending videos and the remaining
  show/building/isolation/hungry verbs are open.

## Verification recipes

Retail (virtual input; keep the game active for clicks):

```powershell
.\scripts\x32dbg\start-campaign.ps1 -UnlockCampaigns
# defaults: -CampaignRow 3 (Cpg04) -MissionRow 0
```

The script writes `run/save/dino.dat = 0F`, clicks
`395,189` -> `409,285` -> `364,530` -> `521,449`, dismisses the briefing at
`322,402` (inside `MISSION_PANEL_OUTCOME_OK`), and restores `dino.dat` on
exit. Trace switches: `-NoTrace`, `-GenotypeTrace`, `-SecuTrace`. Logs go to
`notes/x32dbg-campaign.out.log` by default; screenshots to
`notes/harness/`.

Viewer pages (mask 0F unlocks Cpg01–Cpg04):

```powershell
[IO.File]::WriteAllBytes("$env:TEMP\dino-0f.dat", [byte[]](0x0F,0,0,0))
.\build\port\dino_viewer.exe --frames 2 --menu-page 2 --width 800 --height 600 `
  --no-vsync --no-save-config --asset-root "game\%MAINDIR%" `
  --dino-dat "$env:TEMP\dino-0f.dat" --menu-click 404,285 `
  --screenshot notes\harness\campaign-select.bmp
.\build\port\dino_viewer.exe --frames 2 --menu-page 2 --width 800 --height 600 `
  --no-vsync --no-save-config --asset-root "game\%MAINDIR%" `
  --dino-dat "$env:TEMP\dino-0f.dat" --menu-click 404,285 --menu-click 364,530 `
  --screenshot notes\harness\campaign-info.bmp
```

Starting Cpg04_Mission1 through the menu (no `--lnd`/`--wld-park` needed):

```powershell
.\build\port\dino_viewer.exe --frames 4 --menu-page 2 --width 800 --height 600 `
  --no-vsync --no-save-config --asset-root "game\%MAINDIR%" `
  --dino-dat "$env:TEMP\dino-0f.dat" `
  --menu-click 404,285 --menu-click 364,530 --menu-click 521,449 `
  --menu-loading-time 0 --screenshot notes\harness\campaign-start.bmp
```

Expect `menu campaigns: 7 unlocked=0xF`, `wld: placed 57/57 buildings`,
`dinos: 20/20 spawned`, `start: campaign=Cpg04 mission=Cpg04_Mission1
island=Isl04 budget=20000`, and the three Cpg04_Mission1 objective lines.
With a fresh mask (`01`) the same row click stays on page 2 because Cpg04 is
locked.

Sandbox park:

```powershell
.\build\port\dino_viewer.exe --frames 2 --no-menu --no-hud --width 800 --height 600 `
  --no-vsync --no-save-config `
  --lnd "game\%MAINDIR%\Data\Design\landscape\isl04\isl04.lnd" `
  --asset-root "game\%MAINDIR%" --no-vegetation --no-water `
  --wld-park "game\%MAINDIR%\Data\Design\Scenario\Cpg04\Cpg04_Save.wld"
```

Expect `wld park: ... buildings=57 ... dinos=20` and the park HUD counts.

Reader:

```powershell
.\build\port\dino_viewer.exe --campaign-info "game\%MAINDIR%\Data\Design\Scenario"
.\build\port\dino_viewer.exe --campaign-mission "...\Cpg04\Cpg04_Mission1.ini"
```

Expect `campaigns: 7 missions: 39 objectives: 89 unknown: 0 verbs: 33`, the
Cpg04 line (`island=Isl04 budget=20000 missions=4`), and the three
Cpg04_Mission1 objectives (`isolateddinos 71` id 18, `treasury 25000 1` id 0,
`dyingdinos 5` id 2).

Mission run (Cpg04/isl04, treasury satisfied, isolation pending):

```powershell
.\build\port\dino_viewer.exe --frames 2 --no-menu --width 800 --height 600 `
  --no-vsync --no-save-config --lnd "...\isl04.lnd" --asset-root game\%MAINDIR% `
  --no-vegetation --no-water --wld-park "...\Cpg04_Save.wld" `
  --park-budget 25000 --start-mission "...\Cpg04_Mission1.ini"
```

Expect `mission: Cpg04_Mission1 objectives=3`, the three objective lines,
`mission objective 1 treasury -> success`, `mission objective 2 dyingdinos ->
success`, and no success/failure aggregate (isolateddinos is pending).

Live employees: Cpg01_Mission6 on isl01 with `--hire-nutritionist 2` prints
`mission objective 1 employees -> success`; `connectedbuildings` stays
pending, so the mission does not complete.

Mission save/load: the `--save-port` run writes `mission Cpg04_Mission1` into
the `DINOPSAV`; a later `--load-port` run with the same island resumes the
mission and prints `mission: Cpg04_Mission1 objectives=3` plus the live
verdicts (no `--wld-park`/`--start-mission` needed on load).

A mission that completes: `Cpg01_Mission11` (`visitors all 30`) on isl01 with
a 24-cell path loop and `--visitors 30` prints `mission objective 0 visitors
-> success` and `mission Cpg01_Mission11: success`, then opens
`CPGEND_PANEL`. The panel shows the briefing description, tips, objective
lines and ON/OFF lights (`notes/mission-panel.png`).

## Gaps / next work

- Persist the mission index and advance missions after `CPGEND_PANEL`; write
  the next campaign's unlock bit to `dino.dat`.
- Port the retail 4-page campaign briefing (`1/4` counter, next/previous and
  idea-bulb buttons) instead of reusing the mission description box.
- Populate `MISSION_PANEL` objective text and the objective-light HUD (done
  for the started mission; campaign-tier row gating remains).
- Decide the difficulty source (`mission+0x50`) and the cut `Cpg07` policy.
  The captured retail Cpg04 briefing showed 17,000 while `Cpg04.ini` says
  `Budget=20000` and `Cpg04_Mission1` inherits `Budget=0`; the retail
  starting-budget source is still open (the port uses 20,000).
- Ending videos (`mvVideo`/DirectShow) are decoded but not ported.

## Evidence

- `../formats/campaign.md` — campaign/mission INI contract, budgets, content
  groups, objective verbs.
- `../decomp/function-status.md` — evaluator addresses and status.
- `notes/campaign-recon.md`, `notes/objectives-recon.md`.
- `docs/platform/world-import.md` — `--wld-park` / `--sav-park`.
- `src/ui/menu.cpp` — page mapping; `src/viewer/main.cpp` — page rendering,
  `--menu-page`, `--wld-park`.
- `src/formats/campaign.{h,cpp}` — C++ `CampaignList`/`CampaignBase`/
  `MissionBase` reader and 33-verb table; `--campaign-info`/
  `--campaign-mission`.
- Tests: `tests/test_viewer_cpp.py::test_cpp_retail_menu_pages`,
  `test_cpp_wld_park_import`, `tests/test_campaign.py`,
  `tests/test_campaign_cpp.py` (`test_cpp_campaign_unlock_mask`,
  `test_cpp_menu_campaign_select`, `test_cpp_menu_campaign_locked`,
  `test_cpp_menu_campaign_mission_start`,
  `test_cpp_campaign_mission_port_save`,
  `test_cpp_campaign_live_employees`).
- Runtime: `scripts/x32dbg/start-campaign.ps1`; retail screenshots
  `notes/campaign-park.png`, `notes/campaign-sim.png`; port screenshots
  `notes/campaign-menu-select.png`, `notes/campaign-menu-briefing.png`,
  `notes/campaign-menu-start.png`.
