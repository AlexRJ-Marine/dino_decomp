# Dino food gauge and LunchBox meal — exact model (2026-10-31)

Status: **formulas proven from Ghidra + the stats audit; port still uses the
tuned `food_decay = 0.5/s`.** This note is the no-placeholder replacement.

## Fields

| field | offset | meaning |
| --- | ---: | --- |
| `max_food` | `dino+0x1dc` (`def+0x18c`) | float = **weight** (kg) |
| `food_gauge` | `dino+0x25c` (`int` index `0x97`) | float, init `max_food * 0.5` (`FUN_00570fd0`) |
| `food_consumption` | `dino+0x6c` (`def+0x1c`) | float = `weight * 0.1 * 0.01` = `weight * 0.001` per second |
| FOOD need | `dino+0x244` (`int`) | `ftol(food_gauge * 99 / max_food)`, clamped 0..99 |
| max food (clamp) | `dino+0x77` (int index) = byte `0x1dc` | same `max_food` |

## Decay — `FUN_00571370`, every 10 s

`FUN_0055869e0`/`FUN_00571540` drive a 10 s pass over every dinosaur:
`dino->consume_food(10.0f)`.

```c
amount = arg * *(float*)(dino + 0x6c);          // arg = 10.0
if (*(float*)(dino + 0x25c) > amount) {
    if (food >= 0.1f && food - amount <= 0) { /* hungry flags */ }
    damage = max(1, (int)((food - amount) * 0.1f));
    vtable+0xc(damage);                          // health
    amount = food;                               // clamp to the bar
}
*(float*)(dino + 0x25c) -= amount;
*(int*)(dino + 0x244) = __ftol(food * 99.0f / *(float*)(dino + 0x1dc));
```

So the gauge drops `10 * weight * 0.001 = weight * 0.01` per 10 s = **1 % of
the bar per 10 s for every dino**, and the need is derived from the gauge.

## Eating — `FUN_00576e50` + `FUN_005727b0` case 0x17

`FUN_00569a00` (LunchBox/StaticBait branch, element profile contains kind 8):

```c
amount = (float)*(int*)(dino + 0x288);   /* +0x9c = attack_head */
value  = (int)FUN_00576e50(source, amount);   // amount * nutrition
FUN_005727b0(0x17, value, 100, 0, ...);
```

`FUN_00576e50` drain (see `notes/lunchbox-eat-recon.md`):

```c
if (source->quantity < amount) amount = source->quantity;  // +0x08
source->quantity -= amount;
pf = *(float **)(source->+0x0c);            // {nutrition, capacity}
state 0/1/2/3 by pf[1]*0.66666 / 0.33333 / >0 / empty
return amount * pf[0];                      // nutrition
```

`FUN_005727b0` case 0x17 (`param_1` = int* dino, `param_1[0x97]` = gauge):

```c
f = (float)value + (float)percent * (float)param_1[0x97] * 0.01f;
param_1[0x97] = (int)f;
if (f > (float)param_1[0x77]) param_1[0x97] = param_1[0x77];
else if (f < 0)              param_1[0x97] = 0;
```

With the LunchBox call (`value = amount*nutrition`, `percent = 100`) this is
`gauge = gauge + value`, clamped to `max_food`; the need updates on the next
10 s pass. The BFI values: LunchBox `quantity=10000`, `nutrition=100`;
StaticBait `quantity=10000`, `nutrition=70`.

## Port plan (exact, no tuned decay)

1. Add `DinoNeeds::max_food`, `food_gauge`, `food_consumption`, and a 10 s
   accumulator. Set them in `add_imported_dino` from
   `DinoAttributes::weight_kg`/`food_consumption`; init the gauge to
   `max_food * 0.5`.
2. In `DinoActionLoop::tick`, replace the FOOD tuned decay with the 10 s
   gauge pass; set `value[0] = clamp(ftol(gauge * 99 / max_food), 0, 99)`.
3. Add `apply_food(value)` implementing the case-0x17 formula and clamp.
4. LunchBox/StaticBait actor: `quantity = bfi.field_3c`, `nutrition =
   bfi.field_40`, drain with `FUN_00576e50` and `apply_food`; remove the
   `.bsi` 3-unit consumption.
5. Tests: real phenotype (`allo.gen`: weight 2000 → max_food 2000,
   consumption 2.0/s, 10 s drop 20 = 1 % of 2000; need from gauge), and a
   LunchBox meal raising the gauge by `amount * nutrition`.

## Open

- The retail stand-cell search `FUN_0055b5b0` / `FUN_0055cb50` (the walk
  target is a min-cost cell at radius r, not the bowl centre); the port uses
  the bowl centre and the outer `1.2 r²` bound.
- The animation-state duration (`FUN_00522300`) versus the port's `.bai`
  `field_2e`; the port keeps `field_2e × action_scale`.
- The other three needs still use the port's tuned decay; the same gauge
  treatment is needed for VIOLENCE (`FUN_00571410`), REPOS (`FUN_00571540`)
  and SECU (already from the danger grid).

## Evidence

- Ghidra: `FUN_00571370`, `FUN_00571540`, `FUN_00570fd0`, `FUN_005727b0`,
  `FUN_00576e50`, `FUN_00569a00`, `FUN_0055869e0`, `FUN_0053acd0`.
- `notes/dino-stats-economy-gap-audit.md` §3.4/§4.2/§4.3/G4.
- `notes/lunchbox-eat-recon.md`; `LunchBox.bfi`/`StaticBait*.bfi`.
