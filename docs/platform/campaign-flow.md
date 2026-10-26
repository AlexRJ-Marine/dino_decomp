# Campaign flow — data, briefing and port status

Status: **retail data and briefing flow decoded; the campaign runtime is not
ported.** The port renders the campaign menu pages and can import a campaign
park for sandbox tests, but it does not select a mission, evaluate objectives
or advance the campaign.

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
  info; both render their retail backgrounds and widget sets from
  `MenuInterface.Ini` (`--menu-page 2`, `--menu-page 3`).
- `src/formats/campaign.{h,cpp}` parses `CampaignList.ini`, the
  `CampaignBase.ini`/`MissionBase.ini` includes and every mission:
  **7 campaigns, 39 missions, 89 objectives, 33 known verbs**. CLI:
  `--campaign-info Data/Design/Scenario` prints the tree with resolved verb
  ids; `--campaign-mission FILE` prints one mission. `tests/test_campaign_cpp.py`
  pins the counts and Cpg04_Mission1's three objectives.
- The port does **not** handle `CAMPAIGN_LIST` selection, `CAMPAIGN_START`
  or `CAMPAIGN_INFO_START`; only `ISLAND_LIST`/`FREE_START` are wired. A
  campaign mission cannot be started from the port menu.
- `MISSION_PANEL` renders, but no objective text or verdict is populated.
  `CPGEND_PANEL` is available through `--campaign-end TEXT`.
- Sandbox path: `--wld-park <CpgNN_Save.wld>` or `--sav-park <save>` loads
  the campaign park (roads, fences, buildings, dinosaurs, budget, clock) for
  port tests without campaign rules. Cpg04/isl04 is the main fixture.
- Campaign progress/full mission flow, objective evaluation, mission
  selection, campaign-end trigger and ending videos are not ported. The
  reader is the first slice; the evaluator is next (see
  `decoded-features.md`).

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

Viewer pages (no campaign start):

```powershell
.\build\port\dino_viewer.exe --frames 2 --menu-page 2 --width 800 --height 600 `
  --no-vsync --no-save-config --asset-root "game\%MAINDIR%" `
  --screenshot notes\harness\campaign-select.bmp
.\build\port\dino_viewer.exe --frames 2 --menu-page 3 ... `
  --screenshot notes\harness\campaign-info.bmp
```

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

## Gaps / next work

- Port the objective evaluator (33 verbs) and the campaign state machine:
  mission select, briefing, objective tracking, success/failure, campaign-end
  dialog and unlock persistence.
- Wire `CAMPAIGN_LIST`/`CAMPAIGN_START`/`CAMPAIGN_INFO_START` in `Menu`.
- Populate `MISSION_PANEL` objective text and the objective-light HUD.
- Decide the difficulty source (`mission+0x50`) and the cut `Cpg07` policy.
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
  `tests/test_campaign_cpp.py`.
- Runtime: `scripts/x32dbg/start-campaign.ps1`; screenshots
  `notes/campaign-park.png`, `notes/campaign-sim.png`.
