# Park clock and show schedule

Status: **decompiled**; the day-counter increment source is still open.
`0x0058dfc0` is the show/attraction render-record builder, not the clock.

## Schedule manager

`spScheduleManager` (`0x00594b80`) owns:

```text
+0x00  task vector       # 0x0058c480, 12 entries, stride 0x2c
+0x04  time manager      # 0x0058c1c0, 9-slot timing table
```

### Time manager

`0x0058c1c0(time_scale, 9)`:

| Offset | Meaning |
| ---: | --- |
| `+0x00` | base hour (integer) |
| `+0x04` | **day counter** (HUD shows `day + 1`) |
| `+0x08` | elapsed game time (float seconds) |
| `+0x0c` | previous elapsed time |
| `+0x10` | time scale |
| `+0x14` | `1 / time_scale` |
| `+0x18` | minutes per second (`time_scale / 60`) |
| `+0x1c` | inverse |
| `+0x20` | hours per second (`time_scale / 3600`) |
| `+0x24` | inverse |
| `+0x28` | new-hour flag (one frame) |
| `+0x29` | end-of-day flag |

### Advance (`0x0058c2f0`)

```text
new_time = old_time + dt
if int(new_time * +0x20) >= 11:
    +0x29 = 1          # day finished
    return
if int(new_time * +0x20) != int(old_time * +0x20):
    +0x28 = 1          # new hour
+0x08 = new_time
+0x0c = old_time
```

A park day is **12 hours**; the 11th hour ends it. The base hour (`+0x00`)
comes from the park/scenario.

### Read (`0x0058c350`)

```text
hour   = int(+0x20 * +0x08) + +0x00
minute = int(+0x18 * +0x08) % 60
day    = +0x04
```

`0x00526010` formats the HUD time as `%.2d:%.2dAM` / `%.2d:%.2dPM`
(or `%.2d:%.2d` when `panel+0x8d` is clear), writes the time to
`PARK_CURRENT_TIME`, and writes `day + 1` as `%d` to `PARK_CURRENT_DAY`.
Both widgets are created by `0x005234e0` at `panel+0x1e0` / `panel+0x1e4`.

## Show schedule (`0x0058c760`)

`spScheduleManager::Update` (`0x00594d00`) advances the clock and then:

- on a normal slot (`+0x28 == 0`, `+0x29 == 0`), keeps the current slot if
  the show is running (`+0xd3 != 0`), is in state `+0xe0 == 2`, has
  `+0xe4 == 0`, and passes the time-window check; then `0x0057b110` starts it;
- on a new hour or end of day, resets the slot and scans slots 1..10; for
  each valid show it calls `0x0058c8f0` (feature/actor list) and
  `0x0057b130` (activate).

`0x0058c390(time)`:

```text
window = (int(time * +0x20) + 1) * +0x24 - time * +0x14
active = (+0x0c <= window <= +0x08)
```

`0x0058c8f0(manager, show)` fills the show's participant list (`+0xf4`) from
its cage/team assignment vector (`+0xb0`), skipping any dino that already
appears in another show's list - that is the `dinoexclusiveshow` rule.
`0x0057b110` pre-arms a show (`+0x14 = 1`, `+0xd4 = 1`, state `+0xe4 = 1`)
and `0x0057b130(show, budget)` activates it (`+0xd5 = 1`, `+0x94 = budget`).

### Show runtime state machine (`0x0057b220`)

`0x0057b220(show, dt)` runs every frame for a placed attraction. The state is
the dword at `show+0xe4` (0..7); the live fields are:

| Offset | Meaning |
| ---: | --- |
| `+0x88` | performances done today |
| `+0x8c` | performances per day (the `ITERATION` buttons) |
| `+0x90` | current performance elapsed time |
| `+0x94` | current performance budget (time limit) |
| `+0x98` | repeat/cooldown timer |
| `+0xb0`/`+0xb4` | cage/team assignment vector |
| `+0xf4`/`+0xf8` | participant list built by `0x0058c8f0` |
| `+0x14` | dirty flag (forces a redraw) |
| `+0xd1` | cancel request |
| `+0xd2` | running flag |
| `+0xd3` | open to the public (lock/unlock) |
| `+0xd4` | scheduled this hour |
| `+0xd5` | activated by `0x0057b130` |
| `+0xd6` | started |
| `+0xd7` | repeat requested |
| `+0xda` | finished |
| `+0xdb` | capture requested |
| `+0xe0` | validation state (2 = valid) |
| `+0x100` | hour slot 1..10 |

| State | Behaviour |
| ---: | --- |
| 0 | idle. `+0xd1` clears the actor list and calls `0x0057b150`. `+0xd5` starts a performance: `+0x90 = +0x88 = 0`, `+0xd6 = 1`, `+0xd2 = 0`, camera `0x00484d30(+0x7c,+0x80)`, each assignment `0x00571190`, `0x0057ba50`, `0x00576b80(0)`, `0x0057fe40(+0x94)`. |
| 1 | pre-armed by `0x0057b110`; same start branch as 0 when `+0xd5` is set. |
| 2 | intro: `+0x90 += dt`, `vtable+0x18`, `0x0057ff60`; when every byte flag at `+0x7c` is set, notify the `+0xa0` actor list (`vtable+0x18`) and go to 3. |
| 3 | running: wait until every actor in `+0xa0` has `+0x44 == 2`; then `+0x36 = 1`, state 4, and per element/participant `0x005838c0` / `0x00583b60`. |
| 4 | performance: `+0x90 += dt`; captured dinos (`+0x16 < 1`, `+0x4b4 == 3`) and the `+0x138` list are appended to the participant list (`0x0058ba00`); when `+0x94 <= +0x90` -> `+0xda = 1`, state 6, `0x00580010`; else if the judge object `+0xcc` `vtable+0x20` returns true -> `+0x88 += 1`, reset elements (`0x005838b0`), `+0xd7 = 1`, state 5, `+0x98 = 0`. |
| 5 | repeat: `+0x90 += dt`, `+0x98 += dt`; when `+0x98 > *(float *)(DAT_00675770 + 8)`: if `+0x8c <= +0x88` -> state 6 + `0x00580010`; otherwise reset the actor list (`vtable+4`), `0x0057ba50`, `+0xd7 = 1`, and stay in 5 for another performance. |
| 6 | drain: when the participant list `+0xf4` is empty -> state 7. |
| 7 | teardown: clear the actor list, `0x00576b80(0)`, state 0. |

The port's `ShowRuntime` (`src/world/show_runtime.h`) keeps the same state
numbers and field meanings, with the `.bfi` arena animation replaced by a
performance length (`duration_minutes_`, 30 park minutes by default):

- state 2 counts `elapsed` in park minutes;
- a completed performance calls the judge callback once and increments
  `done`;
- `done >= iterations` returns the show to state 0, otherwise it stays in
  state 2 and starts the next performance immediately. States 5..7
  (cooldown/drain/teardown) are collapsed into that direct return, and the
  `+0xa0` actor list is not simulated.

`start_hour` mirrors the schedule manager's slot scan (`0x0057b110` pre-arm,
`0x0057b130` activate) and `reset_day` mirrors `0x0058c2d0`.

## Serialization

The time manager writes `+0x04`, `+0x08`, `+0x0c`
(`0x0058c210`/`0x0058c260`); the task vector writes a count and one
object-handle per entry (`0x0058c620`/`0x0058c6b0`).

## Day rollover

`0x00524630` (park daily update) checks the end-of-day flag `+0x29` and
runs:

```text
FUN_0058ae60(simulation)   # daily actor/dino reset
FUN_0058c2d0(time)         # day += 1, clear flags, elapsed = 0
FUN_0058ad00(simulation)   # daily economy settlement
```

`0x0058c2d0`:

```text
time+0x04 += 1
time+0x28 = 0
time+0x29 = 0
time+0x08 = 0
time+0x0c = 0
```

The settlement `0x0058ad00` is documented in `docs/decomp/economy.md`
(shop/category costs, gate costs, ticket income, roll-up `0x00585e10`,
shop daily counters `+0x38` → `+0x3c`).

## Confirmed start values (follow-up)

- The schedule manager ctor passes `PUSH 9` as the base hour and `PUSH 9` as
  the timing-table size; `0x58c1c0` stores `9` in `+0x00`. Free Game starts at
  **09:00**, not a scenario value.
- The `time_scale` argument is a float from the park/scene object
  (`+0x1a0`), not `Dino.ini GameSpeed`. `GeneralParams.bgd` contains `60.0`
  adjacent to the `10` written at `+0x1a0`; 60.0 matches the observed pace and
  is the port default. `GameSpeed` only drives the scheduler timer
  (`0x00506d20`).
- `0x5234e0` clears `panel+0x8d`, so the HUD uses `%.2d:%.2d` (24-hour, no
  AM/PM) for Free Game.
- The day counter has no explicit initializer; fresh zeroed memory gives 0
  (HUD day 1).

## Open

- The fixed economy-category labels; see `docs/decomp/economy.md`.
- Runtime confirmation of the 60.0 time scale (the static owner of the
  `+0x1a0` float is not fully resolved).

## Evidence

`notes/park-time-recon.md` — function-by-function evidence and offsets.
`0x00594b80`, `0x0058c1c0`, `0x0058c2f0`, `0x0058c350`, `0x0058c390`,
`0x0058c760`, `0x0058c210`, `0x0058c260`, `0x0058c620`, `0x0058c6b0`,
`0x00526010`, `0x005234e0`.
