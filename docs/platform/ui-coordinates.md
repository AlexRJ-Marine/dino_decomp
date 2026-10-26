# Retail UI click map

Status: **generated from the shipped overlay INIs** for the Dino Island
testing agents. This is the 800x600 client-space map used by the windowed
and virtual-input harnesses.

Do not hand-edit this file. Change `tools/formats/ui_map.py` and run:

```powershell
.venv\Scripts\python.exe tools\formats\ui_map.py build
```

Machine-readable source: `docs/platform/ui-coordinates.json`.

## Coordinate system

- Origin is the **top-left of the game client**, +X right, +Y down.
- Design space is **800x600**; the windowed runtime keeps an 800x600
  client, so these are window client coordinates.
- `Click` is the integer centre of the control rectangle. Any point
  inside `Rect` works; the activating click is also passed to the UI.
- Rectangles come from `MenuInterface.Ini`, `ParkInterface.Ini` and its
  nine includes, after the `ALIGN_CENTER` runtime pass. They are not
  screenshot estimates.
- `On open` means the control is visible when its screen/panel is open.
  `no` means a parent group is hidden by default and must be switched on
  first (for example a lab gene tab or a construction category).
- Zero-size scrollbar sub-widgets are omitted; click the scroll track or
  use the mouse wheel instead.

This is the **retail** contract. The portable viewer does not implement
every panel yet. One known port discrepancy: `Menu::click` still uses
`height / 10` (28 px) for `ISLAND_LIST` rows, while retail uses the
declared 34-px rows. If a port test selects the wrong island, treat it
as a port bug, not a map error.

## Querying the map

```powershell
# exact control name
.venv\Scripts\python.exe tools\formats\ui_map.py find OPEN_LABORATORY

# all controls on one screen
.venv\Scripts\python.exe tools\formats\ui_map.py find --screen park-hud

# only controls visible when that screen opens
.venv\Scripts\python.exe tools\formats\ui_map.py find --screen park-lab-breed --active

# substring search, JSON output
.venv\Scripts\python.exe tools\formats\ui_map.py find SORT --json
```

Click through the windowed harness:

```powershell
.\scripts\x32dbg\Send-VirtualClick.ps1 -X 757 -Y 46 -GameDir run-test
```

Retail sets DirectInput to `DISCL_FOREGROUND`: if the game is fully
inactive it logs `virtual mouse: state calls=0` and ignores virtual
clicks. Keep the game active for a click test, or check that log line
first. The coordinate map is independent of that input-state issue.

## Verified quick paths

These points are verified to fall inside the controls and are pinned by
`tests/test_ui_map.py`; several are the exact points used by the
harness scripts. A table centre can differ by 1-2 px from a harness
point and still be inside the same rectangle.

| Action | Click | Control |
| --- | --- | --- |
| Main menu -> Free Game | 395,234 | `FREE_GAME_BUTTON` |
| Main menu -> Campaigns | 395,189 | `CAMPAIGN_BUTTON` |
| Free Game -> island row 0 | 269,183 | `ISLAND_LIST` row 0 (IGUANAO) |
| Free Game -> easy | 494,451 | `FREE_EASY_BUTTON` |
| Free Game -> Start | 526,531 | `FREE_START_BUTTON` |
| Campaign row 3 | 409,285 | `CAMPAIGN_LIST` row 3 |
| Campaign -> Start | 365,530 | `CAMPAIGN_START_BUTTON` |
| Campaign info -> Start | 521,449 | `CAMPAIGN_INFO_START_BUTTON` |
| Park -> Laboratory | 757,46 | `OPEN_LABORATORY` |
| Park -> Options | 765,348 | `OPEN_OPTIONS` |
| Options -> Save tab | 348,73 | `OPTIONS_MENU_SAVE_BUTTON` |
| Save tab -> name box | 349,382 | `OPTIONS_NAME_SLOT` |
| Save tab -> Validate | 334,436 | `OPTIONS_VALIDATE_BUTTON` |
| Park -> Budget | 97,584 | `OPEN_BUDGET_PANEL` |
| Park -> Visitors | 207,584 | `OPEN_VISITOR_PANEL` |
| Park -> Dino list | 302,584 | `OPEN_DINO_LIST_PANEL` |
| Park -> Show list | 397,584 | `OPEN_SHOW_LIST_PANEL` |
| Park -> Paddock list | 492,584 | `OPEN_PADDOCK_LIST_PANEL` |

`ISLAND_LIST` row `n` is at `y = 183 + 34 * n`; only eight rows
(0-7) fit, so islands 9-10 require the list scrollbar (down click
**352,440**, up **352,177**). The harness click at 260,183 is row 0.
`CAMPAIGN_LIST` row `n` is at `y = 183 + 34 * n` (eight campaign
rows). `CAMPAIGN_INFO_DESCRIPTION` rows are 17 px apart:
`y = 150 + 17 * n`.

## Screen index

| Screen id | Title | Kind | Controls | Roots |
| --- | --- | --- | --- | --- |
| `menu-main` | Main menu | menu | 7 | MAIN_MENU_PAGE |
| `menu-free-game` | Free Game: island and difficulty | menu | 6 | FREE_GAME_PAGE |
| `menu-campaign-select` | Campaign select | menu | 3 | CAMPAIGN_SELECT_PAGE |
| `menu-campaign-info` | Campaign briefing / mission list | menu | 3 | CAMPAIGN_INFO_PAGE |
| `menu-load-game` | Load game | menu | 3 | LOAD_GAME_PAGE |
| `menu-options` | Main-menu options | menu | 11 | OPTIONS_PAGE |
| `menu-cinematics` | Cinematics | menu | 3 | CINEMATICS_PAGE |
| `menu-credits` | Credits | menu | 1 | CREDITS_PAGE |
| `menu-loading` | Loading cover | menu | 0 | LOADING_PAGE |
| `park-hud` | Park HUD (always visible) | hud | 28 | RIGHT_PANEL, NAVIGATION_PANEL, GAUGE_PANEL, MAP_PANEL, MESSAGE_PANEL |
| `park-construction` | Construction bar | park-panel | 3 | CONSTRUCTION_PANEL |
| `park-construction-kit` | Construction kit (attraction editor) | park-panel | 73 | CONSTRUCTION_KIT_PANEL |
| `park-construction-fence` | Fence construction | park-panel | 3 | FENCE_CONSTRUCTION_AND_INFO_PANEL |
| `park-construction-show` | Ready-made show list | park-panel | 4 | PREDEFINED_SHOW_PANEL |
| `park-construction-road` | Road / metro construction | park-panel | 4 | CONSTRUCTION_ROAD_METRO_PANEL |
| `park-construction-buildings` | Buildings and shops | park-panel | 37 | CONSTRUCTION_BUILDINGS_PANEL |
| `park-dino-purchase` | Dinosaur purchase / save | park-panel | 4 | DINO_ACHAT_SAVE_PANEL |
| `park-options` | In-park options / save / load | park-panel | 12 | OPTIONS_PANEL |
| `park-mission` | Mission / objectives | park-panel | 3 | MISSION_PANEL |
| `park-dino-list` | Dinosaur list | park-panel | 11 | DINO_LIST_PANEL |
| `park-show-list` | Show / attraction list | park-panel | 73 | ATTRACTION_LIST_PANEL |
| `park-paddock-list` | Paddock / fence list | park-panel | 7 | FENCE_LIST_PANEL |
| `park-cage-list` | Cage list (legacy) | park-panel | 10 | CAGE_LIST_PANEL |
| `park-lab-main` | Laboratory main | lab | 8 | DINO_LAB_MAIN_CENTER |
| `park-lab-breed` | Laboratory: breed | lab | 83 | LAB_BREED_CENTER |
| `park-lab-modify` | Laboratory: modify | lab | 8 | LAB_MODIFY_CENTER |
| `park-lab-fridge` | Laboratory: fridge / egg storage | lab | 14 | LAB_FRIDGE_CENTER |
| `park-lab-clone` | Laboratory: clone | lab | 7 | LAB_CLONE_CENTER |
| `park-food` | Food distribution | park-panel | 20 | FOOD_DISTRIBUTION_PANEL |
| `park-attraction-stats` | Attraction stats | park-panel | 3 | ATTRACTION_STATS_PANEL |
| `park-dino-info` | Dinosaur info / portrait | park-panel | 7 | DINO_INFO_PANEL, DINO_INFO_DINO_PIC |
| `park-budget` | Budget ledger | park-panel | 1 | BUDGET_PANEL |
| `park-visitors` | Visitor statistics | park-panel | 1 | VISITEURS_STATS_PANEL |
| `park-cage-info` | Cage / paddock info | park-panel | 6 | CAGE_INFO_PANEL |
| `park-building-info` | Building info | park-panel | 0 | BATIMENTS_INFO_PANEL |
| `park-staff-security` | Security staff panel | park-panel | 4 | EFFECTIFS_SECU_PANEL |
| `park-staff-food` | Food staff panel | park-panel | 2 | EFFECTIFS_NUTRI_PANEL |
| `park-dino-placement` | Dinosaur placement marker | park-panel | 0 | DINO_LIST_PARK_PLACEMENT |
| `park-objective-light` | Campaign objective light | hud | 1 | OBJECTIF_PANEL_LIGHTS_GROUP |
| `dialog-warning` | Warning dialog | dialog | 2 | WARNING_PANEL |
| `dialog-request` | Request dialog | dialog | 3 | REQUEST_PANEL |
| `dialog-campaign-end` | Campaign-end dialog | dialog | 2 | CPGEND_PANEL |
| `radial-show` | Show radial menu (unused data) | radial | 7 | SHOW_RADIAL_MENU |
| `radial-building` | Building radial menu (unused data) | radial | 2 | BUILDING_RADIAL_MENU |
| `radial-paddock` | Paddock radial menu (unused data) | radial | 4 | PADDOCK_RADIAL_MENU |
| `radial-security` | Security radial menu (unused data) | radial | 5 | SECURITY_RADIAL_MENU |

## Menu screens

### Main menu — `menu-main`

Boot screen. The seven main buttons; QUIT is at the bottom right.

Roots: `MAIN_MENU_PAGE`. Controls: 7.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `CAMPAIGN_BUTTON` | `MAIN_MENU_PANEL` | BUTTON | 288,178 214x23 | **395,189** | yes | CAMPAIGNS |
| `FREE_GAME_BUTTON` | `MAIN_MENU_PANEL` | BUTTON | 288,223 214x23 | **395,234** | yes | FREE MODE |
| `LOAD_GAME_BUTTON` | `MAIN_MENU_PANEL` | BUTTON | 288,268 214x23 | **395,279** | yes | LOAD GAME |
| `OPTIONS_BUTTON` | `MAIN_MENU_PANEL` | BUTTON | 288,313 214x23 | **395,324** | yes | OPTIONS |
| `CINEMATICS_BUTTON` | `MAIN_MENU_PANEL` | BUTTON | 288,358 214x23 | **395,369** | yes | CINEMATICS |
| `CREDITS_BUTTON` | `MAIN_MENU_PANEL` | BUTTON | 288,403 214x23 | **395,414** | yes | CREDITS |
| `QUIT_BUTTON` | `MAIN_MENU_PAGE` | BUTTON | 559,484 56x55 | **587,511** | yes |  |

### Free Game: island and difficulty — `menu-free-game`

Ten islands in an eight-row scrollable list, three difficulty buttons, Start and Cancel. A row must be selected before Start.

Open path: `FREE_GAME_BUTTON` (395,234)

Roots: `FREE_GAME_PAGE`. Controls: 6.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `ISLAND_LIST` | `FREE_GAME_PANEL` | LIST | 175,166 188x285 | **269,308** | yes |  |
| `FREE_EASY_BUTTON` | `FREE_GAME_DIFFICULTY_PANEL` | BUTTON | 468,425 52x52 | **494,451** | yes |  |
| `FREE_MIDDLE_BUTTON` | `FREE_GAME_DIFFICULTY_PANEL` | BUTTON | 542,425 52x52 | **568,451** | yes |  |
| `FREE_HARD_BUTTON` | `FREE_GAME_DIFFICULTY_PANEL` | BUTTON | 615,425 52x52 | **641,451** | yes |  |
| `FREE_START_BUTTON` | `FREE_GAME_PANEL2` | BUTTON | 502,506 49x50 | **526,531** | yes |  |
| `FREE_CANCEL_BUTTON` | `FREE_GAME_PANEL2` | BUTTON | 582,506 49x50 | **606,531** | yes |  |

`ISLAND_LIST` rows (row height 34 px):

| Row | Click | Label |
| --- | --- | --- |
| 0 | **269,183** | IGUANAO |
| 1 | **269,217** | DINOLULU |
| 2 | **269,251** | TYRANNO BEACH |
| 3 | **269,285** | DINOKO ANKYLO |
| 4 | **269,319** | SALTA ISLAND |
| 5 | **269,353** | GIGANOBONGO |
| 6 | **269,387** | BEAURIVAGE |
| 7 | **269,421** | SANTO DIPLODO |

Scrollbar width 22 px: up **352,177**, down **352,440**.

### Campaign select — `menu-campaign-select`

Eight campaign rows. Rows 1-4 are unlocked by `save/dino.dat`; the harness writes 0x0F for them.

Open path: `CAMPAIGN_BUTTON` (395,189)

Roots: `CAMPAIGN_SELECT_PAGE`. Controls: 3.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `CAMPAIGN_LIST` | `CAMPAIGN_SELECT_PANEL` | LIST | 321,166 176x285 | **409,308** | yes |  |
| `CAMPAIGN_START_BUTTON` | `CAMPAIGN_SELECT_PAGE` | BUTTON | 340,505 49x50 | **364,530** | yes | START |
| `CAMPAIGN_CANCEL_BUTTON` | `CAMPAIGN_SELECT_PAGE` | BUTTON | 420,505 49x50 | **444,530** | yes | CANCEL |

`CAMPAIGN_LIST` rows (row height 34 px):

| Row | Click | Label |
| --- | --- | --- |
| 0 | **409,183** | PROJECT D.IN.O.S. |
| 1 | **409,217** | DINO ETHICS |
| 2 | **409,251** | WELCOME TO DINOLAND |
| 3 | **409,285** | PANIC IN THE SAVANNA |
| 4 | **409,319** | DINO OF THE YEAR |
| 5 | **409,353** | QUIET PLEASE - ACTION! |
| 6 | **409,387** | UN PARC A LA DERIVE |
| 7 | **409,421** | THE DINOS' LAST STAND |

Scrollbar width 10 px: up **492,177**, down **492,440**.

### Campaign briefing / mission list — `menu-campaign-info`

Mission rows are 17 px apart; the harness uses `142 + row*17 + 8`.

Open path: `CAMPAIGN_BUTTON` (395,189) -> `CAMPAIGN_START_BUTTON` (364,530)

Roots: `CAMPAIGN_INFO_PAGE`. Controls: 3.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `CAMPAIGN_INFO_DESCRIPTION` | `CAMPAIGN_INFO_PANEL` | LIST | 149,142 235x280 | **266,282** | yes |  |
| `CAMPAIGN_INFO_START_BUTTON` | `CAMPAIGN_INFO_PANEL2` | BUTTON | 497,424 49x50 | **521,449** | yes | START |
| `CAMPAIGN_INFO_CANCEL_BUTTON` | `CAMPAIGN_INFO_PANEL2` | BUTTON | 577,424 49x50 | **601,449** | yes | CANCEL |

`CAMPAIGN_INFO_DESCRIPTION` rows (row height 17 px):

| Row | Click | Label |
| --- | --- | --- |
| 0 | **266,150** |  |
| 1 | **266,167** |  |
| 2 | **266,184** |  |
| 3 | **266,201** |  |
| 4 | **266,218** |  |
| 5 | **266,235** |  |
| 6 | **266,252** |  |
| 7 | **266,269** |  |
| 8 | **266,286** |  |
| 9 | **266,303** |  |
| 10 | **266,320** |  |
| 11 | **266,337** |  |
| 12 | **266,354** |  |
| 13 | **266,371** |  |
| 14 | **266,388** |  |
| 15 | **266,405** |  |

Scrollbar width 22 px: up **373,153**, down **373,411**.

### Load game — `menu-load-game`

The save-name list is the clickable region; row height has not been measured against a populated list yet.

Open path: `LOAD_GAME_BUTTON` (395,279)

Roots: `LOAD_GAME_PAGE`. Controls: 3.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `LOAD_GAME_NAMES_LIST` | `LOAD_GAME_PANEL` | LIST | 292,140 234x280 | **409,280** | yes |  |
| `LOAD_LOAD_BUTTON` | `LOAD_GAME_PAGE` | BUTTON | 330,500 49x50 | **354,525** | yes |  |
| `LOAD_CANCEL_BUTTON` | `LOAD_GAME_PAGE` | BUTTON | 409,500 49x50 | **433,525** | yes |  |

`LOAD_GAME_NAMES_LIST` rows (row height 20 px):

| Row | Click | Label |
| --- | --- | --- |
| 0 | **409,150** |  |
| 1 | **409,170** |  |
| 2 | **409,190** |  |
| 3 | **409,210** |  |
| 4 | **409,230** |  |
| 5 | **409,250** |  |
| 6 | **409,270** |  |
| 7 | **409,290** |  |
| 8 | **409,310** |  |
| 9 | **409,330** |  |
| 10 | **409,350** |  |
| 11 | **409,370** |  |
| 12 | **409,390** |  |
| 13 | **409,410** |  |

Scrollbar width 22 px: up **515,151**, down **515,409**.

### Main-menu options — `menu-options`

Three horizontal sliders and six value indicators. The port renders this page but the controls are not wired yet.

Open path: `OPTIONS_BUTTON` (395,324)

Roots: `OPTIONS_PAGE`. Controls: 11.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `OPTIONS_GRAPH_TEXTURE_QUALITY_INDICATION` | `OPTIONS_PANEL` | BUTTON | 484,172 78x32 | **523,188** | yes |  |
| `OPTIONS_MUSIC_HSCROLL` | `OPTIONS_PANEL` | HSCROLL | 264,180 89x16 | **308,188** | yes | Music Volume |
| `OPTIONS_GRAPH_QUALITY_INDICATION` | `OPTIONS_PANEL` | BUTTON | 484,225 78x32 | **523,241** | yes |  |
| `OPTIONS_SOUNDS_HSCROLL` | `OPTIONS_PANEL` | HSCROLL | 264,233 89x16 | **308,241** | yes | Sound Effects Volume |
| `OPTIONS_GRAPH_TEXTURE_DEPTH_INDICATION` | `OPTIONS_PANEL` | BUTTON | 484,278 78x32 | **523,294** | yes |  |
| `OPTIONS_GAME_SPEED_HSCROLL` | `OPTIONS_PANEL` | HSCROLL | 264,286 89x16 | **308,294** | yes | Game Speed |
| `OPTIONS_MUSIC_MIXING_INDICATION` | `OPTIONS_PANEL` | BUTTON | 271,331 78x32 | **310,347** | yes |  |
| `OPTIONS_GRAPH_MIXING_INDICATION` | `OPTIONS_PANEL` | BUTTON | 484,331 78x32 | **523,347** | yes | This change will take effect the next time the game is launched. |
| `OPTIONS_SCREEN_SIZE_INDICATION` | `OPTIONS_PANEL` | BUTTON | 484,384 78x32 | **523,400** | yes |  |
| `OPTIONS_OK_BUTTON` | `OPTIONS_PAGE` | BUTTON | 338,512 49x50 | **362,537** | yes |  |
| `OPTIONS_CANCEL_BUTTON` | `OPTIONS_PAGE` | BUTTON | 417,512 49x50 | **441,537** | yes |  |

### Cinematics — `menu-cinematics`

Video list with Start and Cancel.

Open path: `CINEMATICS_BUTTON` (395,369)

Roots: `CINEMATICS_PAGE`. Controls: 3.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `CINEMATICS_LIST` | `CINEMATICS_SELECT_PANEL` | LIST | 327,167 176x285 | **415,309** | yes |  |
| `CINEMATICS_START_BUTTON` | `CINEMATICS_PAGE` | BUTTON | 345,505 49x50 | **369,530** | yes |  |
| `CINEMATICS_CANCEL_BUTTON` | `CINEMATICS_PAGE` | BUTTON | 425,505 49x50 | **449,530** | yes |  |

`CINEMATICS_LIST` rows (row height 34 px):

| Row | Click | Label |
| --- | --- | --- |
| 0 | **415,184** |  |
| 1 | **415,218** |  |
| 2 | **415,252** |  |
| 3 | **415,286** |  |
| 4 | **415,320** |  |
| 5 | **415,354** |  |
| 6 | **415,388** |  |
| 7 | **415,422** |  |

Scrollbar width 10 px: up **498,178**, down **498,441**.

### Credits — `menu-credits`

Single OK button.

Open path: `CREDITS_BUTTON` (395,414)

Roots: `CREDITS_PAGE`. Controls: 1.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `CREDITS_OK_BUTTON` | `CREDITS_PANEL` | BUTTON | 453,402 30x30 | **468,417** | yes |  |

### Loading cover — `menu-loading`

No interactive controls; it is a timed cover frame.

Open path: `FREE_GAME_BUTTON` (395,234) -> `FREE_START_BUTTON` (526,531)

Roots: `LOADING_PAGE`. Controls: 0.

_No interactive controls._

## Park HUD

### Park HUD (always visible) — `park-hud`

Visible for the whole park session. Right panel opens the six main panels; gauge opens budget/visitors/dino-list/show-list/paddock-list; nav and map controls move the camera.

Roots: `RIGHT_PANEL`, `NAVIGATION_PANEL`, `GAUGE_PANEL`, `MAP_PANEL`, `MESSAGE_PANEL`. Controls: 28.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `OPEN_LABORATORY` | `RIGHT_PANEL` | BUTTON | 734,23 46x47 | **757,46** | yes | Laboratory |
| `OPEN_CONSTRUCTION` | `RIGHT_PANEL` | BUTTON | 747,117 35x36 | **764,135** | yes | Construction Kit |
| `OPEN_BUILDING` | `RIGHT_PANEL` | BUTTON | 748,177 35x36 | **765,195** | yes | Buildings |
| `OPEN_TRANSPORT` | `RIGHT_PANEL` | BUTTON | 748,230 35x36 | **765,248** | yes | Infrastructure |
| `OPEN_PURCHASE` | `RIGHT_PANEL` | BUTTON | 748,280 35x36 | **765,298** | yes | Buy Dino |
| `OPEN_OPTIONS` | `RIGHT_PANEL` | BUTTON | 748,330 35x36 | **765,348** | yes | Options |
| `NAVIGATION_ZOOM` | `NAVIGATION_PANEL` | BUTTON | 671,346 16x16 | **679,354** | yes | Zoom [+] |
| `NAVIGATION_DEZOOM` | `NAVIGATION_PANEL` | BUTTON | 699,346 16x16 | **707,354** | yes | Zoom [-] |
| `NAVIGATION_GOTOP` | `NAVIGATION_PANEL` | BUTTON | 687,364 13x13 | **693,370** | yes | Forward |
| `NAVIGATION_TURNLEFT` | `NAVIGATION_PANEL` | BUTTON | 684,378 10x20 | **689,388** | yes | Turn Left |
| `NAVIGATION_TURNRIGHT` | `NAVIGATION_PANEL` | BUTTON | 694,378 10x20 | **699,388** | yes | Turn Right |
| `NAVIGATION_GOLEFT` | `NAVIGATION_PANEL` | BUTTON | 669,382 13x13 | **675,388** | yes | Left |
| `NAVIGATION_GORIGHT` | `NAVIGATION_PANEL` | BUTTON | 705,382 13x13 | **711,388** | yes | Right |
| `NAVIGATION_GOBOTTOM` | `NAVIGATION_PANEL` | BUTTON | 687,398 13x13 | **693,404** | yes | Backward |
| `MAP_ATTRACTION_TEAM1_SLOT` | `MAP_ATTRACTION_INFO_TEAM1_BACK_SLOT` | EDIT | 660,486 27x17 | **673,494** | no |  |
| `MAP_ATTRACTION_TEAM2_SLOT` | `MAP_ATTRACTION_INFO_TEAM2_BACK_SLOT` | EDIT | 690,486 27x17 | **703,494** | no |  |
| `MAP_ATTRACTION_TEAM3_SLOT` | `MAP_ATTRACTION_INFO_TEAM3_BACK_SLOT` | EDIT | 720,486 27x17 | **733,494** | no |  |
| `MAP_ATTRACTION_TEAM4_SLOT` | `MAP_ATTRACTION_INFO_TEAM4_BACK_SLOT` | EDIT | 750,486 27x17 | **763,494** | no |  |
| `MAP_ATTRACTION_TIME_SLOT` | `MAP_ATTRACTION_INFO_TIME_BACK_SLOT` | EDIT | 721,517 27x17 | **734,525** | no |  |
| `MAP_PANEL_ZOOMIN_BUTTON` | `MAP_PANEL` | BUTTON | 625,544 16x16 | **633,552** | yes | Zoom [+] |
| `MAP_ATTRACTION_ITERATIONS_SLOT` | `MAP_ATTRACTION_INFO_ITERATIONS_BACK_SLOT` | EDIT | 721,550 27x17 | **734,558** | no |  |
| `MAP_PANEL_ZOOMOUT_BUTTON` | `MAP_PANEL` | BUTTON | 625,561 16x16 | **633,569** | yes | Zoom [-] |
| `OPEN_BUDGET_PANEL` | `GAUGE_PANEL` | BUTTON | 82,569 31x31 | **97,584** | yes | Budget |
| `OPEN_VISITOR_PANEL` | `GAUGE_PANEL` | BUTTON | 192,569 31x31 | **207,584** | yes | Visitor Statistics |
| `OPEN_DINO_LIST_PANEL` | `GAUGE_PANEL` | BUTTON | 287,569 31x31 | **302,584** | yes | Dino List |
| `OPEN_SHOW_LIST_PANEL` | `GAUGE_PANEL` | BUTTON | 382,569 31x31 | **397,584** | yes | Show Calendar |
| `OPEN_PADDOCK_LIST_PANEL` | `GAUGE_PANEL` | BUTTON | 477,569 31x31 | **492,584** | yes | Enclosure Panel |
| `OPEN_SELLING` | `WARNING_INFO_PANEL` | BUTTON | 579,572 25x25 | **591,584** | yes | Sell |

### Campaign objective light — `park-objective-light`

20x100 campaign indicator at the left edge; visible only while a mission objective is active. Opens MISSION_PANEL.

Roots: `OBJECTIF_PANEL_LIGHTS_GROUP`. Controls: 1.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `OBJECTIF_PANEL_LIGHTS_GROUP` | `ROOT` | BUTTON | 2,0 20x100 | **12,50** | no | Mission Objectives |

## Park panels

### Construction bar — `park-construction`

Three sub-buttons: kit, fence and ready-made show. It is hidden while one of those sub-panels is open.

Open path: `OPEN_CONSTRUCTION` (764,135)

Roots: `CONSTRUCTION_PANEL`. Controls: 3.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `OPEN_CONSTRUCTION_FENCE` | `CONSTRUCTION_PANEL` | BUTTON | 604,131 28x28 | **618,145** | yes | Enclosure |
| `OPEN_CONSTRUCTION_KIT` | `CONSTRUCTION_PANEL` | BUTTON | 634,131 28x28 | **648,145** | yes | Show |
| `OPEN_CONSTRUCTION_ATTRACTION_PREDEF` | `CONSTRUCTION_PANEL` | BUTTON | 664,131 28x28 | **678,145** | yes | Ready Made Show |

### Construction kit (attraction editor) — `park-construction-kit`

Switches on the left choose seat count, judge type, feature category and cage size; the feature tiles share one 3x3 area.

Open path: `OPEN_CONSTRUCTION` (764,135) -> `OPEN_CONSTRUCTION_KIT` (648,145)

Roots: `CONSTRUCTION_KIT_PANEL`. Controls: 73.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `CONSTRUCTION_KIT_NAME_INDICS` | `CONSTRUCTION_KIT_NAME_PANEL` | EDIT | 17,15 128x14 | **81,22** | yes | Name |
| `SWITCH_SEATS_SMALL` | `CONSTRUCTION_KIT_SEATS_GROUP` | BUTTON | 39,85 28x28 | **53,99** | yes | Small Seating |
| `SWITCH_SEATS_MEDIUM` | `CONSTRUCTION_KIT_SEATS_GROUP` | BUTTON | 69,85 28x28 | **83,99** | yes | Medium Seating |
| `SWITCH_SEATS_LARGE` | `CONSTRUCTION_KIT_SEATS_GROUP` | BUTTON | 99,85 28x28 | **113,99** | yes | Large Seating |
| `SWITCH_JUDGE_RACE` | `CONSTRUCTION_KIT_JUDGE_PANEL` | BUTTON | 53,160 28x28 | **67,174** | yes | Race |
| `SWITCH_JUDGE_OBSTACLE` | `CONSTRUCTION_KIT_JUDGE_PANEL` | BUTTON | 83,160 28x28 | **97,174** | yes | Obstacle Course |
| `SWITCH_JUDGE_FIGHT` | `CONSTRUCTION_KIT_JUDGE_PANEL` | BUTTON | 53,189 28x28 | **67,203** | yes | Combat |
| `SWITCH_JUDGE_DESTRUCTION` | `CONSTRUCTION_KIT_JUDGE_PANEL` | BUTTON | 83,189 28x28 | **97,203** | yes | Destruction Derby |
| `SWITCH_FEATURE_CATEGORY_WAYPOINT` | `CONSTRUCTION_KIT_FEATURE_CATEGORY_GROUP` | BUTTON | 69,245 28x28 | **83,259** | yes | Course Markers |
| `SWITCH_FEATURE_CATEGORY_BAIT` | `CONSTRUCTION_KIT_FEATURE_CATEGORY_GROUP` | BUTTON | 97,245 28x28 | **111,259** | yes | Bait |
| `SWITCH_FEATURE_CATEGORY_CAGE` | `CONSTRUCTION_KIT_FEATURE_CATEGORY_GROUP` | BUTTON | 41,246 28x28 | **55,260** | yes | Cages |
| `SWITCH_FEATURE_CATEGORY_BONUS` | `CONSTRUCTION_KIT_FEATURE_CATEGORY_GROUP` | BUTTON | 41,273 28x28 | **55,287** | yes | Bonus |
| `SWITCH_FEATURE_CATEGORY_SPECIAL` | `CONSTRUCTION_KIT_FEATURE_CATEGORY_GROUP` | BUTTON | 69,273 28x28 | **83,287** | yes | Special |
| `SWITCH_FEATURE_CATEGORY_OBSTACLE` | `CONSTRUCTION_KIT_FEATURE_CATEGORY_GROUP` | BUTTON | 97,273 28x28 | **111,287** | yes | Obstacles |
| `SWITCH_FEATURE_TILE_SMELL` | `CONSTRUCTION_KIT_WAYPOINT_FEATURE_GROUP` | BUTTON | 50,317 32x33 | **66,333** | no | Scented Bait |
| `SWITCH_FEATURE_TILE_SMELL_BAD` | `CONSTRUCTION_KIT_WAYPOINT_FEATURE_GROUP` | BUTTON | 82,317 32x33 | **98,333** | no | Stink Bomb |
| `SWITCH_BAIT_FLOWER` | `CONSTRUCTION_KIT_BAIT_FEATURE_GROUP` | BUTTON | 38,318 28x28 | **52,332** | no | Flowers |
| `SWITCH_BAIT_AUTRUCHE` | `CONSTRUCTION_KIT_BAIT_FEATURE_GROUP` | BUTTON | 67,318 28x28 | **81,332** | no | Ostrich |
| `SWITCH_FEATURE_TILE_TRAPPE` | `CONSTRUCTION_KIT_SPECIAL_FEATURE_GROUP` | BUTTON | 35,319 32x33 | **51,335** | no | Pit |
| `SWITCH_FEATURE_TILE_SCOTCH` | `CONSTRUCTION_KIT_SPECIAL_FEATURE_GROUP` | BUTTON | 67,319 32x33 | **83,335** | no | Super Glue |
| `SWITCH_FEATURE_TILE_KICKTHEIRASS` | `CONSTRUCTION_KIT_SPECIAL_FEATURE_GROUP` | BUTTON | 99,319 32x33 | **115,335** | no | Paralysis |
| `SWITCH_FEATURE_OBSTACLE_BEAM1` | `CONSTRUCTION_KIT_OBSTACLE_FEATURE_GROUP` | BUTTON | 35,321 32x33 | **51,337** | no | Small Barrier |
| `SWITCH_FEATURE_TILE_PERCEPTION` | `CONSTRUCTION_KIT_BONUS_FEATURE_GROUP` | BUTTON | 35,321 32x33 | **51,337** | no | Perception |
| `SWITCH_FEATURE_OBSTACLE_CAR1` | `CONSTRUCTION_KIT_OBSTACLE_FEATURE_GROUP` | BUTTON | 67,321 32x33 | **83,337** | no | Car |
| `SWITCH_FEATURE_TILE_COURAGE` | `CONSTRUCTION_KIT_BONUS_FEATURE_GROUP` | BUTTON | 67,321 32x33 | **83,337** | no | Courage |
| `SWITCH_FEATURE_TILE_HOLE` | `CONSTRUCTION_KIT_OBSTACLE_FEATURE_GROUP` | BUTTON | 99,321 32x33 | **115,337** | no | Hole |
| `SWITCH_FEATURE_TILE_INTELLIGENCE` | `CONSTRUCTION_KIT_BONUS_FEATURE_GROUP` | BUTTON | 99,321 32x33 | **115,337** | no | Intelligence |
| `SWITCH_BAIT_FRUITS` | `CONSTRUCTION_KIT_BAIT_FEATURE_GROUP` | BUTTON | 38,346 28x28 | **52,360** | no | Fruit |
| `SWITCH_BAIT_GOAT` | `CONSTRUCTION_KIT_BAIT_FEATURE_GROUP` | BUTTON | 67,346 28x28 | **81,360** | no | Goat |
| `SWITCH_FEATURE_TILE_PIEGE` | `CONSTRUCTION_KIT_SPECIAL_FEATURE_GROUP` | BUTTON | 35,352 32x33 | **51,368** | no | Trap |
| `SWITCH_FEATURE_TILE_GLISSE` | `CONSTRUCTION_KIT_SPECIAL_FEATURE_GROUP` | BUTTON | 67,352 32x33 | **83,368** | no | Skid |
| `SWITCH_FEATURE_TILE_FORCE_FIELD` | `CONSTRUCTION_KIT_SPECIAL_FEATURE_GROUP` | BUTTON | 99,352 32x33 | **115,368** | no | Force Field |
| `SWITCH_CAGE_SMALL` | `CONSTRUCTION_KIT_CAGE_FEATURE_GROUP` | BUTTON | 53,353 28x28 | **67,367** | yes | Small Cage |
| `SWITCH_CAGE_MEDIUM` | `CONSTRUCTION_KIT_CAGE_FEATURE_GROUP` | BUTTON | 83,353 28x28 | **97,367** | yes | Medium Cage |
| `SWITCH_FEATURE_OBSTACLE_BEAM2` | `CONSTRUCTION_KIT_OBSTACLE_FEATURE_GROUP` | BUTTON | 35,354 32x33 | **51,370** | no | Barrier |
| `SWITCH_FEATURE_TILE_JUMP` | `CONSTRUCTION_KIT_BONUS_FEATURE_GROUP` | BUTTON | 35,354 32x33 | **51,370** | no | Super Jump |
| `SWITCH_FEATURE_OBSTACLE_CAR2` | `CONSTRUCTION_KIT_OBSTACLE_FEATURE_GROUP` | BUTTON | 67,354 32x33 | **83,370** | no | Upturned Car |
| `SWITCH_FEATURE_TILE_SPEED` | `CONSTRUCTION_KIT_BONUS_FEATURE_GROUP` | BUTTON | 67,354 32x33 | **83,370** | no | Lightning Speed |
| `SWITCH_FEATURE_TILE_ENDURANCE` | `CONSTRUCTION_KIT_BONUS_FEATURE_GROUP` | BUTTON | 99,354 32x33 | **115,370** | no | Stamina |
| `SWITCH_FEATURE_TILE_GRIDHOLE` | `CONSTRUCTION_KIT_OBSTACLE_FEATURE_GROUP` | BUTTON | 99,354 32x33 | **115,370** | no | Gutter |
| `SWITCH_FEATURE_WAYPOINT_TEAM1` | `CONSTRUCTION_KIT_WAYPOINT_FEATURE_GROUP` | BUTTON | 50,355 32x33 | **66,371** | no | Marker Team Red |
| `SWITCH_FEATURE_WAYPOINT_TEAM2` | `CONSTRUCTION_KIT_WAYPOINT_FEATURE_GROUP` | BUTTON | 82,355 32x33 | **98,371** | no | Marker Team Blue |
| `SWITCH_BAIT_GOAL` | `CONSTRUCTION_KIT_BAIT_FEATURE_GROUP` | BUTTON | 99,359 28x28 | **113,373** | no | Goal |
| `SWITCH_BAIT_PASTEQUE` | `CONSTRUCTION_KIT_BAIT_FEATURE_GROUP` | BUTTON | 38,374 28x28 | **52,388** | no | Watermelon |
| `SWITCH_BAIT_MOUTON` | `CONSTRUCTION_KIT_BAIT_FEATURE_GROUP` | BUTTON | 67,374 28x28 | **81,388** | no | Sheep |
| `SWITCH_CAGE_LARGE` | `CONSTRUCTION_KIT_CAGE_FEATURE_GROUP` | BUTTON | 53,383 28x28 | **67,397** | yes | Large Cage |
| `SWITCH_CAGE_XLARGE` | `CONSTRUCTION_KIT_CAGE_FEATURE_GROUP` | BUTTON | 83,383 28x28 | **97,397** | yes | Enormous Cage |
| `SWITCH_FEATURE_TILE_MINE` | `CONSTRUCTION_KIT_SPECIAL_FEATURE_GROUP` | BUTTON | 35,385 32x33 | **51,401** | no | Mine |
| `SWITCH_FEATURE_TILE_DANSE` | `CONSTRUCTION_KIT_SPECIAL_FEATURE_GROUP` | BUTTON | 67,385 32x33 | **83,401** | no | Disco |
| `SWITCH_FEATURE_TILE_SLEEP` | `CONSTRUCTION_KIT_SPECIAL_FEATURE_GROUP` | BUTTON | 99,385 32x33 | **115,401** | no | Sleep |
| `SWITCH_FEATURE_OBSTACLE_BEAM3` | `CONSTRUCTION_KIT_OBSTACLE_FEATURE_GROUP` | BUTTON | 35,387 32x33 | **51,403** | no | Large Barrier |
| `SWITCH_FEATURE_TILE_HEAL` | `CONSTRUCTION_KIT_BONUS_FEATURE_GROUP` | BUTTON | 35,387 32x33 | **51,403** | no | Heal |
| `SWITCH_FEATURE_OBSTACLE_BUSSCHOOL` | `CONSTRUCTION_KIT_OBSTACLE_FEATURE_GROUP` | BUTTON | 67,387 32x33 | **83,403** | no | Schoolbus |
| `SWITCH_FEATURE_TILE_SHIELD` | `CONSTRUCTION_KIT_BONUS_FEATURE_GROUP` | BUTTON | 67,387 32x33 | **83,403** | no | Armour |
| `SWITCH_FEATURE_TILE_FIRE` | `CONSTRUCTION_KIT_OBSTACLE_FEATURE_GROUP` | BUTTON | 99,387 32x33 | **115,403** | no | Wall of Fire |
| `SWITCH_FEATURE_TILE_QUAD_DAMAGE` | `CONSTRUCTION_KIT_BONUS_FEATURE_GROUP` | BUTTON | 99,387 32x33 | **115,403** | no | Super Damage |
| `SWITCH_FEATURE_WAYPOINT_TEAM3` | `CONSTRUCTION_KIT_WAYPOINT_FEATURE_GROUP` | BUTTON | 50,388 32x33 | **66,404** | no | Marker Team Green |
| `SWITCH_FEATURE_WAYPOINT_TEAM4` | `CONSTRUCTION_KIT_WAYPOINT_FEATURE_GROUP` | BUTTON | 82,388 32x33 | **98,404** | no | Marker Team Yellow |
| `SWITCH_BAIT_DEVORAGE` | `CONSTRUCTION_KIT_BAIT_FEATURE_GROUP` | BUTTON | 99,390 28x28 | **113,404** | no | Devour |
| `SWITCH_BAIT_CITROUILLE` | `CONSTRUCTION_KIT_BAIT_FEATURE_GROUP` | BUTTON | 38,402 28x28 | **52,416** | no | Pumpkin |
| `SWITCH_BAIT_PIG` | `CONSTRUCTION_KIT_BAIT_FEATURE_GROUP` | BUTTON | 67,402 28x28 | **81,416** | no | Pig |
| `SWITCH_FEATURE_OBSTACLE_BEAM4` | `CONSTRUCTION_KIT_OBSTACLE_FEATURE_GROUP` | BUTTON | 35,420 32x33 | **51,436** | no | Enormous Barrier |
| `SWITCH_FEATURE_TILE_INVINCIBILITY` | `CONSTRUCTION_KIT_BONUS_FEATURE_GROUP` | BUTTON | 35,420 32x33 | **51,436** | no | Invincibility |
| `SWITCH_FEATURE_OBSTACLE_BUSLONDON` | `CONSTRUCTION_KIT_OBSTACLE_FEATURE_GROUP` | BUTTON | 67,420 32x33 | **83,436** | no | London Bus |
| `SWITCH_FEATURE_TILE_INVISIBILITY` | `CONSTRUCTION_KIT_BONUS_FEATURE_GROUP` | BUTTON | 67,420 32x33 | **83,436** | no | Invisibility |
| `SWITCH_FEATURE_TILE_ROCK` | `CONSTRUCTION_KIT_OBSTACLE_FEATURE_GROUP` | BUTTON | 99,420 32x33 | **115,436** | no | Rock |
| `SWITCH_FEATURE_TILE_TELEPORT` | `CONSTRUCTION_KIT_SPECIAL_FEATURE_GROUP` | BUTTON | 67,421 32x33 | **83,437** | no | Teleporter |
| `SWITCH_FEATURE_OBSTACLE_STONEWALL` | `CONSTRUCTION_KIT_WAYPOINT_FEATURE_GROUP` | BUTTON | 66,425 32x33 | **82,441** | no | Brick Wall |
| `SWITCH_BAIT_HAY` | `CONSTRUCTION_KIT_BAIT_FEATURE_GROUP` | BUTTON | 38,430 28x28 | **52,444** | no | Hay |
| `SWITCH_BAIT_COW` | `CONSTRUCTION_KIT_BAIT_FEATURE_GROUP` | BUTTON | 67,430 28x28 | **81,444** | no | Cow |
| `CONSTRUCTION_KIT_TEST_ICON` | `CONSTRUCTION_KIT_TEST_BUTTON` | BUTTON | 39,513 28x28 | **53,527** | yes | Test |
| `CONSTRUCTION_KIT_STATS_ICON` | `CONSTRUCTION_KIT_TEST_BUTTON` | BUTTON | 70,513 28x28 | **84,527** | yes | Statistics Panel |
| `CONSTRUCTION_KIT_LIST_ICON` | `CONSTRUCTION_KIT_TEST_BUTTON` | BUTTON | 100,513 28x28 | **114,527** | yes | Show Calendar |

### Fence construction — `park-construction-fence`

Fence name box plus lunch-box and observation-area switches.

Open path: `OPEN_CONSTRUCTION` (764,135) -> `OPEN_CONSTRUCTION_FENCE` (618,145)

Roots: `FENCE_CONSTRUCTION_AND_INFO_PANEL`. Controls: 3.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `FENCE_NAME_INDICS` | `FENCE_NAME_PANEL` | EDIT | 557,15 128x14 | **621,22** | yes | Name |
| `SWITCH_FENCE_LUNCHBOX` | `FENCE_FEATURE_GROUP` | BUTTON | 608,231 25x25 | **620,243** | yes | Feeding Dish |
| `SWITCH_OBSERVATION_AREA` | `FENCE_SEATS_GROUP` | BUTTON | 608,284 25x25 | **620,296** | yes | Observation Area |

### Ready-made show list — `park-construction-show`

Seven-column list of pre-built attractions; Validate/Cancel at the bottom.

Open path: `OPEN_CONSTRUCTION` (764,135) -> `OPEN_CONSTRUCTION_ATTRACTION_PREDEF` (678,145)

Roots: `PREDEFINED_SHOW_PANEL`. Controls: 4.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `DUMMY_PREDEFINED_SHOW_LIST` | `PREDEFINED_SHOW_PANEL` | LIST | 183,69 367x365 | **366,251** | no |  |
| `PREDEFINED_SHOW_LIST` | `PREDEFINED_SHOW_PANEL` | LIST | 183,69 367x365 | **366,251** | yes |  |
| `PREDEFINED_SHOW_VALIDATE` | `PREDEFINED_SHOW_OUTCOME_PANEL` | BUTTON | 340,460 28x28 | **354,474** | yes |  |
| `PREDEFINED_SHOW_CANCEL` | `PREDEFINED_SHOW_OUTCOME_PANEL` | BUTTON | 370,460 28x28 | **384,474** | yes |  |

`DUMMY_PREDEFINED_SHOW_LIST` rows (row height 28 px):

| Row | Click | Label |
| --- | --- | --- |
| 0 | **366,83** |  |
| 1 | **366,111** |  |
| 2 | **366,139** |  |
| 3 | **366,167** |  |
| 4 | **366,195** |  |
| 5 | **366,223** |  |
| 6 | **366,251** |  |
| 7 | **366,279** |  |
| 8 | **366,307** |  |
| 9 | **366,335** |  |
| 10 | **366,363** |  |
| 11 | **366,391** |  |
| 12 | **366,419** |  |

Scrollbar width 30 px: up **535,80**, down **535,423**.

`PREDEFINED_SHOW_LIST` rows (row height 30 px):

| Row | Click | Label |
| --- | --- | --- |
| 0 | **366,84** |  |
| 1 | **366,114** |  |
| 2 | **366,144** |  |
| 3 | **366,174** |  |
| 4 | **366,204** |  |
| 5 | **366,234** |  |
| 6 | **366,264** |  |
| 7 | **366,294** |  |
| 8 | **366,324** |  |
| 9 | **366,354** |  |
| 10 | **366,384** |  |
| 11 | **366,414** |  |

Scrollbar width 15 px: up **543,80**, down **543,423**.

### Road / metro construction — `park-construction-road`

Road and metro mode switches plus the farm building; close with PUTBACK_PANEL_BUTTON.

Open path: `OPEN_TRANSPORT` (765,248)

Roots: `CONSTRUCTION_ROAD_METRO_PANEL`. Controls: 4.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `CONSTRUC_ROAD_BUTTON` | `CONSTRUCTION_ROAD_METRO_PANEL` | BUTTON | 631,72 28x28 | **645,86** | yes | Road |
| `CONSTRUC_FARM_BUTTON` | `CONSTRUCTION_ROAD_METRO_PANEL` | BUTTON | 625,144 39x39 | **644,163** | yes | Farm |
| `CONSTRUC_METRO_BUTTON` | `CONSTRUCTION_ROAD_METRO_PANEL` | BUTTON | 631,227 28x28 | **645,241** | yes | Portal |
| `PUTBACK_PANEL_BUTTON` | `CONSTRUCTION_ROAD_METRO_PANEL` | BUTTON | 667,266 24x24 | **679,278** | yes | Close |

### Buildings and shops — `park-construction-buildings`

Six category tabs. Each category shows five product buttons at the same five y positions; only the active category is clickable.

Open path: `OPEN_BUILDING` (765,195)

Roots: `CONSTRUCTION_BUILDINGS_PANEL`. Controls: 37.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `CONSTRUCTION_BARAQUES_BUTTON` | `CONSTRUCTION_BUILDINGS_MENU_PANEL` | BUTTON | 538,14 28x28 | **552,28** | yes | Stands |
| `CONSTRUCTION_BUILDINGS_BUTTON` | `CONSTRUCTION_BUILDINGS_MENU_PANEL` | BUTTON | 568,14 28x28 | **582,28** | yes | Restaurants |
| `CONSTRUCTION_SOUVENIRS_BUTTON` | `CONSTRUCTION_BUILDINGS_MENU_PANEL` | BUTTON | 598,14 28x28 | **612,28** | yes | Gift Shops |
| `CONSTRUCTION_BLACKBOX_BUTTON` | `CONSTRUCTION_BUILDINGS_MENU_PANEL` | BUTTON | 628,14 28x28 | **642,28** | yes | Attractions |
| `CONSTRUCTION_FOUNTAIN_BUTTON` | `CONSTRUCTION_BUILDINGS_MENU_PANEL` | BUTTON | 568,61 28x28 | **582,75** | yes | Parks |
| `CONSTRUCTION_TREEDECO_BUTTON` | `CONSTRUCTION_BUILDINGS_MENU_PANEL` | BUTTON | 600,61 28x28 | **614,75** | yes | Decorations |
| `CONSTRUC_BARAQUES1_BUTTON` | `CONSTRUC_BARAQUES1_BUTTON_BACK` | BUTTON | 558,124 39x39 | **577,143** | no | "Big Bite Burgers" |
| `CONSTRUC_BLACKBOX1_BUTTON` | `CONSTRUC_BLACKBOX1_BUTTON_BACK` | BUTTON | 558,124 39x39 | **577,143** | no | Haunted House |
| `CONSTRUC_FOUNTAIN1_BUTTON` | `CONSTRUC_FOUNTAIN1_BUTTON_BACK` | BUTTON | 558,124 39x39 | **577,143** | no | Fountain |
| `CONSTRUC_RESTAURANT1_BUTTON` | `CONSTRUC_RESTAURANT1_BUTTON_BACK` | BUTTON | 558,124 39x39 | **577,143** | no | "Sir Loin's Steaks" |
| `CONSTRUC_SOUVENIRS1_BUTTON` | `CONSTRUC_SOUVENIRS1_BUTTON_BACK` | BUTTON | 558,124 39x39 | **577,143** | no | "The Junk Trunk" |
| `CONSTRUC_TREEDECO1_BUTTON` | `CONSTRUC_TREEDECO1_BUTTON_BACK` | BUTTON | 558,124 39x39 | **577,143** | no |  |
| `CONSTRUC_BARAQUES2_BUTTON` | `CONSTRUC_BARAQUES2_BUTTON_BACK` | BUTTON | 558,174 39x39 | **577,193** | no | "Mr. Frosty's" |
| `CONSTRUC_BLACKBOX2_BUTTON` | `CONSTRUC_BLACKBOX2_BUTTON_BACK` | BUTTON | 558,174 39x39 | **577,193** | no | Big Wheel |
| `CONSTRUC_FOUNTAIN2_BUTTON` | `CONSTRUC_FOUNTAIN2_BUTTON_BACK` | BUTTON | 558,174 39x39 | **577,193** | no | Bush |
| `CONSTRUC_RESTAURANT2_BUTTON` | `CONSTRUC_RESTAURANT2_BUTTON_BACK` | BUTTON | 558,174 39x39 | **577,193** | no | "Flavoured Feasts" |
| `CONSTRUC_SOUVENIRS2_BUTTON` | `CONSTRUC_SOUVENIRS2_BUTTON_BACK` | BUTTON | 558,174 39x39 | **577,193** | no | "Planet Nature" |
| `CONSTRUC_TREEDECO2_BUTTON` | `CONSTRUC_TREEDECO2_BUTTON_BACK` | BUTTON | 558,174 39x39 | **577,193** | no |  |
| `CONSTRUC_BARAQUES3_BUTTON` | `CONSTRUC_BARAQUES3_BUTTON_BACK` | BUTTON | 558,224 39x39 | **577,243** | no | "Little Circus" |
| `CONSTRUC_BLACKBOX3_BUTTON` | `CONSTRUC_BLACKBOX3_BUTTON_BACK` | BUTTON | 558,224 39x39 | **577,243** | no | Panoramic View |
| `CONSTRUC_FOUNTAIN3_BUTTON` | `CONSTRUC_FOUNTAIN3_BUTTON_BACK` | BUTTON | 558,224 39x39 | **577,243** | no | Flower Bed |
| `CONSTRUC_RESTAURANT3_BUTTON` | `CONSTRUC_RESTAURANT3_BUTTON_BACK` | BUTTON | 558,224 39x39 | **577,243** | no | "Gallery of Greed" |
| `CONSTRUC_SOUVENIRS3_BUTTON` | `CONSTRUC_SOUVENIRS3_BUTTON_BACK` | BUTTON | 558,224 39x39 | **577,243** | no | "Cave of Wonders" |
| `CONSTRUC_TREEDECO3_BUTTON` | `CONSTRUC_TREEDECO3_BUTTON_BACK` | BUTTON | 558,224 39x39 | **577,243** | no |  |
| `CONSTRUC_BARAQUES4_BUTTON` | `CONSTRUC_BARAQUES4_BUTTON_BACK` | BUTTON | 558,274 39x39 | **577,293** | no | "Caravan of Joy" |
| `CONSTRUC_BLACKBOX4_BUTTON` | `CONSTRUC_BLACKBOX4_BUTTON_BACK` | BUTTON | 558,274 39x39 | **577,293** | no | Roundabout |
| `CONSTRUC_FOUNTAIN4_BUTTON` | `CONSTRUC_FOUNTAIN4_BUTTON_BACK` | BUTTON | 558,274 39x39 | **577,293** | no | Playground |
| `CONSTRUC_RESTAURANT4_BUTTON` | `CONSTRUC_RESTAURANT4_BUTTON_BACK` | BUTTON | 558,274 39x39 | **577,293** | no | "Sweet Sin" |
| `CONSTRUC_SOUVENIRS4_BUTTON` | `CONSTRUC_SOUVENIRS4_BUTTON_BACK` | BUTTON | 558,274 39x39 | **577,293** | no | "Gorgeous Gifts" |
| `CONSTRUC_TREEDECO4_BUTTON` | `CONSTRUC_TREEDECO4_BUTTON_BACK` | BUTTON | 558,274 39x39 | **577,293** | no |  |
| `CONSTRUC_BARAQUES5_BUTTON` | `CONSTRUC_BARAQUES5_BUTTON_BACK` | BUTTON | 558,324 39x39 | **577,343** | no | "Aloha Bar" |
| `CONSTRUC_BLACKBOX5_BUTTON` | `CONSTRUC_BLACKBOX5_BUTTON_BACK` | BUTTON | 558,324 39x39 | **577,343** | no | Dino Dodgems |
| `CONSTRUC_FOUNTAIN5_BUTTON` | `CONSTRUC_FOUNTAIN5_BUTTON_BACK` | BUTTON | 558,324 39x39 | **577,343** | no | Statue |
| `CONSTRUC_RESTAURANT5_BUTTON` | `CONSTRUC_RESTAURANT5_BUTTON_BACK` | BUTTON | 558,324 39x39 | **577,343** | no | "MacDino's" |
| `CONSTRUC_SOUVENIRS5_BUTTON` | `CONSTRUC_SOUVENIRS5_BUTTON_BACK` | BUTTON | 558,324 39x39 | **577,343** | no | "Ultra-Fun Megastore" |
| `CONSTRUC_TREEDECO5_BUTTON` | `CONSTRUC_TREEDECO5_BUTTON_BACK` | BUTTON | 558,324 39x39 | **577,343** | no |  |
| `CONSTRUCTION_BUILDINGS_PUTBACK_BUTTON` | `CONSTRUCTION_BUILDINGS_PANEL` | BUTTON | 620,385 24x24 | **632,397** | yes | Close |

### Dinosaur purchase / save — `park-dino-purchase`

Dinosaur list, description and price. The SAVE name is `DINO_ACHAT_SAVE_OUTCOME_SAVE`; it purchases/saves the selected dinosaur, not a save-game slot.

Open path: `OPEN_PURCHASE` (765,298)

Roots: `DINO_ACHAT_SAVE_PANEL`. Controls: 4.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `DINO_ACHAT_SAVE_NAMES_LIST` | `DINO_ACHAT_SAVE_PANEL` | LIST | 97,70 143x216 | **168,178** | yes |  |
| `DINO_ACHAT_SAVE_TEXT_ZONE_PANEL` | `DINO_ACHAT_SAVE_TEXT_BACKZONE_PANEL` | LIST | 168,312 264x158 | **300,391** | yes |  |
| `DINO_ACHAT_SAVE_OUTCOME_SAVE` | `DINO_ACHAT_SAVE_OUTCOME_PANEL` | BUTTON | 457,507 28x28 | **471,521** | yes | Confirm |
| `DINO_ACHAT_SAVE_OUTCOME_CANCEL` | `DINO_ACHAT_SAVE_OUTCOME_PANEL` | BUTTON | 488,507 28x28 | **502,521** | yes | Close |

`DINO_ACHAT_SAVE_NAMES_LIST` rows (row height 12 px):

| Row | Click | Label |
| --- | --- | --- |
| 0 | **168,76** |  |
| 1 | **168,88** |  |
| 2 | **168,100** |  |
| 3 | **168,112** |  |
| 4 | **168,124** |  |
| 5 | **168,136** |  |
| 6 | **168,148** |  |
| 7 | **168,160** |  |
| 8 | **168,172** |  |
| 9 | **168,184** |  |
| 10 | **168,196** |  |
| 11 | **168,208** |  |
| 12 | **168,220** |  |
| 13 | **168,232** |  |
| 14 | **168,244** |  |
| 15 | **168,256** |  |
| 16 | **168,268** |  |
| 17 | **168,280** |  |

Scrollbar width 15 px: up **233,81**, down **233,275**.

`DINO_ACHAT_SAVE_TEXT_ZONE_PANEL` rows (row height 12 px):

| Row | Click | Label |
| --- | --- | --- |
| 0 | **300,318** |  |
| 1 | **300,330** |  |
| 2 | **300,342** |  |
| 3 | **300,354** |  |
| 4 | **300,366** |  |
| 5 | **300,378** |  |
| 6 | **300,390** |  |
| 7 | **300,402** |  |
| 8 | **300,414** |  |
| 9 | **300,426** |  |
| 10 | **300,438** |  |
| 11 | **300,450** |  |
| 12 | **300,462** |  |

Scrollbar width 15 px: up **425,323**, down **425,459**.

### In-park options / save / load — `park-options`

Top row switches between options, save and load. The save tab shows `OPTIONS_SAVE_NAMES_LIST` (12-px rows), the name edit box and Validate/Cancel; those save controls are hidden until the Save tab is selected.

Open path: `OPEN_OPTIONS` (765,348)

Roots: `OPTIONS_PANEL`. Controls: 12.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `OPTIONS_MENU_EXIT_ICON` | `OPTIONS_MENU_EXIT_GAME` | BUTTON | 430,56 35x36 | **447,74** | yes | Quit Game |
| `OPTIONS_MENU_OPTIONS_BUTTON` | `OPTIONS_MENU_PANEL` | BUTTON | 304,59 28x28 | **318,73** | yes | Options |
| `OPTIONS_MENU_SAVE_BUTTON` | `OPTIONS_MENU_PANEL` | BUTTON | 334,59 28x28 | **348,73** | yes | Save Game |
| `OPTIONS_MENU_LOAD_BUTTON` | `OPTIONS_MENU_PANEL` | BUTTON | 364,59 28x28 | **378,73** | yes | Load Game |
| `OPTIONS_SAVE_NAMES_LIST` | `OPTIONS_SAVE_PANEL` | LIST | 282,125 143x216 | **353,233** | no |  |
| `OPTIONS_MUSIC_HS` | `OPTIONS_OPTIONS_PANEL` | HSCROLL | 275,185 150x20 | **350,195** | yes | Music Volume |
| `OPTIONS_BRUITAGE_HS` | `OPTIONS_OPTIONS_PANEL` | HSCROLL | 275,275 150x20 | **350,285** | yes | Sound Effects Volume |
| `OPTIONS_GAMESPEED_HS` | `OPTIONS_OPTIONS_PANEL` | HSCROLL | 275,365 150x20 | **350,375** | yes | Game Speed |
| `OPTIONS_NAME_SLOT` | `OPTIONS_NAME_PANEL` | EDIT | 285,375 128x14 | **349,382** | no | File Name |
| `OPTIONS_OPTIONS_VALIDATE_BUTTON` | `OPTIONS_OPTIONS_OUTCOME_PANEL` | BUTTON | 341,418 28x28 | **355,432** | yes | Close |
| `OPTIONS_VALIDATE_BUTTON` | `OPTIONS_OUTCOME_PANEL` | BUTTON | 320,422 28x28 | **334,436** | no | Confirm |
| `OPTIONS_UNVALIDATE_BUTTON` | `OPTIONS_OUTCOME_PANEL` | BUTTON | 350,422 28x28 | **364,436** | no | Cancel |

`OPTIONS_SAVE_NAMES_LIST` rows (row height 12 px):

| Row | Click | Label |
| --- | --- | --- |
| 0 | **353,131** |  |
| 1 | **353,143** |  |
| 2 | **353,155** |  |
| 3 | **353,167** |  |
| 4 | **353,179** |  |
| 5 | **353,191** |  |
| 6 | **353,203** |  |
| 7 | **353,215** |  |
| 8 | **353,227** |  |
| 9 | **353,239** |  |
| 10 | **353,251** |  |
| 11 | **353,263** |  |
| 12 | **353,275** |  |
| 13 | **353,287** |  |
| 14 | **353,299** |  |
| 15 | **353,311** |  |
| 16 | **353,323** |  |
| 17 | **353,335** |  |

Scrollbar width 15 px: up **418,136**, down **418,330**.

### Mission / objectives — `park-mission`

Opened by the campaign objective light, not by a HUD button. Two scrolling text zones and an OK button.

Open path: `OBJECTIF_PANEL_LIGHTS_GROUP` (12,50)

Roots: `MISSION_PANEL`. Controls: 3.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `MISSION_PANEL_EXPLICATION` | `MISSION_PANEL` | LIST | 122,67 410x100 | **327,117** | yes |  |
| `MISSION_PANEL_OUTCOME_OK` | `MISSION_PANEL_OUTCOME_PANEL` | BUTTON | 306,393 28x28 | **320,407** | yes | Close |
| `MISSION_PANEL_TIPS` | `MISSION_PANEL` | LIST | 125,437 410x100 | **330,487** | yes |  |

`MISSION_PANEL_EXPLICATION` rows (row height 15 px):

| Row | Click | Label |
| --- | --- | --- |
| 0 | **327,74** |  |
| 1 | **327,89** |  |
| 2 | **327,104** |  |
| 3 | **327,119** |  |
| 4 | **327,134** |  |
| 5 | **327,149** |  |

Scrollbar width 15 px: up **525,78**, down **525,156**.

`MISSION_PANEL_TIPS` rows (row height 15 px):

| Row | Click | Label |
| --- | --- | --- |
| 0 | **330,444** |  |
| 1 | **330,459** |  |
| 2 | **330,474** |  |
| 3 | **330,489** |  |
| 4 | **330,504** |  |
| 5 | **330,519** |  |

Scrollbar width 15 px: up **528,448**, down **528,526**.

### Dinosaur list — `park-dino-list`

Five 64x64 snapshot slots per page, 85 px apart, with sort buttons on the left and page scroll on the right.

Open path: `OPEN_DINO_LIST_PANEL` (302,584)

Roots: `DINO_LIST_PANEL`. Controls: 11.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `DINO_SORT1` | `DINO_SORT_PANEL` | BUTTON | 125,467 25x25 | **137,479** | yes | Sort by Name |
| `DINO_SNAPSHOT_SLOT1` | `DINO_BACKGROUND_SLOT1` | BUTTON | 176,475 64x64 | **208,507** | yes |  |
| `DINO_SNAPSHOT_SLOT2` | `DINO_BACKGROUND_SLOT2` | BUTTON | 261,475 64x64 | **293,507** | yes |  |
| `DINO_SNAPSHOT_SLOT3` | `DINO_BACKGROUND_SLOT3` | BUTTON | 346,475 64x64 | **378,507** | yes |  |
| `DINO_SNAPSHOT_SLOT4` | `DINO_BACKGROUND_SLOT4` | BUTTON | 431,475 64x64 | **463,507** | yes |  |
| `DINO_SNAPSHOT_SLOT5` | `DINO_BACKGROUND_SLOT5` | BUTTON | 516,475 64x64 | **548,507** | yes |  |
| `DINO_SCROLL_UP` | `DINO_LIST_PANEL` | BUTTON | 597,480 19x18 | **606,489** | yes |  |
| `DINO_SORT3` | `DINO_SORT_PANEL` | BUTTON | 125,492 25x25 | **137,504** | yes | Sort by Size |
| `DINO_SORT5` | `DINO_SORT_PANEL` | BUTTON | 125,517 25x25 | **137,529** | yes | Sort by Food Category |
| `DINO_SCROLL_DOWN` | `DINO_LIST_PANEL` | BUTTON | 597,538 19x18 | **606,547** | yes |  |
| `DINO_SORT7` | `DINO_SORT_PANEL` | BUTTON | 125,542 25x25 | **137,554** | yes | Sort by Needs |

### Show / attraction list — `park-show-list`

Ten rows, 29 px apart. Each row has selection light, permutation, name, lock/unlock and iteration buttons.

Open path: `OPEN_SHOW_LIST_PANEL` (397,584)

Roots: `ATTRACTION_LIST_PANEL`. Controls: 73.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `ATTRACTION_LIST_PERMUTATION1` | `ATTRACTION_LIST_PERMUTATION_GROUP` | BUTTON | 173,260 28x28 | **187,274** | yes | Change Schedule |
| `ATTRACTION_LIST_LOCK1` | `ATTRACTION_LIST_LOCKUNLOCK_GROUP1` | BUTTON | 340,260 28x28 | **354,274** | yes | Close to the Public |
| `ATTRACTION_LIST_UNLOCK1` | `ATTRACTION_LIST_LOCKUNLOCK_GROUP1` | BUTTON | 368,260 28x28 | **382,274** | yes | Open to the Public |
| `ATTRACTION_LIST_ITERATION_UP1` | `ATTRACTION_LIST_ITERATION_GROUP` | BUTTON | 402,261 21x13 | **412,267** | yes | Increase Number of Rounds |
| `ATTRACTION_LIST_SELECTION_LIGHT1` | `ATTRACTION_LIST_SELECTION_LIGHT_GROUP` | BUTTON | 89,264 20x20 | **99,274** | yes | Select Show |
| `ATTRACTION_LIST_NAME1` | `ATTRACTION_LIST_NAMES_GROUP` | EDIT | 208,265 126x18 | **271,274** | yes | 10:00 AM / Name of Show |
| `ATTRACTION_LIST_ITERATION_DOWN1` | `ATTRACTION_LIST_ITERATION_GROUP` | BUTTON | 402,274 21x13 | **412,280** | yes | Decrease Number of Rounds |
| `ATTRACTION_LIST_PERMUTATION2` | `ATTRACTION_LIST_PERMUTATION_GROUP` | BUTTON | 173,289 28x28 | **187,303** | yes | Change Schedule |
| `ATTRACTION_LIST_LOCK2` | `ATTRACTION_LIST_LOCKUNLOCK_GROUP2` | BUTTON | 340,289 28x28 | **354,303** | yes | Close to the Public |
| `ATTRACTION_LIST_UNLOCK2` | `ATTRACTION_LIST_LOCKUNLOCK_GROUP2` | BUTTON | 368,289 28x28 | **382,303** | yes | Open to the Public |
| `ATTRACTION_LIST_ITERATION_UP2` | `ATTRACTION_LIST_ITERATION_GROUP` | BUTTON | 402,290 21x13 | **412,296** | yes | Increase Number of Rounds |
| `ATTRACTION_LIST_SELECTION_LIGHT2` | `ATTRACTION_LIST_SELECTION_LIGHT_GROUP` | BUTTON | 89,293 20x20 | **99,303** | yes | Select Show |
| `ATTRACTION_LIST_NAME2` | `ATTRACTION_LIST_NAMES_GROUP` | EDIT | 208,294 126x18 | **271,303** | yes | 10:00 AM / Name of Show |
| `ATTRACTION_LIST_ITERATION_DOWN2` | `ATTRACTION_LIST_ITERATION_GROUP` | BUTTON | 402,303 21x13 | **412,309** | yes | Decrease Number of Rounds |
| `ATTRACTION_LIST_PERMUTATION3` | `ATTRACTION_LIST_PERMUTATION_GROUP` | BUTTON | 173,318 28x28 | **187,332** | yes | Change Schedule |
| `ATTRACTION_LIST_LOCK3` | `ATTRACTION_LIST_LOCKUNLOCK_GROUP3` | BUTTON | 340,318 28x28 | **354,332** | yes | Close to the Public |
| `ATTRACTION_LIST_UNLOCK3` | `ATTRACTION_LIST_LOCKUNLOCK_GROUP3` | BUTTON | 368,318 28x28 | **382,332** | yes | Open to the Public |
| `ATTRACTION_LIST_ITERATION_UP3` | `ATTRACTION_LIST_ITERATION_GROUP` | BUTTON | 402,319 21x13 | **412,325** | yes | Increase Number of Rounds |
| `ATTRACTION_LIST_SELECTION_LIGHT3` | `ATTRACTION_LIST_SELECTION_LIGHT_GROUP` | BUTTON | 89,322 20x20 | **99,332** | yes | Select Show |
| `ATTRACTION_LIST_NAME3` | `ATTRACTION_LIST_NAMES_GROUP` | EDIT | 208,323 126x18 | **271,332** | yes | 10:00 AM / Name of Show |
| `ATTRACTION_LIST_ITERATION_DOWN3` | `ATTRACTION_LIST_ITERATION_GROUP` | BUTTON | 402,332 21x13 | **412,338** | yes | Decrease Number of Rounds |
| `ATTRACTION_LIST_OUTCOME_STATS` | `ATTRACTION_LIST_RIGHT_MENU_PANEL` | BUTTON | 529,333 28x28 | **543,347** | yes | Statistics Panel |
| `ATTRACTION_LIST_PERMUTATION4` | `ATTRACTION_LIST_PERMUTATION_GROUP` | BUTTON | 173,347 28x28 | **187,361** | yes | Change Schedule |
| `ATTRACTION_LIST_LOCK4` | `ATTRACTION_LIST_LOCKUNLOCK_GROUP4` | BUTTON | 340,347 28x28 | **354,361** | yes | Close to the Public |
| `ATTRACTION_LIST_UNLOCK4` | `ATTRACTION_LIST_LOCKUNLOCK_GROUP4` | BUTTON | 368,347 28x28 | **382,361** | yes | Open to the Public |
| `ATTRACTION_LIST_ITERATION_UP4` | `ATTRACTION_LIST_ITERATION_GROUP` | BUTTON | 402,348 21x13 | **412,354** | yes | Increase Number of Rounds |
| `ATTRACTION_LIST_SELECTION_LIGHT4` | `ATTRACTION_LIST_SELECTION_LIGHT_GROUP` | BUTTON | 89,351 20x20 | **99,361** | yes | Select Show |
| `ATTRACTION_LIST_NAME4` | `ATTRACTION_LIST_NAMES_GROUP` | EDIT | 208,352 126x18 | **271,361** | yes | 10:00 AM / Name of Show |
| `ATTRACTION_LIST_ITERATION_DOWN4` | `ATTRACTION_LIST_ITERATION_GROUP` | BUTTON | 402,361 21x13 | **412,367** | yes | Decrease Number of Rounds |
| `ATTRACTION_LIST_OUTCOME_CANCEL` | `ATTRACTION_LIST_RIGHT_MENU_PANEL` | BUTTON | 529,367 28x28 | **543,381** | yes | Close |
| `ATTRACTION_LIST_OUTCOME_CAGE_LIST` | `ATTRACTION_LIST_RIGHT_MENU_PANEL` | BUTTON | 529,373 28x28 | **543,387** | no | Cage Panel |
| `ATTRACTION_LIST_PERMUTATION5` | `ATTRACTION_LIST_PERMUTATION_GROUP` | BUTTON | 173,376 28x28 | **187,390** | yes | Change Schedule |
| `ATTRACTION_LIST_LOCK5` | `ATTRACTION_LIST_LOCKUNLOCK_GROUP5` | BUTTON | 340,376 28x28 | **354,390** | yes | Close to the Public |
| `ATTRACTION_LIST_UNLOCK5` | `ATTRACTION_LIST_LOCKUNLOCK_GROUP5` | BUTTON | 368,376 28x28 | **382,390** | yes | Open to the Public |
| `ATTRACTION_LIST_ITERATION_UP5` | `ATTRACTION_LIST_ITERATION_GROUP` | BUTTON | 402,377 21x13 | **412,383** | yes | Increase Number of Rounds |
| `ATTRACTION_LIST_SELECTION_LIGHT5` | `ATTRACTION_LIST_SELECTION_LIGHT_GROUP` | BUTTON | 89,380 20x20 | **99,390** | yes | Select Show |
| `ATTRACTION_LIST_NAME5` | `ATTRACTION_LIST_NAMES_GROUP` | EDIT | 208,381 126x18 | **271,390** | yes | 10:00 AM / Name of Show |
| `ATTRACTION_LIST_ITERATION_DOWN5` | `ATTRACTION_LIST_ITERATION_GROUP` | BUTTON | 402,390 21x13 | **412,396** | yes | Decrease Number of Rounds |
| `ATTRACTION_LIST_PERMUTATION6` | `ATTRACTION_LIST_PERMUTATION_GROUP` | BUTTON | 173,405 28x28 | **187,419** | yes | Change Schedule |
| `ATTRACTION_LIST_LOCK6` | `ATTRACTION_LIST_LOCKUNLOCK_GROUP6` | BUTTON | 340,405 28x28 | **354,419** | yes | Close to the Public |
| `ATTRACTION_LIST_UNLOCK6` | `ATTRACTION_LIST_LOCKUNLOCK_GROUP6` | BUTTON | 368,405 28x28 | **382,419** | yes | Open to the Public |
| `ATTRACTION_LIST_ITERATION_UP6` | `ATTRACTION_LIST_ITERATION_GROUP` | BUTTON | 402,406 21x13 | **412,412** | yes | Increase Number of Rounds |
| `ATTRACTION_LIST_SELECTION_LIGHT6` | `ATTRACTION_LIST_SELECTION_LIGHT_GROUP` | BUTTON | 89,409 20x20 | **99,419** | yes | Select Show |
| `ATTRACTION_LIST_NAME6` | `ATTRACTION_LIST_NAMES_GROUP` | EDIT | 208,410 126x18 | **271,419** | yes | 10:00 AM / Name of Show |
| `ATTRACTION_LIST_ITERATION_DOWN6` | `ATTRACTION_LIST_ITERATION_GROUP` | BUTTON | 402,419 21x13 | **412,425** | yes | Decrease Number of Rounds |
| `ATTRACTION_LIST_PERMUTATION7` | `ATTRACTION_LIST_PERMUTATION_GROUP` | BUTTON | 173,434 28x28 | **187,448** | yes | Change Schedule |
| `ATTRACTION_LIST_LOCK7` | `ATTRACTION_LIST_LOCKUNLOCK_GROUP7` | BUTTON | 340,434 28x28 | **354,448** | yes | Close to the Public |
| `ATTRACTION_LIST_UNLOCK7` | `ATTRACTION_LIST_LOCKUNLOCK_GROUP7` | BUTTON | 368,434 28x28 | **382,448** | yes | Open to the Public |
| `ATTRACTION_LIST_ITERATION_UP7` | `ATTRACTION_LIST_ITERATION_GROUP` | BUTTON | 402,435 21x13 | **412,441** | yes | Increase Number of Rounds |
| `ATTRACTION_LIST_SELECTION_LIGHT7` | `ATTRACTION_LIST_SELECTION_LIGHT_GROUP` | BUTTON | 89,438 20x20 | **99,448** | yes | Select Show |
| `ATTRACTION_LIST_NAME7` | `ATTRACTION_LIST_NAMES_GROUP` | EDIT | 208,439 126x18 | **271,448** | yes | 10:00 AM / Name of Show |
| `ATTRACTION_LIST_ITERATION_DOWN7` | `ATTRACTION_LIST_ITERATION_GROUP` | BUTTON | 402,448 21x13 | **412,454** | yes | Decrease Number of Rounds |
| `ATTRACTION_LIST_PERMUTATION8` | `ATTRACTION_LIST_PERMUTATION_GROUP` | BUTTON | 173,463 28x28 | **187,477** | yes | Change Schedule |
| `ATTRACTION_LIST_LOCK8` | `ATTRACTION_LIST_LOCKUNLOCK_GROUP8` | BUTTON | 340,463 28x28 | **354,477** | yes | Close to the Public |
| `ATTRACTION_LIST_UNLOCK8` | `ATTRACTION_LIST_LOCKUNLOCK_GROUP8` | BUTTON | 368,463 28x28 | **382,477** | yes | Open to the Public |
| `ATTRACTION_LIST_ITERATION_UP8` | `ATTRACTION_LIST_ITERATION_GROUP` | BUTTON | 402,464 21x13 | **412,470** | yes | Increase Number of Rounds |
| `ATTRACTION_LIST_SELECTION_LIGHT8` | `ATTRACTION_LIST_SELECTION_LIGHT_GROUP` | BUTTON | 89,467 20x20 | **99,477** | yes | Select Show |
| `ATTRACTION_LIST_NAME8` | `ATTRACTION_LIST_NAMES_GROUP` | EDIT | 208,468 126x18 | **271,477** | yes | 10:00 AM / Name of Show |
| `ATTRACTION_LIST_ITERATION_DOWN8` | `ATTRACTION_LIST_ITERATION_GROUP` | BUTTON | 402,477 21x13 | **412,483** | yes | Decrease Number of Rounds |
| `ATTRACTION_LIST_PERMUTATION9` | `ATTRACTION_LIST_PERMUTATION_GROUP` | BUTTON | 173,492 28x28 | **187,506** | yes | Change Schedule |
| `ATTRACTION_LIST_LOCK9` | `ATTRACTION_LIST_LOCKUNLOCK_GROUP9` | BUTTON | 340,492 28x28 | **354,506** | yes | Close to the Public |
| `ATTRACTION_LIST_UNLOCK9` | `ATTRACTION_LIST_LOCKUNLOCK_GROUP9` | BUTTON | 368,492 28x28 | **382,506** | yes | Open to the Public |
| `ATTRACTION_LIST_ITERATION_UP9` | `ATTRACTION_LIST_ITERATION_GROUP` | BUTTON | 402,493 21x13 | **412,499** | yes | Increase Number of Rounds |
| `ATTRACTION_LIST_SELECTION_LIGHT9` | `ATTRACTION_LIST_SELECTION_LIGHT_GROUP` | BUTTON | 89,496 20x20 | **99,506** | yes | Select Show |
| `ATTRACTION_LIST_NAME9` | `ATTRACTION_LIST_NAMES_GROUP` | EDIT | 208,497 126x18 | **271,506** | yes | 10:00 AM / Name of Show |
| `ATTRACTION_LIST_ITERATION_DOWN9` | `ATTRACTION_LIST_ITERATION_GROUP` | BUTTON | 402,506 21x13 | **412,512** | yes | Decrease Number of Rounds |
| `ATTRACTION_LIST_PERMUTATION10` | `ATTRACTION_LIST_PERMUTATION_GROUP` | BUTTON | 173,521 28x28 | **187,535** | yes | Change Schedule |
| `ATTRACTION_LIST_LOCK10` | `ATTRACTION_LIST_LOCKUNLOCK_GROUP10` | BUTTON | 340,521 28x28 | **354,535** | yes | Close to the Public |
| `ATTRACTION_LIST_UNLOCK10` | `ATTRACTION_LIST_LOCKUNLOCK_GROUP10` | BUTTON | 368,521 28x28 | **382,535** | yes | Open to the Public |
| `ATTRACTION_LIST_ITERATION_UP10` | `ATTRACTION_LIST_ITERATION_GROUP` | BUTTON | 402,522 21x13 | **412,528** | yes | Increase Number of Rounds |
| `ATTRACTION_LIST_SELECTION_LIGHT10` | `ATTRACTION_LIST_SELECTION_LIGHT_GROUP` | BUTTON | 89,525 20x20 | **99,535** | yes | Select Show |
| `ATTRACTION_LIST_NAME10` | `ATTRACTION_LIST_NAMES_GROUP` | EDIT | 208,526 126x18 | **271,535** | yes | 10:00 AM / Name of Show |
| `ATTRACTION_LIST_ITERATION_DOWN10` | `ATTRACTION_LIST_ITERATION_GROUP` | BUTTON | 402,535 21x13 | **412,541** | yes | Decrease Number of Rounds |

### Paddock / fence list — `park-paddock-list`

Five snapshot slots and page scroll. The HUD paddock button opens this panel, not CAGE_LIST_PANEL.

Open path: `OPEN_PADDOCK_LIST_PANEL` (492,584)

Roots: `FENCE_LIST_PANEL`. Controls: 7.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `FENCE_SNAPSHOT_SLOT1` | `FENCE_LIST_BACKGROUND_SLOT1` | BUTTON | 175,475 64x64 | **207,507** | yes |  |
| `FENCE_SNAPSHOT_SLOT2` | `FENCE_LIST_BACKGROUND_SLOT2` | BUTTON | 260,475 64x64 | **292,507** | yes |  |
| `FENCE_SNAPSHOT_SLOT3` | `FENCE_LIST_BACKGROUND_SLOT3` | BUTTON | 345,475 64x64 | **377,507** | yes |  |
| `FENCE_SNAPSHOT_SLOT4` | `FENCE_LIST_BACKGROUND_SLOT4` | BUTTON | 430,475 64x64 | **462,507** | yes |  |
| `FENCE_SNAPSHOT_SLOT5` | `FENCE_LIST_BACKGROUND_SLOT5` | BUTTON | 515,475 64x64 | **547,507** | yes |  |
| `FENCE_LIST_SCROLL_UP` | `FENCE_LIST_PANEL` | BUTTON | 597,480 19x18 | **606,489** | yes |  |
| `FENCE_LIST_SCROLL_DOWN` | `FENCE_LIST_PANEL` | BUTTON | 597,538 19x18 | **606,547** | yes |  |

### Cage list (legacy) — `park-cage-list`

Never selected by the HUD controller in retail; kept for completeness. `ATTRACTION_LIST_OUTCOME_CAGE_LIST` is invisible.

Roots: `CAGE_LIST_PANEL`. Controls: 10.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `CAGE_LIST_SNAPSHOT_SLOT1` | `CAGE_LIST_BACKGROUND_SLOT1` | BUTTON | 93,475 64x64 | **125,507** | yes |  |
| `CAGE_LIST_SNAPSHOT_SLOT2` | `CAGE_LIST_BACKGROUND_SLOT2` | BUTTON | 178,475 64x64 | **210,507** | yes |  |
| `CAGE_LIST_SNAPSHOT_SLOT3` | `CAGE_LIST_BACKGROUND_SLOT3` | BUTTON | 263,475 64x64 | **295,507** | yes |  |
| `CAGE_LIST_SNAPSHOT_SLOT4` | `CAGE_LIST_BACKGROUND_SLOT4` | BUTTON | 348,475 64x64 | **380,507** | yes |  |
| `CAGE_LIST_SNAPSHOT_SLOT5` | `CAGE_LIST_BACKGROUND_SLOT5` | BUTTON | 433,475 64x64 | **465,507** | yes |  |
| `CAGE_LIST_SCROLL_UP` | `CAGE_LIST_PANEL` | BUTTON | 515,480 19x18 | **524,489** | yes |  |
| `CAGE_LIST_ACTION_ASSIGN` | `CAGE_LIST_PANEL` | BUTTON | 540,489 28x28 | **554,503** | yes |  |
| `CAGE_LIST_ACTION_UNASSIGN` | `CAGE_LIST_PANEL` | BUTTON | 570,489 28x28 | **584,503** | yes |  |
| `CAGE_LIST_ACTION_DOWN` | `CAGE_LIST_PANEL` | BUTTON | 570,519 28x28 | **584,533** | yes |  |
| `CAGE_LIST_SCROLL_DOWN` | `CAGE_LIST_PANEL` | BUTTON | 515,538 19x18 | **524,547** | yes |  |

### Food distribution — `park-food`

Opened by selecting a feeder/food dispenser in the park; not a HUD button and not ported yet. Vegetarian/carnivore choices and ten 15-px time slots.

Roots: `FOOD_DISTRIBUTION_PANEL`. Controls: 20.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `FOOD_DISTRIBUTION_VEGETARIAN_FLOWER_BUTTON` | `FOOD_DISTRIBUTION_TYPE_GROUP` | BUTTON | 566,20 28x28 | **580,34** | yes | Flowers |
| `FOOD_DISTRIBUTION_VEGETARIAN_FRUITS_BUTTON` | `FOOD_DISTRIBUTION_TYPE_GROUP` | BUTTON | 595,20 28x28 | **609,34** | yes | Fruit |
| `FOOD_DISTRIBUTION_VEGETARIAN_PASTEQUE_BUTTON` | `FOOD_DISTRIBUTION_TYPE_GROUP` | BUTTON | 624,20 28x28 | **638,34** | yes | Watermelon |
| `FOOD_DISTRIBUTION_VEGETARIAN_CITROUILLE_BUTTON` | `FOOD_DISTRIBUTION_TYPE_GROUP` | BUTTON | 580,48 28x28 | **594,62** | yes | Pumpkin |
| `FOOD_DISTRIBUTION_VEGETARIAN_HAY_BUTTON` | `FOOD_DISTRIBUTION_TYPE_GROUP` | BUTTON | 610,48 28x28 | **624,62** | yes | Hay |
| `FOOD_DISTRIBUTION_CARNIVOROUS_AUTRUCHE_BUTTON` | `FOOD_DISTRIBUTION_TYPE_GROUP` | BUTTON | 566,85 28x28 | **580,99** | yes | Ostrich |
| `FOOD_DISTRIBUTION_CARNIVOROUS_GOAT_BUTTON` | `FOOD_DISTRIBUTION_TYPE_GROUP` | BUTTON | 595,85 28x28 | **609,99** | yes | Goat |
| `FOOD_DISTRIBUTION_CARNIVOROUS_MOUTON_BUTTON` | `FOOD_DISTRIBUTION_TYPE_GROUP` | BUTTON | 624,85 28x28 | **638,99** | yes | Sheep |
| `FOOD_DISTRIBUTION_CARNIVOROUS_PIG_BUTTON` | `FOOD_DISTRIBUTION_TYPE_GROUP` | BUTTON | 580,113 28x28 | **594,127** | yes | Pig |
| `FOOD_DISTRIBUTION_CARNIVOROUS_COW_BUTTON` | `FOOD_DISTRIBUTION_TYPE_GROUP` | BUTTON | 610,113 28x28 | **624,127** | yes | Cow |
| `FOOD_DISTRIBUTION_TIME_BUTTON1` | `FOOD_DISTRIBUTION_TIME_SLOTS_GROUP` | BUTTON | 617,187 28x15 | **631,194** | yes | Feeding Time |
| `FOOD_DISTRIBUTION_TIME_BUTTON2` | `FOOD_DISTRIBUTION_TIME_SLOTS_GROUP` | BUTTON | 617,202 28x15 | **631,209** | yes | Feeding Time |
| `FOOD_DISTRIBUTION_TIME_BUTTON3` | `FOOD_DISTRIBUTION_TIME_SLOTS_GROUP` | BUTTON | 617,217 28x15 | **631,224** | yes | Feeding Time |
| `FOOD_DISTRIBUTION_TIME_BUTTON4` | `FOOD_DISTRIBUTION_TIME_SLOTS_GROUP` | BUTTON | 617,232 28x15 | **631,239** | yes | Feeding Time |
| `FOOD_DISTRIBUTION_TIME_BUTTON5` | `FOOD_DISTRIBUTION_TIME_SLOTS_GROUP` | BUTTON | 617,247 28x15 | **631,254** | yes | Feeding Time |
| `FOOD_DISTRIBUTION_TIME_BUTTON6` | `FOOD_DISTRIBUTION_TIME_SLOTS_GROUP` | BUTTON | 617,262 28x15 | **631,269** | yes | Feeding Time |
| `FOOD_DISTRIBUTION_TIME_BUTTON7` | `FOOD_DISTRIBUTION_TIME_SLOTS_GROUP` | BUTTON | 617,277 28x15 | **631,284** | yes | Feeding Time |
| `FOOD_DISTRIBUTION_TIME_BUTTON8` | `FOOD_DISTRIBUTION_TIME_SLOTS_GROUP` | BUTTON | 617,292 28x15 | **631,299** | yes | Feeding Time |
| `FOOD_DISTRIBUTION_TIME_BUTTON9` | `FOOD_DISTRIBUTION_TIME_SLOTS_GROUP` | BUTTON | 617,307 28x15 | **631,314** | yes | Feeding Time |
| `FOOD_DISTRIBUTION_TIME_BUTTON10` | `FOOD_DISTRIBUTION_TIME_SLOTS_GROUP` | BUTTON | 617,322 28x15 | **631,329** | yes | Feeding Time |

### Attraction stats — `park-attraction-stats`

Also reachable from `ATTRACTION_LIST_OUTCOME_STATS`. Close and open-show-list buttons on the right.

Open path: `OPEN_CONSTRUCTION` (764,135) -> `OPEN_CONSTRUCTION_KIT` (648,145) -> `CONSTRUCTION_KIT_STATS_ICON` (84,527)

Roots: `ATTRACTION_STATS_PANEL`. Controls: 3.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `ATTRACTION_STATS_ATTRACTION_NAME` | `ATTRACTION_STATS_ATTRACTION_NAME_PANEL` | EDIT | 172,199 128x14 | **236,206** | yes | 11:00 / Name of Show |
| `ATTRACTION_STATS_OUTCOME_LIST` | `ATTRACTION_STATS_RIGHT_MENU_PANEL` | BUTTON | 474,327 28x28 | **488,341** | yes | Show Calendar |
| `ATTRACTION_STATS_OUTCOME_CANCEL` | `ATTRACTION_STATS_RIGHT_MENU_PANEL` | BUTTON | 474,361 28x28 | **488,375** | yes | Close |

### Dinosaur info / portrait — `park-dino-info`

Selection-driven: click a dinosaur in the park. The portrait root overlaps the top of the info strip; its teleport and next-dinosaur buttons are there.

Roots: `DINO_INFO_PANEL`, `DINO_INFO_DINO_PIC`. Controls: 7.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `DINO_INFO_TELEPORTATION_ICON` | `DINO_INFO_TELEPORTATION_PANEL` | BUTTON | 128,16 28x28 | **142,30** | yes | Change Enclosure |
| `DINO_INFO_DINO_SELECTION_ICON` | `DINO_INFO_DINO_SELECTION_PANEL` | BUTTON | 130,64 28x28 | **144,78** | yes | Next Dinosaur |
| `DINO_NAME_SLOT` | `DINO_INFO_DINO_PICANDNAME_PANEL` | EDIT | 14,79 64x16 | **46,87** | yes | Name |
| `DINO_INFO_VAL_CALCU_BUTTON` | `DINO_INFO_MAIN_MENU` | BUTTON | 30,117 28x28 | **44,131** | yes | Attributes |
| `DINO_INFO_BESOINS_BUTTON` | `DINO_INFO_MAIN_MENU` | BUTTON | 60,117 28x28 | **74,131** | yes | Needs |
| `DINO_INFO_FOOD_PREF_BUTTON` | `DINO_INFO_MAIN_MENU` | BUTTON | 30,147 28x28 | **44,161** | yes | Visitor Appreciation |
| `DINO_INFO_CALENDAR_BUTTON` | `DINO_INFO_MAIN_MENU` | BUTTON | 60,147 28x28 | **74,161** | yes | Calendar |

### Budget ledger — `park-budget`

Display-only ledger with a single close button.

Open path: `OPEN_BUDGET_PANEL` (97,584)

Roots: `BUDGET_PANEL`. Controls: 1.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `BUDGET_OUTCOME_OK` | `BUDGET_OUTCOME_BUTTON` | BUTTON | 415,468 28x28 | **429,482** | yes | Close |

### Visitor statistics — `park-visitors`

Display-only statistics with a single close button.

Open path: `OPEN_VISITOR_PANEL` (207,584)

Roots: `VISITEURS_STATS_PANEL`. Controls: 1.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `VISITEURS_STATS_OUTCOME_OK` | `VISITEURS_STATS_OUTCOME_PANEL` | BUTTON | 181,386 28x28 | **195,400** | yes | Close |

### Cage / paddock info — `park-cage-info`

Selection-driven: click a cage or paddock. Assign/unassign share one position; team switches are 32x33.

Roots: `CAGE_INFO_PANEL`. Controls: 6.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `CAGE_INFO_ACTION_ASSIGN` | `CAGE_INFO_PANEL` | BUTTON | 605,162 28x28 | **619,176** | yes | Assign Dino to Cage |
| `CAGE_INFO_ACTION_UNASSIGN` | `CAGE_INFO_PANEL` | BUTTON | 605,162 28x28 | **619,176** | yes | Free Dino |
| `SWITCH_CAGE_TEAM1` | `CAGE_TEAM_GROUP` | BUTTON | 601,210 32x33 | **617,226** | yes | Team Red |
| `SWITCH_CAGE_TEAM2` | `CAGE_TEAM_GROUP` | BUTTON | 638,210 32x33 | **654,226** | yes | Team Blue |
| `SWITCH_CAGE_TEAM3` | `CAGE_TEAM_GROUP` | BUTTON | 601,247 32x33 | **617,263** | yes | Team Green |
| `SWITCH_CAGE_TEAM4` | `CAGE_TEAM_GROUP` | BUTTON | 638,247 32x33 | **654,263** | yes | Team Yellow |

### Building info — `park-building-info`

Selection-driven and display-only: no interactive widgets.

Roots: `BATIMENTS_INFO_PANEL`. Controls: 0.

_No interactive controls._

### Security staff panel — `park-staff-security`

Selection/context panel; not wired to a HUD opener in the port. Two 13-px up/down pairs.

Roots: `EFFECTIFS_SECU_PANEL`. Controls: 4.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `EFFECTIFS_SECU_EFFECTIFS_UP1` | `EFFECTIFS_SECU_PANEL` | BUTTON | 563,103 21x13 | **573,109** | yes |  |
| `EFFECTIFS_SECU_EFFECTIFS_DOWN1` | `EFFECTIFS_SECU_PANEL` | BUTTON | 563,116 21x13 | **573,122** | yes |  |
| `EFFECTIFS_SECU_LOCKS_UP2` | `EFFECTIFS_SECU_PANEL` | BUTTON | 563,144 21x13 | **573,150** | yes |  |
| `EFFECTIFS_SECU_LOCKS_DOWN2` | `EFFECTIFS_SECU_PANEL` | BUTTON | 563,157 21x13 | **573,163** | yes |  |

### Food staff panel — `park-staff-food`

Selection/context panel; hire/fire feeder up/down buttons.

Roots: `EFFECTIFS_NUTRI_PANEL`. Controls: 2.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `EFFECTIFS_NUTRI_EFFECTIFS_UP1` | `EFFECTIFS_NUTRI_PANEL` | BUTTON | 602,99 21x13 | **612,105** | yes | Hire Feeder |
| `EFFECTIFS_NUTRI_EFFECTIFS_DOWN1` | `EFFECTIFS_NUTRI_PANEL` | BUTTON | 602,112 21x13 | **612,118** | yes | Fire Feeder |

### Dinosaur placement marker — `park-dino-placement`

1x1 placement marker; no interactive widgets.

Roots: `DINO_LIST_PARK_PLACEMENT`. Controls: 0.

_No interactive controls._

## Laboratory centres

### Laboratory main — `park-lab-main`

Four lab centres (breed, modify, clone, fridge) and Exit. The two patient lights are selection indicators.

Open path: `OPEN_LABORATORY` (757,46)

Roots: `DINO_LAB_MAIN_CENTER`. Controls: 8.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `RIGHT_ATTRIBUTES_SPEED_INDICS` | `RIGHT_ATTRIBUTES_SPEED_GAUGE_PANEL` | EDIT | 727,111 32x10 | **743,116** | yes |  |
| `RIGHT_PATIENT_LIGHT` | `LAB_MAIN_PANEL` | BUTTON | 436,251 30x30 | **451,266** | yes |  |
| `LEFT_PATIENT_LIGHT` | `LAB_MAIN_PANEL` | BUTTON | 335,252 30x30 | **350,267** | yes |  |
| `SELECT_LAB_BREED` | `LAB_MENU_MODE_GROUP` | BUTTON | 244,495 35x36 | **261,513** | yes | Cross Two Dinos |
| `SELECT_LAB_MODIFY` | `LAB_MENU_MODE_GROUP` | BUTTON | 294,495 35x36 | **311,513** | yes | &Labomodify |
| `SELECT_LAB_CLONE` | `LAB_MENU_MODE_GROUP` | BUTTON | 344,495 35x36 | **361,513** | yes | Clone Dino |
| `CANCEL_LAB_MENU` | `CANCEL_LAB_MENU_BACK` | BUTTON | 515,495 35x36 | **532,513** | yes | Exit Lab |
| `SELECT_LAB_FRIDGE` | `LAB_MENU_BACK_PANEL` | BUTTON | 436,496 35x36 | **453,514** | yes | Open the Cooler |

### Laboratory: breed — `park-lab-breed`

Four gene tabs (physical, adornment, texture, general). Only one gene group is visible at a time; the variation buttons in the other groups are inactive until their tab is selected.

Open path: `OPEN_LABORATORY` (757,46) -> `SELECT_LAB_BREED` (261,513)

Roots: `LAB_BREED_CENTER`. Controls: 83.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `BREED_SWITCH_PHYSICAL` | `BREED_GENE_SELECTION_PANEL` | BUTTON | 52,41 28x28 | **66,55** | yes | Morphology |
| `BREED_SWITCH_ADORNMENT` | `BREED_GENE_SELECTION_PANEL` | BUTTON | 84,41 28x28 | **98,55** | yes | Distinguishing Features |
| `BREED_SWITCH_TEXTURE` | `BREED_GENE_SELECTION_PANEL` | BUTTON | 116,41 28x28 | **130,55** | yes | Skin & Shell |
| `BREED_SWITCH_GENERAL` | `BREED_GENE_SELECTION_PANEL` | BUTTON | 148,41 28x28 | **162,55** | yes | General |
| `BREED_ADORN_HEAD_PLACEMENT_VARIATION_1` | `BREED_GENE_ADORN_HEAD_PLACEMENT_GROUP` | BUTTON | 64,112 29x22 | **78,123** | no | Variation of Genotype 1 |
| `BREED_DIET_VARIATION_1` | `BREED_GENE_DIET_GROUP` | BUTTON | 64,112 29x22 | **78,123** | no | Variation of Genotype 1 |
| `BREED_HEAD_TYPE_VARIATION_1` | `BREED_GENE_HEAD_TYPE_GROUP` | BUTTON | 64,112 29x22 | **78,123** | no | Variation of Genotype 1 |
| `BREED_SKIN_TYPE_VARIATION_1` | `BREED_GENE_SKIN_TYPE_GROUP` | BUTTON | 64,112 29x22 | **78,123** | yes | Variation of Genotype 1 |
| `BREED_ADORN_HEAD_PLACEMENT_VARIATION_2` | `BREED_GENE_ADORN_HEAD_PLACEMENT_GROUP` | BUTTON | 94,112 29x22 | **108,123** | no | Variation of Genotype 2 |
| `BREED_DIET_VARIATION_2` | `BREED_GENE_DIET_GROUP` | BUTTON | 94,112 29x22 | **108,123** | no | Variation of Genotype 2 |
| `BREED_HEAD_TYPE_VARIATION_2` | `BREED_GENE_HEAD_TYPE_GROUP` | BUTTON | 94,112 29x22 | **108,123** | no | Variation of Genotype 2 |
| `BREED_SKIN_TYPE_VARIATION_2` | `BREED_GENE_SKIN_TYPE_GROUP` | BUTTON | 94,112 29x22 | **108,123** | yes | Variation of Genotype 2 |
| `BREED_ADORN_HEAD_PLACEMENT_VARIATION_3` | `BREED_GENE_ADORN_HEAD_PLACEMENT_GROUP` | BUTTON | 124,112 29x22 | **138,123** | no | Variation of Genotype 3 |
| `BREED_DIET_VARIATION_3` | `BREED_GENE_DIET_GROUP` | BUTTON | 124,112 29x22 | **138,123** | no | Variation of Genotype 3 |
| `BREED_HEAD_TYPE_VARIATION_3` | `BREED_GENE_HEAD_TYPE_GROUP` | BUTTON | 124,112 29x22 | **138,123** | no | Variation of Genotype 3 |
| `BREED_SKIN_TYPE_VARIATION_3` | `BREED_GENE_SKIN_TYPE_GROUP` | BUTTON | 124,112 29x22 | **138,123** | yes | Variation of Genotype 3 |
| `BREED_ADORN_HEAD_PLACEMENT_VARIATION_4` | `BREED_GENE_ADORN_HEAD_PLACEMENT_GROUP` | BUTTON | 154,112 29x22 | **168,123** | no | Variation of Genotype 4 |
| `BREED_DIET_VARIATION_4` | `BREED_GENE_DIET_GROUP` | BUTTON | 154,112 29x22 | **168,123** | no | Variation of Genotype 4 |
| `BREED_HEAD_TYPE_VARIATION_4` | `BREED_GENE_HEAD_TYPE_GROUP` | BUTTON | 154,112 29x22 | **168,123** | no | Variation of Genotype 4 |
| `BREED_SKIN_TYPE_VARIATION_4` | `BREED_GENE_SKIN_TYPE_GROUP` | BUTTON | 154,112 29x22 | **168,123** | yes | Variation of Genotype 4 |
| `BREED_ADORN_HEAD_TYPE_VARIATION_1` | `BREED_GENE_ADORN_HEAD_TYPE_GROUP` | BUTTON | 64,154 29x22 | **78,165** | no | Variation of Genotype 1 |
| `BREED_ARMS_TYPE_VARIATION_1` | `BREED_GENE_ARMS_TYPE_GROUP` | BUTTON | 64,154 29x22 | **78,165** | no | Variation of Genotype 1 |
| `BREED_COURAGE_VARIATION_1` | `BREED_GENE_COURAGE_GROUP` | BUTTON | 64,154 29x22 | **78,165** | no | Variation of Genotype 1 |
| `BREED_SKIN_SUBTYPE_VARIATION_1` | `BREED_GENE_SKIN_SUBTYPE_GROUP` | BUTTON | 64,154 29x22 | **78,165** | yes | Variation of Genotype 1 |
| `BREED_ADORN_HEAD_TYPE_VARIATION_2` | `BREED_GENE_ADORN_HEAD_TYPE_GROUP` | BUTTON | 94,154 29x22 | **108,165** | no | Variation of Genotype 2 |
| `BREED_ARMS_TYPE_VARIATION_2` | `BREED_GENE_ARMS_TYPE_GROUP` | BUTTON | 94,154 29x22 | **108,165** | no | Variation of Genotype 2 |
| `BREED_COURAGE_VARIATION_2` | `BREED_GENE_COURAGE_GROUP` | BUTTON | 94,154 29x22 | **108,165** | no | Variation of Genotype 2 |
| `BREED_SKIN_SUBTYPE_VARIATION_2` | `BREED_GENE_SKIN_SUBTYPE_GROUP` | BUTTON | 94,154 29x22 | **108,165** | yes | Variation of Genotype 2 |
| `BREED_ADORN_HEAD_TYPE_VARIATION_3` | `BREED_GENE_ADORN_HEAD_TYPE_GROUP` | BUTTON | 124,154 29x22 | **138,165** | no | Variation of Genotype 3 |
| `BREED_ARMS_TYPE_VARIATION_3` | `BREED_GENE_ARMS_TYPE_GROUP` | BUTTON | 124,154 29x22 | **138,165** | no | Variation of Genotype 3 |
| `BREED_COURAGE_VARIATION_3` | `BREED_GENE_COURAGE_GROUP` | BUTTON | 124,154 29x22 | **138,165** | no | Variation of Genotype 3 |
| `BREED_SKIN_SUBTYPE_VARIATION_3` | `BREED_GENE_SKIN_SUBTYPE_GROUP` | BUTTON | 124,154 29x22 | **138,165** | yes | Variation of Genotype 3 |
| `BREED_ADORN_HEAD_TYPE_VARIATION_4` | `BREED_GENE_ADORN_HEAD_TYPE_GROUP` | BUTTON | 154,154 29x22 | **168,165** | no | Variation of Genotype 4 |
| `BREED_ARMS_TYPE_VARIATION_4` | `BREED_GENE_ARMS_TYPE_GROUP` | BUTTON | 154,154 29x22 | **168,165** | no | Variation of Genotype 4 |
| `BREED_COURAGE_VARIATION_4` | `BREED_GENE_COURAGE_GROUP` | BUTTON | 154,154 29x22 | **168,165** | no | Variation of Genotype 4 |
| `BREED_SKIN_SUBTYPE_VARIATION_4` | `BREED_GENE_SKIN_SUBTYPE_GROUP` | BUTTON | 154,154 29x22 | **168,165** | yes | Variation of Genotype 4 |
| `BREED_ADORN_BODY_PLACEMENT_VARIATION_1` | `BREED_GENE_ADORN_BODY_PLACEMENT_GROUP` | BUTTON | 64,196 29x22 | **78,207** | no | Variation of Genotype 1 |
| `BREED_AGGRESSIVENESS_VARIATION_1` | `BREED_GENE_AGGRESSIVENESS_GROUP` | BUTTON | 64,196 29x22 | **78,207** | no | Variation of Genotype 1 |
| `BREED_BODY_TYPE_VARIATION_1` | `BREED_GENE_BODY_TYPE_GROUP` | BUTTON | 64,196 29x22 | **78,207** | no | Variation of Genotype 1 |
| `BREED_CARAPACE_TYPE_VARIATION_1` | `BREED_GENE_CARAPACE_TYPE_GROUP` | BUTTON | 64,196 29x22 | **78,207** | yes | Variation of Genotype 1 |
| `BREED_ADORN_BODY_PLACEMENT_VARIATION_2` | `BREED_GENE_ADORN_BODY_PLACEMENT_GROUP` | BUTTON | 94,196 29x22 | **108,207** | no | Variation of Genotype 2 |
| `BREED_AGGRESSIVENESS_VARIATION_2` | `BREED_GENE_AGGRESSIVENESS_GROUP` | BUTTON | 94,196 29x22 | **108,207** | no | Variation of Genotype 2 |
| `BREED_BODY_TYPE_VARIATION_2` | `BREED_GENE_BODY_TYPE_GROUP` | BUTTON | 94,196 29x22 | **108,207** | no | Variation of Genotype 2 |
| `BREED_CARAPACE_TYPE_VARIATION_2` | `BREED_GENE_CARAPACE_TYPE_GROUP` | BUTTON | 94,196 29x22 | **108,207** | yes | Variation of Genotype 2 |
| `BREED_ADORN_BODY_PLACEMENT_VARIATION_3` | `BREED_GENE_ADORN_BODY_PLACEMENT_GROUP` | BUTTON | 124,196 29x22 | **138,207** | no | Variation of Genotype 3 |
| `BREED_AGGRESSIVENESS_VARIATION_3` | `BREED_GENE_AGGRESSIVENESS_GROUP` | BUTTON | 124,196 29x22 | **138,207** | no | Variation of Genotype 3 |
| `BREED_BODY_TYPE_VARIATION_3` | `BREED_GENE_BODY_TYPE_GROUP` | BUTTON | 124,196 29x22 | **138,207** | no | Variation of Genotype 3 |
| `BREED_CARAPACE_TYPE_VARIATION_3` | `BREED_GENE_CARAPACE_TYPE_GROUP` | BUTTON | 124,196 29x22 | **138,207** | yes | Variation of Genotype 3 |
| `BREED_ADORN_BODY_PLACEMENT_VARIATION_4` | `BREED_GENE_ADORN_BODY_PLACEMENT_GROUP` | BUTTON | 154,196 29x22 | **168,207** | no | Variation of Genotype 4 |
| `BREED_AGGRESSIVENESS_VARIATION_4` | `BREED_GENE_AGGRESSIVENESS_GROUP` | BUTTON | 154,196 29x22 | **168,207** | no | Variation of Genotype 4 |
| `BREED_BODY_TYPE_VARIATION_4` | `BREED_GENE_BODY_TYPE_GROUP` | BUTTON | 154,196 29x22 | **168,207** | no | Variation of Genotype 4 |
| `BREED_CARAPACE_TYPE_VARIATION_4` | `BREED_GENE_CARAPACE_TYPE_GROUP` | BUTTON | 154,196 29x22 | **168,207** | yes | Variation of Genotype 4 |
| `BREED_ADORN_BODY_TYPE_VARIATION_1` | `BREED_GENE_ADORN_BODY_TYPE_GROUP` | BUTTON | 64,238 29x22 | **78,249** | no | Variation of Genotype 1 |
| `BREED_CARAPACE_SUBTYPE_VARIATION_1` | `BREED_GENE_CARAPACE_SUBTYPE_GROUP` | BUTTON | 64,238 29x22 | **78,249** | yes | Variation of Genotype 1 |
| `BREED_STAMINA_VARIATION_1` | `BREED_GENE_STAMINA_GROUP` | BUTTON | 64,238 29x22 | **78,249** | no | Variation of Genotype 1 |
| `BREED_TAIL_TYPE_VARIATION_1` | `BREED_GENE_TAIL_TYPE_GROUP` | BUTTON | 64,238 29x22 | **78,249** | no | Variation of Genotype 1 |
| `BREED_ADORN_BODY_TYPE_VARIATION_2` | `BREED_GENE_ADORN_BODY_TYPE_GROUP` | BUTTON | 94,238 29x22 | **108,249** | no | Variation of Genotype 2 |
| `BREED_CARAPACE_SUBTYPE_VARIATION_2` | `BREED_GENE_CARAPACE_SUBTYPE_GROUP` | BUTTON | 94,238 29x22 | **108,249** | yes | Variation of Genotype 2 |
| `BREED_STAMINA_VARIATION_2` | `BREED_GENE_STAMINA_GROUP` | BUTTON | 94,238 29x22 | **108,249** | no | Variation of Genotype 2 |
| `BREED_TAIL_TYPE_VARIATION_2` | `BREED_GENE_TAIL_TYPE_GROUP` | BUTTON | 94,238 29x22 | **108,249** | no | Variation of Genotype 2 |
| `BREED_ADORN_BODY_TYPE_VARIATION_3` | `BREED_GENE_ADORN_BODY_TYPE_GROUP` | BUTTON | 124,238 29x22 | **138,249** | no | Variation of Genotype 3 |
| `BREED_CARAPACE_SUBTYPE_VARIATION_3` | `BREED_GENE_CARAPACE_SUBTYPE_GROUP` | BUTTON | 124,238 29x22 | **138,249** | yes | Variation of Genotype 3 |
| `BREED_STAMINA_VARIATION_3` | `BREED_GENE_STAMINA_GROUP` | BUTTON | 124,238 29x22 | **138,249** | no | Variation of Genotype 3 |
| `BREED_TAIL_TYPE_VARIATION_3` | `BREED_GENE_TAIL_TYPE_GROUP` | BUTTON | 124,238 29x22 | **138,249** | no | Variation of Genotype 3 |
| `BREED_ADORN_BODY_TYPE_VARIATION_4` | `BREED_GENE_ADORN_BODY_TYPE_GROUP` | BUTTON | 154,238 29x22 | **168,249** | no | Variation of Genotype 4 |
| `BREED_CARAPACE_SUBTYPE_VARIATION_4` | `BREED_GENE_CARAPACE_SUBTYPE_GROUP` | BUTTON | 154,238 29x22 | **168,249** | yes |  |
| `BREED_STAMINA_VARIATION_4` | `BREED_GENE_STAMINA_GROUP` | BUTTON | 154,238 29x22 | **168,249** | no | Variation of Genotype 4 |
| `BREED_TAIL_TYPE_VARIATION_4` | `BREED_GENE_TAIL_TYPE_GROUP` | BUTTON | 154,238 29x22 | **168,249** | no | Variation of Genotype 4 |
| `BREED_ADORN_TAIL_PLACEMENT_VARIATION_1` | `BREED_GENE_ADORN_TAIL_PLACEMENT_GROUP` | BUTTON | 64,280 29x22 | **78,291** | no | Variation of Genotype 1 |
| `BREED_ADORN_TAIL_PLACEMENT_VARIATION_2` | `BREED_GENE_ADORN_TAIL_PLACEMENT_GROUP` | BUTTON | 94,280 29x22 | **108,291** | no | Variation of Genotype 2 |
| `BREED_ADORN_TAIL_PLACEMENT_VARIATION_3` | `BREED_GENE_ADORN_TAIL_PLACEMENT_GROUP` | BUTTON | 124,280 29x22 | **138,291** | no | Variation of Genotype 3 |
| `BREED_ADORN_TAIL_PLACEMENT_VARIATION_4` | `BREED_GENE_ADORN_TAIL_PLACEMENT_GROUP` | BUTTON | 154,280 29x22 | **168,291** | no | Variation of Genotype 4 |
| `BREED_ADORN_TAIL_TYPE_VARIATION_1` | `BREED_GENE_ADORN_TAIL_TYPE_GROUP` | BUTTON | 64,322 29x22 | **78,333** | no | Variation of Genotype 1 |
| `BREED_ADORN_TAIL_TYPE_VARIATION_2` | `BREED_GENE_ADORN_TAIL_TYPE_GROUP` | BUTTON | 94,322 29x22 | **108,333** | no | Variation of Genotype 2 |
| `BREED_ADORN_TAIL_TYPE_VARIATION_3` | `BREED_GENE_ADORN_TAIL_TYPE_GROUP` | BUTTON | 124,322 29x22 | **138,333** | no | Variation of Genotype 3 |
| `BREED_ADORN_TAIL_TYPE_VARIATION_4` | `BREED_GENE_ADORN_TAIL_TYPE_GROUP` | BUTTON | 154,322 29x22 | **168,333** | no | Variation of Genotype 4 |
| `BREED_GENE_REVERT` | `BREED_GENE_MONITOR_PANEL` | BUTTON | 68,362 24x24 | **80,374** | yes | Cancel Modifications |
| `VALIDATE_BREED` | `VALIDATE_BREED_PANEL` | BUTTON | 395,382 35x36 | **412,400** | yes | Confirm |
| `BREED_EGG_NAME` | `BREED_EGG_NAME_PANEL` | EDIT | 348,510 128x14 | **412,517** | yes | Name |
| `BREED_FENCE` | `BREED_FENCE_PANEL` | BUTTON | 322,547 35x36 | **339,565** | yes | Place in Enclosure |
| `BREED_FRIDGE` | `BREED_FRIDGE_PANEL` | BUTTON | 372,547 35x36 | **389,565** | yes | Store in the Cooler |
| `BREED_BACK` | `BREED_BACK_PANEL` | BUTTON | 422,547 35x36 | **439,565** | yes | Back |
| `BREED_EXIT` | `BREED_EXIT_PANEL` | BUTTON | 472,547 35x36 | **489,565** | yes | Exit Lab |

### Laboratory: modify — `park-lab-modify`

Three beam buttons plus fence/fridge/back/exit.

Open path: `OPEN_LABORATORY` (757,46) -> `SELECT_LAB_MODIFY` (311,513)

Roots: `LAB_MODIFY_CENTER`. Controls: 8.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `MODIFY_BEAM1` | `MODIFY_BEAM1_MIDPART1` | BUTTON | 225,60 35x36 | **242,78** | yes | Mutant Alpha Ray |
| `MODIFY_ATTRIBUTES_SPEED_INDICS` | `MODIFY_ATTRIBUTES_SPEED_GAUGE_PANEL` | EDIT | 727,111 32x10 | **743,116** | yes |  |
| `MODIFY_BEAM2` | `MODIFY_BEAM2_MIDPART1` | BUTTON | 139,180 35x36 | **156,198** | yes | Mutant Delta Ray |
| `MODIFY_BEAM3` | `MODIFY_BEAM3_MIDPART1` | BUTTON | 152,308 35x36 | **169,326** | yes | Mutant Omega Ray |
| `MODIFY_FENCE` | `MODIFY_FENCE_PANEL` | BUTTON | 322,489 35x36 | **339,507** | yes | Place in Enclosure |
| `MODIFY_FRIDGE` | `MODIFY_FRIDGE_PANEL` | BUTTON | 372,489 35x36 | **389,507** | yes | Store in the Cooler |
| `MODIFY_BACK` | `MODIFY_BACK_PANEL` | BUTTON | 422,489 35x36 | **439,507** | yes | Back |
| `MODIFY_EXIT` | `MODIFY_EXIT_PANEL` | BUTTON | 472,489 35x36 | **489,507** | yes | Exit Lab |

### Laboratory: fridge / egg storage — `park-lab-fridge`

Eight 45x75 egg slots in two rows, name edit, Sell/Fence/Back/Exit.

Open path: `OPEN_LABORATORY` (757,46) -> `SELECT_LAB_FRIDGE` (453,514)

Roots: `LAB_FRIDGE_CENTER`. Controls: 14.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `FRIDGE_ATTRIBUTES_SPEED_INDICS` | `FRIDGE_ATTRIBUTES_SPEED_GAUGE_PANEL` | EDIT | 727,111 32x10 | **743,116** | yes |  |
| `FRIDGE_SLOT1` | `FRIDGE_CASE` | BUTTON | 87,145 45x75 | **109,182** | yes | Egg No. 1 |
| `FRIDGE_SLOT2` | `FRIDGE_CASE` | BUTTON | 138,145 45x75 | **160,182** | yes | Egg No. 2 |
| `FRIDGE_SLOT3` | `FRIDGE_CASE` | BUTTON | 189,145 45x75 | **211,182** | yes | Egg No. 3 |
| `FRIDGE_SLOT4` | `FRIDGE_CASE` | BUTTON | 240,145 45x75 | **262,182** | yes | Egg No. 4 |
| `FRIDGE_SLOT5` | `FRIDGE_CASE` | BUTTON | 87,245 45x75 | **109,282** | yes | Egg No. 5 |
| `FRIDGE_SLOT6` | `FRIDGE_CASE` | BUTTON | 138,245 45x75 | **160,282** | yes | Egg No. 6 |
| `FRIDGE_SLOT7` | `FRIDGE_CASE` | BUTTON | 189,245 45x75 | **211,282** | yes | Egg No. 7 |
| `FRIDGE_SLOT8` | `FRIDGE_CASE` | BUTTON | 240,245 45x75 | **262,282** | yes | Egg No. 8 |
| `FRIDGE_EGG_NAME` | `FRIDGE_EGG_NAME_PANEL` | EDIT | 419,374 128x14 | **483,381** | yes |  |
| `FRIDGE_FENCE` | `FRIDGE_FENCE_PANEL` | BUTTON | 322,507 35x36 | **339,525** | yes | Place in Enclosure |
| `FRIDGE_KILL` | `FRIDGE_KILL_PANEL` | BUTTON | 372,507 35x36 | **389,525** | yes | Sell Egg |
| `FRIDGE_BACK` | `FRIDGE_BACK_PANEL` | BUTTON | 422,507 35x36 | **439,525** | yes | Back |
| `FRIDGE_EXIT` | `FRIDGE_EXIT_PANEL` | BUTTON | 472,507 35x36 | **489,525** | yes | Exit Lab |

### Laboratory: clone — `park-lab-clone`

Clone trigger, offspring name, fence/fridge/back/exit.

Open path: `OPEN_LABORATORY` (757,46) -> `SELECT_LAB_CLONE` (361,513)

Roots: `LAB_CLONE_CENTER`. Controls: 7.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `CLONE_ATTRIBUTES_SPEED_INDICS` | `CLONE_ATTRIBUTES_SPEED_GAUGE_PANEL` | EDIT | 727,111 32x10 | **743,116** | yes |  |
| `CLONE_TRIGGER` | `CLONE_TRIGGER_MIDPANEL` | BUTTON | 141,215 35x36 | **158,233** | yes | Clone Dino |
| `CLONE_OFFSPRING_NAME` | `CLONE_OFFSPRING_NAME_PANEL` | EDIT | 347,388 128x14 | **411,395** | yes |  |
| `CLONE_FENCE` | `CLONE_FENCE_PANEL` | BUTTON | 322,489 35x36 | **339,507** | yes | Place in Enclosure |
| `CLONE_FRIDGE` | `CLONE_FRIDGE_PANEL` | BUTTON | 372,489 35x36 | **389,507** | yes | Store in the Cooler |
| `CLONE_BACK` | `CLONE_BACK_PANEL` | BUTTON | 422,489 35x36 | **439,507** | yes | Back |
| `CLONE_EXIT` | `CLONE_EXIT_PANEL` | BUTTON | 472,489 35x36 | **489,507** | yes | Exit Lab |

## Modal dialogs

### Warning dialog — `dialog-warning`

Modal warning, e.g. an invalid purchase or placement. Centred by ALIGN_CENTER; the outcome icon closes it.

Roots: `WARNING_PANEL`. Controls: 2.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `WARNING_TEXT_ZONE` | `WARNING_WOOD_PANEL` | LIST | 283,276 237x47 | **401,299** | yes |  |
| `WARNING_OUTCOME_ICON` | `WARNING_OUTCOME_PANEL` | BUTTON | 387,354 28x28 | **401,368** | yes |  |

`WARNING_TEXT_ZONE` rows (row height 15 px):

| Row | Click | Label |
| --- | --- | --- |
| 0 | **401,283** |  |
| 1 | **401,298** |  |
| 2 | **401,313** |  |

Scrollbar width 15 px: up **513,287**, down **513,312**.

### Request dialog — `dialog-request`

Modal visitor/request prompt. Validate and Cancel sit side by side at the bottom.

Roots: `REQUEST_PANEL`. Controls: 3.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `REQUEST_TEXT_ZONE` | `REQUEST_WOOD_PANEL` | LIST | 283,274 237x47 | **401,297** | yes |  |
| `REQUEST_VALIDATE_ICON` | `REQUEST_OUTCOME_PANEL` | BUTTON | 370,356 28x28 | **384,370** | yes |  |
| `REQUEST_CANCEL_ICON` | `REQUEST_OUTCOME_PANEL` | BUTTON | 400,356 28x28 | **414,370** | yes |  |

`REQUEST_TEXT_ZONE` rows (row height 15 px):

| Row | Click | Label |
| --- | --- | --- |
| 0 | **401,281** |  |
| 1 | **401,296** |  |
| 2 | **401,311** |  |

Scrollbar width 15 px: up **513,285**, down **513,310**.

### Campaign-end dialog — `dialog-campaign-end`

Campaign completion panel; the outcome icon closes it.

Roots: `CPGEND_PANEL`. Controls: 2.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `CPGEND_TEXT_ZONE` | `CPGEND_WOOD_PANEL` | LIST | 282,200 238x200 | **401,300** | yes |  |
| `CPGEND_OUTCOME_ICON` | `CPGEND_OUTCOME_PANEL` | BUTTON | 387,433 28x28 | **401,447** | yes |  |

`CPGEND_TEXT_ZONE` rows (row height 15 px):

| Row | Click | Label |
| --- | --- | --- |
| 0 | **401,207** |  |
| 1 | **401,222** |  |
| 2 | **401,237** |  |
| 3 | **401,252** |  |
| 4 | **401,267** |  |
| 5 | **401,282** |  |
| 6 | **401,297** |  |
| 7 | **401,312** |  |
| 8 | **401,327** |  |
| 9 | **401,342** |  |
| 10 | **401,357** |  |
| 11 | **401,372** |  |
| 12 | **401,387** |  |

Scrollbar width 15 px: up **513,211**, down **513,389**.

## Radial menus (dead data)

`RadialMenu.Ini` is dead data. The executable has no string references to these sections and the icon names are absent from every GraphicDB. They are listed only so a test plan does not mistake them for reachable menus.

### Show radial menu (unused data) — `radial-show`

Dead data: no executable string references and the icon names do not exist in any GraphicDB. Do not test.

Roots: `SHOW_RADIAL_MENU`. Controls: 7.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `SHOW_MENU_KIT` | `SHOW_RADIAL_MENU` | BUTTON | 80,5 40x40 | **100,25** | yes | &ShowMenuKit |
| `SHOW_MENU_STATS` | `SHOW_RADIAL_MENU` | BUTTON | 27,27 40x40 | **47,47** | yes | &ShowMenuStats |
| `SHOW_MENU_TEST` | `SHOW_RADIAL_MENU` | BUTTON | 133,27 40x40 | **153,47** | yes | &ShowMenuTest |
| `SHOW_MENU_DESTROY` | `SHOW_RADIAL_MENU` | BUTTON | 5,80 40x40 | **25,100** | yes | &ShowMenuDestroy |
| `SHOW_MENU_CLOSE` | `SHOW_RADIAL_MENU` | BUTTON | 155,80 40x40 | **175,100** | yes | &ShowMenuClose |
| `SHOW_MENU_OPEN` | `SHOW_RADIAL_MENU` | BUTTON | 155,80 40x40 | **175,100** | yes | &ShowMenuOpen |
| `SHOW_MENU_SCHEDULE` | `SHOW_RADIAL_MENU` | BUTTON | 133,133 40x40 | **153,153** | yes | &ShowMenuSchedule |

### Building radial menu (unused data) — `radial-building`

Dead data; do not test.

Roots: `BUILDING_RADIAL_MENU`. Controls: 2.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `BUILDING_MENU_INFO` | `BUILDING_RADIAL_MENU` | BUTTON | 80,5 40x40 | **100,25** | yes | &BuildingMenuInfo |
| `BUILDING_MENU_DESTROY` | `BUILDING_RADIAL_MENU` | BUTTON | 80,155 40x40 | **100,175** | yes | &BuildingMenuDestroy |

### Paddock radial menu (unused data) — `radial-paddock`

Dead data; do not test.

Roots: `PADDOCK_RADIAL_MENU`. Controls: 4.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `PADDOCK_MENU_BOWL` | `PADDOCK_RADIAL_MENU` | BUTTON | 80,5 40x40 | **100,25** | yes | &PaddockMenuBowl |
| `PADDOCK_MENU_MODIF` | `PADDOCK_RADIAL_MENU` | BUTTON | 5,80 40x40 | **25,100** | yes | &PaddockMenuModif |
| `PADDOCK_MENU_INFO` | `PADDOCK_RADIAL_MENU` | BUTTON | 155,80 40x40 | **175,100** | yes | &PaddockMenuInfo |
| `PADDOCK_MENU_DESTROY` | `PADDOCK_RADIAL_MENU` | BUTTON | 80,155 40x40 | **100,175** | yes | &PaddockMenuDestroy |

### Security radial menu (unused data) — `radial-security`

Dead data; do not test.

Roots: `SECURITY_RADIAL_MENU`. Controls: 5.

| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |
| --- | --- | --- | --- | --- | --- | --- |
| `SECURITY_MENU_NEUTRALIZE` | `SECURITY_RADIAL_MENU` | BUTTON | 80,5 40x40 | **100,25** | yes | &SecurityMenuNeutralize |
| `SECURITY_MENU_TEAM` | `SECURITY_RADIAL_MENU` | BUTTON | 5,80 40x40 | **25,100** | yes | &SecurityMenuTeam |
| `SECURITY_MENU_LOCK` | `SECURITY_RADIAL_MENU` | BUTTON | 155,80 40x40 | **175,100** | yes | &SecurityMenuLock |
| `SECURITY_MENU_UNLOCK` | `SECURITY_RADIAL_MENU` | BUTTON | 155,80 40x40 | **175,100** | yes | &SecurityMenuUnlock |
| `SECURITY_MENU_DESTROY` | `SECURITY_RADIAL_MENU` | BUTTON | 80,155 40x40 | **100,175** | yes | &SecurityMenuDestroy |

## Regenerating and verifying

```powershell
.venv\Scripts\python.exe tools\formats\ui_map.py build
.venv\Scripts\python.exe -m pytest -q tests/test_ui_map.py
```

Counts: 46 screens, 499 interactive controls (37 menu, 444 park/dialog/lab, 18 radial dead data).
