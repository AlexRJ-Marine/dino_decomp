# Visitor AI: attraction choice, travel and leave flow

Status: **decompiled**; the scoring vector and its constant `.bvi` source are
solved. The consumer of the five variable profile weights is still open.

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

Score (`W` = `DAT_00675638`):

```text
score = (rand() % 6000) - 3000
        + (appeal[needIndex] / appealScale) * (99 - needA)^2 * W[0]
        + attr10 * (99 - needB)^2 * W[1]
        + attr11 * (99 - needC)^2 * W[2]
        - (|dx| + |dy|) * W[4]
        - attraction[0x10]->0x1c * W[3]
```

The decompiler rendered `pow(x, 2.0)` as three `FUN_0059aef0` calls; the
double at `0x006296a8` is `2.0`. The random base is
`rand() % (int)(W[6] - W[5]) + W[5]`, i.e. `rand() % 6000 - 3000`.

- `needA/B/C` are actor `+0x48`, `+0x4c`, `+0x50` (percent needs, 0–99).
- `needIndex` is actor `+0x68`; the attraction block `+0x18` is a float array.
- `appealScale` is attraction block `+0x24`; `attr10`/`attr11` are `+0x28`
  and `+0x2c`.
- `dx`/`dy` are actor tile `+0x08`/`+0x0c` minus attraction tile
  `+0x18`/`+0x1c`; `+0x1c` of the descriptor is the price/cost field.

`W` is **not** the five per-profile `.bvi` weights. It points at the profile
record `+0xD0`, which is the constant seven-float block at file `0x38..0x50`
(identical in all 24 profiles): `[1, 1, 1, 500, 300, -3000, 3000]`. Runtime
dump of the live pointer under x32dbg:

```text
0A0CBED8 (record+0xD0) = 3F800000 3F800000 3F800000 43FA0000 43960000 C53B8000 453B8000
```

The five variable profile weights (`0x1C..0x2C`) are copied to record
`+0x24..+0x34` (SurferMan: 0, 0.5, 1, −0.2, 0) but are not read by this
scorer. Their consumer is still open.

Kind 6 (shows) uses a show-specific block at `attraction[0x31]->0xEC + 0x54`
instead of `attraction + 0x74`, with the same field offsets.

The highest score wins; ties are broken randomly. If nothing is eligible, the
visitor marks itself for leaving.

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

The scorer needs the constant `W` block, the actor's three needs and need
index, the attraction position/radius, the price/tag field `+0x1c`, duration
`+0x28`, kind `+0x6c`, and show data.

## Open

- Consumer of the five variable `.bvi` weights (record `+0x24..+0x34`).
- `+0x1c` (price/queue/tag) and `+0x28` duration units.
- `0x005873e0` arrival contract.
- Visitor need decay/satisfaction and the `VISITEURS_STATS_*` gauges.
- The five attraction-block fields `+0x18` (array pointer), `+0x24` (scale),
  `+0x28`, `+0x2c` still need names.

## Evidence

- `notes/visitor-ai-recon.md` — decompiled functions and fields.
- `notes/visitor-weights-runtime.md` — live x32dbg dump of the weight vector
  and record offsets.
- `0x00522820`, `0x00522940`, `0x00522a10`, `0x00522ff0`,
  `0x00521490`, `0x00522cb0`, `0x005219f0`, `0x00522f30`, `0x00521df0`.
- `0x0051f560` — sets `DAT_00675638` from profile `+0xD0`.
- `0x00544110` — copies profiles into the 0xEC record.
- `tools/formats/bvi.py` / `docs/formats/bvi.md` — visitor profile data.
- `tools/recon/dump-visitor-weights.ps1` — headless x32dbg reproduction.
