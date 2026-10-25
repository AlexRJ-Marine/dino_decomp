# Visitor AI: attraction choice, travel and leave flow (SIM-2b recon)

Date: 2026-10-25
Scope: static decompilation of `apVisitorBehavior` / `apBehaviorManager`.
No writes to `game/` or `original/`.

Status: **visitor state machine and attraction scoring decompiled; the weight
vector is a constant `.bvi` block. The consumer of the five variable profile
weights is still open.**

## 1. Behaviour object

`FUN_00522820` is `apVisitorBehavior::Update(dt)`. The behavior object points
at a `spVisitor` actor and keeps:

| Offset | Meaning |
| --- | --- |
| `+0x08` | local action/status |
| `+0x0c` | visitor actor pointer |
| `+0x18`/`+0x1c` | path node vector (element stride `0x14`) |
| `+0x20` | path vector end |
| `+0x24` | current/previous attraction |
| `+0x28` | target attraction (current goal) |
| `+0x2c` | newly chosen attraction, pending arrival |
| `+0x30` | attraction list (vector of pointers) |
| `+0x38` | leaving-the-park flag |

The actor itself (`+0x0c`) uses:

```text
+0x08/+0x0c   tile x/y
+0x28         visitor action state
+0x2c/+0x30   float position
+0x34/+0x38   float direction
+0x40         attraction-use timer
+0x6c         "leave park" flag
+0x74         active flag
```

## 2. State machine

`FUN_00522820` dispatches by the actor's `+0x28`:

| Actor state | Update |
| ---: | --- |
| 0 | choose an attraction via `0x00522a10` |
| 1 | travel to the target via `0x00521490` |
| 2 | use the attraction via `0x005219f0` |

`FUN_00522940` handles transitions:

- state 0: if `0x005213e0()` returns 1, switch to state 1; otherwise mark
  `actor+0x6c = 1` when the target list is exhausted and call `0x00522cb0`
  (walk to the exit).
- state 1: if `0x005213e0()` is no longer 1, reset to state 0; otherwise call
  the actor's movement update and continue. If the target list is exhausted,
  set `actor+0x6c = 1`; otherwise set `actor+0x78` to the next path node and
  call `0x00576530`.
- state 2: call `0x00522f30` (attraction interaction) and the virtual
  completion handler.

`FUN_00521df0` is the per-visitor tick: when state is 1, or state is 2 and
the attraction kind is 4 or 5, it calls `0x0056d880` (movement/animation).

## 3. Attraction scoring (`0x00522a10`)

If the visitor still has a target (`+0x28`) and its enabled byte is nonzero,
the visitor keeps it. Otherwise it scores every enabled attraction in the
`+0x30` list:

1. **1 in 5 chance to leave immediately.** `rand() % 5 == 0` jumps to the
   "leave" path (`local state = 2`).
2. For each attraction:
   - reject if it is the previous target (`+0x28`);
   - reject if outside the attraction radius:
     `(dx*dx + dy*dy) > attraction[0x10]->0x58²`;
   - reject if `0x00522ff0` (show-specific availability) fails;
   - reject if the attraction's virtual `+0x18` check fails, except kind 6.
3. Score (float):

```text
score = (rand() % 6000) - 3000
        + (appeal[needIndex] / appealScale) * (99 - needA)^2 * W[0]
        + attr10 * (99 - needB)^2 * W[1]
        + attr11 * (99 - needC)^2 * W[2]
        - (|dx| + |dy|) * W[4]
        - attraction[0x10]->0x1c * W[3]
```

`FUN_0059aef0` is `pow` (the MSVC `_CIpow` helper), not `rand`; the double
constant at `0x006296a8` is `2.0`, so the three need terms are squared.
The random base comes from `rand() % (int)(W[6] - W[5]) + W[5]`.

- `needA/B/C` = actor `+0x48` / `+0x4c` / `+0x50` (0–99).
- `needIndex` = actor `+0x68`; `appeal` = attraction block `+0x18` (float
  array), `appealScale` = `+0x24`, `attr10` = `+0x28`, `attr11` = `+0x2c`.
- `dx`/`dy` = actor tile `+0x08`/`+0x0c` minus attraction tile
  `+0x18`/`+0x1c`.
- `attraction[0x10]->0x1c` = descriptor price/cost field.

## 3a. The weight vector is constant, not the profile preferences

`DAT_00675638` is set only by `FUN_0051f560`:

```c
DAT_00675638 = param_2;
```

The single caller is the park application (`FUN_0051d1d0`) with

```c
*(int *)(*(int *)(*(int *)(*(int *)(param_1 + 0xc) + 0x34) + 200) + 4) + 0xd0
```

i.e. the first profile record + `0xD0`. `FUN_00544110` copies seven dwords
from the parsed profile into record `+0xD0..0xE8`. Those seven values are
the constant block from `.bvi` file offset `0x38..0x50`:

```text
1.0, 1.0, 1.0, 500.0, 300.0, -3000.0, 3000.0
```

All 24 `.bvi` files store the same block. The live dump under x32dbg was
`3F800000 3F800000 3F800000 43FA0000 43960000 C53B8000 453B8000`, and
`FUN_00522a10` reads W0..W5 plus W6−W5 = 6000.

The five **variable** profile weights at file `0x1C..0x2C` land at record
`+0x24..+0x34` (verified live: SurferMan = 0, 0.5, 1.0, −0.2, 0.0). They are
not read by the scorer. A hardware read breakpoint on record `+0x24`
produced no hits in an empty free-game park or a loaded two-visitor save
within 15 s; the consumer is still open (likely visitor creation or the
need cycle).

The highest score wins; ties are broken randomly. If nothing is eligible, the
visitor marks itself for leaving.

For kind 6 (shows), the preference vector is taken from
`attraction[0x31]->0xec + 0x54` instead of the normal `attraction + 0x74`
block.

## 4. Show-specific availability (`0x00522ff0`)

`0x522ff0` only applies to kind 6. It checks the show's schedule/time window:

- `0x005873f0()` returns whether the show is running/available;
- the show's `+0xd3` flag;
- `0x0058c3e0` gives the show start/length;
- `0x005596b0` computes the path/time to the show;
- the visitor accepts the show only if the arrival time falls inside
  `[start - 0.5*length, start]` (using constants from `DAT_00675770`).

This is why show attractions can be skipped even when in range.

## 5. Movement and path following

`FUN_00521490` advances the visitor along the 0x14-byte path nodes:

- if the current node matches the actor's tile, consume it;
- if the path is empty, use the target attraction's cell (`+0x18/+0x1c`);
- normalise the direction, move by `dt * speed(+0x04)`, update the tile x/y;
- when the final node is reached, set `+0x28 = target`, clear `+0x2c`, and call
  `0x005873e0`; if that check fails, return to state 1, otherwise state 2.

`FUN_00522cb0` inserts/rewrites path nodes around blocked cells:
it picks a random offset, computes a Manhattan path length, and shifts the
remaining nodes by up to four cells when the route is blocked. This is the
local obstacle-avoidance step.

## 6. Attraction use (`0x005219f0`)

While state 2:

- accumulate `actor+0x40 += dt`;
- if the attraction kind is 6, call `0x005901f0` (show progress);
- otherwise end the visit when `actor+0x40 >= attraction[0x10]->0x28`
  (attraction duration), returning to state 1.

`FUN_00522f30` applies the attraction's `+0x50`/`+0x54` counters and clamps
the visitor's `+0x58` value (queue/patience) while inside.

## 7. What this gives the port

A faithful visitor update is:

```text
if leaving: walk to exit
elif state 0: choose attraction by weighted score or leave
elif state 1: follow path to target; on arrival state 2
elif state 2: wait for attraction duration; then state 0/1
```

The weight vector is global (`DAT_00675638`) and is the constant `.bvi`
block, not per-visitor data. The attraction list provides position, radius,
price/tag (`+0x1c`), duration (`+0x28`), kind (`+0x6c`), and show data.

## Open

- Consumer of the five variable profile weights (record `+0x24..+0x34`).
- The `+0x1c` attraction field (price, queue or tag) and `+0x28` duration unit.
- Names of the attraction block fields `+0x18` (array), `+0x24` (scale),
  `+0x28`, `+0x2c`.
- What `0x005873e0` checks on arrival.
- The leave-at-exit condition and park rating effect.
- Visitor need decay/satisfaction (`+0x34..+0x50`) and its link to the
  `DINO/VISITEURS_STATS_*` gauges.

## Evidence

- `FUN_00522820` — visitor update dispatcher.
- `FUN_00522940` — state transitions.
- `FUN_00522a10` — attraction scoring (assembly at `0x522ae0`–`0x522c56`).
- `FUN_0051f560` — writes `DAT_00675638`.
- `FUN_00544110` — copies a profile into the 0xEC list record.
- `FUN_0053d0b0` — visitor spawner; uses profile `+0x94`.
- `FUN_00522ff0` — show availability.
- `FUN_00521490` — path following.
- `FUN_00522cb0` — path obstacle avoidance.
- `FUN_005219f0` / `0x00522f30` — attraction use.
- `FUN_00521df0` — per-visitor movement tick.
- `tools/formats/bvi.py` / `docs/formats/bvi.md` — profile data.
- `tools/recon/dump-visitor-weights.ps1` — live weight dump.
