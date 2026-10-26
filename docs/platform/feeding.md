# Feeding — food sources and the Eat gate

Status: **first slice.** A placed `.bsi` food source gates the dinosaur's Eat
action by paddock and diet, and each completed meal consumes a portion.
Employee feeding and the exact `.bsi` quantity semantics remain open.

## Retail chain

- `Eat.bai` and `EatForSmallies.bai` have `consume_flag = 1`. The gate
  `FUN_0056e0e0` rejects an action when its target item is absent, and
  `FUN_0056e670` only scores available nodes. Without food, a hungry dinosaur
  falls back to Exploration (`.bai` base score 1580).
- Food sources are player-built via `FOOD_DISTRIBUTION`:
  `FoodSourceList.ini` + `.bsi`. Kinds 0-4 are vegetarian, 5-9 carnivorous
  (the `.bsi` UI button names and `.bni` animal kinds agree).
- `asset[0]` is the placed animal/plant model (pig, cow, apple, …); the
  `feeder_mesh` in the tail is the employee's carrying model. The texture is
  `<texture_dir>/<mesh stem>.tga`, or the stem without its `_lodN`/`_partN`
  suffix for the plant meshes (`apple_lod1` -> `apple.tga`).
- The hourly schedule registers each food source with the nearest kind-8
  (nutritionist) manager (`0x5953F0`/`0x58B5B0`). Employees are not ported
  yet, so the port gates the dinosaur directly on the source.

## Port

- `src/formats/bsi.{h,cpp}` reads the definitions.
- `src/world/food.{h,cpp}` resolves the placed mesh + texture and exposes
  `food_is_carnivore(kind)`.
- CLI:
  ```text
  --place-food NAME|KIND   e.g. PigFood or 8
  --food-pos CELL_X,CELL_Y build-grid cell (default: first paddock centre)
  ```
  The food model is added to the scene draw list and placed on the terrain.
- **Diet**: genotype gene 14. The `Diet` expression matrix is
  `[[1,1],[1,2]]` with translation `1 = herbivore, 2 = carnivore`, so only a
  `(1,1)` pair is carnivorous. Cpg04 has 19 carnivores and one herbivore
  (the anky row).
- **Gate**: `DinoActionLoopConfig::inventory` is the `.bai` consume flag; the
  viewer sets it per dinosaur each frame from a source in the dinosaur's
  paddock whose kind matches its diet and whose remaining quantity is > 0.
- **Meal**: a completed `Eat`/`EatForSmallies` removes one portion. Treating
  `quantity` as the number of portions is **inferred**; the exact `.bsi`
  quantity semantics are open. At 0 the source is removed from the scene and
  Eat becomes unavailable (the dinosaur explores again).
- Without `--place-food`, no dinosaur can Eat. This replaces the earlier
  port assumption that food was always available; the action-loop tests now
  expect Exploration in that case.

## Verification

- `test_cpp_food_feeding`: PigFood at cell (118,94) (paddock 1) → the three
  allos in that paddock choose `Eat score=9009`, the herbivore anky in
  paddock 0 sees `source=none`, and five fast meals print
  `remaining=0.00` + `food: PigFood depleted`.
- `test_cpp_dino_action_loop`: without food, starving dinos pick Exploration
  1580 and the 50 % steady state picks Exploration 75; Sleep is unaffected
  (no consume flag).
- `notes/food-pig-close.png`: the placed pig renders with its texture.

## Open

- Employee feeding (nutritionists, hourly schedule) — STAFF research is
  documented, but no staff actors exist in the port.
- The interactive `FOOD_DISTRIBUTION_PANEL` is not ported. Its state ctor is
  `FUN_00537040`: it resolves `FOOD_DISTRIBUTION_PANEL`, `PRICE_SLOT`,
  `FOOD_DISTRIBUTION_TYPE_GROUP` and ten `FOOD_DISTRIBUTION_TIME_BUTTONn`
  widgets from a 200-byte source-record list (`record+0xB8` name, callbacks
  `LAB_00537940`/`LAB_00537AF0`); `FUN_00537DC0` re-selects the type group and
  toggles the time buttons from `state+0x58`. The placement state string is
  `LUNCHBOX_SELECTED`. Until it is ported, `--place-food` is the only way to
  place a source.
- Exact `quantity`/nutrition consumption per meal, and whether a meal refills
  the FOOD need independently of the `.bai` delta.
- Carnivore prey animals (`.bni`) vs the static `.bsi` sources; the retail
  carnivore food may be a live animal actor.
- The port save does not store placed food sources or paddocks, so feeding
  after `--load-port` starts from no sources.
