# Park clock and show schedule

Date: 2026-10-25
Scope: static decompilation. Read-only; no game writes.

Status: **park clock, hour flags and the 12-slot show scheduler decoded; the
day-counter increment source is still open.** `0x58dfc0` is *not* the clock —
it builds show/attraction render records. The clock is `spScheduleManager`.

## 1. `spScheduleManager` object

`0x00594b80` (constructor):

```text
param_1[0] = task vector      # 0x0058c480, 12 entries, stride 0x2c
param_1[1] = time manager     # 0x0058c1c0, 9-slot timing table
```

`0x0058c1c0(time_scale, slot_count=9)` builds the time manager:

| Offset | Meaning |
| ---: | --- |
| `+0x00` | base hour (integer) |
| `+0x04` | **day counter** (integer; HUD displays `day + 1`) |
| `+0x08` | elapsed game time (float, seconds) |
| `+0x0c` | previous elapsed time (float) |
| `+0x10` | time scale |
| `+0x14` | `1 / time_scale` |
| `+0x18` | minutes per second (`time_scale / 60`) |
| `+0x1c` | inverse |
| `+0x20` | hours per second (`time_scale / 3600`) |
| `+0x24` | inverse |
| `+0x28` | new-hour flag (one frame) |
| `+0x29` | end-of-day flag |

The 9-slot table at `+0x2c` / the 12-entry vector are serialized by
`0x0058c210` / `0x0058c620` (time fields `+0x04`, `+0x08`, `+0x0c`) and
`0x0058c260` / `0x0058c6b0` (task handles).

## 2. Clock advance (`0x0058c2f0`)

```text
new_time = old_time (+0x08) + dt
hour = int(new_time * +0x20)
if hour >= 11:
    +0x29 = 1          # day finished; stop advancing
    return
if int(hour) changed vs +0x0c:
    +0x28 = 1          # new hour
+0x08 = new_time
+0x0c = old_time
```

So a park day is **12 hours** (hour 0..10 playable, hour 11 ends it). The
exact base hour (`+0x00`) is set by the park/scenario; the HUD formats it.

## 3. Clock read (`0x0058c350`)

```text
hour   = int(+0x20 * +0x08) + +0x00
minute = int(+0x18 * +0x08) % 60
day    = +0x04
```

`0x00526010` is the HUD time-panel update. It formats
`%.2d:%.2dAM` / `%.2d:%.2dPM` (or `%.2d:%.2d` when the 12-hour flag
`panel+0x8d` is clear), sets `PARK_CURRENT_TIME`, and writes `day + 1` as
`%d` into `PARK_CURRENT_DAY`. Both widgets are created by `0x005234e0` and
stored at `panel+0x1e0` / `panel+0x1e4`.

## 4. Show scheduler (`0x0058c760`)

Called by `0x00594d00` (`spScheduleManager::Update`) after
`0x0058c2f0` advances the clock:

- If `+0x28` (new hour) is clear and `+0x29` (end of day) is clear:
  - slot index `manager+0x18` is in 1..10;
  - the show at `task_vector[slot]` must have `+0xd3 != 0` (running),
    `+0xe0 == 2` and `+0xe4 == 0`;
  - `0x0058c390(time)` must accept the show window;
  - then `0x0057b110` starts it.
- If `+0x28` is set or `+0x29` is set:
  - slot index resets to 0;
  - every task slot 1..10 is checked with the same show flags;
  - `0x0058c8f0` adds the show's feature/actor list and `0x0057b130`
    activates it.

`0x0058c390(time)` computes

```text
window = (int(time * +0x20) + 1) * +0x24 - time * +0x14
active = (+0x0c <= window <= +0x08)
```

so each show has a start window in the current hour.

## 5. Day rollover and daily settlement

`0x00524630` is the park daily update. When the time manager's end-of-day
flag (`+0x29`) is set:

```text
FUN_0058ae60(simulation)   # daily actor/dino reset pass
FUN_0058c2d0(time)         # day += 1; clear flags; elapsed = 0
FUN_0058ad00(simulation)   # daily economy settlement
... bankruptcy / lunchbox / starving warnings ...
```

`0x0058c2d0` is the actual day counter increment:

```text
time+0x04 += 1
time+0x28 = 0
time+0x29 = 0
time+0x08 = 0
time+0x0c = 0
```

`0x0058ad00` is the settlement already documented in `economy.md`: it posts
shop/category costs, attraction gate costs and visitor ticket income, calls
the roll-up `0x00585e10`, and resets each shop's daily counter
(`+0x38` → `+0x3c`).

## 6. What is open

- The fixed economy-category labels; see `docs/decomp/economy.md`.
- The exact `+0x00` base hour per island/campaign.
- `0x0058dfc0` is the show/attraction render-record builder, not the clock.

## 7. Evidence

- `0x00594b80` — `spScheduleManager` constructor.
- `0x00524630` — park daily update (end-of-day trigger).
- `0x0058c2d0` — day counter increment and clock reset.
- `0x0058ae60` — daily actor/dino reset pass.
- `0x0058ad00` — daily economy settlement.
- `0x0058c1c0` — time-manager constructor/conversions.
- `0x0058c2f0` — clock advance, new-hour/end-of-day flags.
- `0x0058c350` — H:M:S read.
- `0x0058c390` — show-window check.
- `0x0058c760` — 12-slot show scheduler.
- `0x0058c210` / `0x0058c260` / `0x0058c620` / `0x0058c6b0` — schedule
  serialization.
- `0x00526010` — HUD time panel.
- `0x005234e0` — `PARK_CURRENT_TIME` / `PARK_CURRENT_DAY` widgets.
