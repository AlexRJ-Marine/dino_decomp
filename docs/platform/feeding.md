# Feeding — visitor/employee sources and the dinosaur LunchBox

Status: **LunchBox actor ported (herbivore path), exact arrival gate and
bite amount.** The `.bsi` `FoodSource` feeds visitors/employees; park
dinosaurs eat a **LunchBox/StaticBait** item actor via `FUN_00576e50`, which
fills the food gauge `+0x25c`. The port's former dino-Eat-from-`.bsi` gate
and 3-unit meal were a placeholder and are removed. `--place-lunchbox` places
the `.bfi` bowl; the object executor `0x0056c490` gates arrival with
`0.8r²..1.2r²`, `r = 3.1 × 0.01 × body` for `Eat`, and the bite is
`attack_head` (`dino+0x288`). Interactive placement, the other baits and save
persistence remain open.

## Retail chain

- `.bsi`/`FoodSourceList.ini` defines player-built food sources (kinds 0-4
  vegetarian, 5-9 carnivorous). `saFoodSource::SetLevel 0x580e60` has one
  caller (`0x522335`) in the **visitor** eat state machine; the employee
  hourly schedule registers each source with the kind-8 nutritionist
  (`0x5953F0`/`0x58B5B0`).
- Park dinosaurs use the `.bfi` `LunchBox` and `StaticBait*` features. The
  runtime item actor publishes the `Eat`/`EatBait` action list; when its
  element move-profile kinds contain **8**, `FUN_00569a00` calls
  `FUN_00576e50(source, amount)` and applies the result as
  `FUN_005727b0(0x17, value, 100)` to the **food gauge** `dino+0x97`
  (byte `+0x25c`, max `+0x1dc` = weight).
- `FUN_00576e50`: clamp the amount to the source quantity (`+0x08`), subtract,
  set state `+0x10` 0/1/2/3 at the 2/3 and 1/3 thresholds, return
  `amount * nutrition` (`+0xc[0]`).
- The `.bfi` fields: LunchBox `food_kind 0`, quantity `field_3c=10000`,
  nutrition `field_40=100`; StaticBait quantity 10000, nutrition 70.
- The 60 s Cpg04 probe saw no dino Eat and no `SetLevel`; its six `0x522300`
  controllers are visitor-side with a null source. The **180 s** run then saw
  `FUN_00569660` fire 24 kind-8 Eat records and `SetLevel` 7 times once a dino
  reached food (~112 s), so the dino path is live; the real dino target is a
  LunchBox/bait item actor (`notes/dino-mover-return-probe.md`).

## Port

- `src/formats/bsi.{h,cpp}` reads `.bsi`; `src/world/food.{h,cpp}` resolves
  the placed mesh/texture and exposes `food_is_carnivore`.
- `--place-food NAME|KIND` / `--food-pos` place a **visitor/employee**
  `.bsi` source. It no longer gates a dinosaur's Eat and is not consumed by
  dinosaurs.
- Nutritionist refill: `set_food_level` implements the exact 100/66/33 %
  amounts; the completion uses level 0 and charges the `.bsi` price to FOOD
  (`BudgetLedger::spend_food`). `--feed-now` with PigFood logs
  `food_spent=700 budget=73300` and `level=0`; level 0 is exact because the
  only slot writer (`FUN_00580e30`) passes 0 at every call site. The 66/33
  branches are retained for an edited/scripted slot, not produced by retail.
- `DinoNeeds` carries the exact food gauge: `max_food = weight`,
  `food_gauge` (init `max_food*0.5`), `food_consumption = weight*0.001/s`.
  Every 10 s: `gauge -= 10*consumption`, `FOOD = ftol(gauge*99/max_food)`.
  `DinoActionLoop::apply_food(value, percent)` implements the event-0x17
  formula `gauge = value + percent*gauge/100`, clamped.
- `src/formats/bfi.*` keeps `food_quantity`/`food_nutrition` and the
  `.bfi` action names (`BfiFeature.actions`); `Feeder` + `feeder_drain` in
  `src/world/food.*` are the exact `FUN_00576e50` model. `--place-lunchbox`
  copies the action list to `Feeder.actions` and logs
  `actions=Eat,EatForSmallies`.
- Retail's park dino graph is `Exploration/RunAway/Sleep`; `Eat` is the
  **food-item behavior's** action list. `tick_dino` compares the best dino
  graph action with the best item action (the feeder's `.bfi` list, inventory
  gated) and switches the chooser's candidate set to the item list when it
  wins. No retail counterpart for the item arbitration was decoded;
  `+0xb8`/`+0xf4` are the two sub-behaviors' mode/activation lists, not item
  attach lists. The score comparison is the port's model.
- `--place-lunchbox` (optional `--lunchbox-pos CX,CY` in 4-unit build cells,
  default first paddock centre) loads `LunchBox.bfi`, creates a `Feeder`
  (quantity 10000, nutrition 100, kind 0, radius = width/2 = the `.bfi`
  footprint, **not** the Eat gate), uploads `bowl.mesh` + `bowl.tga`, and
  adds it to the draw list.
- **Static bait variants (wired):**
  `StaticBaitFlower/Apple/Hay/Pumpkin/Watermelon` are ground tiles
  (`display_type 3`, no mesh, one `animaux.tga` texture) with
  `food_kind 0/1/4/3/2`, quantity 10000, nutrition 70, actions
  `EatBait`/`EatBaitForSmallies`, prices 10/10/30/50/40, and switch keys
  `SWITCH_BAIT_FLOWER`/`FRUITS`/`HAY`/`CITROUILLE`/`PASTEQUE`. They use the
  same generic `FUN_0052bbb0`/`FUN_0052b0d0` placement (4x4 overlap, CONSTRUCTION
  charge) with **no 16-dish cap** (that cap is LunchBox-only). The port places
  them with `--place-bait NAME` or the construction-kit path
  (`OPEN_CONSTRUCTION` -> `OPEN_CONSTRUCTION_KIT` -> Bait -> Flowers/Fruit/...),
  drawing one full-texture `animaux.tga` 4x4 ground quad. The exact retail
  display-type branch/UV builder is OPEN; see `notes/static-bait-recon.md` and
  `docs/formats/bfi.md`.
- `ParkSession::paddock_feeder` returns the nearest non-empty feeder in the
  paddock whose `food_kind` matches the diet. `Eat` is offered only when one
  exists. `tick_dino` uses the selected `.bai` record to compute the retail
  gate (`eat_arrival_radius`: `Eat`/`EatBait` mode 3 =
  `field_18 × 0.01 × body`, `EatForSmallies`/`EatBaitForSmallies` mode 4 =
  `field_18 × (size_radius + 1)`), walks with the `0x34`/`0x35` marker while
  outside `1.2 r²`, and stops inside it. At the feeder it drains
  `feeder_drain(feeder, attack_head)` and calls `apply_food(value)` (event
  0x17).
- Cpg04 verification (`--place-lunchbox --lunchbox-pos 113,78`): the anky
  (herbivore, paddock 0) starts at (463.7,327.5), walks to the stand cell
  (457,321) with `r=7.01` (`3.1 × 0.01 × body 226`; feeder at 454,314) and
  eats `attack_head 30 × nutrition 100`:
  `dino eat: anky <- LunchBox nutrition=3000 quantity=9970 gauge=4255`.
  The allos are carnivores and cannot use the kind-0 LunchBox.

## Verification

- `test_cpp_lunchbox_eat`: the `.bfi` LunchBox quantity 10000/nutrition 100,
  the anky chooses Eat, walks to the bowl with the retail gate `r=7.01`
  (`0x0056c490`, `3.1 × 0.01 × body 226`), drains
  `attack_head 30 × nutrition 100` (`nutrition=3000 quantity=9970
  gauge=4255`), and the allos never eat it.
- `test_cpp_food_source_is_visitor_only`: a placed PigFood shows
  `source=PigFood` in the food log but no dinosaur chooses Eat and the source
  is not depleted.
- `test_cpp_employee_feeding_and_wages`: the nutritionist refills the
  `.bsi` source (`level=0`), charges its 700 price to FOOD and pays wages.
- `--selftest`: food gauge 10 s pass/meal clamp and `feeder_drain`
  30/empty checks.
- `test_cpp_dino_action_loop`: without a LunchBox, starving dinos pick
  Exploration 1580; Sleep is unaffected.

## Open

- Interactive LunchBox placement is **wired**: `SWITCH_FENCE_LUNCHBOX`
  (`0x531c80`) arms feature mode; each ground click creates one dish at the
  clicked point, with the 4x4 `FUN_005812b0` overlap check
  (`&WARNING_BUSY_SPACE`), cap 16 per fence (`0x527150`), price 100 charged
  to CONSTRUCTION (`0x585d20`), fence dish push `0x57e440`. The mode persists
  after a placement (the switch is not re-clicked per dish). See
  `notes/fence-lunchbox-switch-recon.md`.
- StaticBait variants (Apple/Flower/Hay/Pumpkin/Watermelon) and save
  persistence of placed feeders; only the LunchBox actor is placed now.
- `dino+0x288` is **solved**: a fresh park copies `+0x9c..+0xac` (the five
  phenotype attack parts) to `+0x288..+0x298`, so the Eat amount is
  `attack_head` (phenotype `+0x4c`; anky 30). `GetCourage` overwrites the
  slot from `+0x270` when it completes; the port's park node set excludes
  that action. The stand-cell search (`FUN_0055b5b0`) is ported (disc +
  occupancy + minimum squared distance to the dino cell, filtered to the
  band); exact K-square clearance and spiral tie order remain open. The Eat
  element now plays when the mover is idle, with `field_2e` only the action
  cap.
- The employee schedule actors and the visitor `.bsi` quantity/nutrition
  semantics; the carnivore live-prey (`.bni`) path.
