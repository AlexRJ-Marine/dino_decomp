# `.bsi` food source

Status: **solved** for the header, assets and build-menu button; the exact
meaning of `quantity` is partial.

## Inventory

10 files in `Data/Design/foodsource/`, one per `FoodSourceList.ini` entry
(371–409 bytes). All 40 asset paths and all feeder meshes exist; all 10 UI
names resolve in the interface INIs.

| Kind | File | Nutrition | Quantity | Price | Build button |
| ---: | --- | ---: | ---: | ---: | --- |
| 0 | FlowerFood | 100 | 0.25 | 100 | `..._VEGETARIAN_FLOWER_BUTTON` |
| 1 | AppleFood | 200 | 2 | 200 | `..._VEGETARIAN_FRUITS_BUTTON` |
| 2 | WatermelonFood | 120 | 12 | 400 | `..._VEGETARIAN_PASTEQUE_BUTTON` |
| 3 | PumpkinFood | 150 | 20 | 600 | `..._VEGETARIAN_CITROUILLE_BUTTON` |
| 4 | HayFood | 250 | 50 | 1,000 | `..._VEGETARIAN_HAY_BUTTON` |
| 5 | OstrichFood | 200 | 0.15 | 100 | `..._CARNIVOROUS_AUTRUCHE_BUTTON` |
| 6 | GoatFood | 250 | 0.3 | 250 | `..._CARNIVOROUS_GOAT_BUTTON` |
| 7 | SheepFood | 400 | 2.5 | 500 | `..._CARNIVOROUS_MOUTON_BUTTON` |
| 8 | PigFood | 600 | 5 | 700 | `..._CARNIVOROUS_PIG_BUTTON` |
| 9 | CowFood | 800 | 7 | 1,200 | `..._CARNIVOROUS_COW_BUTTON` |

`...` is `FOOD_DISTRIBUTION`; labels are a mix of English and French.

## Layout

```text
0   f32 nutrition / food value
4   f32 quantity / portion (semantics partial)
8   u32 price
12  u32 kind id (FoodSourceList order)
16  u32 0
20  u32 0
24  u32 flags (256 in all files)
28  u8  flag (0)
29  four length-prefixed strings:
      food LOD1 mesh, part1 mesh, part2 mesh, texture directory
... tail: numeric bytes + feeder mesh + FOOD_DISTRIBUTION_*_BUTTON
```

The four asset strings are at the start of the table; the feeder mesh
(`...\personnel\feed\feed_apple.mesh`) and the build button are in the tail,
interleaved with numeric data, so the reader scans for them.

## Reading

`tools/formats/bsi.py`:

```text
python tools/formats/bsi.py --dump <file.bsi>
python tools/formats/bsi.py --scan <foodsource-dir>
```

Returns nutrition, quantity, price, kind id, flags, the four assets, feeder
mesh and UI button. Tests: `tests/test_bsi.py` (Apple values, 10-file scan,
kind order, all assets/UI names).

The portable reader is src/formats/bsi.{h,cpp}; the same values are printed
by the viewer (compare against the Python oracle):

`	ext
dino_viewer --asset-root <game> --food-info Data/Design/foodsource
food: FlowerFood nutrition=100 quantity=0.25 price=100 kind=0 flags=256 ...
food sources: 10
`

	est_cpp_food_info checks all ten rows and that every feeder mesh exists.

## Evidence

- `Data/Design/foodsource/*.bsi`, `FoodSourceList.ini` (read-only).
- Interface INIs for the `FOOD_DISTRIBUTION_*` button names.
- `tools/formats/bsi.py`, `tests/test_bsi.py`.

## Open

- Exact meaning of `quantity` (portion count, feeding time, or amount).
- `flags = 256` and the byte at offset 28.
- Numeric tail fields (likely feeding/ai parameters).
- Why carnivore entries use much smaller quantities (0.2–7) than hay (50).
