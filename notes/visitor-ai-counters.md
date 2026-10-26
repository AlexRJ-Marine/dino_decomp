# Visitor AI: need snapshot, decay gating and initial values

Status: **closes the three remaining P1 unknowns.** Follow-up to
`notes/visitor-needs-recon.md` and `docs/decomp/visitor-ai.md`.

## `actor+0x58` is the need0 snapshot, not a visit counter

`FUN_00576530` copies four actor dwords:

```text
actor+0x58 = actor+0x48   (need0)
actor+0x5c = actor+0x4c   (need1)
actor+0x60 = actor+0x50   (need2)
actor+0x64 = actor+0x54   (satisfaction gauge)
```

`FUN_00522940` calls it in action state 1 right after

```text
actor+0x78 = current attraction block + 0x74
FUN_00576530(actor)
```

So `+0x58` is the **need0 value at the moment the visitor reaches the
attraction**, and `+0x78` is the current attraction block pointer (0 when not
using one).

`FUN_00522f30` (use completion) and the state-2 branch of `FUN_005763f0`
(the 12.5 s tick) then do:

```text
target = (int)(appeal[profile])          // ftol of the raw appeal
scaled = (int)(target / appealScale)     // ftol of the divided value
if (target < 0):          need0 += scaled
elif (target <= actor+0x58): need0 -= 1       // already satisfied: bored
else:                     need0 = min(scaled + actor+0x58, target)
```

The comparison is against the **arrival snapshot** `+0x58`, not the live
need0 and not a visit count. `FUN_005763f0` skips this branch entirely when
`actor+0x78 == 0`.

## Decay gating

`FUN_00576320` decrements need1 only when:

```text
state != 2  OR  current_block->need_delta1 == 0
```

so using an attraction that feeds the visitor (`def+0x50 != 0`) pauses the
need1 decay for the duration of the use. need2 decays only in action state 1
(-1) and state 3 (-3); need0 is handled by `FUN_005763f0` (state 0/1 -1,
state 3 -10, state 2 the snapshot rule above).

## Need0 may go negative

`FUN_005764c0` / `FUN_00576500` clamp negatives to 0 **except for index 0**:

```text
value = value + delta
if (value < 0 && index != 0) value = 0
if (value > 99) value = 99
```

A negative need0 is therefore allowed and means "leave": both decay
functions set `actor+0x74 = 1` and `actor+0x44 = 0` when the value goes
strictly below 0. need1/need2 are clamped to 0 and also set the leave flag.

## Initial needs are 39/99/99

The actor ctor `FUN_005761b0` sets `+0x48..+0x64 = 99` and then
`need0 = (int)(99 * 0.4) = 39`. Two live retail visitors in
`run/save/slot001.sav` (both mid-travel) serialise seven need dwords:

| Visitor | Needs |
| --- | --- |
| id 38 | 97, 96, 99, 99, 99, 99, 99 |
| id 39 | 99, 98, 99, 99, 99, 99, 99 |

Those are the first three fields (`+0x48/+0x4c/+0x50`) after the ctor and a
few seconds of decay. New visitors start at **39/99/99**.

## Profile index queue

`FUN_0058a970` does not create the actor; it picks a profile index by
`rand() % (N*100)` against the `spPark+0x78` table (weight/cap/count) and
queues the index at `park+0xc`. The ctor that consumes the queue was not
located, but the actor field is `+0x68` (proven by the scorer).

## Consequences for the port

- Record `need0_at_use` on entering the Use state and use it in the
  completion/tick formula above.
- Pause need1 decay while using an attraction with `need_delta1 != 0`.
- Allow need0 below 0; trigger leave at `< 0` (not `<= 0`).
- Start needs at 99/99/99.
- The score still uses the live needs `(99 - need)^2`, not the snapshot.
