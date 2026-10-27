# `Market.ini` ? dinosaur store catalog

Status: **solved.** 54 entries in 6 store pages, every price and `.gen` file
validated against the retail tree.

## Inventory

`Data/Design/Scenario/Market.ini` ? 1,763 bytes, cp1252. It includes
`MissionBase.ini` with `[#INCLUDE]` and then has one `[DINOSTORE]` section:

```ini
[DINOSTORE]
DinoStore0="Allosaurus 9000 allo.gen Allosaurus_Leoparus 10000 allo2.gen ..."
DinoStore2="..."
DinoStore3="..."
DinoStore4="..."
DinoStore5="..."
DinoStore6="..."
```

`DinoStore1` is **absent** in the retail file (keys are 0,2,3,4,5,6).

## Entry format

Each store value is a flat list of **triplets**:

```text
<display name> <price> <genotype file>
```

- Display names use underscores for spaces (`Tyrannosaurus_Rex`,
  `Ankylosaurus_Minus`); they are raw strings, not localisation ids.
- Price is an integer in park budget units.
- Genotype is a `.gen` file in `Data/Design/Scenario/Genotype/`.

| Store | Entries | Examples |
| ---: | ---: | --- |
| 0 | 10 | Allosaurus 9000, Brachiosaurus 42000, Catrosaurus 33000 |
| 2 | 14 | Diplodocus 35000, Giganotosaurus 20000, Gradiobodus 7000 |
| 3 | 14 | Iguanodon 6000, Ogrosaurus 39000, Pachycephalosaurus 7000 |
| 4 | 9 | Paracephalodops 39000, Protoceratops 4000, Saltasaurus 30000 |
| 5 | 5 | Stegosaurus 6500, Tyrannosaurus_Rex 18000, Triceratops 12000 |
| 6 | 2 | Velociraptor 15000, Zarbosaurus 4900 |

Total 54 entries, 54 distinct genotypes, prices 3,200-42,000.

## Cross-checks

- All 54 genotypes exist on disk.
- The 57 design `.gen` files minus the market's 54 leave exactly
  `anky2.gen`, `peleca3.gen`, `proto2.gen` (mission/cut data, not for sale).
- The store prices are the `DINOACHAT` category source in the park economy
  (`docs/decomp/economy.md`).

## Reader

`tools/formats/market.py`:

```text
python tools/formats/market.py <Market.ini>
```

`read_market` returns `Market.entries` (`store`, `name`, `price`,
`genotype`) in file order, plus `stores` and `by_genotype()`. It validates
the triplet count, integer prices and `.gen` suffix. Tests:
`tests/test_market.py` (54 entries, store counts, spot entries, genotypes
exist, the three unsold design files).

### C++ reader

`src/formats/market.{h,cpp}`: `load_market(path)` returns the same entries in
store order (`DinoStore0,2..6`) and validates the triplets, integer prices and
`.gen` suffix. The includes are ignored because all `[DINOSTORE]` keys live in
`Market.ini` itself.

`market_from_values(CampaignMission::values)` parses the same triplet format
from a campaign mission's effective `[DINOSTORE]` block (keys
`DINOSTORE.DINOSTORE<N>`): the mission's own block, or the all-empty pages
inherited from `MissionBase.ini` when it has none. Empty blocks yield an empty
catalog rather than an error. The viewer selects the source per mode:
`Market.ini` for the plain park, `FreeGame.ini` for free game (20 entries), and
the mission block for a campaign (`Cpg04_Mission2` = 4 entries; Cpg01/Cpg02 and
several later missions = 0). Each switch prints
`store catalog: <source> entries=N`.

```text
dino_viewer.exe --market-info --asset-root game/%MAINDIR%
market: entries=54 stores=6 prices=3200..42000
market stores: 0=10 2=14 3=14 4=9 5=5 6=2
first=Allosaurus 9000 allo.gen last=Zarbosaurus 4900 zarbo.gen
```

Tests: `tests/test_viewer_cpp.py::test_cpp_market_info`.

The same parser handles the free-game subset in `FreeGame.ini` (20 entries in
the same six store keys, prices 4,000..42,000):

```text
dino_viewer.exe --store-info Data/Design/Scenario/FreeGame.ini --asset-root game/%MAINDIR%
store: entries=20 stores=6 prices=4000..42000
store stores: 0=3 2=4 3=4 4=4 5=4 6=1
first=Allosaurus 9000 allo.gen last=Velociraptor 15000 velo.gen
```

Tests: `tests/test_viewer_cpp.py::test_cpp_freegame_store`.

## Evidence

- `Data/Design/Scenario/Market.ini` (read-only).
- `Data/Design/Scenario/Genotype/*.gen`.
- `tools/formats/market.py`, `tests/test_market.py`,
  `src/formats/market.{h,cpp}`.

## Open

- The store page order used by the in-game shop UI.

Page 1 is simply absent from `Market.ini` (`DinoStore0`, `DinoStore2`..`6`);
the loader walks the keys that exist, so this is an authoring gap, not a
format feature. Prices are **not** modified by difficulty: `FUN_00561bb0`
selects only `Budget`, `SellBackFactor` and `VisProfiles` from the difficulty
branch, and the saved 20-entry free-game store keeps the `Market.ini` prices
(see `docs/formats/campaign.md` and `docs/formats/sav.md`).
