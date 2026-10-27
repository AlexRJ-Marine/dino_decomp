# Target policy trace: live campaign park (chooser scorer + availability gate)

Status: **answered in the live dinosaur park.** The chooser scorer uses the
**no-target** (`0x006430AC`) and **actor** (`0x006430E4`) classes. The
availability gate also sees the **box** class (`0x00642FFC`). **No new class
vtable appeared.** Reaction 1 (`0x521d80`) fired 20 times and created 20
**actor** targets; reaction 2 (`0x521d10`) did not fire in this window.

The full run kept the park simulating for 32.4 s after the first behaviour
tick (requirement: at least 20 s after the first dinos).

## Why the campaign park, not Free Game

The free-game `IGUANO` park and the loaded `cppwrite` save both show a **HUD
dinosaur count of 0** (`notes/harness/load-cppwrite-hud3.png`: cash 75000,
visitors 0, dinosaurs 0). The evaluator/scorer never runs there because there
are no live park dinos. The campaign Cpg04 mission has 20 dinosaurs
(`scripts/x32dbg/start-campaign.ps1` header and `notes/campaign-unlock-runtime.md`),
so the trace uses that park.

`run-test/save` lacked `campaign-progress.txt`, so the campaign row could not
be selected. Staged for the run:

```powershell
Copy-Item run\save\campaign-progress.txt run-test\save\campaign-progress.txt -Force
Copy-Item run-test\save\dino.dat run-test\save\dino.dat.target-trace-backup -Force
[IO.File]::WriteAllBytes((Resolve-Path 'run-test\save\dino.dat'), [byte[]](0x0F,0,0,0))
```

`dino.dat` was restored to its original `0x01` after the run; the backup was
removed. `campaign-progress.txt` remains staged in `run-test` (remove it to
restore the exact pre-run test runtime).

## Command

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\notes\harness\target-vtable-trace.ps1 -Campaign `
  -SecondsAfterStart 35 -PostBehaviorSeconds 20 -MaxTotalSeconds 70 `
  -PolicyWindowSeconds 18 `
  -Log notes\harness\target-vtable-campaign-final.log
```

The script launches `run-test/DinoIsland.exe` under x32dbg headless with
`DINO_VIRTUAL_MOUSE=1` and `DINO_FORCE_INPUT_ACTIVE=1`, clicks Campaigns ->
row 3 -> campaign Start -> mission 0 -> mission Start, dismisses the briefing,
and logs the target breakpoints with virtual input only. No `-PhysicalInput`.

Analysis:

```powershell
.venv\Scripts\python.exe notes\harness\analyze-target-trace.py `
  notes\harness\target-vtable-campaign-final.log `
  notes\harness\target-vtable-campaign-final.tsv
```

## Class vtables and action contexts

`SCORE_GETTARGET` is `0x0056e455` in `FUN_0056e410`; `SCORE_THREAT` is
`0x0056e4d1`; `GATE_GETTARGET` is `0x0056d9db` in the availability gate.

| Class | vtable | Scorer hits | Scorer action names | Gate hits | Gate action names | Targets | tx/ty |
|---|---|---|---:|---|---:|---|---|---|
| no-target | `0x006430AC` | 414 | Exploration 138, RunAway 138, Sleep 138 | 139 | Exploration 139 | 5 | `-1/-1` (all) |
| actor | `0x006430E4` | 159 | DinoAttackForCarniv 159 | 571 | DinoAttackForHerbiv 571 | 14 scorer / 17 gate | real, `0x182..0x219` / `0x14A..0x18C` |
| box | `0x00642FFC` | **0** | - | 446 | Eat 446 | 12 | - |
| new | (none) | - | - | - | - | - | - |

- The box class is installed and gated for `Eat`, but none of its nodes were
  scored (`kind != 1`) during this window; that is why the scorer sees only
  no-target and actor.
- No-target actions write `tx=-1, ty=-1`; the threat term is skipped.
- Actor actions write real target coordinates. Target cells are
  `(tx>>3, ty>>3)`: `tx` 0x182..0x219 -> cells 48..67, `ty` 0x14A..0x18C ->
  cells 41..49. Acting-dino cells are `(ax>>3, ay>>3)`: 48..67 / 41..56
  (no-target) and 48..66 / 41..49 (actor).
- The same target object is shared across an action list: e.g. target
  `136FB528` / owner `12A81398` / tactor `12B99828` is used by
  `DinoAttackForHerbiv` at the gate (`action=136FB590`) and by
  `DinoAttackForCarniv` at the scorer (`action=136FB5B8`). This matches the
  "one target per action list" contract (`FUN_00569640`/`FUN_0056a9a0`).

Representative hits (the full ordered set is the TSV):

```text
elapsed  marker            vt        class      target    actor(raw) owner     tactor    action    name                 tx   ty
35.398   GATE_GETTARGET    006430AC  no-target  12CB4768  00000000   12DD9448  12CC1464  12B6FAD0  Exploration          -    -
35.400   GATE_GETTARGET    00642FFC  box        12D198B8  12CBF468   00000000  -         12B907D0  Eat                  -    -
35.403   SCORE_GETTARGET   006430AC  no-target  12CB4768  00000000   12DD9448  12CC1464  12B6FAD0  Exploration          -1   -1
35.421   GATE_GETTARGET    006430E4  actor      136FB528  00000000   12A81398  12B99828  136FB590  DinoAttackForHerbiv  -    -
35.460   SCORE_GETTARGET   006430E4  actor      136FB528  00000000   12A81398  12B99828  136FB5B8  DinoAttackForCarniv  1DF  176
```

Notes on the fields:

- `actor` is the raw target field `[target+0x28]` requested in the task. It is
  **0** for both the no-target and actor classes in this build. For the box
  class, `+0x28` is the box/show-element pointer (`12CBF468` in the sample),
  not an actor.
- `owner` is `[target+0x38]`, the owner context. `tactor` is
  `[[target+0x38]+0x28]`: for the actor class it is the **perceived actor**
  (14 distinct, e.g. `12B99828`), which is what the actor target's `+0x14`
  reads. For the no-target class it resolves to an object in the self/action
  owner (5 distinct); it is **not** the same pointer as the `SCORE_THREAT`
  `actor` field, so do not label it the acting dino without further evidence.
  The TSV has all three columns.
- The `SCORE_THREAT` line carries the acting dino `actor=[chooser+0x28]` and
  its position `ax/ay` separately.

## The template filler (`0x56e190` / `0x56e1cb`)

`TMPL_ACTION` fired 268 times at `0x56e1cb` (after `MOV ESI,[EDI+0x08]`), with
26 distinct first-dwords. Those values are **heap pointers** (`0x12xxxxxx`..
`0x14xxxxxx`), not class vtables. They are the `action+0x08` wrapper at the
reaction template-fill moment, before the final policy object is installed.
Example for the same action:

```text
34.460  TMPL_ACTION      action=12B6FAD0 target=12CBF540 vt=12B6FAD0 rec=09827668
35.403  SCORE_GETTARGET action=12B6FAD0 target=12CB4768 vt=006430AC apol=12CB4768
```

So the class vtables must be read at the scorer/gate (`node+0x3c` target), not
from `[action+0x08]` at template-fill time. This is consistent with the
factory / one-fill-lag described in `notes/dino-target-vtable-recon.md` and
`notes/NOW.md`.

## Reaction starters

- `REACT1_ENTER` (`0x00521d80`): **20 hits**, all at 34.1-35.1 s.
- `REACT2_ENTER` (`0x00521d10`): **0 hits** in this window.
- `REACT_TARGET` (`0x0056af65`, the target created by `FUN_0056ac80`):
  **20 hits**, every one `vt=0x006430E4` (actor).

First/last reaction targets:

```text
34.306  REACT_TARGET target=1380B928 vt=006430E4 owner=1377C240 chooser=12EFC4D0
35.056  REACT_TARGET target=13614210 vt=006430E4 owner=136C1180 chooser=12BA3740
```

`ownerctx=BAADF00D` in the raw lines is the chooser `+0xa4` field before it is
initialised; it is not a valid pointer. Use the target's `+0x38` `owner` for
the owner context. Reaction 1 is the live producer of actor targets in this
park; reaction 2 stayed cold.

## Timing

```text
21.778 s  click Campaigns
31.377 s  click mission Start
32.377 s  PARK_INIT_ENTER
34.120 s  first REACT1_ENTER / first behaviour tick
35.403 s  first SCORE_GETTARGET
36.732 s  campaign world-manager marker
64.871 s  last SCORE_GETTARGET
64.671 s  policy breakpoints disabled (29.266 s after first score)
66.488 s  pause: postBehavior=32.359 s, total=66.5 s
```

The scorer was live for ~29.5 s; the pause is 32.4 s after the first behaviour
tick, so the >=20 s-after-dinos requirement is met.

## Raw logs

- `notes/harness/target-vtable-campaign-final.log` - x32dbg transcript plus
  probe lines, **UTF-8 without BOM**, 954 KB. Probe lines carry
  `t=<GetTickCount ms>`; breakpoint-command fields have spaces stripped.
- `notes/harness/target-vtable-campaign-final.tsv` - all ordered rows
  (`SCORE_GETTARGET`, `SCORE_THREAT`, `GATE_GETTARGET`, `TMPL_ACTION`), UTF-8,
  columns: `seq marker elapsed tick vtable class target actor owner tactor
  action name tx ty ax ay node`. The `class` is `wrapper` for `TMPL_ACTION`
  rows (their `vtable` is the action+0x08 first dword, not a class vtable).
- `notes/harness/target-vtable-campaign-final-console.log` - PowerShell host
  output, UTF-8 with BOM (`Out-File`).
- `notes/harness/target-vtable-trace.ps1` - note-only trace script.
- `notes/harness/analyze-target-trace.py` - parser/summary.
- Pilot runs that established the campaign navigation fix:
  `target-vtable-campaign-pilot3.log` (10 s window, same classes) and
  `target-vtable-campaign-full.log`.

## Harness state

- `run-test/save/dino.dat` restored to `01 00 00 00`; the temporary
  `dino.dat.target-trace-backup` was removed.
- `run-test/save/campaign-progress.txt` is staged (copy of `run/save`); it was
  missing before. Remove it to restore the exact previous test runtime.
- No `DinoIsland.exe` or `headless.exe` left running; `run-test/dino_input.txt`
  is at mouse-up (`526 531 0`), not a held button.
- This probe made no `src/` edits and changed no notes outside
  `notes/harness/`. The working tree has concurrent changes from another lane
  (`CMakeLists.txt`, `src/viewer`, `src/world`, `opencode.jsonc`); they are not
  from this trace.
- The x32dbg `.dd32` database may retain the probe breakpoints; both this
  script and `play-freegame.ps1` clear software breakpoints with `bc` on the
  next run, and this script also sends `bphwc`.
- The free-game/loaded-save HUD evidence is in
  `notes/harness/direct-t5-28s-hud2.png` (free game: cash 75000, visitors 0,
  dinosaurs 0) and `notes/harness/load-cppwrite-hud3.png` (loaded cppwrite:
  cash 75000, dinosaurs 0).
