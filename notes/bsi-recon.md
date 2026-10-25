# Food source recon (`.bsi`) — 2026-10-25

## Inventory

10 files in `Data/Design/foodsource/`, 371–409 bytes, one per
`FoodSourceList.ini` entry. All 40 asset paths and 10 feeder meshes exist;
all 10 `FOOD_DISTRIBUTION_*` buttons resolve.

## Header (29 bytes)

```text
0   f32 nutrition
4   f32 quantity
8   u32 price
12  u32 kind id (list order 0..9)
16  u32 0
20  u32 0
24  u32 flags = 256
28  u8  flag = 0
29  4 length-prefixed strings (food LOD1, part1, part2, texture dir)
... tail: numbers + feeder mesh + FOOD_DISTRIBUTION_*_BUTTON
```

## Values

```text
kind file            nutrition quantity price  ui button
0    FlowerFood      100       0.2      100    VEGETARIAN_FLOWER
1    AppleFood       200       2        200    VEGETARIAN_FRUITS
2    WatermelonFood  120      12        400    VEGETARIAN_PASTEQUE
3    PumpkinFood     150      20        600    VEGETARIAN_CITROUILLE
4    HayFood         250      50       1000    VEGETARIAN_HAY
5    OstrichFood     200       0.2      100    CARNIVOROUS_AUTRUCHE
6    GoatFood        250       0.3      250    CARNIVOROUS_GOAT
7    SheepFood       400       2.5      500    CARNIVOROUS_MOUTON
8    PigFood         600       5        700    CARNIVOROUS_PIG
9    CowFood         800       7       1200    CARNIVOROUS_COW
```

Prefix is `FOOD_DISTRIBUTION_` and suffix `_BUTTON`. Labels mix English and
French.

## Assets

Each file has four header strings:

- food LOD1 mesh
- part1 mesh
- part2 mesh
- texture directory (`herbifood` or `animals`)

Tail strings add:

- feeder/person mesh, e.g. `feed_apple.mesh`, `feed_cow.mesh`
- build button name.

## Open

- `quantity` meaning (portion/time/amount).
- flags 256 and the offset-28 byte.
- numeric tail fields.
- Carnivore quantities are much smaller than hay (0.2–7 vs 50); likely a
  different unit or feeding duration.

## Evidence

- `Data/Design/foodsource/*.bsi`, `FoodSourceList.ini`.
- `tools/formats/bsi.py`, `tests/test_bsi.py`, `docs/formats/bsi.md`.
