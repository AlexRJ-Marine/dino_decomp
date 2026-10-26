# `Market.ini` recon (2026-10-26)

Read-only analysis of `Data/Design/Scenario/Market.ini`.

## Structure

`Market.ini` includes `MissionBase.ini` and contains a single `[DINOSTORE]`
section with keys `DinoStore0`, `DinoStore2`, `DinoStore3`, `DinoStore4`,
`DinoStore5`, `DinoStore6` (page 1 absent).

Each value is a flat triplet list: display name, price, `.gen` file:

```
Allosaurus 9000 allo.gen
Allosaurus_Leoparus 10000 allo2.gen
...
```

Names use underscores instead of spaces and are not loc ids (not present in
ParkEng.loc). Prices are integers in park budget units.

## Counts

```
store 0: 10   store 2: 14   store 3: 14
store 4: 9    store 5: 5    store 6: 2
total 54 entries, 54 distinct genotypes
```

Prices range 3,200 (Rex) to 42,000 (Brachiosaurus). Examples: Allosaurus
9000, Tyrannosaurus_Rex 18000, Triceratops 12000, Velociraptor 15000.

## Cross-checks

- All 54 genotypes exist.
- Design genotypes not in the market: exactly `anky2.gen`, `peleca3.gen`,
  `proto2.gen`.
- The prices feed the park economy `DINOACHAT` category.

## Tooling

- `tools/formats/market.py` ? reads the triplets, validates price/genotype.
- `tests/test_market.py` ? 54 entries, store counts, spot values, file
  existence, unsold design files.
- `docs/formats/market.md` ? spec.

## Open

- Missing store page 1.
- Difficulty price modification.
- Shop UI page order.
