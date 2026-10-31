# Food-source refill level and charge (`0x576d50` / `0x580e60`)

Date: 2026-10-31. Read-only Ghidra pass; no runtime probe yet.

## Question

What exactly does a nutritionist refill do: how much food is added, and is
the `.bsi` price charged to the budget?

## Proven

`FUN_00576d50(source, level)` (`source` = `saFoodSource`):

```c
source->level(+0x10) = level;
capacity = *(float *)(source->def(+0xc) + 4);
if (level == 0) source->amount(+0x08) = capacity;
else if (level == 1) source->amount = capacity * 0.66666f;
else if (level == 2) source->amount = capacity * 0.33333f;
else { assert "Invalid food quantity"; }      // level 3 is invalid here
source->flags(+0x00, +0x04[0], +0x15) = 1;
price = *(int *)(source->def(+0xc) + 8);      // .bsi price
call *(*DAT_0067572c)(__ftol(price));         // budget charge hook
```

- Level 0/1/2 = 100 % / 2/3 / 1/3 of the definition capacity; level 3 is
  rejected (the empty state is set elsewhere).
- The price is taken from `def+0x8` (the `.bsi` price, e.g. 10..50) and
  passed to the function at `*DAT_0067572c`; the budget hook is the budget
  vtable slot 0 `0x585c90` = `spend_food` (dino-stats audit §4.3/§6), i.e.
  `budget -= price; food_total += price; food_count++`.

`FUN_00580e60(set, index)`:

```c
set->used(+0x59 + index*8) = 1;
FUN_00576d20(*(set + 0xb0));                  // refresh/charge context
FUN_00576d50(*(set + 0x5c + index*8), level); // level read from the slot
```

The nutritionist completion path `FUN_00522300` calls
`FUN_00580e60(owner, hour)`. The level slot `*(set + 0x5c + hour*8)` has
**one writer**, `FUN_00580e30(set, hour, level)` (`0x00580e30`), and all
three call sites pass `level = 0`: the clock push (`0x00527404`/`0x00527418`,
hour `ftol(clock)` or `hour+1` with the 10 -> 1 wrap) and the UI schedule
toggle (`0x00537a4b`). No loop/memset writer and no 1/2 caller exists, so
retail always does the 100 % (level 0) refill; the 66/33 branches are
unreachable without an edited/scripted slot.

## Port status

**Implemented (2026-10-31).** `src/world/food.h` carries `FoodSource::level`;
`set_food_level` implements the exact 100/66/33 % amounts and rejects level 3.
`EmployeeSystem::update` returns `FeedResult {completed, food_spent}`; the
nutritionist completion calls `set_food_level(source, def, 0)` and both call
sites deduct the budget and call `BudgetLedger::spend_food(price)`.
`--feed-now` logs `food_spent=700 budget=73300` for PigFood and the per-source
`level=0`. Level 0 is exact: the only writer of the slot, `FUN_00580e30`,
passes 0 at all three call sites. The 66/33 `set_food_level` branches are
retained for a future edited/scripted slot but no retail writer produces them.

## Next probe

Break on `0x576d50` during a Cpg04 hour rollover to confirm the hour index
domain (the Farm path uses `ftol(clock)` while the UI wraps `10 -> 1`); the
charge and level-0 value are already proven.

## Evidence

- Ghidra `FUN_00576d50`, `FUN_00580e60`, `FUN_00522300`.
- `notes/employee-ai-recon.md` §completion; `notes/dino-stats-economy-gap-audit.md` G6/G9.
