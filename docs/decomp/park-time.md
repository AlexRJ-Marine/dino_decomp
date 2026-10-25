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

## Open

- The fixed economy-category labels; see `docs/decomp/economy.md`.
- The exact `+0x00` base hour per island/campaign.

## Evidence

`notes/park-time-recon.md` — function-by-function evidence and offsets.
`0x00594b80`, `0x0058c1c0`, `0x0058c2f0`, `0x0058c350`, `0x0058c390`,
`0x0058c760`, `0x0058c210`, `0x0058c260`, `0x0058c620`, `0x0058c6b0`,
`0x00526010`, `0x005234e0`.
