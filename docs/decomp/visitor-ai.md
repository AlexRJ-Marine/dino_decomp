# Visitor AI: attraction choice, travel and leave flow

Status: **decompiled**; preference-weight indices and some attraction fields
still need a runtime/data cross-check.

## State machine

`apVisitorBehavior::Update` (`0x00522820`) dispatches on the visitor actor's
action state (`actor+0x28`):

| State | Meaning | Update |
| ---: | --- | --- |
| 0 | choose an attraction | `0x00522a10` |
| 1 | travel to target | `0x00521490` |
| 2 | use attraction | `0x005219f0` |

`FUN_00522940` performs transitions. On state 0 it selects a target and moves
to state 1, or marks `actor+0x6c = 1` and walks to the exit when the list is
exhausted. On state 1 it moves along the path; arrival switches to state 2.
On state 2 it runs the attraction interaction and then returns to state 1/0.

`FUN_00521df0` calls the movement update when the visitor is travelling, or
when it is using an attraction of kind 4/5.

## Attraction scoring (`0x00522a10`)

A visitor keeps its current target if it is still enabled. Otherwise it
scores every enabled attraction in its `+0x30` list:

- 1 in 5 chance to leave the park immediately;
- reject the previous target;
- reject attractions outside `radius` (`attraction[0x10]+0x58`);
- reject shows that fail the schedule check `0x00522ff0`;
- reject attractions whose virtual `+0x18` check fails (except kind 6).

Score:

```text
score = rand() % N
        + W[5]
        + attr11 * f20 * W[2]
        + attr10 * f19 * W[1]
        + (attr6 / attr9) * f18 * W[0]
        - (|dx| + |dy|) * W[4]
        - attraction[0x10]->0x1c * W[3]
```

`W` is the global six-float preference vector `DAT_00675638`, populated from
the visitor profile (`.bvi`). The highest score wins; ties are random. If
nothing is eligible, the visitor leaves.

Kind 6 (shows) uses a show-specific preference block instead of the normal
attraction block.

## Show availability (`0x00522ff0`)

Shows are accepted only if they are running (`0x005873f0`), their `+0xd3`
flag is set, and the computed arrival time lies inside the show's start window
(start minus half the show length through start). `0x005596b0` computes the
travel time; `DAT_00675770` supplies the timing constants.

## Movement

`FUN_00521490` advances along 0x14-byte path nodes. It consumes nodes whose
tile matches the actor, uses the target attraction's cell when the path is
empty, normalises direction, moves by `dt * speed(+0x04)`, and updates the
actor's tile. Arrival calls `0x005873e0`; failure returns to state 1, success
to state 2.

`FUN_00522cb0` repairs the path around blocked cells by shifting pending nodes
by up to four cells and recomputing the Manhattan route.

## Attraction use

`FUN_005219f0` accumulates `actor+0x40` while in state 2. Shows use
`0x005901f0`; other attractions finish when the timer reaches the attraction's
`+0x28` duration. `FUN_00522f30` applies the attraction's queue/patience fields.

## Port model

```text
if leaving: walk to exit
elif state 0: choose attraction by weighted score, or leave
elif state 1: follow path; on arrival state 2
elif state 2: wait for duration; then state 0/1
```

The scorer needs the visitor preference vector, the attraction position/radius,
the price/tag field `+0x1c`, duration `+0x28`, kind `+0x6c`, and show data.

## Open

- `DAT_00675638[0..5]` → `.bvi` preference mapping.
- `+0x1c` (price/queue/tag) and `+0x28` duration units.
- `0x005873e0` arrival contract.
- Visitor need decay/satisfaction and the `VISITEURS_STATS_*` gauges.

## Evidence

- `notes/visitor-ai-recon.md` — decompiled functions and fields.
- `0x00522820`, `0x00522940`, `0x00522a10`, `0x00522ff0`,
  `0x00521490`, `0x00522cb0`, `0x005219f0`, `0x00522f30`, `0x00521df0`.
- `tools/formats/bvi.py` / `docs/formats/bvi.md` — visitor profile data.
