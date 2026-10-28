# Dino Island - reverse engineering and native port

Reverse-engineering and interoperability project for **Dino Island**
(Monte Cristo, 2002). The retail game is a 32-bit MSVC 6 / DirectX 8 build.
This repository contains:

- tested parsers and format specifications for the retail data;
- a portable C++20 + SDL3/OpenGL viewer and port in `src/`;
- Ghidra research and a live hybrid-decompilation harness (`DINPUT8.dll`);
- the evidence trail in `notes/` and `docs/`.

**No retail game files or extracted assets are included or committed.**
`original/`, `game/`, `run/`, `analysis/`, `build/`, `.venv/` and `*.iso` are
gitignored. Supply your own copy of the game to parse assets, run the viewer,
or play the original.

## Project status

> Last updated: 2026-10-28. The authoritative running log is
> [`notes/STATE.md`](notes/STATE.md); the dashboard is
> [`notes/NOW.md`](notes/NOW.md); the task queue is [`TASKS.md`](TASKS.md).

| Track | Status | Summary |
|---|---|---|
| Retail formats | Solved | 39 specs in `docs/formats/`; parsers round-trip the full retail sets (536 meshes, 299 animations, 64 skeletons, 104 skins, 649 TGA textures, 231 WAV files). Residual numeric semantics are listed in `notes/OPEN-TASKS.md`. |
| Asset tools | Working | Mesh/skeleton/animation/texture/audio/config readers plus CLI dumpers and OBJ export. |
| Portable viewer | Working | 10 islands through the retail menu, vegetation sector LOD, skinned dinosaurs, retail road network, visitors, retail menu/HUD, minimap, live clock, music/clicks, store and lab panels. |
| Gameplay simulation | Partial | Visitor needs/attractions state machine, dinosaur chooser/needs/actions, danger grid, daily settlement, show scheduling, employees and campaign objectives are ported; lab gameplay, live show actors and the full visitor loop remain. |
| Decompilation | Research-heavy | 232 function entries documented: 188 decompiled, 20 verified, **9 asset readers replaced live** in the retail exe. |
| Test suite | Green | **492 tests** in the suite; the last full run passed all 492. `dino_viewer --selftest` passes 189 logic checks. |

### What works today

- **Islands:** all 10 retail islands start from the Free Game menu with the
  recovered camera (`camera.ini`: target centre, distance 39, elevation 52.7,
  yaw 0; FOV 80). `isl01` renders a 512x512 heightmap (522,242 triangles),
  basemap, 1,536 placed trees with sector LOD and frustum culling, skybox and
  two-stage animated water. The Entrance dock and free-game start road match
  retail (`notes/entrance-dock-road-recon.md`).
- **Actors:** `.mesh` + `.skel` + `.skin` + `.anim` load together; the
  allosaurus is skinned with the corrected skeleton-rest bind (pachy LOD2/LOD3
  included) and plays walk/idle/run clips. The same pipeline drives the store's
  3D preview. `--start-dino` places it in a menu-started island.
- **Camera:** the retail park camera is the interactive default (per-update pan
  on a 60 Hz tick, 5/s yaw low-pass, `6 -> 2` target/eye lerp, mouse edge
  scroll, no roll). `--camera-orbit` keeps the legacy orbit view; free-fly and
  key rebinding remain available.
- **Building:** retail road network (segments, junction merge, T-snap,
  per-segment paint), 4-unit path/fence grid, terrain ray-picking, enclosure
  preview, undo and save/load.
- **Visitors:** spawn on the path network, choose attractions, travel, use them
  and leave; visitor profiles and needs come from the retail `.bvi` data
  (24 profiles).
- **Park HUD:** 2,694 widgets / 38 roots, re-anchored (not scaled) to any
  window size; budget/visitor/dino/show/paddock panels, store, lab centres,
  minimap detail view, live clock and ledger. Window-space clicks now match the
  drawn buttons at every window size.
- **Menus:** nine-page retail main menu with exact `.fnt` glyph metrics and
  list scrolling, Free Game island selection, Options sliders/indications that
  apply live, Cinematics ending-video playback, Load Game for port `.psav`
  slots, and the warning/request/campaign-end dialogs.
- **Campaigns:** 7 campaigns / 39 missions / 89 objectives are read, all 33
  objective verbs evaluate, `dino.dat` gates unlocks, mission progress resumes,
  success advances and the last mission raises the unlock count.
- **Saves:** the C++ `.sav` writer re-serializes the sample byte-identically
  and a written park loads in the retail game. Campaign `list0`/`list1`/`list2`
  are typed and byte-identical; manual/autosave loading restores the HUD bank.
- **Media/QoL:** menu/park click sounds and island themes (chunk-streamed,
  looped), WAV playback, MPEG ending movies, windowed/fullscreen/borderless
  modes, persisted window and key configuration, BMP screenshots.
- **Stability:** repeated 60,000-frame (~10 minute) runs with a flat working
  set.

### What is still missing

- Exact object placement (PLACE-1): wire the occupancy grid into
  roads/buildings/fences, then the exact fence flow (margin scan, `4*margin-1`,
  cost, wall cells, sell gate/refund) and building/food/show placement.
- The rest of the visitor loop: queueing, spending, show attendance, and the
  five attraction-scoring weights (SIM-2b-W).
- Dinosaur perception: the visitor/obstacle reaction lists have **no writer in
  this build**, so the list-driven starters are inert; the element bone/event
  overlay source remains open.
- Live show simulation: `.bfi` arena/judge actors, per-show weight authoring
  and name-suffix iteration.
- Laboratory gameplay (breeding/cloning/fridge) is decoded but panel-only; the
  employee hire panel, guard role and walking animation are incomplete.
- Retail `.sav` slots in Load Game (island/mode headers are decoded, listing is
  R-16b) and the Campaign Info mission list (retail has no mission list; use
  `campaign-progress.txt` or a port-only override).
- Terrain patch LOD, vegetation slope tilt, minimap water colour/partition,
  skybox bottom/side seam and smoother retail shading.
- MP2 audio and the intro movie; visitor travel and clip-playback phase in
  saves.
- Background-safe retail input (TOOL-4): the window stays behind the user's
  work and screenshots are background-safe, but retail DirectInput is
  foreground-only, so click tests need the game active.

### Current focus

1. **PLACE-1 — exact object placement.** The road cursor uses the retail
   nearest 4-unit anchor snap (fixing the 45-degree direction flip), and the
   occupancy-grid foundation is in. Next: the exact fence flow, then
   building/food/show placement costs and validators.
2. **MENU-1 — menu parity.** Retail `.sav` slots (R-16b) and the Campaign Info
   mission list.
3. **CAM-1 — camera feel.** Re-test the measured retail camera in hand; pan is
   one step per rendered frame at vsync 60.
4. **P3 perception.** Wire the direct `start_visitor`/`start_dino` entry points
   in the show/attraction controller; do not invent a list producer.
5. **CAMPAIGN population.** Feed `WldWorld.records` shows/animals/employees
   into `load_campaign_park` and assert them on Cpg04.
6. **SHOW-4 rest.** Live `.bfi` arena/judge actors and per-show weights.
7. **SAVE-2 HUD.** Runtime-watch the remaining 26,000-cash restore difference
   on manual/autosave load.
8. **WORLD-3.** Capture the final retail/port side-by-side.

### Playable flow (retail menu -> park)

```powershell
$G = "game/%MAINDIR%"
.\build\mingw\dino_viewer.exe --asset-root $G `
  --menu-page 1 --menu-click 200,180 --menu-click 494,451 `
  --menu-click 526,531 --menu-loading-time 0 `
  --start-dino --music
```

This shows the retail Free Game page, selects IGUANO/easy, loads isl01 with
the recovered start camera, places the animated allosaurus at the centre,
and streams the island theme. Interactive runs enable music and click sounds
by default; add `--no-music`/`--no-sounds` to silence them. Without
`--start-dino` the park starts empty, as in the original.

## Format coverage

A format is listed as solved when a parser round-trips the complete retail set
and the header/layout is documented. Some semantic fields remain open even
where the container is fully parsed.

| Group | Formats / files |
|---|---|
| Geometry and animation | `.mesh`, `.skin`, `.skel`, `.anim`, `.seq`, `anim_trans.dic`, `proportion.dat` |
| World and saves | `.lnd`, `.raw`, `.imp`, `.blf`, `.wld`, `.sav` (containers solved; some semantics open) |
| Text and UI | `.fnt`, `.loc`, `MenuInterface.Ini`, `ParkInterface.Ini`, `MenuGraphicDB.ini`, `ParkGraphicDB.ini` |
| Gameplay data | `.shw`, `.gen`, `.bvi`, `.bbi`, `.bsi`, `.bni`, `.bei`, `.bai`, `.bwi`, `.bfi`, campaigns/missions |
| Simulation parameters | `GeneralParams.bgd`, `Actions.bad`, `simu_trans.dic`, `TranslatorConstants.ini`, `Data/States` |
| Media | `.tga`, `.wav`, `MenuSoundDB.ini`, `ParkSoundDB.ini` |

Validated scale: 536 meshes, 299 animations, 64 skeletons, 104 skins,
649 TGA textures and 231 WAV files. The exact layouts and evidence are in
[`docs/formats/`](docs/formats/).

## Decompilation progress

The retail executable has **7,273 functions** (725,709 instructions) in Ghidra.
[`docs/decomp/function-status.md`](docs/decomp/function-status.md) currently
tracks **232 entries** with address, purpose, status and evidence: 188
decompiled, 20 verified and 9 replaced live.

Solved or documented:

- **Loaders:** `.mesh`, `.skin`, `.skel`, `.anim`, `.sav`, `.wld` and font
  readers, plus the hybrid reader contracts.
- **UI:** `moWidget::Load`, anchoring/resize, event setters, the nine-page menu
  state machine, park HUD binders, list viewports/VSCROLL and info-panel
  controllers.
- **Runtime/save:** manager load/save order, world object class IDs `0x12`-`0x18`,
  the `.sav`/`.wld` object graph, campaign list0/list1/list2, simulation
  prefix, AI/camera tail and the retail `.sav` acceptance path.
- **Simulation:** `lpScheduler` and the ten-task park frame order,
  `spSimuManager`, `apBehaviorManager`, dinosaur action IDs 0-63, need gauges,
  the `0x1f` idle sentinel, visitor attraction scoring, employee job AI,
  park budget/daily settlement, park clock, show scheduler and judge score,
  campaign objective evaluator (33 verbs/predicates), road network and
  occupancy-grid wall rules.
- **Presentation:** overlay INI semantics, sound database, DirectShow movie
  paths, the retail park camera (pan/yaw/zoom/edge scroll) and the water
  `Modulate2x` stage.

Nine asset readers are **replaced live** in the retail executable through the
32-bit `DINPUT8.dll` proxy (eight inline detours and one vtable patch):

| Original | Purpose | Evidence |
|---|---|---|
| `0x004663a0` | 64-byte (16-float) matrix reader | ~281,000 calls |
| `0x004809a0` | 12-byte (3-float) normal reader | ~97,000 calls |
| `0x0047d460` | `u16` per-triangle array reader | 2,000+ calls on Cpg04 |
| `0x0047cc90` | child-index vector reader (`.skel`) | 2,000+ calls on Cpg04 |
| `0x0047d880` | skin bone-record reader | 2,221 bones |
| `0x004839e0` | animation bone reader (`u64` key times) | 8,911+ keys |
| `0x00477890` | skeleton reader | 21 skeletons |
| `0x00477f30` | `.mesh` whole-file block reader (vtable `0x626abc`) | 121 meshes |
| `0x00430fb0` | `mgFontDx8::LoadFile` | 5 fonts, 247 glyphs each |

The portable port is the main deliverable; the hybrid layer keeps the original
executable runnable while functions are recovered. See
[`docs/decomp/hybrid.md`](docs/decomp/hybrid.md).

## Repository layout

| Path | Purpose |
|---|---|
| `original/` | Read-only ISO-derived files (installer, docs). Never edit. |
| `game/%MAINDIR%/` | Extracted retail game tree. Read-only reference. |
| `run/` | Local playable copy, patched exe and hook DLL. Gitignored. |
| `notes/` | Findings, logs, evidence images, `NOW.md`/`STATE.md`, harness output. |
| `docs/formats/` | Proven data-format specifications. |
| `docs/decomp/` | Function map and subsystem research. |
| `docs/platform/` | Viewer, terrain, UI, audio and QoL notes. |
| `docs/patches/` | Documented runtime patches. |
| `tools/formats/` | Python parsers and exporters. |
| `src/` | Portable C++20 port and the hook DLL. |
| `tests/` | Python and C++ integration tests. |
| `scripts/` | Build, test, Ghidra, patch and play helpers. |
| `.opencode/` | Project-specific OpenCode agents, skills and commands. |

## Building the portable port

This builds our port, **not the retail game**. The output is
`build/mingw/dino_viewer.exe` plus `dino_tools.exe`. `run/DinoIsland.exe` is a
copy of the retail executable used as an oracle; it is never built here.

### Prerequisites

You need Windows 10/11 (the scripts are PowerShell) and Python 3.11+ on
`PATH` (python.org installer, "Add to PATH"). Everything else is installed by
one script, at versions pinned in `scripts/setup-tools.ps1`:

| Tool | Pinned | Needed for |
|---|---|---|
| `.venv` (CMake, Ninja, pytest, pyghidra-mcp, …) | `requirements.txt` | everything |
| w64devkit (MinGW-w64 GCC) | 2.10.1 | viewer + 32-bit hook DLL |
| SDL3 MinGW | 3.4.18 | viewer |
| Temurin JDK | 25.0.5+8 | Ghidra only |
| Ghidra | 12.2 | reverse engineering only |
| x64dbg | 2026.05.27 | x32dbg harness only |

### New machine / new contributor

```powershell
git clone https://github.com/AlexRJ-Marine/dino_decomp "Dino decomp"
cd "Dino decomp"

# all tools (or: -Only venv,w64devkit,sdl3 for just the port build)
.\scripts\setup-tools.ps1
.\scripts\build.ps1 -BuildDir build\port
```

- **Tool location.** Tools go under `$env:DINO_TOOLS`, default `C:\Tools`.
  To use another drive or folder, set it once, persistently:
  `[Environment]::SetEnvironmentVariable('DINO_TOOLS','D:\dev\tools','User')`,
  then open a new shell. All scripts resolve tools through
  `scripts/tools.ps1`; never hard-code a tool path in a tracked file.
- **Your own game copy.** `game/`, `original/` and `run*/` are gitignored and
  never shared. Put your own retail tree in `game/%MAINDIR%/` (extract it from
  your ISO's `SETUP.EXE`; see `tools/run_extract.sh`). Data-dependent tests
  skip when it is absent. For retail runs, make `run/` a copy of that tree,
  apply `scripts/patch_nocd.ps1 -Target run\DinoIsland.exe`, run
  `scripts/build-hook.ps1`, then `scripts/setup-test-runtime.ps1`.
- **Copied checkouts.** If you copy someone's folder instead of cloning:
  `build.ps1` reconfigures build dirs whose CMake cache was made elsewhere,
  and `start-ghidra.ps1 -TakeOwnership` adopts their Ghidra project
  (otherwise Ghidra refuses it with "Project is owned by …"). A copy turns the
  `Data` junction in `run-test/` and `run-window/` into a full copy of the
  game data; that still works, it just uses more disk.
- **Zip bundles.** Some zip tools skip dot-directories, so a bundle built from
  Explorer can silently omit `.opencode/` (agents, skills, commands). The
  repository tracks it; if `git status` shows those files deleted, restore
  them with `git restore --source=origin/main -- .opencode`. For a clean
  bundle use `git archive --format=zip --output=dino-repo.zip HEAD` and keep
  `game/` and `original/` in a separate archive.
- **Ghidra settings.** `start-ghidra.ps1` keeps Ghidra's user settings in
  `analysis\ghidra-settings` (per checkout, gitignored) instead of
  `%APPDATA%`. Packaged apps such as the Claude desktop app redirect
  `%APPDATA%` writes, which breaks Ghidra's script system; see
  `notes/env-check.md`.
- **Git identity.** Each person sets their identity once, globally
  (`git config --global user.name "…"` / `user.email "…"`), and nobody sets it
  inside the repo. Commits are then credited to whoever makes them on their
  own PC. A copied checkout can carry the previous owner's repo-local
  identity; `check-env.ps1` warns about it, and
  `git config --local --unset user.name` / `--unset user.email` removes it.

`check-env.ps1` validates the full reverse-engineering environment. It expects
the retail executable and a running Ghidra MCP server, so start Ghidra first,
then run `.\scripts\check-env.ps1`. If you only want to build the viewer,
`scripts\build.ps1` needs just the venv, w64devkit and SDL3.

### Build

```powershell
.\scripts\build.ps1
```

The script configures `build/mingw` with Ninja and w64devkit, then builds:

- `build/mingw/dino_viewer.exe` - SDL3/OpenGL viewer
- `build/mingw/dino_tools.exe` - small CLI skeleton
- `build/mingw/SDL3.dll` - copied next to the viewer

### Run the island + dinosaur scene

```powershell
$G = "game/%MAINDIR%"

.\build\mingw\dino_viewer.exe `
  --lnd "$G/Data/Design/landscape/isl01/isl01.lnd" --asset-root $G `
  --mesh "$G/Data/Graphics/Actors/Dinos/allo/allo_lod1.mesh" `
  --skel "$G/Data/Graphics/Actors/Dinos/allo/allo.skel" `
  --skin "$G/Data/Graphics/Actors/Dinos/allo/allo_lod1.skin" `
  --anim "$G/Data/Graphics/Actors/Dinos/animations/HeavyBiped/HeavyBiped_Walk.anim" `
  --dino-texture "$G/Data/Graphics/Textures/actors/dinos/allo.tga" `
  --dino-pos 512,512 --dino-angle 200 --time 1200 `
  --yaw 35 --pitch 16 --distance 17
```

Controls: left-drag orbits, wheel zooms, `R` resets. `C` toggles free-fly
(WASD, Q/E, Shift, mouse look). `F1` toggles the HUD, `F10`/`F11` change the
window mode.

### Run the retail-style menu

```powershell
$G = "game/%MAINDIR%"

.\build\mingw\dino_viewer.exe --menu `
  --lnd "$G/Data/Design/landscape/isl01/isl01.lnd" --asset-root $G
```

Use `--menu-page 1` for the Free Game page and `--menu-click X,Y` to drive it
from scripts or screenshots. Starting a game loads the selected island through
`IslandList.ini`.

### Useful viewer options

| Option | Effect |
|---|---|
| `--validate` | Parse all inputs and print counts without opening a window. |
| `--selftest` | Run the 189 grid/visitor/build/gameplay logic checks and exit. |
| `--free-camera` | Start in free-fly mode. |
| `--camera-orbit` | Use the legacy orbit camera instead of the retail park camera. |
| `--build` | Enable path/fence placement (`1` path, `2` fence, `0` off, `U` undo). |
| `--visitors N` | Spawn visitors on the placed path network. |
| `--place-show NAME[:X,Y]` | Place a scheduled show from the mission `[SHOWS]` catalog. |
| `--hire-nutritionist N` | Hire N nutritionists (500 each). |
| `--sav-write IN OUT` | Rebuild a `.sav` and write it to OUT; `--sav-roundtrip` validates the bytes. |
| `--park-click X,Y` | Script a park HUD click in design coordinates. |
| `--park-window-click X,Y` | Script a park HUD click in window coordinates (real mapping). |
| `--skybox DIR` | Six `front/back/left/right/top/bottom` PNG faces. |
| `--frames N --screenshot out.bmp` | Deterministic headless capture. |
| `--wav FILE --mute` | Play a retail WAV; `M` mutes. |

Run `dino_viewer.exe --help` for the complete list.

### Tests

```powershell
.\scripts\run-tests.ps1
```

The suite is **492 tests** covering the Python parsers, the C++ viewer via
deterministic subprocess runs, screenshot checks, the campaign/gameplay logic
and the viewer selftest. Use `.\scripts\run-tests.ps1 -Fast` for the quick
subset during development. Tests that need retail data skip automatically when
`game/%MAINDIR%` is absent.

`scripts/verify.ps1` runs the environment check, the build and the tests in
sequence.

## Reverse engineering with Ghidra

The project uses a headless `pyghidra-mcp` server at
`http://127.0.0.1:8000/mcp`. Ghidra 12.2 requires JDK 25; `start-ghidra.ps1`
sets `JAVA_HOME` for you. OpenCode reaches the server through `mcp-proxy`.

```powershell
.\scripts\start-ghidra.ps1
.\scripts\ghidra-smoke.ps1

# when finished
.\scripts\stop-ghidra.ps1
```

The Ghidra project is `analysis/ghidra/dino.gpr`; the imported binary is the
retail `game/%MAINDIR%/DinoIsland.exe` (SHA-256
`7ba16f38db4fe9461e700e6005e29a37c0d7a6d9441a233880a7c4405bbd717c`).
Start with [`docs/decomp/function-status.md`](docs/decomp/function-status.md)
and [`notes/FINDINGS.md`](notes/FINDINGS.md).

## Running the original game locally

`run/` is a gitignored local copy of the retail tree. It is used only for
dynamic testing of the original executable.

- `run/DinoIsland.exe` is the retail executable with the documented six-byte
  no-CD startup patch at `0x1BFC` (five bytes differ).
- `run/DinoIsland.exe.orig` is the untouched retail backup (identical to
  `game/%MAINDIR%/DinoIsland.exe`). Copy it back to revert.
- `run/DINPUT8.dll` is our 32-bit proxy. It forwards `DirectInput8Create` and
  installs the live asset-reader detours. With `DINO_VIRTUAL_MOUSE=1` it also
  supplies scripted mouse input for the windowed harness. Delete it to run the
  original code path.

```powershell
# apply / verify / revert the no-CD patch on the copy
.\scripts\patch_nocd.ps1 -Target run\DinoIsland.exe
.\.venv\Scripts\python.exe tools\patch_nocd.py --verify run\DinoIsland.exe
.\.venv\Scripts\python.exe tools\patch_nocd.py --revert run\DinoIsland.exe

# rebuild the hook proxy
.\scripts\build-hook.ps1

# launch Free Mode -> IGUANO -> easy -> Start and capture the park
.\scripts\play-freegame-direct.ps1 -Seconds 55 -GameDir run -Shot notes\park-hook.png
```

The patch and hybrid mechanisms are documented in
[`docs/patches/no-cd.md`](docs/patches/no-cd.md) and
[`docs/decomp/hybrid.md`](docs/decomp/hybrid.md). The game runs under the
dgVoodoo2 Direct3D 8 / DirectDraw wrappers already present in `run/`.

### Windowed play

For normal play in a bordered 800x600 window, with the mouse left alone until
you click inside the game:

```powershell
.\scripts\play-windowed-game.ps1
```

The first run creates `run-window/` (Data junction, its own `save/`). The
window is a normal overlapped window with a title bar and minimize/close
buttons. It starts behind the current window and the launcher returns focus to
whatever you were using. Mouse input is suppressed until you click inside the
game; that click is passed through, so it can operate the UI (Windows may
raise the game when you click it). Click outside the game window to release it.
The cursor is never clipped or moved. `-NoClickCapture` disables the gate.

### Windowed x32dbg harness

The x32dbg harness now defaults to a **windowed 800x600 game** and **virtual
DirectInput input**, so automated runs do not take over the screen or the
physical mouse.

```powershell
.\scripts\setup-test-runtime.ps1

# windowed + virtual input (default)
.\scripts\x32dbg\play-freegame.ps1 -Seconds 30

# old fullscreen + physical-click path
.\scripts\x32dbg\play-freegame.ps1 -PhysicalInput -Seconds 30
```

`run-test/` uses a Data junction and a dgVoodoo2 config that forces the
window. The `DINPUT8.dll` proxy reads `run-test/dino_input.txt` and supplies
DirectInput mouse state directly, so the game is clicked without
`SetCursorPos` or `mouse_event`. The harness sends the game to the back and
restores the previous foreground window; screenshots work there, but retail
stops polling DirectInput when inactive, so click tests need the game active.
`-PhysicalInput` restores the old `run/` fullscreen behavior. See
[`docs/platform/windowed-testing.md`](docs/platform/windowed-testing.md).

## Documentation map

| Start here | Contents |
|---|---|
| [`notes/NOW.md`](notes/NOW.md) | Current dashboard: focus, lanes, next actions, blockers. |
| [`notes/STATE.md`](notes/STATE.md) | Done, in progress, next action, blockers, decisions. |
| [`TASKS.md`](TASKS.md) | Task queue with acceptance criteria and evidence. |
| [`notes/evidence-index.md`](notes/evidence-index.md) | Claim -> evidence -> test index. |
| [`notes/FINDINGS.md`](notes/FINDINGS.md) | Early inventory and evidence index. |
| [`docs/formats/`](docs/formats/) | 39 byte-level format specifications. |
| [`docs/decomp/`](docs/decomp/) | Function map, scheduler, AI, economy, save, media. |
| [`docs/platform/`](docs/platform/) | Viewer, terrain, visitors, HUD, UI click map, QoL and audio. |
| [`docs/ROADMAP.md`](docs/ROADMAP.md) | Four-phase plan and exit criteria; task status lives in `notes/STATE.md`. |

## Working method

Read `AGENTS.md`, `notes/NOW.md`, `notes/STATE.md` and `TASKS.md` before
starting. Pick one task, do the smallest experiment that resolves the unknown,
run the relevant tests, and record the evidence path. Format and behavior
claims need a hex dump, a decompiled function, a trace, or a parser that
round-trips real data.

The repository is set up for OpenCode agents: `orchestrator` is the default
primary agent, with `archivist`, `formats`, `ghidra`, `harness`,
`oracle-reviewer`, `port`, `recon`, `reviewer` and `triage` subagents. Skills
(`dino-hook`, `dino-format`, `dino-verify`, `dino-retail-run`, `ghidra-query`)
and commands (`/next`, `/verify`, `/handoff`, `/evidence`, `/compact-state`)
are in `.opencode/`.

## Legal

This is a personal preservation and interoperability project. The repository
does not contain retail game assets, extracted assets, or the patched
executable. Do not redistribute `game/`, `original/`, `run/`, or any retail
data; keep the original executable as the oracle and use the documented
patches only on your own copy.
