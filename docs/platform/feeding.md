# Feeding — visitor/employee sources and the dinosaur LunchBox

Status: **LunchBox actor ported (herbivore path).** The `.bsi` `FoodSource`
feeds visitors/employees; park dinosaurs eat a **LunchBox/StaticBait** item
actor via `FUN_00576e50`, which fills the food gauge `+0x25c`. The port's
former dino-Eat-from-`.bsi` gate and 3-unit meal were a placeholder and are
removed. `--place-lunchbox` places the `.bfi` bowl; the herbivore anky eats it
and the gauge clamps to its weight. Interactive placement, the other baits
and save persistence remain open.

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
- The Cpg04 60 s probe saw no dino Eat and no `SetLevel`; its six `0x522300`
  controllers are visitor-side with a null source. The real dino path is only
  active when a LunchBox/bait item actor is present.

## Port

- `src/formats/bsi.{h,cpp}` reads `.bsi`; `src/world/food.{h,cpp}` resolves
  the placed mesh/texture and exposes `food_is_carnivore`.
- `--place-food NAME|KIND` / `--food-pos` place a **visitor/employee**
  `.bsi` source. It no longer gates a dinosaur's Eat and is not consumed by
  dinosaurs.
- `DinoNeeds` carries the exact food gauge: `max_food = weight`,
  `food_gauge` (init `max_food*0.5`), `food_consumption = weight*0.001/s`.
  Every 10 s: `gauge -= 10*consumption`, `FOOD = ftol(gauge*99/max_food)`.
  `DinoActionLoop::apply_food(value, percent)` implements the event-0x17
  formula `gauge = value + percent*gauge/100`, clamped.
- `src/formats/bfi.*` keeps `food_quantity`/`food_nutrition`; `Feeder` +
  `feeder_drain` in `src/world/food.*` are the exact `FUN_00576e50` model.
- `--place-lunchbox` (optional `--lunchbox-pos CX,CY`, default first paddock
  centre) loads `LunchBox.bfi`, creates a `Feeder` (quantity 10000,
  nutrition 100, kind 0, radius = width/2), uploads `bowl.mesh` +
  `bowl.tga`, and adds it to the draw list.
- `ParkSession::paddock_feeder` returns the nearest non-empty feeder in the
  paddock whose `food_kind` matches the diet. `Eat` is offered only when one
  exists; the actor walks with the `0x34` marker. At the feeder, the Eat
  completion drains it (`feeder_drain`) and calls
  `apply_food(value)` (event 0x17).
- Cpg04 verification: the anky (herbivore, paddock 0) starts at
  (463.7,327.5), walks to the LunchBox at (460,328), and drains it:
  `dino eat: anky <- LunchBox nutrition=1000000 quantity=0 gauge=4255`.
  The allos are carnivores and cannot use the kind-0 LunchBox.

## Verification

- `test_cpp_lunchbox_eat`: the `.bfi` LunchBox quantity 10000/nutrition 100,
  the anky chooses Eat, walks to the bowl, drains it
  (`nutrition=1000000 quantity=0 gauge=4255`), and the allos never eat it.
- `test_cpp_food_source_is_visitor_only`: a placed PigFood shows
  `source=PigFood` in the food log but no dinosaur chooses Eat and the source
  is not depleted.
- `test_cpp_employee_feeding_and_wages`: the nutritionist still feeds the
  `.bsi` source and pays wages.
- `--selftest`: food gauge 10 s pass/meal clamp and `feeder_drain`
  30/empty checks.
- `test_cpp_dino_action_loop`: without a LunchBox, starving dinos pick
  Exploration 1580; Sleep is unaffected.

## Open

- Interactive LunchBox placement (`LUNCHBOX_SELECTED`, `FUN_00537040`); the
  `--place-lunchbox` CLI is the current test hook.
- StaticBait variants (Apple/Flower/Hay/Pumpkin/Watermelon) and save
  persistence of placed feeders; only the LunchBox actor is placed now.
- `dino+0x288`: `FILD [dino+0x288]` is the amount passed to `FUN_00576e50`;
  the field's identity/scale (probe read a float bit pattern there) is open.
  The LunchBox quantity (10000) is below the clamped amount, so one meal
  drains the feeder.
- The employee schedule actors and the visitor `.bsi` quantity/nutrition
  semantics; the carnivore live-prey (`.bni`) path.
