# `simu_trans.dic` recon (2026-10-25)

Read-only parse of the genotype/phenotype database. No writes to `game/` or
`original/`.

## Inventory

`Data/Design/Genotype/simu_trans.dic` is 135,383 bytes / 7,773 lines of
cp1252 text, split by `// Part N : title` headers:

```text
Part 1  line    1  gene translation table
Part 2  line 1230  merge ratio table
Part 3  line 1405  attributes linked to members
Part 4  line 3773  dummy disposition
Part 5  line 3929  dummy type
Part 6  line 4085  dummy influence
Part 7  line 6096  skin translation/influence
Part 8  line 7023  carapace translation/influence
Part 9  line 7699  min & max attributes
Part 10 line 7749  default family per base dinosaur
```

`gene_trans.py` already parsed Part 1 (18 genes). `simu_trans.py` now parses
Parts 2, 3 and 10 and exposes all ten headers.

## Part 2 — merge ratios

Four blocks (`Head`, `ArmLeg`, `Body`, `Tail`), each `20` + 20 diagonal ints +
20×20 matrix. Cross-checked against `anim_trans.dic`: all **1,600** matrix
cells match exactly. `anim_trans.dic` is a duplicate export of this part.

## Part 3 — 55 attribute tables

Eleven attributes (`Length`, `Width`, `Height`, `Weight`, `Perception`,
`Intelligence`, `Beauty`, `Armor`, `Speed`, `Attack`, `Agility`) × five parts
(`Head`, `Arm`, `Leg`, `Body`, `Tail`), each `20` + 20 diagonal + 20×20.

Spot values (species order allo..velo):

```text
Head:Length diag[0..4] = 224 167 662 54 91   matrix[0][0..4] = 224 195 443 138 157
Body:Length diag[0..4] = 200 226 582 53 80   matrix[0][0..4] = 200 213 391 126 139
Tail:Length diag[0..4] = 318 213 680 68 95   matrix[0][0..4] = 318 265 499 192 206
Head:Beauty diag[0..4] =   2   2  15 14  2   matrix[0][0..4] =   2   2   8   8   2
Head:Attack diag[0..4] =  80  30  40 60 75   matrix[0][0..4] =  80  54  59  69  77
Head:Speed  = all zeros
```

The diagonal is the pure-species value; the matrix is the merge ratio when
the part comes from another species. These are the data behind the phenotype
accumulator in `docs/formats/translator-constants.md` (the translator's
gene/part records use fields `+0x128..+0x19c`).

## Part 10 — default families

```text
2 4 0 3 3 0 2 3 1 1 3 1 3 4 0 1 2 4 3 3
```

Family order is `HighQuadruped, LightQuadruped, HeavyBiped, LightBiped,
LowQuadruped`. Counts: 3 / 4 / 3 / 7 / 3. Pachy is index 10 → **LightBiped**
despite its quadruped build. This is the mapping the animation system uses
when a species has no explicit family.

## Parts 4–9 (inventory)

- Part 4: five parts, 48 rows × six ints (dummy disposition tags 0–5).
- Part 5: five parts, 48 rows × five ints (dummy type ids 0–49).
- Part 6: `50:` then 50 × (`18:` + 18 ints) — dummy influence over 18 groups.
- Part 7: `8:` skin types × (`3:` + 3 values), then `582` × (`22:` + `18:` +
  18 ints) — skin allele mapping + influence.
- Part 8: `6:` carapace types × (`3:` + 3 values), then `16` × (`18:` + 18
  ints).
- Part 9: 20 min values (all 0 except a 0.000000 at index 2) and 20 max
  values (`100000, 100000, 100.0, 100, 200×14, 65536`), plus a trailing 100.

## Tooling

- `tools/formats/simu_trans.py` — parts 1/2/3/10, `--dump`.
- `tests/test_simu_trans.py` — 18 genes, 10 headers, 4 merge tables equal to
  `anim_trans.dic`, 55 attributes with spot values, family table.
- `docs/formats/simu-trans.md` — format spec.

## Evidence

- `Data/Design/Genotype/simu_trans.dic` (read-only).
- `Data/Design/Genotype/anim_trans.dic` — Part 2 duplicate.
- `tools/formats/simu_trans.py`, `gene_trans.py`,
  `tests/test_simu_trans.py`, `docs/formats/simu-trans.md`.

## Open

- Parts 4–9 record semantics.
- How Part 3's merge matrices combine hybrid parts (the 20×20 values are not
  symmetric; Part 2 is directional too).
- Which gene allele selects which Part 3 part/attribute row.
