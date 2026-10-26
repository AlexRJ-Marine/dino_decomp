# Function status

Hybrid-decompilation tracker. Statuses:

- **identified** — address and purpose known.
- **decompiled** — Ghidra pseudo-C recovered and understood.
- **verified** — a parser/test or runtime trace confirms the behavior against
  retail data.
- **replaced** — a C++ replacement is live in the original exe and the running
  game exercises it.

Addresses are virtual addresses in the unpatched `DinoIsland.exe` (image base
`0x00400000`). Names are Ghidra `FUN_*` unless a better name is known.

## Loaders / formats

| Address | Purpose | Status | Evidence |
| --- | --- | --- | --- |
| `0x004714b0` | generic `mgXFileImport` loader; calls vtable `+0x10` reader | decompiled, verified | used by `.skel`/`.skin`; `docs/formats/*.md` |
| `0x00477b50` | `mgVMeshXMdl` header reader (mesh/skin; magic/hash/sentinel/name) | decompiled | vtable `0x626aac`; `.skin` doc |
| `0x00477f30` | mesh/skin block-count + block-array reader | decompiled | `.skin` doc |
| `0x00472240` | per-block reader: name, vertices, indices, UVs, normals, colors, materials | decompiled | `.skin` doc |
| `0x0047d460` | `u16` per-triangle array reader | decompiled | `.skin` doc |
| `0x0047d880` | skin bone-record reader: name, indices, weights, matrix | decompiled, verified | 60 dinosaur skins |
| `0x004663a0` | 64-byte (16-float) matrix reader | **replaced** | `src/hook/dinput8_proxy.cpp`; 281k calls, park reached |
| `0x004809a0` | 12-byte (3-float) normal reader | **replaced** | same proxy; 95k calls, park reached |
| `0x0048b870` | `.skin` loader wrapper (`0x02468ace`) | identified | `.skin` doc |
| `0x00474320` | animation constructor (`0xdef01234`); vtable `0x625ec4` | identified | `.anim` doc |
| `0x00475990` | animation reader: duration/start, bone count, 0x2c records | decompiled, verified | 299/299 `.anim` files |
| `0x004839e0` | animation bone reader: name, `u64` key times, matrices | decompiled, verified | `.anim` doc |
| `0x00475e30` | skeleton constructor (`0x56789abc`) | decompiled | `.skel` doc |
| `0x00430fb0` | `mgFontDx8::LoadFile` (`.fnt` header/tables/glyphs) | decompiled, verified | 5/5 fonts + reference render |
| `0x00430ea0` | `mgFontDx8::CreateTexture` | identified | `.fnt` doc |
| `0x00477890` | skeleton reader: matrices, children, parents, names, flags | decompiled, verified | 64/64 `.skel` files |
| `0x00477610` | skeleton writer/export counterpart | decompiled (partial) | `.skel` doc |

## Overlay / menu (UI-2)

| Address | Purpose | Status | Evidence |
| --- | --- | --- | --- |
| `0x005bca20` | `moWidget::Load` — reads all INI properties, text/font fields and align bits | decompiled | `notes/ui-recon.md`, `docs/formats/overlay-ini.md` |
| `0x005bc000` | `moWidget::OnResize` – fixed/right/down/stretch/center anchoring pass | decompiled, verified | same; 800→1024 MAD check; `src/ui/overlay.cpp` |
| `0x005c5f00` | `moWidget::OnEvent` — property opcodes and flag setters (bits 9–15) | decompiled | same |
| `0x004227d0` | `moFontCtrl` font cache/loader; calls `mgFontDx8::LoadFile` | decompiled | same |
| `0x00517c60` | `mvwMenuStateManager` ctor — registers the nine menu pages in order | decompiled | same |
| `0x0051c510` | menu page base ctor (page index + display name) | decompiled | same |
| `0x0051bbf0` | `MAIN_MENU_PAGE` page ctor; binds seven buttons by name | decompiled | same |
| `0x0051b120` | `FREE_GAME_PAGE` page ctor; island list, difficulty, start | decompiled | same |
| `0x0051aa90` / `0x0051a560` / `0x00519ba0` | campaign select/info, load-game page ctors | decompiled (strings) | same |
| `0x00518830` / `0x00518210` / `0x00517fe0` / `0x00517e90` | options, cinematics, credits, loading page ctors | decompiled (strings) | same |
| `0x0051be70` … | button handlers; set `page + 0x18 = next state` | identified | transition table in `notes/ui-recon.md` |
| `0x00421630` | `moGraphicCtrl::Load` — GraphicDB entries, `NB_OBJECTS`, `SKIP_LIST` bitmask | decompiled | frame-strip section in `notes/ui-recon.md` |
| `0x004219c0` | graphic lookup by name (returns atlas index) | decompiled | same |
| `0x005c3a20` / `0x005c3a60` / `0x005c3a40` | widget texture index / `TEXTURE_FRAME` / `TEXTURE_MOSAIC` setters | decompiled | same |
| `0x00527d30` | in-park HUD button binder (`OPEN_*` → state-manager handlers) | decompiled, verified | `notes/ui-recon.md`; `src/ui/park.cpp` |
| `0x0050be00` | `ivwInfoPanelController::SetActivePanel` — pages 0..8 (dino/show/fence lists, stats, budget, visitors, mission, options, close) | decompiled, verified | `notes/ui-recon.md` ("Park HUD panel switching") |
| `0x00529120` | registers the park state classes `0..0x14` (lab, neutral, selections, construction, purchase, selling) | decompiled | same |
| `0x00532590` | `CONSTRUCTION_MAIN` binder — kit/fence/predefined sub-buttons | decompiled, verified | same |
| `0x0052bd10` | state construction helper `(id, name, ...)` | decompiled | same |
| `0x0051b370` | `FREE_START_BUTTON` — validates island index, sets free-game mode/action 0, requests state 8 | decompiled, verified | `notes/ui-recon.md` ("Free Game start") |
| `0x0051bb30` | difficulty setter — writes `Y+0x44` (0/1/2) | decompiled | same |
| `0x00517e90` / `0x00517ef0` | `LOADING_PAGE` ctor / Activate — sets the start request; no loader | decompiled | same |
| `0x00402460` | `daMainApplication::Update` — consumes start request, tears down the menu, calls new-game/load | decompiled | same |
| `0x00403e10` | new-game start — park init + island load; free game calls `FUN_0051dad0` | decompiled, verified | `src/viewer/main.cpp` (`load_island`) |
| `0x0051dad0` | builds `Data\Design\Landscape\<isl>\<isl>.lnd` and opens the park | decompiled, verified | same |
| `0x00506530` | `gdtIslandDataBase` — reads `IslandList.ini` | decompiled, verified | `IslandList.ini`, 10 records |
| `0x0055dab0` | free-game/campaign scenario setup (`FreeGame.ini`) | decompiled | same |
| `0x00561bb0` | difficulty branch — budget/sell-back/visitor profile keys | decompiled | same |
| `0x005c5bf0` | `moWidget::Draw` — self then child list (priority/insertion order) | decompiled | `notes/ui-recon.md` |
| `0x005c5d60` | child draw thunk (`Draw` + `+0x3c`) | identified | same |
| `0x005bae40` | widget manager add-child (priority-ordered list) | decompiled | same |
| `0x00529880` | minimap controller (`MAP_PANEL`, `MAP_RENDERVIEW`, zoom buttons) | decompiled, verified | `src/viewer/main.cpp` (minimap) |
| `0x00529a90` / `0x00529b60` | minimap zoom-in/out press — set held flags `+0x24`/`+0x25` | decompiled, verified | `notes/ui-recon.md` ("Minimap implementation") |
| `0x005fb7e0` | minimap zoom integration — scale `+0x38` ±0.15/s, clamp 0.1..2.0 | decompiled, verified | same |
| `0x005fb120` | detail-map quad — target centre (x/1024, 1-y/1024), angle 90° + camera heading | decompiled, verified | same |
| `0x005face0` | detail-map target setup — 256×256, scale default 0.25 | decompiled | same |
| `0x005ba6b0` / `0x005ba8f0` | widget type `RENDER` (10), `RV_ZMIN`/`RV_ZMAX` | decompiled | same |
| `0x00529d00` | park observer update — camera FOV 80°, aspect 4:3, near ≥ 1, far 1000, clamps | decompiled | same |
| `0x00527650` | `pvwPark3DRenderView` ctor — park camera/renderer | decompiled (partial) | same |
| `0x0050c1a0` | in-park `OPTIONS_PANEL` controller (options/save/load tabs) | decompiled | same |
| `0x005257a0` | camera/navigation controller; `PauseGame` action | decompiled (partial) | same |

## Runtime / save / startup

| Address | Purpose | Status | Evidence |
| --- | --- | --- | --- |
| `0x00401bfc` | CD-check branch patched for no-CD | verified | `docs/patches/no-cd.md` |
| `0x00598bad` | `SetCurrentDirectoryA` helper | decompiled | save-path investigation |
| `0x004160c0` | change working directory | decompiled | save-path investigation |
| `0x00402bc0` | load-save wrapper (opens `save/`, calls load chain) | decompiled | `notes/save-graph-recon.md` |
| `0x00403340` | save/autosave wrapper | decompiled | same |
| `0x0051e6c0` | manager load chain: campaign → world → sim → AI → scenario | decompiled | same |
| `0x0051e7e0` | manager save chain (same order) | decompiled | same |
| `0x0053a140` | `pwdWorldManager` class table IDs `0x12`–`0x18` | decompiled | `notes/world-objects-recon.md` |
| `0x0053c560` | `pwdWorldManager` load — typed object lists | decompiled | same |
| `0x0053c9d0` | `pwdWorldManager` save | decompiled | same |
| `0x005e8e30`…`0x005e8f80` | world serializer classes `0x12`–`0x18` (constructor, load, save) | decompiled | same |
| `0x005e93e0` / `0x005e9620` | placed-object base load/save (waypoints) | decompiled | same |
| `0x005e97f0` / `0x005e9820` | simple-object base load/save | decompiled | same |
| `0x005e8860` / `0x005e8910` | `drsFence` load/save | decompiled | same |
| `0x005e7670` / `0x005e7860` | `drsShow` load/save | decompiled | same |
| `0x005e8610` / `0x005e86a0` | `drsBuilding` load/save | decompiled | same |
| `0x005e8cf0` / `0x005e8d60` | `drsVisitor` load/save | decompiled | same |
| `0x005e8a70` / `0x005e8aa0` | `drsAnimal` load/save | decompiled | same |
| `0x005e8bc0` / `0x005e8be0` | `drsEmployee` load/save | decompiled | same |
| `0x00584ed0` | `drsDinosaur` payload — 18 gene pairs | decompiled | same |
| `0x005869b0` / `0x00615bb0` | objective list record add | decompiled | same |
| `0x00543d20` / `0x00543e70` | unlock arrays: 62 features / 39 buildings / 10 food / 6 misc | decompiled | same |
| `0x00403e10` | `.wld` loader (reads version + mode, campaign flag 1) | decompiled | same |
| `0x00558340` / `0x00558230` | show manager save/load (`0x2c`-byte records) | decompiled | same |
| `0x00585780` / `0x00585240` | economy save writer / reader (sequential stream) | decompiled | `docs/formats/sav.md` |
| `0x00585bd0` | park budget/setup setter (confirms `+0x08`) | decompiled | same |
| `0x0058aed0` | `spSimuManager` state read (road, visitors, park, stores) | decompiled | same |
| `0x0058ccc0` | road-network save/load (rectangles) | decompiled | same |
| `0x00593a60` | dinosaur-store load (20 name/id/path/description records) | decompiled | same |
| `0x00592060` / `0x00592a00` | visitor-model values + lab store (flags) | decompiled | same |
| `0x0051f290` / `0x0051f400` | AI manager load/save (5 lists, 17-byte entries) | decompiled | same |
| `0x005e5b80` / `0x005e5ba0` | camera-switcher load/save (10 floats) | decompiled | same |
| `0x005e5920` / `0x005e5940` | scenario-manager load/save wrapper | decompiled | same |
| `0x00585c00` / `0x00585c40` | budget subtract / VENTES generic income | decompiled | `docs/decomp/economy.md` |
| `0x00585c90` | FOOD expense (`+0x0c` / `+0x10`) | decompiled | same |
| `0x00585cc0` / `0x00585cf0` / `0x00585d20` | DINOACHAT / LABOACTION / CONSTRUCTION groups | decompiled | same |
| `0x00585d50` / `0x00585d80` | EFFECTIFS_SECU / staff-role expense buckets | decompiled | same |
| `0x00585db0` | shop revenue `+0x24+kind*4` and count `+0x58+kind*4` | decompiled | same |
| `0x00585de0` | ticket income (visitor entry) | decompiled | same |
| `0x00585e10` | daily counter roll-up and bankruptcy flags | decompiled | same |
| `0x00586070` | budget totals (`+0x144` / `+0x148` / `+0x14c`) | decompiled | same |
| `0x0058ad00` | daily economy settlement | decompiled | same |
| `0x0050fd30` / `0x00510430` | budget panel constructor / row refresh (14 rows) | decompiled | same |
| `0x00541940` / `0x0053cf10` / `0x00588600` | `employeelist.ini`, employee create/add | decompiled | same |

## Scheduler / simulation / AI (DEC-2)

| Address | Purpose | Status | Evidence |
| --- | --- | --- | --- |
| `0x00506ec0` | `lpScheduler` constructor | decompiled | `notes/scheduler-recon.md` |
| `0x00507050` | `lpScheduler::AddTask(name, callback, context)` | decompiled | same |
| `0x00507490` | `lpScheduler::SetTaskEnabled(name, bool)` | decompiled | same |
| `0x00507530` | `lpScheduler::Update` — fixed-order task tick | decompiled | same |
| `0x0051d050` | park task registration (10 tasks, frame order) | decompiled | same |
| `0x00596500` | menu task registration (3 tasks) | decompiled | same |
| `0x005869e0` | `spSimuManager::Update` — actors, 5 s/10 s timers | decompiled | same |
| `0x00589cd0` | park-wide simulation step (1 s ticks, visitors, economy, shows) | decompiled (partial) | same |
| `0x0051fab0` | `apBehaviorManager::Update` — perception, reactions, AI order | decompiled | same |
| `0x00521b30` | `apDinosaurBehavior` constructor / state fields | decompiled | same |
| `0x00521d80` / `0x00521d10` / `0x00521d60` | dinosaur reaction state transitions (1/2/idle) | decompiled | same |
| `0x005213f0` | behavior event callback; state 1 → 2 on target match | decompiled | same |
| `0x00521760` / `0x00521790` | stop action / write target cell and insert path record | decompiled | same |
| `0x0051fa10` / `0x0051f8d0` | dinosaur notify/attack and forced target reactions | decompiled | same |
| `0x005f6fb0` | linear integer-vector `find` (begin, end, &value) | decompiled | same; not a visibility test |
| `0x00522820` | `apVisitorBehavior::Update` — action dispatcher | decompiled | `docs/decomp/visitor-ai.md` |
| `0x00522940` | visitor state transitions (choose/travel/use/leave) | decompiled | same |
| `0x00522a10` | visitor attraction scoring; weight vector = constant `.bvi` block, `(99−need)²` terms | decompiled + runtime-verified | same, `notes/visitor-weights-runtime.md` |
| `0x0051f560` | sets `DAT_00675638` from profile record `+0xD0` | decompiled + runtime-verified | same |
| `0x00544110` | copies a parsed `.bvi` profile into the 0xEC list record | decompiled + runtime-verified | same |
| `0x00522ff0` | show schedule/arrival check | decompiled | same |
| `0x00521490` | visitor path following | decompiled | same |
| `0x00522cb0` | path obstacle avoidance / reroute | decompiled | same |
| `0x005219f0` / `0x00522f30` | attraction use and timer | decompiled | same |
| `0x00521df0` | per-visitor movement tick | decompiled | same |
| `0x005654f0` | `snoObjective::Load` — verb id, args, evaluator pointer | decompiled | `notes/objectives-recon.md` |
| `0x005658d0` | objective verb → id lookup (33 verbs) | decompiled | same |
| `0x00566080` | objective visitor counter (`visitors`/`happyvisitors`) | decompiled | same |
| `0x00566620` | objective building counter (`connectedbuildings`) | decompiled | same |
| `0x00567190` | objective show query (`happyshow`/`fightshow`/`happydinoshow`) | decompiled | same |
| `0x00567cb0` | objective genotype/trait matcher (10 dino fields) | decompiled | same |
| `0x005220e0` | `apEmployeeBehavior::Update` — job dispatcher | decompiled | `docs/decomp/employee-ai.md` |
| `0x005224c0` | employee job-queue pop / target selection | decompiled | same |
| `0x00522390` | employee kind-0xb work animation | decompiled | same |
| `0x00522300` | employee state transitions | decompiled | same |
| `0x00594b80` | `spScheduleManager` constructor | decompiled | `docs/decomp/park-time.md` |
| `0x0058c1c0` | time-manager constructor / second-minute-hour conversions | decompiled | same |
| `0x0058c2f0` | clock advance; new-hour and end-of-day flags | decompiled | same |
| `0x0058c350` | clock read: hour / minute / day | decompiled | same |
| `0x0058c390` | show start-window check | decompiled | same |
| `0x0058c760` | 12-slot show scheduler on hour boundaries | decompiled | same |
| `0x0058c210` / `0x0058c260` / `0x0058c620` / `0x0058c6b0` | schedule serialization | decompiled | same |
| `0x00526010` | HUD time panel (`%.2d:%.2dAM/PM`, day+1) | decompiled | same |
| `0x005234e0` | `PARK_CURRENT_TIME` / `PARK_CURRENT_DAY` widgets | decompiled | same |
| `0x00524630` | park daily update / end-of-day trigger | decompiled | `docs/decomp/park-time.md` |
| `0x0058c2d0` | day counter `+0x04 += 1`, clear flags, reset elapsed | decompiled | same |
| `0x0058ae60` | daily actor/dino reset pass | decompiled | same |
| `0x0058ad00` | daily economy settlement (triggered by `0x524630`) | decompiled | `docs/decomp/economy.md` |
| `0x0053e060` | `actionlist.ini` loader; action IDs 0..63 | decompiled | `docs/decomp/dinosaur-actions.md` |
| `0x005727b0` | `saDinosaur::ApplyActionEffect(action_id, …)` | decompiled | same |
| `0x00571540` | `saDinosaur::Update` — rest decay, action countdown, activity table | decompiled | same |
| `0x00571190` | action completion; sets `+0x2c0 = 0x1f` | decompiled | same |
| `0x005725e0` / `0x00571370` / `0x00571410` | 5 s/10 s need passes (food/violence) | decompiled | same |
| `0x00570590` | need threshold init `[80,60,30]` | decompiled | same |
| `0x00570fd0` | need/raw-gauge reset (rest full, food 50%, needs 99) | decompiled | same |
| `0x005714b0` | need value → GOOD/MID/BAD state index | decompiled | same |
| `0x00571350` | recompute all four need states | decompiled | same |
| `0x00572e10` | most-urgent need index (0..3, 4 = none) | decompiled | same |
| `0x00511d80` / `0x005138b0` / `0x005df900` | dinosaur need widgets and tab update | decompiled | same |
| `0x005ca960` | dynamic `dsound.dll` / `winmm.dll` loader | decompiled | `docs/decomp/media.md` |
| `0x005b6f30` / `0x005b70a0` / `0x005b7d00` | `msSoundEngine` / sound DB load / lookup | decompiled | same |
| `0x004fdb00` / `0x004fd910` | `sdpAudioPark` constructor / music playback | decompiled | same |
| `0x005c0210`..`0x005c90f0` | `mvVideo` / `mvMovie` / `mvSubtitle` (DirectShow) | decompiled | same |
| `0x00517a40` / `0x00517ab0` | menu movie wrapper / playing poll | decompiled | same |
| `0x00401d40` / `0x00402600` | startup intro / generic movie paths | decompiled | same |

## C++ reimplementations

These are readable ports used by the portable viewer. The first one is now
hooked into the original exe (Phase 3 `HYB-1`).

| Original | C++ | Status |
| --- | --- | --- |
| `0x4663a0` matrix reader | `src/hook/dinput8_proxy.cpp` | **replaced live**, 281k calls |
| `0x4809a0` normal reader | `src/hook/dinput8_proxy.cpp` | **replaced live**, 95k calls |
| `0x477890` skeleton reader | `src/formats/skel.cpp` | reimplemented, tested on 80-bone allo |
| `0x47d880` skin bone records | `src/formats/skin.cpp` | reimplemented, 317 assignments |
| `0x475990` / `0x4839e0` animation reader | `src/formats/anim.cpp` | reimplemented, quaternion sampling |
| `.mesh` body reader | `src/formats/mesh.cpp` | reimplemented, `--validate` |
| `.tga` reader | `src/formats/tga.cpp` | reimplemented, 24/32/8-bit + RLE |
| `.skel`/`.skin`/`.anim` CPU skinning | `src/viewer/main.cpp` | reimplemented, VIEW-3 |
| park HUD panel switching | `src/ui/park.cpp` | reimplemented, 12 HUD buttons + info pages + lab centres |
| menu start -> selected island | `src/viewer/main.cpp` (`load_island`) | reimplemented, IslandList + `.lnd` rebuild, budgets |
| minimap detail view | `src/viewer/main.cpp` (FBO) | reimplemented, 256² top-down map, camera-centred, zoom + marker |
| HUD resize anchoring | `src/ui/overlay.cpp` (`layout_for_size`) | reimplemented, 800→1024 rects verified |
| park clock | `src/viewer/main.cpp` (`ParkClock`) | reimplemented, GameSpeed-driven 12 h day + HUD time/day |
| overlay INI/GraphicDB + frame strips | `src/ui/overlay.cpp`, `src/ui/menu.cpp` | reimplemented, UI-2 |

## Open items

- SFX/music playback port and the MPEG intro player — MEDIA-1 rest; the sound
  DB, music map and DirectShow pipeline are solved in `docs/decomp/media.md`.
- Park day-counter increment source and the fixed economy-category labels —
  `docs/decomp/park-time.md`, `docs/decomp/economy.md`.
- 1024×768 HUD anchoring and dynamic sub-panel fields — UI-2 Phase C rest.
- Warning/request/campaign-end dialogs (`DIALOG_BACKGROUND`) and the save/load
  panel — UI-2 Phase D.
- Dinosaur AI action chooser that maps BAD FOOD/REPOS to `Eat`/`Sleep`;
  the gauges, four needs and `0x1f` idle sentinel are solved in
  `docs/decomp/dinosaur-actions.md`.
- `.sav` world object lists are solved; the remaining DEC-3 item is populated
  lab records.
