# Visitor needs and the park profile table (Ghidra)

Status: **needs model solved except the initial values and the writer of the
profile index.** This corrects the earlier "park need table" premise.

## `spPark+0x78` is the visitor-profile table, not a need table

- `FUN_00586200` (simulation-manager ctor) allocates a 0x40-byte object via
  `FUN_00615a60`, stores it at `simu+0x1c`, and `FUN_005874a0` (spPark ctor)
  stores it at `spPark+0x78`.
- `FUN_00615a60` has embedded vectors at `+0x04`, `+0x14`, `+0x28`, `+0x34`,
  each reserved to **6** entries.
- `FUN_005869b0` appends one profile per scenario `VisProfiles` entry:
  `FUN_00615bb0` pushes profile ptr → `+0x34`, arg → `+0x14`, arg → `+0x28`,
  0 → `+0x04`, then `FUN_00589ba0` re-scores the park.
- `FreeGame.ini` supplies 6 triplets per difficulty (easy/medium/hard); a
  campaign may have 0–6. So the **profile count is 6 in free game**, not a
  fixed 3-need table.

The count used by the building ctor is
`(profileTable->end - profileTable->begin) / 4` (`FUN_00586b80` end calls
`FUN_00576860(profileTable count)`), so the per-building appeal array has one
float per profile.

## The three needs

`+0x48`, `+0x4c`, `+0x50` are int needs in `0..99`. The only writers are
`FUN_005764c0` / `FUN_00576500`:

```text
param_3 = *(int *)(actor + 0x48 + index*4) + param_3;
if (param_3 < 0 && index != 0) param_3 = 0;
if (param_3 > 99) param_3 = 99;
```

Only caller: `FUN_00522f30` (state 2, using an attraction; every frame and at
completion):

```text
actor[0x4c] += def+0x50   (if > 0)       // 0x522f3f-0x522f4e
actor[0x50] += def+0x54   (if > 0)       // 0x522f53-0x522f65
target = (int)(appeal[actor+0x68] / block+0x24)   // 0x522f6a-0x522f8b
if (target < 0)              actor[0x48] += target
else if (actor+0x58 >= target) actor[0x48] += -1
else actor[0x48] = min(actor+0x58 + target, target)
```

`block+0x24` = `def+0x48` appeal scale (4.0). `block+0x28/+0x2c` are
`def+0x50/+0x54` (int), i.e. the per-building need increments, not unknown
flags. **There is no time-based need decay**; no other xrefs to the setters.

`actor+0x68` indexes the per-building appeal array (`[EDI+0x18]`,
`[EDI+0x24]` divisor at `0x522b2c..0x522b37`), so it is the visitor's
**profile index**, not a need index. Its writer was not located.

## Attraction scoring (corrected)

`FUN_00522a10` reads all three needs (`0x522b0f/0x522b3e/0x522b66`):

```text
score = rand()%6000 - 3000
        + (appeal[actor+0x68] / block+0x24) * (99 - need0)^2
        + block+0x28 * (99 - need1)^2
        + block+0x2c * (99 - need2)^2
        - 300 * manhattanDistance
        - 500 * price
```

The `W` constant is `[1,1,1,500,300,-3000,3000]`; the divisor is the appeal
scale. `block+0x28/+0x2c` are `def+0x50/+0x54` (need deltas) and are used
directly as the need1/need2 weights.

## HUD gauges

The real panel code is `0x50f0c0` (ctor) / `0x50f9d0` (update), not
`0x511d80`/`0x5138b0`.
`FUN_0058abf0` builds per-profile averages:

```text
out[0x00+p] += actor[0x48]
out[0x18+p] += actor[0x4c]
out[0x30+p] += actor[0x50]
```

Panel ctor `0x50f0c0`: profile block `+0x30 ICON`, `+0x34 QUANTITY`,
`+0x38 GAUGE`, `+0x3c INDICS`, `+0x40 FOOD_STATISFACTION`,
`+0x44 REST_STATISFACTION` (stride 0x18); `panel+0xc0 = *(spPark+0x78)`.
Update mapping: GAUGE/INDICS ← average `+0x48`; FOOD_STATISFACTION ← average
`+0x4c`; REST_STATISFACTION ← average `+0x50`; QUANTITY ← `table+0x04[p]`.

## Initial values and spawn

- Profile count: free game 6; campaign 0–6.
- Spawn selection `FUN_0058a970`: profile weight `table+0x14`, cap
  `table+0x28`, current count `table+0x04`; `rand() % (N*100)` picks the
  profile, queued at `park+0xc`.
- **New-visitor initial needs are unproven.** No initializer was found; the
  only writers are the two setters above. A fresh retail save's 0x50-byte
  visitor records show 96–99 in all seven serialized "need" dwords (not
  necessarily `+0x48..50`), so the absolute start values need a runtime or
  ctor check.

## Uncertainties

- Writer of `actor+0x68` and the visitor actor ctor (hence initial needs).
- `+0x4c` = FOOD / `+0x50` = REST is proven at the widget level; souvenir
  shops add `+0x54` (→ REST gauge) 10, which is semantically odd and worth a
  runtime check.
- The profile record's five floats at `+0x18` are the dot-product operands
  (proven); that they are exactly the `.bvi` preference weights is inferred.
