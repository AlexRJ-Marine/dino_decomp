# Static bait variants — placement and data (2026-10-31)

Read-only Ghidra pass on the retail `DinoIsland.exe` plus the retail
`FeatureList.ini`/`.bfi` data. Extends the LunchBox path
(`notes/fence-lunchbox-switch-recon.md`) to the five ground-tile baits.

## Question

How are the construction-kit static baits (Flowers, Fruit, Hay, Pumpkin,
Watermelon) selected and placed, and what data/cap/budget apply?

## Answer

The construction-kit **bait category** switch sets category 5; the bait
button selects the corresponding `FeatureList.ini` entry; the shared
`FUN_0052bbb0` then sets the construction-state feature record and enters
feature mode. The world click is the **same generic `FUN_0052b0d0`** as the
LunchBox dish: 4x4 overlap (`FUN_005812b0`), budget gate (`FUN_0052bed0`),
create actor (`0x53acd0`), charge the `.bfi` price to CONSTRUCTION
(`0x585d20`). The cap `0x527150` is generic but only special-cases the
`"LunchBox"` tag (16); **static baits have no cap**. Static baits are
ground tiles (`display_type 3`, no mesh), quantity 10000, nutrition 70,
actions `EatBait`/`EatBaitForSmallies`.

## 1. Category and selection

- `CONSTRUCTION_KIT_BAIT_FEATURE_GROUP` (`0x663b3c`) → construction-state
  `+0xa8`; all six category switches share `LAB_00531a80` (`0x531a80`):
  click → `[state+0x8c] = category`, then show only that category's group.
  Binding order: `0 Cage, 1 Bonus, 2 Obstacle, 3 Special, 4 Waypoint, 5 Bait`.
- The bait group callbacks `LAB_00531b60`/`LAB_00531b80` populate/refresh the
  group; `FUN_0052bbb0` reads the current UI list selection, stores it at
  `state+0x78` and sets feature mode `+0x48 = 1`.
- The LunchBox fence switch also calls `FUN_0052bbb0`; the feature/tile list
  selection path at `0x532372` calls it too. There is no hard-coded `.bfi`
  index in the click; the record comes from the UI list.
- **OPEN:** the exact per-`SWITCH_BAIT_*` callback binding (list-row user
  data vs. fixed tile slot). The `switch_key` field in each `.bfi` is the
  intended UI mapping.

## 2. Data (FeatureList.ini + `.bfi`)

FeatureList order: `StaticBaitFlower` (31), `StaticBaitApple` (32),
`StaticBaitHay` (33), `StaticBaitPumpkin` (34), `StaticBaitWatermelon` (35)
after the waypoint teams; the `.bfi` `switch_key` is authoritative.

| Feature | switch_key | price | food_kind | field_3c | field_40 | actions |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| `StaticBaitFlower` | `SWITCH_BAIT_FLOWER` | 10 | 0 | 10000 | 70 | `EatBait`, `EatBaitForSmallies` |
| `StaticBaitApple` | `SWITCH_BAIT_FRUITS` | 10 | 1 | 10000 | 70 | same |
| `StaticBaitHay` | `SWITCH_BAIT_HAY` | 30 | 4 | 10000 | 70 | same |
| `StaticBaitPumpkin` | `SWITCH_BAIT_CITROUILLE` | 50 | 3 | 10000 | 70 | same |
| `StaticBaitWatermelon` | `SWITCH_BAIT_PASTEQUE` | 40 | 2 | 10000 | 70 | same |

All: width 4, height 4, vertical 4.0, `display_type 3` (ground tile),
`category 2`, `state_count 1`, `anim_speed 0.1`, one texture
`DATA\graphics\TEXTURES\buildings\animaux.tga` in mesh slot 6, no `.mesh`.
The live baits are `AnimalBaitPig/Cow/Goat/Ostrich/Sheep` (indices 36..40),
not static feeders.

## 3. Port recipe

```text
SWITCH_FEATURE_CATEGORY_BAIT: target group = CONSTRUCTION_KIT_BAIT_FEATURE_GROUP
SWITCH_BAIT_FLOWER/FRUITS/HAY/CITROUILLE/PASTEQUE:
    selected feature = StaticBaitFlower/Apple/Hay/Pumpkin/Watermelon
    build_tool = 7 (generic feature mode)
world click:
    cell = floor(ground); rect = cell..cell+3
    if overlap -> WARNING_BUSY_SPACE
    if budget < price -> WARNING_NOT_ENOUGH_MONEY
    create Feeder (food_kind, 10000, 70, EatBait actions) at the point
    budget -= price; ledger.spend_construction(price)
    no 16 cap (LunchBox only)
```

The port's `.bfi` parser already exposes `price`, `food_kind`,
`food_quantity`/`food_nutrition` (field_3c/field_40), `actions`,
`switch_key`, `display_type`, `textures`; `tests/test_bfi.py::test_static_bait_variants`
locks the five rows against the retail files.

## Port status (2026-10-31)

Wired: `BfiFeature.display_type`; `Game::place_feature_at` (generic; the
LunchBox bowl/mesh path plus the `display_type 3` full-texture ground quad);
`--place-bait NAME --bait-pos X,Y`; the construction-kit UI path
`OPEN_CONSTRUCTION` (764,135) -> `OPEN_CONSTRUCTION_KIT` (648,145) ->
`SWITCH_FEATURE_CATEGORY_BAIT` (111,259) -> `SWITCH_BAIT_FLOWER` (52,332) ->
ground click. The five switches map through their `.bfi` `switch_key` to
`park_session.selected_feature`; the generic tool 7 path loads the `.bfi` and
applies the 4x4 overlap/budget, with the 16 cap only for the LunchBox class.
`--place-bait` is the uncharged boot hook; the UI/ground-click path charges
CONSTRUCTION. Tests: `test_static_bait_variants` (data),
`test_cpp_static_bait_place`, `test_cpp_static_bait_ui`; full suite 546.

The ground quad uses the full `animaux.tga` UV (0,0)-(1,1), footprint 4x4,
yaw 0, terrain Z + 0.08. The exact retail display-type branch/quad builder
inside the `FUN_005f4e10` virtual (`vtable+0x28`) remains OPEN; the atlas
reading is disproven (the texture is one 128x128 image, not a grid).

## Open

1. The ground-tile visual: the full-texture quad is the only data-supported
   reading; the exact display-type branch/UV builder inside the
   `FUN_005f4e10` virtual remains OPEN.
2. The exact per-`SWITCH_BAIT_*` callback binding (the selection mechanism is
   shared and proven; the port maps by the `.bfi` `switch_key`).
3. The live `AnimalBait*` actor behavior (moving goat/cow/etc.) is not part of
   this static path.

## Evidence

- Ghidra: `LAB_00531a80`, `FUN_005314c0`, `LAB_00531b60`/`LAB_00531b80`,
  `FUN_0052b760`/`FUN_0052b960`/`FUN_0052bb10`, `FUN_0052bbb0`,
  `FUN_0052b0d0`, `FUN_005812b0`, `FUN_0052bed0`, `0x527150`, `0x53acd0`,
  `0x585d20`.
- `Data/Design/attractions/features/FeatureList.ini`,
  `StaticBait{Flower,Apple,Hay,Pumpkin,Watermelon}.bfi`;
  `docs/platform/ui-coordinates.md` (`SWITCH_FEATURE_CATEGORY_BAIT`,
  `SWITCH_BAIT_*`); `tests/test_bfi.py`.
