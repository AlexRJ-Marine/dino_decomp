# Show performance budget `stShow+0x94` — runtime probe (Cpg04)

**2026-10-31.** Windowed x32dbg harness, retail `DinoIsland.exe` under
`run-test/`, virtual input only (no focus stealing, no pointer movement).
Question: what is the value and unit of the show budget `stShow+0x94` and of
the `dt` added in the performance state?

Result in one line: **`show+0x94 = 75.0` real seconds (= 50 park minutes at the
measured `time_scale` 40, not 0.667)**, because `dt` is the raw frame delta in
seconds, bit-identical to the dt handed to the park clock. In Cpg04 the show is
closed (`+0xd3 = 0`) and fails validation (`+0xe0 = 0`), so no performance
ran: there is no `SHOWSTART`/`SHOWTICK` series to report.

## Command

```powershell
.\scripts\x32dbg\start-campaign.ps1 -CampaignRow 3 -Seconds 30 -UnlockCampaigns `
    -SimSeconds 120 -ShowProbe -Log notes\harness\show-budget.log
```

`-ShowProbe` is new (this pass, `scripts/x32dbg/start-campaign.ps1`; other
modes untouched). It uses the `-MoverTrace` keyboard-only briefing dismissal
(Esc scancode `0x01`) and suppresses the hot `0x56e670` ACTOR trace. Log:
`notes/harness/show-budget.log` (164 KiB; two earlier diagnostic runs are
described below — the log quoted is the final documented run).

Attempts that did **not** work, recorded so they are not repeated:

- `SetBreakpointCondition 0x58c775, "[[[esi+0x14]+8]]>0x42340000"` and the same
  at `0x58c79e`/`0x58c7b5` **froze the park load** (x64dbg stopped producing
  output at "clicking Start"; the game never resumed). Conditional breakpoints
  on those addresses were abandoned.
- The replacement is the existing `Sample-Sim` pattern: install an
  unconditional breakpoint, run for a bounded window (600 ms), `pause`, `bc`,
  `run`. That is what all samples below use.
- `0x57b8da` is **state 5**, not state 4. Jump table `0x57ba08[0..7]`:
  `0 -> 0x57b2b2`, `1 -> 0x57b389`, `2 -> 0x57b418`, `3 -> 0x57b4ad`,
  `4 -> 0x57b5ab`, `5 -> 0x57b8da`, `6 -> 0x57b99e`, `7 -> 0x57b9c6`.
  The shared end check `+0x94 <= +0x90` is at `0x57b81f` and is reached from
  states 4 and 5; `0x57b8da` is only the state-5 case entry.

## Breakpoints

| marker | address | what it reads |
| --- | --- | --- |
| `SHOWBUDGET` | `0x58c4ba` (after `fstp [ebx]` at `0x58c4b8`) | `FUN_0058c480` schedule-list ctor (called from `0x587a6d`): `[ebx]` = `scheduleList[0]`, `[[ebx+0x1c]]`/`+4` = the two config floats, `[[ebx+0x14]+0x14]` = `1/time_scale` |
| `SHOWSTART` | `0x58c82c` | scheduler slot-rollover `FUN_0057b130(show, scheduleList[0])`; `edi` = show, `[esi]` = budget |
| `SHOWSET` | `0x57b211` (after `mov [ecx+0x94],eax`) | display-path overwrite `FUN_0057b200(show, value)` |
| `SHOWCHECK` | `0x57b81f` | `stShow::update(dt)` shared end check in states 4/5 |
| `SHOWTICK` | `0x57b8da` | state-5 `+0x90 += dt` |
| `SHOWBEGIN` / `SHOWPERF` | `0x57b110` / `0x57b150` | show pre-arm / per-performance reset |
| `SHOWSCHED` | `0x58c760` | `spScheduleManager::Update` entry: slot, `+0x28/+0x29`, clock, task array |
| `SHOWSHOW` | `0x58c790` | candidate show after the non-null load: `d3/e0/e4`, `+0x94/+0x90/+0x8c/+0x100` |
| `SHOWTEST` | `0x58c7c1` (after `call 0x58c390`) | the slot-window result |
| `SHOWDT` | `0x589e8f` | `stShow::update` call site in world update `0x589cd0`: `[esp]` = dt |
| `UPDT` | `0x586abe` | caller of `0x589cd0`: `[esp]` = dt |
| `RAWDT` | `0x58c2fe` | `FUN_0058c2f0(dt)`, the dt handed to the park clock: `[esp+8]` |

## Raw samples (verbatim)

Schedule-list ctor (once, during the world load):

```text
SHOWBUDGET|list=42960000|cfg40=453B8000|cfg1a4=44E10000|tm=09876BE8|scale=3CCCCCCD|t=639297906
```

Decoded: `list = 0x42960000 = 75.0`; `cfg40 = 0x453B8000 = 3000.0`;
`cfg1a4 = 0x44E10000 = 1800.0`; `scale = 0x3CCCCCCD = 0.025` (= 1/40, so the
time manager's `+0x10` time scale is **40.0**; `SHOWSCHED` prints
`ts=42200000 = 40.0`).

Scheduler + show gate at clock 1.74 s (`EARLY`):

```text
SHOWSCHED|EARLY|sched=098B1270|slot=0|f28=-1163067392|f29=230337792|clock=3FDE5601|prev=3FC4BC67|ts=42200000|off=44E10000|arr=09876C30
SHOWSHOW|EARLY|show=12769738|slot=0|d3=0|e0=0|e4=0|budget=00000000|elapsed=00000000|iter=1|slot100=1|clock=3FDE5601|t=639309218
```

At the first scheduled boundary, clock 47.53 s (`BOUNDARY`, 44 s later):

```text
SHOWSCHED|BOUNDARY|sched=098B1270|slot=0|f28=-1163067392|f29=230337792|clock=423E1EED|prev=423D5220|ts=42200000|off=44E10000|arr=09876C30
SHOWSHOW|BOUNDARY|show=12769738|slot=0|d3=0|e0=0|e4=0|budget=00000000|elapsed=00000000|iter=1|slot100=1|clock=423E1EED|t=639355671
```

`d3/e0/e4/budget` are identical in all 75 `EARLY` and 87 `BOUNDARY` sample
frames (the `BOUNDARY` sample spans clock 47.53-48.13 s).

`dt` in the same frame (`t=639310437`, then four following frames):

```text
SHOWDT|EARLY|show=12769738|dt=3E4CCCCD|t=639310437
UPDT|EARLY|owner=0A6FEE68|dt=3E4CCCCD|t=639310437
RAWDT|EARLY|dt=3E4CCCCD|clock=4015915D|prev=40151EAD|t=639310437

SHOWDT|EARLY|show=12769738|dt=3C54FDF4|t=639310453
UPDT|EARLY|owner=0A6FEE68|dt=3C54FDF4|t=639310453
RAWDT|EARLY|dt=3C54FDF4|clock=40225E2A|prev=4015915D|t=639310453

SHOWDT|EARLY|show=12769738|dt=3C03126F|t=639310468
UPDT|EARLY|owner=0A6FEE68|dt=3C03126F|t=639310468
RAWDT|EARLY|dt=3C03126F|clock=40233328|prev=40225E2A|t=639310468
```

Marker counts over the whole run:

```text
SHOWBUDGET 1   SHOWSTART 0   SHOWSET 0   SHOWCHECK 0   SHOWTICK 0
SHOWBEGIN  0   SHOWPERF  0   SHOWTEST 0
SHOWSCHED 162  SHOWSHOW 162  SHOWDT 82    UPDT 82       RAWDT 82
```

## Numeric conclusions

**1. The stored budget is 75.0, not 0.667.**

```text
scheduleList[0] = *(float*)(GeneralParams+0x110) * (1/time_scale)
                = 3000.0 * (1/40.0) = 75.0
```

`GeneralParams.bgd` (runtime object) has `+0x110 = 3000.0`,
`+0x114 = 1800.0` and `+0x1a0 = 40.0` (`tools/formats/general_params.py`).
The world ctor at `0x587a6d` calls `spScheduleManager` (`0x594b80`) with
arg1 = the float pointer `GeneralParams+0x1a0` (time scale) and arg2 =
`GeneralParams+0x110` (budget/offset pair). `0x594b80` builds the time manager
with `FUN_0058c1c0(40.0, 9)` and then calls `FUN_0058c480(scheduleList,
timeManager, GeneralParams+0x110)` (`0x594be1`); `fstp [ebx]` at `0x58c4b8`
stores `3000.0 * [timeManager+0x14] = 3000.0 * 0.025 = 75.0`. The earlier claim
(`notes/show-live-actor-recon.md` §9, `docs/decomp/park-time.md` lines 89-94:
"`cfg+0x1a0 = 40.0`, `time_scale = 60` -> 0.667") **mixed up the time-scale
value (`+0x1a0`) with the budget numerator (`+0x110`)** and used the wrong
time scale. The runtime pair is `(3000.0, 1800.0)` at `+0x110/+0x114` with
`time_scale = 40.0`, so
`scheduleList[0] = 75.0 s` and the slot-boundary offset is
`1800.0/40.0 = 45 s` (`end = i*90 - 45`, slot length `3600/40 = 90 s`).

**2. `dt` is the raw frame delta in real seconds.**

`SHOWDT`, `UPDT` and `RAWDT` are bit-identical for every one of the 82 frames
(all three were installed in the same window; `zip(show, raw)` and
`zip(show, upd)` compare equal). The value is the same dt the park clock
accumulates: the clock advanced `2.337 -> 3.131 s` in `609 ms` of wall time in
the same window. First frame after the debugger stop is the clamp
`0x3E4CCCCD = 0.2`; steady state is `0.006`-`0.013 s`.

Therefore `show+0x90` (`+= dt`) and `show+0x94` are in **real seconds**: the
dword passed to `stShow::update` is bit-identical to the dword passed to the
park clock in the same frame, and the chain `0x586abe -> 0x589cd0 ->
0x589e8f -> 0x57b220` forwards it unchanged (no `time_scale` factor on the
show path).

**3. `show+0x94` is not overwritten in this park — it is never written.**

The only writers of `+0x94` in the `stShow` method range (`0x579000-0x57c000`)
are `FUN_0057b130` (`0x57b13b`, the scheduler slot-rollover path at
`0x58c82c`) and `FUN_0057b200` (`0x57b20b`, the display path at `0x57b211`);
no `fstp`/immediate store exists. Both markers are 0 in 120 s. The live field
reads `budget=00000000` in both the `EARLY` and `BOUNDARY` samples.

## Negative result: Cpg04's show never performs

The campaign park has one show (`show=0x12769738`), placed in hour slot 1
(`show+0x100 = 1`), one iteration per day (`+0x8c = 1`), but:

- `show+0xd3 = 0` — **closed to the public** (lock/unlock; `docs/decomp/park-time.md`),
  written by `FUN_0057b0f0(show, bool)` (`0x57b0f0`, UI callers `0x516366`/`0x516aa1`).
- `show+0xe0 = 0` — **fails validation**. `FUN_0057a880` sets `+0xe0 = 2` and
  then clears it unless the show has: a non-empty `+0xa0` actor list, a
  non-empty `+0xb0` cage/team vector, a judge at `+0xcc`, at least two of the
  four judge-group slots (`+0xa0` actor `+0x48`), a Race/Obstacle judge kind
  `[judge+0x28]+0x40` in {1,2}, and a non-empty `+0x128` vector.
- `show+0xe4 = 0` — idle (0); the scheduler never starts it, so the state
  machine is never entered.

The scheduler path is `FUN_0058c760`: after the slot lookup it requires
`show+0xd3 != 0` (`0x58c790`), `show+0xe0 == 2` (`0x58c79e`) and, for the
rollover branch, `show+0xe4` in {0,1}; only then does it call
`FUN_0058c390` (`0x58c7c1`) and `FUN_0057b110`/`FUN_0057b130`. With
`d3=0` the window test is never reached (`SHOWTEST = 0`), so no
`SHOWBEGIN`/`SHOWPERF`/`SHOWSTART` and no state 4/5 tick can occur. This is a
property of the placed campaign show, not a clock problem: the clock reached
47.5 s (the first boundary is 45 s) and the same show object is still
`d3=0, e0=0`.

The UI will not open it either: `0x516a95` checks `show+0xe0 == 2` before
calling the open setter, and this show fails validation. Making it valid needs
the show editor (assign dinos to >= 2 judge groups and a Race/Obstacle judge),
which was not attempted here — no value was forced or guessed.

Mission rows do not help: all four Cpg04 missions share one
`Cpg04_Save.wld` (`notes/campaign-recon.md`), so another mission row loads the
same closed show. To observe a live performance, use a park whose show is
already `+0xd3 != 0` and `+0xe0 == 2` (a free-game show built in the editor,
or a save that contains an open show).

## Port implication (one line)

`ShowRuntime` duration = **50 park minutes** (`GeneralParams+0x110 / 60 =
3000/60`), i.e. **75 real seconds** at the measured `time_scale` 40; the show
starts **30 park minutes before the slot boundary** (`GeneralParams+0x114 / 60
= 1800/60`), not on the hour.

## Harness note

The load-bearing corrections are now in `notes/show-live-actor-recon.md` §9
and `docs/decomp/park-time.md`:

- the budget is `GeneralParams+0x110 / time_scale = 3000/40 = 75.0` seconds,
  not `+0x1a0 / 60 = 0.667`; `dt` is the raw frame dt in seconds (measured);
  `0x57b8da` is state 5 (state 4 is `0x57b5ab`), and `0x57b81f` is the shared
  end check;
- the slot length at `time_scale 40` is 90 s and the start offset is 45 s.

Raw log: `notes/harness/show-budget.log` (gitignored). Console transcript:
`%TEMP%\showprobe-console6.log`.
