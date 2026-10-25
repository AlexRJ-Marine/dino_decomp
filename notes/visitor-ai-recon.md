# Visitor AI: attraction choice, travel and leave flow (SIM-2b recon)

Date: 2026-10-25
Scope: static decompilation of `apVisitorBehavior` / `apBehaviorManager`.
No writes to `game/` or `original/`.

Status: **visitor state machine and attraction scoring decompiled; exact
preference-weight indices and leave conditions need a runtime cross-check.**

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
score = rand() % N
        + DAT_00675638[5]
        + attr11 * f20 * DAT_00675638[2]
        + attr10 * f19 * DAT_00675638[1]
        + (attr6 / attr9) * f18 * DAT_00675638[0]
        - (|dx| + |dy|) * DAT_00675638[4]
        - attraction[0x10]->0x1c * DAT_00675638[3]
```

`DAT_00675638` is the six-float visitor preference vector. The `.bvi` visitor
profiles expose five preference weights plus a personality; this is the
runtime weight table used by the scorer. The exact index-to-preference
mapping is still open.

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

The preference vector is global (`DAT_00675638`) and comes from the visitor
profile data. The attraction list provides position, radius, price/tag
(`+0x1c`), duration (`+0x28`), kind (`+0x6c`), and show data.

## Open

- Exact `DAT_00675638[0..5]` index-to-`.bvi` preference mapping.
- The `+0x1c` attraction field (price, queue or tag) and `+0x28` duration unit.
- What `0x005873e0` checks on arrival.
- The leave-at-exit condition and park rating effect.
- Visitor need decay/satisfaction (`+0x34..+0x40`) and its link to the
  `DINO/VISITEURS_STATS_*` gauges.

## Evidence

- `FUN_00522820` — visitor update dispatcher.
- `FUN_00522940` — state transitions.
- `FUN_00522a10` — attraction scoring.
- `FUN_00522ff0` — show availability.
- `FUN_00521490` — path following.
- `FUN_00522cb0` — path obstacle avoidance.
- `FUN_005219f0` / `0x00522f30` — attraction use.
- `FUN_00521df0` — per-visitor movement tick.
- `tools/formats/bvi.py` / `docs/formats/bvi.md` — profile data (other agent).
