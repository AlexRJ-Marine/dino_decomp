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
  passed to the function at `*DAT_0067572c` (the budget/Food bucket hook; the
  exact object is not yet named).

`FUN_00580e60(set, index)`:

```c
set->used(+0x59 + index*8) = 1;
FUN_00576d20(*(set + 0xb0));                  // refresh/charge context
FUN_00576d50(*(set + 0x5c + index*8), level); // level read from the slot
```

The nutritionist completion path `FUN_00522300` calls
`FUN_00580e60(owner, hour)`. The **level value** stored at
`*(set + 0x5c + index*8)` is produced by the hourly job push
(`FUN_0058c450` / the employee schedule), and that producer is not traced.

## Port status

`src/world/food.h` has `FoodSource::remaining` only; `src/world/employees.cpp`
refills to `source_capacity()` at the end of the nutritionist job and never
charges `FoodDef::price`. This is the documented G6/G9 gap. Do not change it
until the job-level producer and the `DAT_0067572c` hook are identified.

## Next probe

Break on `0x576d50` during a Cpg04 hour rollover (the Cpg04 save has 6
nutritionists) and log the source pointer, `level`, `def+0x4` capacity and
`def+0x8` price. That confirms both the level values chosen in practice and
the charge amount. Then:
- add `FoodSource::level`, use the exact 100/66/33 % amounts;
- call the port economy's FOOD bucket (`spend_food`) with the `.bsi` price;
- test the ledger delta on a real source.

## Evidence

- Ghidra `FUN_00576d50`, `FUN_00580e60`, `FUN_00522300`.
- `notes/employee-ai-recon.md` §completion; `notes/dino-stats-economy-gap-audit.md` G6/G9.
