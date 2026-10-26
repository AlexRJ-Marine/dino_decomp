# Working regime

How the project coordinates agents, builds, tests and evidence. The injected
dashboard is `notes/NOW.md`; this page is the longer explanation.

## Read order

1. `AGENTS.md` (project rules).
2. `notes/NOW.md` (compact dashboard: environment, lanes, focus, next actions).
3. `notes/OPEN-TASKS.md` (generated list of unchecked `TASKS.md` items).
4. `notes/lanes.md` (claim a lane and a build directory).
5. Open `TASKS.md` / `notes/STATE.md` only for the task you claimed.

`notes/STATE.md` is the long archive; do not grow it when a one-line history
entry will do. `notes/history/<yyyy-mm>.md` is the dated completion log.

## State updates

```powershell
.\scripts\state.ps1 -Done "HYB-7 .fnt loader replaced (5 fonts, HUD renders)"
.\scripts\state.ps1 -Next "Wire the PROGRESS gauges"
.\scripts\tasks.ps1 -Update          # regenerates notes/OPEN-TASKS.md
.\scripts\state.ps1 -Show
```

`-Done` updates `NOW.md` and appends to `notes/history/`. Keep `NOW.md` under
~80 lines; the `archivist` agent (or `/compact-state`) moves older material to
history.

## Lanes and builds

| Lane | Build | Runtime |
| --- | --- | --- |
| port/viewer | `build.ps1 -BuildDir build/port` | `run-window/` |
| hybrid/hooks | `build-hook.ps1` (32-bit DLL) | `run-test/` |
| formats | none | none |
| harness | none | `run-test/`, `run-window/` |

See `notes/lanes.md` for ownership. Never build into another lane's directory
and never kill all `DinoIsland`/`dino_viewer` processes; stop the exact PID.

## Verification

```powershell
.\scripts\verify.ps1 -Fast     # quick: env + read-only check + build + non-slow tests
.\scripts\verify.ps1           # full: includes tests/test_viewer_cpp.py (slow)
.\scripts\test.ps1 -Exe build\port\dino_viewer.exe -Pattern phenotype
```

`verify.ps1` fails when `original/` or `game/` has local changes and runs
`git diff --check`. The `slow` marker is registered in `pytest.ini`.

## Evidence

- Runtime artifacts go to `notes/harness/` (gitignored); use
  `scripts/capture-evidence.ps1 -Name ... -Command ... -Path ...` to record the
  command, commit and path in `notes/harness/manifest.md`.
- Load-bearing claims get one row in `notes/evidence-index.md`
  (claim -> evidence file/address -> test).
- `docs/formats/` and `docs/decomp/` hold the proof; `NOW.md`/`STATE.md` link,
  they do not restate numbers.

## Skills and commands

Skills (load by ID): `dino-hook`, `dino-format`, `dino-verify`,
`ghidra-query`, `dino-retail-run`.

Commands: `/next`, `/verify`, `/handoff`, `/evidence`, `/compact-state`.

## Agent routing

| Agent | Use for | Notes |
| --- | --- | --- |
| `orchestrator` | primary session | picks one task |
| `port` | C++ viewer/world/UI/formats | `build/port` |
| `formats` | parsers, exporters, format docs | no build |
| `recon` | Ghidra + runtime evidence, notes | writes notes only |
| `ghidra` | one narrow binary question | evidence, no edits |
| `harness` | build/run isolation, x32dbg, virtual input | owns scripts/run dirs |
| `triage` | failing build/test/runtime log | fast model, returns next command |
| `reviewer` | diff review (fast tier) | no edits |
| `oracle-reviewer` | hooks/ABI/formula review vs Ghidra | strong model |
| `archivist` | compact NOW/TASKS/history | fast model, no code |
| `explore` | broad codebase lookup | no edits |

Independent research (Ghidra questions, log triage, archiving) can run in the
background while the primary lane continues.
