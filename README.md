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

> Last updated: 2026-10-26. The authoritative running log is
> [`notes/STATE.md`](notes/STATE.md); the task queue is [`TASKS.md`](TASKS.md).

| Track | Status | Summary |
|---|---|---|
| Retail formats | Solved | 30+ specs in `docs/formats/`; parsers validate the full retail sets. |
| Asset tools | Working | Mesh/skeleton/animation/texture/audio/config readers plus CLI dumpers. |
| Portable viewer | Working | 10 islands through the retail menu, vegetation, skinned dinosaurs, paths/fences, visitors, retail menu/HUD, minimap, live clock, music/clicks. |
| Gameplay simulation | Early | Visitors only random-walk. Needs, economy, shows, labs and missions are researched but not ported. |
| Decompilation | Research-heavy | 180+ function entries documented; **2 functions replaced live**. |
| Test suite | Green | **291 tests pass**; `dino_viewer --selftest` passes 18 logic checks. |

### What works today

- **Islands:** all 10 retail islands start from the Free Game menu with the
  recovered camera (`camera.ini`: target centre, distance 39, elevation 52.7,
  yaw 0; FOV 80). `isl01` renders a 512x512 heightmap (522,242 triangles),
  basemap, 1,536 placed trees, skybox and water.
- **Actors:** `.mesh` + `.skel` + `.skin` + `.anim` load together; the
  allosaurus is rigid-skinned with the corrected skeleton-rest bind (pachy
  LOD2/LOD3 included) and plays walk/idle/run clips. `--start-dino` places it
  in a menu-started island.
- **Camera:** orbit and free-fly modes, terrain clamping, mouse look,
  rebindable keys.
- **Building:** 4-unit path/fence grid, terrain ray-picking, undo, save/load.
- **Visitors:** spawn on the path network and walk with a two-pose flipbook.
- **UI:** retail main menu (114 widgets, 9 pages), Free Game island selection,
  park HUD (2,694 widgets, 38 roots), panel switching, hover/pressed states,
  lab centres, minimap (2-D detail map, camera-relative, zoom/marker), live
  park clock and budget, Options Save/Load tab listing real `.sav` slots, and
  1024x768 HUD re-anchoring.
- **Media/QoL:** menu/park click sounds and island themes (chunk-streamed,
  looped), WAV playback, windowed/fullscreen/borderless modes, persisted
  window and key configuration, BMP screenshots.
- **Stability:** repeated 60,000-frame (~10 minute) runs with a flat working
  set; the latest retail-menu run (dino + music + clicks + minimap) stayed at
  167.1 MB and finished at 10:30 day 1.

### What is still missing

- The full visitor loop: enter -> choose attraction -> travel -> use -> leave,
  including visitor profiles, needs and the decompiled scoring function.
- Dinosaur needs, action selection, eating/sleeping and reactions; employee
  jobs, roles and wages.
- Economy settlement, show scheduling and the show/lab/campaign gameplay in
  the port (the original logic is decompiled).
- Park-state save/load through the save panel; only the slot browser and the
  path/fence file persistence exist today.
- Warning/request/campaign-end dialogs, dynamic list snapshots/sliders, and
  minimap detail-map water parity.
- Objective field names/weighting, staff job kind `0xb`, populated lab records
  and a few save semantics.
- Culling/LOD2 and final skybox/water parity.

### Current focus

1. UI-2 Phase D rest: wire Save/Load to the port state and add the dialog
   family.
2. UI-2 Phase C rest: dynamic list snapshots/sliders and minimap water polish.
3. SIM-2b: port the decompiled visitor choose -> travel -> use -> leave flow
   and bind visitor profiles/needs.
4. WORLD-3: capture a side-by-side comparison against the original game.

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
| Geometry and animation | `.mesh`, `.skin`, `.skel`, `.anim`, `anim_trans.dic`, `proportion.dat` |
| World and saves | `.lnd`, `.imp`, `.blf`, `.wld`, `.sav` (partial semantics) |
| Text and UI | `.fnt`, `.loc`, `MenuInterface.Ini`, `ParkInterface.Ini`, `MenuGraphicDB.ini`, `ParkGraphicDB.ini` |
| Gameplay data | `.shw`, `.gen`, `.bvi`, `.bbi`, `.bsi`, `.bni`, `.bei`, `.seq`, campaigns/missions |
| Media | `.tga`, `.wav`, `MenuSoundDB.ini`, `ParkSoundDB.ini` |

Validated scale: 536 meshes, 299 animations, 64 skeletons, 104 skins,
649 TGA textures and 231 WAV files. The exact layouts and evidence are in
[`docs/formats/`](docs/formats/).

## Decompilation progress

The retail executable has **7,273 functions** (725,709 instructions) in Ghidra.
[`docs/decomp/function-status.md`](docs/decomp/function-status.md) currently
tracks **167 entries** with address, purpose, status and evidence.

Solved or documented:

- **Loaders:** `.mesh`, `.skin`, `.skel`, `.anim` and font readers.
- **UI:** `moWidget::Load`, anchoring/resize, event setters, the nine-page menu
  state machine, park HUD binders and info-panel controller.
- **Runtime/save:** manager load/save order, world object class IDs `0x12`-`0x18`,
  the `.sav`/`.wld` object graph, simulation prefix, AI/camera tail.
- **Simulation:** `lpScheduler` and the ten-task park frame order,
  `spSimuManager`, `apBehaviorManager`, dinosaur action IDs 0-63, need gauges,
  the `0x1f` idle sentinel, visitor attraction scoring, employee job AI,
  park budget/daily settlement, park clock and show scheduler, and the
  campaign objective evaluator (33 verbs/predicates).
- **Presentation:** overlay INI semantics, sound database and DirectShow movie
  playback paths.

Only **two functions are replaced live** so far, both via the 32-bit
`DINPUT8.dll` proxy:

| Original | Purpose | Calls observed |
|---|---|---|
| `0x004663a0` | 64-byte (16-float) matrix reader | ~281,000 |
| `0x004809a0` | 12-byte (3-float) normal reader | ~97,000 |

The portable port is the main deliverable; the hybrid layer keeps the original
executable runnable while functions are recovered. See
[`docs/decomp/hybrid.md`](docs/decomp/hybrid.md).

## Repository layout

| Path | Purpose |
|---|---|
| `original/` | Read-only ISO-derived files (installer, docs). Never edit. |
| `game/%MAINDIR%/` | Extracted retail game tree. Read-only reference. |
| `run/` | Local playable copy, patched exe and hook DLL. Gitignored. |
| `notes/` | Findings, logs, evidence images, `STATE.md`. |
| `docs/formats/` | Proven data-format specifications. |
| `docs/decomp/` | Function map and subsystem research. |
| `docs/platform/` | Viewer, terrain, UI, audio and QoL notes. |
| `docs/patches/` | Documented runtime patches. |
| `tools/formats/` | Python parsers and exporters. |
| `src/` | Portable C++20 port and the hook DLL. |
| `tests/` | Python and C++ integration tests. |
| `scripts/` | Build, test, Ghidra, patch and play helpers. |
| `.opencode/agents/` | Project-specific OpenCode agents. |

## Building the portable port

This builds our port, **not the retail game**. The output is
`build/mingw/dino_viewer.exe` plus `dino_tools.exe`. `run/DinoIsland.exe` is a
copy of the retail executable used as an oracle; it is never built here.

### Prerequisites

| Requirement | Notes |
|---|---|
| Windows 10/11 | The scripts are PowerShell. |
| Python 3.11+ | A local `.venv` holds the Python tools. |
| w64devkit | Extract to `C:\Tools\w64devkit`. MinGW-w64 toolchain for the viewer and the 32-bit hook DLL. |
| SDL3 3.4.18 | `scripts/setup-sdl3.ps1` installs it under `C:\Tools\SDL3`. |
| CMake + Ninja | Installed into `.venv` by `requirements.txt`. |
| Ghidra 12.2 + JDK 25 | Only needed for reverse engineering, not for the viewer build. |

### One-time setup

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install --upgrade pip
.\.venv\Scripts\python.exe -m pip install -r requirements.txt

.\scripts\setup-sdl3.ps1
.\scripts\build.ps1
```

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
| `--selftest` | Run the 18 grid/visitor/build logic checks and exit. |
| `--free-camera` | Start in free-fly mode. |
| `--build` | Enable path/fence placement (`1` path, `2` fence, `0` off, `U` undo). |
| `--visitors N` | Spawn visitors on the placed path network. |
| `--skybox DIR` | Six `front/back/left/right/top/bottom` PNG faces. |
| `--frames N --screenshot out.bmp` | Deterministic headless capture. |
| `--wav FILE --mute` | Play a retail WAV; `M` mutes. |

Run `dino_viewer.exe --help` for the complete list.

### Tests

```powershell
.\scripts\run-tests.ps1
```

The suite covers the Python parsers, the C++ viewer via deterministic
subprocess runs, screenshot checks, and the viewer selftest. Tests that need
retail data skip automatically when `game/%MAINDIR%` is absent.

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
`SetCursorPos` or `mouse_event`. `-PhysicalInput` restores the old `run/`
fullscreen behavior. See
[`docs/platform/windowed-testing.md`](docs/platform/windowed-testing.md).

## Documentation map

| Start here | Contents |
|---|---|
| [`notes/STATE.md`](notes/STATE.md) | Done, in progress, next action, blockers, decisions. |
| [`TASKS.md`](TASKS.md) | Task queue with acceptance criteria and evidence. |
| [`notes/FINDINGS.md`](notes/FINDINGS.md) | Early inventory and evidence index. |
| [`docs/formats/`](docs/formats/) | 25 byte-level format specifications. |
| [`docs/decomp/`](docs/decomp/) | Function map, scheduler, AI, economy, save, media. |
| [`docs/platform/`](docs/platform/) | Viewer, terrain, visitors, HUD, QoL and audio. |
| [`docs/ROADMAP.md`](docs/ROADMAP.md) | Four-phase plan and exit criteria; task status lives in `notes/STATE.md`. |

## Working method

Read `AGENTS.md`, `notes/STATE.md` and `TASKS.md` before starting. Pick one
task, do the smallest experiment that resolves the unknown, run the relevant
tests, and record the evidence path. Format and behavior claims need a hex
dump, a decompiled function, a trace, or a parser that round-trips real data.

The repository is set up for OpenCode agents: `orchestrator` is the default
primary agent, with `formats`, `port`, `recon`, `ghidra` and `reviewer`
subagents. See `.opencode/agents/`.

## Legal

This is a personal preservation and interoperability project. The repository
does not contain retail game assets, extracted assets, or the patched
executable. Do not redistribute `game/`, `original/`, `run/`, or any retail
data; keep the original executable as the oracle and use the documented
patches only on your own copy.
