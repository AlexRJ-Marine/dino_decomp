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

## Parts 4–6 — dummy tables

Parts 4 and 5 have **three** subparts (`Head`, `Body`, `Tail`), not five,
matching the `.gen` dummy genes 4–9.

- Part 4 "Dummy disposition": `48` then 48 rows × six ints. Head[0] =
  `0 0 0 0 0 0`, Head[1] = `3 0 0 3 3 3`.
- Part 5 "Dummy type": `48` then 48 rows × five ints. Head[0] =
  `0 0 0 0 0`, Head[1] = `7 8 9 8 0`.
- Part 6 "Dummy influence": **two** `50:` tables of `18:` + 18 ints, then
  five reference widths:
  - table 0 block 0 = all zeros; block 49 starts `40 0 0 2 2 1`;
  - table 1 block 0 = all 100; block 49 starts `100 100 101 100 110 100`;
  - widths `100 100 100 300 150`, exactly `graphic_trans.dic`.
  The source comment says they are "Reference width for members, used in
  dummy bonus computation".

The 48 rows align with the `.gen` dummy position range (0..47); the 50 rows
of Part 6 align with the Part 5 type ids (0..49). The 18 columns are the
member/bone groups.

## Parts 7–9 — skin, carapace, min/max

- Part 7: `8:` skin types × (`3:` + 3 values) = type 0 `0,0,0`, type 1
  `1,2,3`, …, type 7 `19,20,21`; a single value `582`; then two `22:` tables
  of 22 × (`18:` + 18 ints). Table 0 row 0 all zeros; table 1 row 0 is
  `100×17, 101`.
- Part 8: `6:` carapace types × 3 values (`0..15`); two `16:` tables of
  16 × (`18:` + 18 ints). Table 0 row 0 zeros; table 1 row 0 all 100.
- Part 9: 20 min values (all 0) and 20 max values
  (`100000, 100000, 100.0, 100, 200×14, 65536, 100`).

The `582` value and the exact bonus/percentage semantics remain open.

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
