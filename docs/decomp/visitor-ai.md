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
        - descriptor(+0x40)[0x1c] * W[3]
```

The decompiler rendered `pow(x, 2.0)` as three `FUN_0059aef0` calls; the
double at `0x006296a8` is `2.0`. The random base is
`rand() % (int)(W[6] - W[5]) + W[5]`, i.e. `rand() % 6000 - 3000`.

- `needA/B/C` are actor `+0x48`, `+0x4c`, `+0x50` (percent needs, 0-99).
- `needIndex` is actor `+0x68`; it indexes the per-building appeal array
  `attraction block +0x18`, which has one float per visitor profile.
- `appealScale` is attraction block `+0x24` (`def+0x48`); `attr10`/`attr11`
  are `+0x28`/`+0x2c` = `def+0x50`/`def+0x54` (per-building need deltas).
- `dx`/`dy` are actor tile `+0x08`/`+0x0c` minus attraction tile
  `+0x18`/`+0x1c`. The price is the **definition** field `descriptor+0x1c`
  (`0x522af2`/`0x522bb2`), `.bbi` file offset 25.

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
`0x005901f0`; other attractions finish when the timer reaches the building
definition's `+0x28` duration (`.bbi` file offset 37, seconds).
`FUN_00522f30` applies the attraction's queue/patience fields.

## Attraction block (buildings)

A building attraction carries a block at `building+0x74` (the object in the
visitor list is the sim building: 0xc4 bytes, vtable `0x64355c`, `+0x40` =
definition). `FUN_005872c0` (vtable slot +0x10) builds it:

| Block | Source | Meaning |
| ---: | --- | --- |
| +0x00..0x10 | `def+0x34..0x44` (`.bbi` 53,57,61,65,69) | five appeal weights |
| +0x14 | `PTR_FUN_00626ad4` | vtable |
| +0x18 | `FUN_00576550` loop | one appeal float per visitor profile |
| +0x24 | `def+0x48` (`.bbi` 73) | appeal scale |
| +0x28 | `def+0x50` (`.bbi` 81) | need-1 increment (int) |
| +0x2c | `def+0x54` (`.bbi` 85) | need-2 increment (int) |

`appeal[profile] = clamp(Σ_{k<5} (def+0x34+4k) × (profile[k]+0x18), −100, +100)`.
The vector is resized to the park profile count by `FUN_00576860`
(`*(spPark+0x78)` is the visitor-profile table, 6 entries in free game). The
Entrance has all five weights 0, so its appeal values are 0. See
`notes/visitor-needs-recon.md`.

## Needs

`actor+0x48/+0x4c/+0x50` are three int needs in 0..99. There is **no
time-based decay**. `FUN_00522f30` (using an attraction) is the only writer:

```text
need1 += def+0x50 (if >0)
need2 += def+0x54 (if >0)
target = (int)(appeal[actor+0x68] / appealScale)
if (target < 0)                 need0 += target
else if (actor+0x58 >= target)  need0 -= 1
else                            need0 = min(actor+0x58 + target, target)
```

The `actor+0x68` profile index selects the appeal value. The `VISITEURS_STATS`
panel averages the needs per profile: GAUGE/INDICS ← `+0x48`,
FOOD_STATISFACTION ← `+0x4c`, REST_STATISFACTION ← `+0x50`. Initial need
values and the writer of `+0x68` are still unproven.

## Port model

```text
if leaving: walk to exit
elif state 0: choose attraction by weighted score, or leave
elif state 1: follow path; on arrival state 2
elif state 2: wait for duration; then state 0/1
```

The scorer needs the constant `W` block, the actor's three needs and profile
index, the attraction position/radius, the definition price `+0x1c`, duration
`+0x28`, kind `+0x6c`, and show data.

## Open

- Consumer of the five variable `.bvi` weights (record `+0x24..+0x34`).
- `0x005873e0` arrival contract.
- Visitor initial need values and the writer of the `actor+0x68` profile index.
- The `.bvi` preference weights are the profile floats at `+0x18` (inferred,
  not proven identical).

## Evidence

- `notes/visitor-ai-recon.md` — decompiled functions and fields.
- `notes/visitor-needs-recon.md` — profile table, three needs, attraction use
  and HUD mapping.
- `notes/visitor-weights-runtime.md` — live x32dbg dump of the weight vector
  and record offsets.
- `0x00522820`, `0x00522940`, `0x00522a10`, `0x00522ff0`,
  `0x00521490`, `0x00522cb0`, `0x005219f0`, `0x00522f30`, `0x00521df0`.
- `0x0051f560` — sets `DAT_00675638` from profile `+0xD0`.
- `0x00544110` — copies profiles into the 0xEC record.
- `tools/formats/bvi.py` / `docs/formats/bvi.md` — visitor profile data.
- `tools/recon/dump-visitor-weights.ps1` — headless x32dbg reproduction.
