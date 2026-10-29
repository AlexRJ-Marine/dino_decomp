# Documentation index

How the project's docs are organized. Start with the injected dashboard
(`notes/NOW.md`) and the generated task list (`notes/OPEN-TASKS.md`); this
page is the map for the `docs/` tree.

## Start here

| Need | Go to |
| --- | --- |
| What is being worked on now | `notes/NOW.md` |
| Every unchecked task | `notes/OPEN-TASKS.md` |
| One row per retail feature: status, evidence, port entry, tests, gaps | [platform/feature-status.md](platform/feature-status.md) |
| Source layout, file naming, layering rules | [platform/naming.md](platform/naming.md) |
| Port lifecycle: boot/frame phases, where new code goes | [platform/app-shell.md](platform/app-shell.md) |
| C++ Core Guidelines policy, ownership rules, deliberate deviations | [platform/cpp-guidelines.md](platform/cpp-guidelines.md) |
| Which claim is proven where | `notes/evidence-index.md` |
| Phase-level picture | [ROADMAP.md](ROADMAP.md) |
| Lanes, builds, state updates, evidence workflow | [workflow.md](workflow.md) |
| How the retail game is run for evidence | [platform/windowed-testing.md](platform/windowed-testing.md) |

## Directories

- `formats/` — file layouts proven against retail samples (`.mesh`, `.skin`,
  `.skel`, `.anim`, `.sav`, `.wld`, `.bbi`, `.bai`, `.shw`, …).
- `decomp/` — recovered engine/game behaviour: scheduler, dinosaur actions,
  visitor/employee AI, economy, shows, hybrid hooks, function status.
- `platform/` — the portable port: terrain, vegetation, buildings,
  construction, feeding, visitors, danger grid, HUD/UI, save/load, audio,
  environment and the test harness.
- `patches/` — runtime patches (no-CD startup).
- `community/` — the XeNTaX/ZenHax post and replies.

## Conventions

- Evidence first: a claim in `docs/` points to a hex dump, a decompiled
  function/address, a runtime trace or a parser/test against real data.
  Working notes live in `notes/`; `docs/` holds the settled version.
- One load-bearing claim gets one row in `notes/evidence-index.md`. Feature
  status lives in `platform/feature-status.md`, not in `ROADMAP.md`.
- Generated files are not hand-edited:
  - `platform/ui-coordinates.md` / `.json` — regenerate with
    `tools/formats/ui_map.py build`;
  - `notes/OPEN-TASKS.md` — regenerate with `scripts/tasks.ps1 -Update`;
  - `notes/NOW.md` — update with `scripts/state.ps1`.
- Relative links in `platform/feature-status.md` and the screen/control ids in
  its table are checked by `tests/test_feature_matrix.py`.
- `original/` and `game/` are read-only; never copy retail assets into `src/`.

## Useful commands

```powershell
# build and test a lane
.\scripts\build.ps1 -BuildDir build\port
.\scripts\test.ps1 -Exe build\port\dino_port.exe -Fast

# quick/full verification
.\scripts\verify.ps1 -Fast
.\scripts\verify.ps1

# retail UI click map
.venv\Scripts\python.exe tools\formats\ui_map.py find OPEN_LABORATORY
.venv\Scripts\python.exe tools\formats\ui_map.py find --screen park-hud --active

# state updates
.\scripts\state.ps1 -Done "one line"
.\scripts\tasks.ps1 -Update
```
