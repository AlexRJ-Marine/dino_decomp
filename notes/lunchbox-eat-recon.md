# LunchBox / StaticBait dino eating — recon (2026-10-31)

Status: **the real dino food path is identified; the port's `.bsi` dino
consumption is a placeholder.** Evidence is static Ghidra plus the `.bfi`
files; no runtime Eat occurred in the 60 s Cpg04 probe.

## 1. The feature files

`Data/Design/attractions/features/FeatureList.ini` lists `LunchBox` and the
five `StaticBait*` features. Parsed with `tools/formats/bfi.py`:

| feature | food_kind | field_3c | field_40 | price | actions |
| --- | ---: | ---: | ---: | ---: | --- |
| LunchBox | 0 | 10000 | 100 | 100 | Eat, EatForSmallies |
| StaticBaitApple | 1 | 10000 | 70 | 10 | EatBait, EatBaitForSmallies |
| StaticBaitFlower | 0 | 10000 | 70 | 10 | EatBait, EatBaitForSmallies |
| StaticBaitHay | 4 | 10000 | 70 | 30 | EatBait, EatBaitForSmallies |
| StaticBaitPumpkin | 3 | 10000 | 70 | 50 | EatBait, EatBaitForSmallies |
| StaticBaitWatermelon | 2 | 10000 | 70 | 40 | EatBait, EatBaitForSmallies |

`field_3c` is 10000 for every feeder (the source quantity), `field_40` is the
nutrition (100 for the LunchBox, 70 for the baits). The current `Bfi` parser
reads but discards `field_3c`/`field_40` (`src/formats/bfi.cpp:93-94`).

## 2. `FUN_00576e50` — the real drain

`FUN_00576e50(source, amount)`:

```c
source->flags[0] = 1;  *(source->+4) = 1;  source->+0x16 = 1;
if (source->quantity < amount) amount = source->quantity;  // +0x08 f32
source->quantity -= amount;
pf = *(float **)(source->+0x0c);            // {nutrition, capacity}
if (pf[1]*0.66666 <= quantity)      source->state(+0x10) = 0;
else if (pf[1]*0.33333 <= quantity) source->state = 1;
else if (0.0 < quantity)            source->state = 2;
else                                source->state = 3;
return amount * pf[0];                      // nutrition delivered
```

`FUN_00569a00` calls it only when the element's move-profile kind list
contains **8** (LunchBox/StaticBait), with
`amount = (float)*(int*)(dino+0x288)` (`+0x288` is the `+0x9c` attack-head
stat slot in a fresh park);
it then applies `FUN_005727b0(0x17, result, 100, ...)`, i.e. the **food gauge**
`dino+0x97` (max `+0x77`), not the port's `DinoNeeds::value[0]`.

## 3. Why the port's `.bsi` consumption is wrong

`saFoodSource::SetLevel 0x580e60` has one caller (`0x522335`), inside the
**visitor** eat state machine; the 60 s Cpg04 probe saw no `SetLevel` and no
kind-8 Eat record. The `.bsi` sources feed visitors/employees. Dinosaurs eat a
`LunchBox`/`StaticBait` **item actor**; the actor is created by
`FUN_0053acd0` (`FUN_00589020` + `FUN_0056a730` box target) from the feature
record at `featureList+0x7c + type*0x1d8`.

## 4. Port plan (no placeholders)

1. `Bfi`: keep `field_3c` (quantity) and `field_40` (nutrition) in the struct.
2. Add a placed `LunchBox`/bait instance (feature actor) with
   `quantity = field_3c`, `nutrition = field_40`, `kind = food_kind`; the
   dino Eat target is that actor (action list Eat/EatBait).
3. Implement the `FUN_00576e50` drain exactly (quantity clamp, 2/3 and 1/3
   state thresholds, `amount * nutrition`).
4. Add the food gauge model (`dino+0x97`, max `+0x77`, decay `FUN_00571370`,
   need `99*food/max` from DINO-STATS-1) before wiring the hit; applying the
   nutrition to `DinoNeeds::value[0]` directly would be a guess.
5. Remove the port's 3-unit `.bsi` dino consumption and retarget the tests to
   the LunchBox.

## Open

- The retail stand-cell search (`FUN_0055b5b0`/`FUN_0055cb50`) is ported
  (disc + occupancy + min dino-cell distance); exact K-square/tie order
  remain open. The Eat element plays at arrival + mover idle and the action
  runs to `.bai field_2e`.
- Confirm the attack-head amount with a live `FUN_00576e50` breakpoint (the
  static path is `+0x288` = `+0x9c` = phenotype `+0x4c`).

## Evidence

- Ghidra: `FUN_00576e50`, `FUN_00569a00`, `FUN_0053acd0`, `FUN_00569e00`,
  `FUN_00580e60`, `FUN_00576d50`.
- `Data/Design/attractions/features/{LunchBox,StaticBait*}.bfi`,
  `FeatureList.ini`.
- `notes/dino-move-record-actions.md` (normal executor; food gesture),
  `notes/dino-stats-economy-gap-audit.md` (food gauge).
