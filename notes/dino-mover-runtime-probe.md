# Dino mover runtime probe - Cpg04 (campaign row 3)

Date: 2026-10-30. Retail `DinoIsland.exe` under the windowed x32dbg harness
(`run-test/`, virtual input, background-safe). Raw log (gitignored):
`notes/harness/dino-mover-runtime.log`, 2.2 MB, one 60 s simulation window.

Status: **the mover breakpoints fired and produced the park's real move
records.** Four claims in `notes/dino-mover-tick-recon.md` and
`notes/dino-ai-gap-audit.md` do not survive the runtime data (see
"Contradictions").

## Command

```powershell
.\scripts\x32dbg\start-campaign.ps1 -CampaignRow 3 -Seconds 30 -UnlockCampaigns `
    -MoverTrace -SimSeconds 60 -MoverHotMs 2000 `
    -Log notes\harness\dino-mover-runtime.log
```

`-MoverTrace` was added to `scripts/x32dbg/start-campaign.ps1` for this probe.
It installs the mover breakpoints after the park loads and (unlike the default
trace set) does **not** install the 0x56e670 action-selector breakpoint, which
fires ~500x/s and would dominate the frame time.

## Harness: the campaign park starts paused

The Cpg04 mission briefing/objectives panel **pauses the simulation**: every
dino's `FUN_00574350` receives `dt == 0` while it is up, so no mover state
advances. Measured with a 1.2 s `0x574350` sample after each dismissal attempt
(`-SimProbe`, `SIMPROBE|<label>|...|dt={p:[esp+4]}`):

| sample | what happened before it | hits | hits with dt > 0 |
| --- | --- | ---: | ---: |
| `PRE` | park loaded, panel up | 1182 | **0** |
| `POSTCLICK` | five clicks at (322,402) | 1189 | 1189 (run A) / **0** (run B) |
| `POSTESC` | one `Esc` scancode 0x01 | 1169 | **1169** |
| `POSTENTER` | one `Enter` 0x1c | 931 | 491 |
| `POSTLONG` | one click at (317,400) | 1160 | **0** |
| `POSTLAMPS` | clicks on the three objective lamps | 860 | **0** |

- The in-game virtual cursor is unreliable: the same five clicks at (322,402)
  dismissed the panel in one run and not in the next. `Esc` dismissed it both
  times. (317,400) is the HUD **objectives** button: clicking it re-opens the
  panel and re-pauses the park. Enter is ambiguous (about half the window ran).
- Consequence for the harness: **the mover probe sends no in-game clicks.** It
  opens the mission with the menu clicks, then sends one `Esc` scancode
  (`DINO_VIRTUAL_KEYS=1`, `run-test/dino_keys.txt`), and from there only
  breakpoints.
- Screenshots: `notes/harness/probe-postclick.png` (live park, clock 09:02)
  against `notes/harness/campaign-sim.png` (panel up, clock 09:00, dinos
  frozen).

## Breakpoints (60 s window)

| tag | address | hits |
| --- | --- | ---: |
| `MOVSTART` | `0x573f40` queue+start | 3282 |
| `MOVAQUEUE` | `0x573dc0` queue append | 99 |
| `TURNPRE` | `0x574727` turn arrival (before rotate) | 128 |
| `TURNPOST` | `0x57472c` after rotate | 3198 (all arrivals, not only turns) |
| `TURNPLAN` | `0x574d99` `FUN_00575410` call | 202 |
| `MOVTICK` | `0x574350` mover tick (2 s window) | 1250 |
| `EATSM` | `0x522300` eat state machine | 144 |
| `EMIT` | `0x569660` element->move emitter | **0** |
| `SETLEVEL` | `0x580e60` `saFoodSource::SetLevel` | **0** |
| `ACTDONE` | `0x571190` action completion | **0** |

## 1. The emitter is not on the park dino path

`0x569660` (and the action-completion switch `0x571190`) never fired in 60 s of
running park, while the queue at `0x573f40` was called 3282 times. The park
issues move records from the behaviour/action layer directly: `0x573f40` has
callers at `0x56f07d`, `0x56f0ff`, `0x56f130`, `0x56f1bb`, `0x56f94a`,
`0x56f98a`, `0x56f9fd`, `0x56fa93`, `0x56c65f`, `0x56c70e`, `0x56e799`
(`FUN_0056e670`), `0x56fc94`/`0x56fdce` (`FUN_0056fb70`), `0x5209d6`/
`0x520a91`/`0x520b85` (`FUN_00520880`) and the mover's own re-path paths.
`FUN_00569660` is reached from `0x56c5f4/0x56c788/0x56c7ff` only, i.e. the
item-element path, and no park dino took it.

## 2. Calling convention of the queue functions - the recon offsets are off by one dword

`FUN_00573dc0`/`FUN_00573f40` are `__thiscall` on the mover but take an **extra
leading `Vec2*` - the live position** - before the target. At entry (first
instruction, before the prologue):

```text
[esp+4]   Vec2*  position pointer (caller passes a stack copy of mover->0x0c)
[esp+8]   Vec2*  target point
[esp+0xc] f32    duration / mode-2 threshold
[esp+0x10] int   mode (0 free, 1 position, 2 timed)
[esp+0x14] int   kind
[esp+0x18] u8    instant flag
[esp+0x1c] u8    allowTurn
```

Probe line (kind `0x35`, mode 1, duration 0 - the "run to cell" marker):

```text
MOVSTART|t=601162859|mv=0E088F78|dino=0E088CB0|id=-1163067377|act=31|cd=00000000|food=41EF5C28|st=2|actk=4|pend=0|px=43F331B5|py=43B937E1|fx=BF1FEC02|fy=3F47E702|qb=0E136AF8|qe=0E136B10|walk=40280BE7|run=41280BE7|a1=0019FD68|a2=0019FD60|ax=43F30000|ay=43BC8000|a3=00000000|a4=00000001|a5=00000035|a6=00000001|a7=00000001
```

`a1`/`a2` are stack pointers (position, target), `ax/ay` = (486.0, 376.5),
`a3 = 0.0` s, `a4 = 1` (mode 1), `a5 = 0x35`, `a6 = 1`, `a7 = 1`.

## 3. Turn sequence - 45 deg confirmed, duration is NOT 1.0 s

128 turn-record arrivals, all `mode = 2`, measured from the `TURNPRE` facing to
the `TURNPOST` facing:

| kind | records | measured facing step |
| --- | ---: | --- |
| 3 | 57 | **+45.00 deg** (57/57) |
| 4 | 46 | **-45.00 deg** (46/46) |
| 5 | 25 | **180.00 deg** (24 x -180, 1 x +180) |

Runs of consecutive same-kind records: 1, 2 and 3 records were observed
(`{(3,1):26, (3,2):8, (3,3):5, (4,1):13, (4,2):6, (4,3):7, (5,1):25}`), i.e. a
plan of `steps = 2` really happens (a 2-record 90 deg turn is visible on one
dino from t=601162843 to t=601164046: -33.69 -> -78.69 -> -123.69). A run of 3
is probably two back-to-back plans; the log cannot separate them.

The record target is the current position (`rx/ry == px/py`), so turn records
never translate the dino.

**Durations differ per species and are not 1.0 s.** Each turn record's `+0x08`
(logged at `0x574727`, corroborated by `mover+0x30` in `MOVTICK`):

| dinos | walk | kind 3 | kind 4 | kind 5 |
| --- | ---: | ---: | ---: | ---: |
| most (15 of 20) | 1.52 - 3.03 | 1.2 s | 1.2 s | 1.5 s |
| 0xb3aa010, 0xb4f1b98, 0xb520fd8, 0xe0820c8 | 2.50 - 2.60 | **2.0 s** | **2.0 s** | **2.0 s** |
| 0xdc1cbe0 | 0.995 | **2.0 s** | - | 1.5 s |

```text
TURNPRE|t=601162843|mv=0E088F78|dino=0E088CB0|kind=4|mode=2|dur=3F99999A|rx=43F331B5|ry=43B937E1|px=43F331B5|py=43B937E1|fx=BF7E6F28|fy=3DE229E7|walk=40280BE7|run=41280BE7
TURNPOST|t=601164046|mv=0E28EFC8|dino=0E28ED00|fx=BF0E00D4|fy=BF55013F
```

`dur=3F99999A` = **1.2 s**; the 2.0 s group logs `dur=40000000`.

## 4. Walk/run speeds are map units per second (audit Open #1 closed)

20 dinos, `[mover+0x1fc]+0x54/+0x58` (= `saDinosaur+0x268/+0x26c`):

| walk (u/s) | run (u/s) | run/walk | dinos |
| ---: | ---: | ---: | ---: |
| 0.5813 | 2.7853 | 4.79 | 1 |
| 0.9951 | 5.3004 | 5.33 | 1 |
| 1.5237 | 6.0950 | 4.00 | 1 |
| 1.6732 | 6.6929 | 4.00 | 1 |
| 1.7455 | 6.9822 | 4.00 | 1 |
| 2.2681 | 9.0724 | 4.00 | 1 |
| 2.5018 | 10.0070 | 4.00 | 3 |
| 2.5992 | 10.3966 | 4.00 | 1 |
| 2.6257 | 10.5029 | 4.00 | 6 |
| 2.9087 | 11.6348 | 4.00 | 2 |
| 2.9545 | 11.8180 | 4.00 | 1 |
| 3.0286 | 12.1143 | 4.00 | 2 |

Per-frame displacement from consecutive `MOVTICK` samples, divided by the tick
`dt` that produced the step (`[esp+4]`):

| dino | attribute | measured | ratio |
| --- | ---: | ---: | ---: |
| 0xdc1cbe0 (walk) | 0.9951 | 0.9951 | **1.0000** |
| 0xe1149d8 (walk) | 2.2681 | 2.2683 | **1.0001** |
| 0xda6b090 (run)  | 2.7853 | 2.7853 | **1.0000** |

So `speed * dt` is applied verbatim in map units; the attribute needs no
scaling. Turn and gesture kinds measured **0.0000** displacement per frame
(their forward/lateral table slots are ~0), confirming that turn and gesture
records do not translate the dino.

```text
MOVTICK|t=601223625|mv=0DC1CEA8|dino=0DC1CBE0|dt=3BC49BA6|st=1|actk=39|px=44058000|py=43E18000|fx=BEE4F92E|fy=3F64F92E|sd=40000000|mt=4007BE7A|walk=3F7EBF8B|run=40A99D36
```

`dt=3BC49BA6` = 0.006 s, `sd=40000000` = 2.0 s, `walk=3F7EBF8B` = 0.9951.

## 5. One record per action is wrong: records come in lists and are re-issued every frame

3381 queue records over 60 s, but only **238 distinct `(kind, target)`**
instructions. Grouping all records emitted in the same millisecond for the same
dino (one element emission) gives 2242 groups and these profiles:

```text
 596 x ((0x35, mode 1, 0.0),)                       run-to-cell marker
 594 x ((0x35, mode 1, 0.0), (0x35, mode 1, 0.0))   ... emitted twice
 378 x ((0x34, mode 1, 0.0),)                       walk-to-cell marker
 327 x ((0x34, mode 1, 0.0), (0x34, mode 1, 0.0))
 105 x ((0x19, mode 2, 2.0), (0x19, mode 2, 2.0))   two identical records
  51 x ((0x19, mode 2, 2.0),)
  13 x ((0x1c, mode 2, 1.4),)
   9 x ((0x1d, mode 2, 1.4), (0x1e, mode 2, 0x7f7fc99e))   sentinel duration
   8 x ((0x27, mode 2, 2.0),)
   4 x ((0x27, mode 2, 2.0), (0x20, mode 2, 1.4), (0x20, mode 2, ~1.25-1.9))
```

- **A single action element emits 1, 2 or 3 records at once.** The `0x20` pair
  is a "move to the sub-target and return to self" list (the second record's
  target equals the dino position). This is a *profile list*, not one record.
- The `0x34`/`0x35` "walk/run to cell" markers are **re-issued every frame**
  while the move is in progress: one dino emitted the same record 883 times at
  ~16 ms spacing, another 911 times, a third 643 times. The queue-start
  function appends a fresh copy each frame (often two copies per frame, per
  `FUN_00573f40`'s "insert a copy when the queue is deeper than n").
- Kind histogram over all records: `{0x00:31, 0x09:1, 0x0b:9, 0x0c:10, 0x0d:11,
  0x0e:11, 0x0f:5, 0x12:7, 0x13:20, 0x14:8, 0x18:1, 0x19:273, 0x1c:13,
  0x1d:13, 0x1e:13, 0x20:86, 0x27:43, 0x34:1034, 0x35:1792}`.
  Kinds `0x0b..0x0f` are the `special_action == 5` "idle/random" arm of
  `FUN_00569660` (`kind = idx + 0xb`), consistent with the
  ObstAttack/LinkObstAttack profiles.

Example - one action on dino 0xb29dd50 (kind `0x27` at t=601172343, then a
two-record `0x20` lunge/return list):

```text
MOVSTART |t=601172343| kind=0x27 mode=2 dur=2.0  tgt=(534,390) pos=(534,390)
MOVAQUEUE|t=601172359| kind=0x20 mode=2 dur=1.4  tgt=(528,389) pos=(534,390)
MOVAQUEUE|t=601172359| kind=0x20 mode=2 dur=1.9  tgt=(534,390) pos=(534,390)
```

## 6. Eat: no dino Eat record, no food level consumed

- **No kind 8 (`Eat`) or kind 9 (`EatBait`) record from a dino action** in
  60 s. One kind-9 record appeared, from a single `MOVSTART` with no follow-up:

  ```text
  MOVSTART|t=601203312|...|dino=0B3AA010|...|a2=0DB489AC|ax=43F135BB|ay=43BEBE00|a3=3FE66666|a4=00000002|a5=00000009|a6=00000000|a7=00000001
  ```

  i.e. kind 9, mode 2, **duration 1.8 s** (`0x3FE66666`), target (482.4, 381.5),
  `allowTurn = 1`. For reference `Eat.bai` has a single effect element whose
  int vector is `[8]` and `EatBait.bai` `[9]` (`tools/formats/bai.py`), so the
  documented emitter would produce exactly one kind-8 record per Eat when it
  runs.
- `0x569660` never fired, so the Eat kind is a prediction for this park, not an
  observation.
- `0x580e60 saFoodSource::SetLevel` never fired. The eat state machine
  `0x522300` fired 144 times for **6 controllers** (24 hits each, 12 s cycle:
  state 2 -> 1 -> 0 -> 1 -> 2), and every one of them has
  `[ctrl+0x38]+0x54 == [ctrl+0x38]+0x58 == [ctrl+0x38]+0x5c == 0`:

  ```text
  EATSM|t=601163093|ctrl=0D91CBC8|vt=00642AC4|state=2|s8=00000001|s10=098CC548|s38=0D91CB50|f54=00000000|f58=00000000|f5c=0|ret=005221C5
  ```

  so the "consume one level" branch (`if (ctrl->0x38->0x54 != 0) SetLevel(...)`)
  can never run. The 6 controllers are **not dinosaurs**: none of the 6
  pointers is in the 20 dino pointer set, and the vtable `0x00642AC4` is
  written by `FUN_00521e40` in the visitor/park-behaviour region.

Reading: in Cpg04 the visitor-side eat controllers cycle without ever being
given a food source, and the dinosaur move records never include an Eat kind.
The port's "one record + consume one level per Eat" is therefore **not
supported by retail**; the closest observed analogue is one gesture record
(the kind-8 profile) plus the per-frame `0x34/0x35` walk marker that carries
the dino to the target.

## 7. Action id and countdown are not usable labels here

`[dino+0x2c0]` (`act`) is **0x1F** and `[dino+0x260]` (`cd`) is **0** on all
3381 records. The park actions in this mission do not set the action id (their
`.bai` elements only carry event kind 1 / no id event), so neither field can be
used to label a record or to measure an Eat duration.

## Contradictions

1. `notes/dino-mover-tick-recon.md` section 4: "duration = X[0x1bc + kind*4] =
   1.00 s for kinds 3, 4 and 5 in all five Actions.bad families". **Wrong at
   runtime**: 1.2 s (45 deg) and 1.5 s (180 deg) for most species, 2.0 s for
   five of the 20 Cpg04 dinos. A kind-9 gesture on one species is 1.8 s. The
   per-species duration table the mover reads is not the raw `Actions.bad`
   value; the file's five family blocks cannot explain 1.2/1.5/1.8/2.0.
2. `notes/dino-mover-tick-recon.md` section 7 and `notes/dino-ai-gap-audit.md`
   section 7 item 5: the element-to-move emitter `FUN_00569660` is *not* how
   the park's dinosaurs move. It never runs in the campaign park; the behaviour
   layer calls `0x573f40` directly (10 call sites) and re-issues the `0x34/0x35`
   marker every frame.
3. `notes/dino-ai-gap-audit.md` section 4.3: "one level is consumed per meal"
   via `FUN_00522300` -> `FUN_00580E60`. In Cpg04 the state machine runs but
   never has a source or level (`+0x54/+0x58/+0x5c` all zero) and `SetLevel`
   never fires; the controllers are not dinosaurs.
4. `notes/dino-mover-tick-recon.md` sections 0/6: the record layout is right,
   but the *argument* offsets quoted for the two queue functions are short by
   one dword (the `Vec2*` position argument) - see section 2 above.

Resolved: `notes/dino-ai-gap-audit.md` Open #1 (speed unit) - `saDinosaur+0x268`
and `+0x26c` are map units per second, applied as `speed * dt` with ratio
1.0000-1.0001 measured.

## Open

1. Which of the `0x573f40` call sites emit the per-frame `0x34/0x35` marker and
   the `0x27 + 0x20 + 0x20` lunge profile. One more run logging `{p:[esp]}`
   (return address) at `0x573f40` answers it.
2. The runtime per-species duration table: is it built by scaling
   `Actions.bad`, or from the animation blocks? Four species share 2.0 s while
   the rest share 1.2/1.5, so it is quantised, not proportional to size.
3. Whether an Eat action ever runs in a park with a stocked food source. The
   probe needs a park where `[ctrl+0x38]+0x54` is non-zero (a visitor at a
   food stand) or a mission that starts with hungry dinos.

## Harness changes made for this probe

- `scripts/x32dbg/start-campaign.ps1`: new `-MoverTrace`, `-SimProbe`,
  `-SimSeconds` and `-MoverHotMs` switches; the mover breakpoints; the
  keyboard-only briefing dismissal for `-MoverTrace`; the hot `0x56e670`
  selector and the fence-editing breakpoints are skipped under `-MoverTrace`
  (they fire hundreds of times per second); `-Log` no longer lets the stderr
  file overwrite the trace log when the name has no `.out.` in it.
- Probe logs: `notes/harness/dino-mover-runtime.log` (main run),
  `notes/harness/dino-sim-probe.log` and `notes/harness/dino-mover-runtime-run2.log`
  (dt ladder), `notes/harness/dino-mover-runtime-run1.log` (frozen park).
