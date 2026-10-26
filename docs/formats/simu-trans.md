# `simu_trans.dic` — the genotype/phenotype database

Status: **solved** for parts 1, 2, 3 and 10; parts 4–9 are inventoried but not
fully parsed.

## Inventory

`Data/Design/Genotype/simu_trans.dic` — 135,383 bytes, 7,773 lines, cp1252
text, ten `// Part N : title` sections:

| Part | Title | Shape | Status |
| ---: | --- | --- | --- |
| 1 | gene translation table | 18 gene blocks | solved (`gene_trans.py`) |
| 2 | merge ratio table | 4 × (20 diagonal + 20×20) | solved |
| 3 | attributes linked to members | 55 × (20 diagonal + 20×20) | solved |
| 4 | dummy disposition table | 5 × 48 × 6 ints | inventoried |
| 5 | dummy type table | 5 × 48 × 5 ints | inventoried |
| 6 | dummy influence table | 50 × 18 ints | inventoried |
| 7 | skin translation/influence | 8 × 3, then 582 × 22 × 18 | inventoried |
| 8 | carapace translation/influence | 6 × 3, then 16 × 18 | inventoried |
| 9 | min & max attributes | 20 + 20 values | inventoried |
| 10 | default family per base dinosaur | 20 indices | solved |

## Part 1 — genes

18 blocks (`// Translation of the gene <Code>` / `// Rarities ...`), parsed by
`tools/formats/gene_trans.py`; see `docs/formats/gen.md`.

## Part 2 — merge ratios

Four blocks (`Head`, `ArmLeg`, `Body`, `Tail`), each:

```text
// <part> : merge ratio
20
<20 diagonal ints>
<20 rows of 20 ints>
```

The matrices are **byte-for-byte identical** to the four tables in
`anim_trans.dic` (verified for all 4 × 400 cells). `anim_trans.dic` is a
derived/duplicated export of this part.

## Part 3 — part attributes

Eleven attributes × five parts = 55 blocks, each with a 20-value diagonal and
a 20×20 matrix:

```text
// Head : Length
20
224 167 662 ...        (20 diagonal values, species order)
<20 rows of 20 ints>   (merge ratios)
```

Attributes: `Length`, `Width`, `Height`, `Weight`, `Perception`,
`Intelligence`, `Beauty`, `Armor`, `Speed`, `Attack`, `Agility`.
Parts: `Head`, `Arm`, `Leg`, `Body`, `Tail`.

Examples (first row = `allo`, first column = `allo`):

| Table | Diagonal[0] | Matrix[0][1] |
| --- | ---: | ---: |
| Head:Length | 224 | 195 |
| Body:Length | 200 | 213 |
| Tail:Length | 318 | 265 |
| Head:Beauty | 2 | 2 |
| Head:Attack | 80 | 54 |
| Head:Speed | 0 | 0 |

`Head:Speed` is all zeros; speed lives in the other parts. These tables are
the source of the translator's phenotype accumulator fields
(`docs/formats/translator-constants.md`).

## Part 10 — default animation family

```text
20
2 4 0 3 3 0 2 3 1 1 3 1 3 4 0 1 2 4 3 3
```

Species order is the `anim_trans.dic` order (`allo`..`velo`); indices map to
`HighQuadruped` (0), `LightQuadruped` (1), `HeavyBiped` (2), `LightBiped` (3),
`LowQuadruped` (4).

| Family | Count | Dinosaurs |
| --- | ---: | --- |
| HighQuadruped | 3 | brachio, diplo, salta |
| LightQuadruped | 4 | igua, kentro, parasau, stego |
| HeavyBiped | 3 | allo, gigano, trex |
| LightBiped | 7 | coelo, deino, hypsilo, pachy, peleca, trood, velo |
| LowQuadruped | 3 | anky, proto, trice |

Note `pachy` is **LightBiped** despite its quadruped build — the animation
family is a data choice, not a morphology rule.

## Parts 4–9 (inventory)

- **Dummy disposition** (Part 4): five parts, each 48 rows of six ints
  (`0`/`1`/`2`/`3`/`4`/`5` tags per dummy slot).
- **Dummy type** (Part 5): five parts, each 48 rows of five ints (dummy type
  ids 0–49).
- **Dummy influence** (Part 6): `50:` followed by 50 blocks of `18:` + 18
  ints (per-dummy influence over the 18 bones/groups).
- **Skin** (Part 7): `8:` skin types, each `3:` + 3 values (allele→skin
  mapping), then `582` rows of `22:` + `18:` + 18 ints (influence matrix).
- **Carapace** (Part 8): `6:` carapace types, each `3:` + 3 values, then
  `16` rows of `18:` + 18 ints.
- **Min/max attributes** (Part 9): 20 min values (0, 0, 0, …) and 20 max
  values (100000, 100000, 100.0, 100, 200×14, 65536), plus a trailing 100.

## Reader

`tools/formats/simu_trans.py`:

```text
python tools/formats/simu_trans.py --dump <simu_trans.dic>
```

Returns `SimuTrans` with:

- `.genes` — the 18 `GeneTable`s (Part 1);
- `.merge` — `{Head, ArmLeg, Body, Tail}` tables (Part 2);
- `.attributes` — `{(part, attribute): MergeTable}` (Part 3, 55 entries);
- `.default_families` and `.family_of(index)` (Part 10);
- `.part_headers` — the ten `(number, title)` pairs.

Tests: `tests/test_simu_trans.py` (18 genes, 10 parts, 4 merge tables equal
to `anim_trans.dic`, 55 attribute tables and spot values, family table).

## Evidence

- `Data/Design/Genotype/simu_trans.dic` (read-only).
- `tools/formats/simu_trans.py`, `gene_trans.py`,
  `tests/test_simu_trans.py`.
- Part 2 == `anim_trans.dic` matrices (cross-check).
- Part 3 values feed the phenotype accumulator in
  `docs/formats/translator-constants.md`.

## Open

- Parts 4–9 exact record semantics (dummy/skin/carapace tables).
- How Part 3's 20×20 merge matrices combine hybrid parts.
- The gene→Part 3 lookup used by the translator (which attribute/part a
  gene allele selects).
