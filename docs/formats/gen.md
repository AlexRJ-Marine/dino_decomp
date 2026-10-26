# `.gen` genotype

Status: **solved** for the file structure and the 18 design genes; the
phenotype/breeding rules and the 27-gene editor set are partial.

## Inventory

| Directory | Files | Genes each | Comments |
| --- | ---: | ---: | --- |
| `Data/Design/Scenario/Genotype` | 57 | 18 | 20 documented, 37 bare |
| `Data/Edition/Genotype` | 20 | 27 | all bare (editor data) |

The design files are the runtime dinosaur genotypes (`allo.gen`,
`anky.gen`, …) and their breed variants (`allo2.gen`, `anky2.gen`, …).
The edition files are the scenario editor's 27-gene set.

## Layout

Text, Windows-1252. The first token is the gene count, then two integers per
gene (allele A, allele B). `//` comments are optional and only document the
values:

```text
18                  <- gene count
0 0                 // head type
...
5 6                 // courage
```

A documented file (`allo.gen`) carries the species header in comments:

```text
//  --00--  ALLOSAURUS
//  Hauteur (m) 5
//  Longueur (m) 12
//  Poids (Kg) 2000
//  Marche B
//  Alimentation C
//  Vitesse Marche (km/h) 8
//  Vitesse Course (km/h) 40
//  Intelligence 1 .. 1.9
```

## Design genes (18)

| # | Gene | Documented range | Observed range |
| ---: | --- | --- | --- |
| 0 | head type | 0..19 | 0..19 |
| 1 | legs type | 0..19 | 0..19 |
| 2 | body type | 0..19 | 0..19 |
| 3 | tail type | 0..19 | 0..19 |
| 4 | head dummy position | 0..39 | 0..47 |
| 5 | head dummy type | 0..39 | 0..47 |
| 6 | body dummy position | 0..39 | 0..47 |
| 7 | body dummy type | 0..39 | 0..47 |
| 8 | tail dummy position | 0..39 | 0..45 |
| 9 | tail dummy type | 0..39 | 0..47 |
| 10 | skin type | 0..7 | 0..7 |
| 11 | skin subtype | 0..2 | 0..2 |
| 12 | carapace type | 0..5 | 0..5 |
| 13 | carapace subtype | 0..3 | 0..2 |
| 14 | diet | 0 herbivore / 1 carnivore | 0..1 |
| 15 | aggressiveness | 0..9 (display 1..10) | 0..9 |
| 16 | stamina | 0..9 (display 10..100) | 0..9 |
| 17 | courage | 0..9 (display 10..80) | 0..9 |

The dummy ranges in the comments are stale: observed values go to 47.

Example `allo.gen`: all phenotype genes 0, `diet (1,1)`,
`aggressiveness (8,7)`, `stamina (2,4)`, `courage (5,6)`.

The two alleles are the maternal/paternal copies; the breed variants
(`allo2.gen` …) are other allele combinations. How a pair maps to the visible
mesh/skin is open.

## Expression and dominance (`simu_trans.dic`)

`Data/Design/Genotype/simu_trans.dic` contains one block per gene:

```text
// Translation of the gene <Code>
<translation_count>        (0 for Head/ArmLeg/Body/Tail)
<translation values>
<translation_count^2 ints> (dominance matrix, row-major; omitted when 0)
// Rarities of the alleles of the gene <Code>
<allele_count>
<rarity weights>
```

The parser validates 18 blocks, allele counts
`20,20,20,20,48,48,48,48,48,48,8,3,6,3,2,10,10,10`, the matrices and the
rarity weights. Two expression rules are used:

| Genes | Rule |
| --- | --- |
| `HDDi`, `HDTy`, `BDDi`, `BDTy`, `TDDi`, `TDTy`, `SknTyp`, `CarTyp`, `Diet` | `translation[min(a, b)]` — the lower allele is dominant |
| `SknSub`, `CarSub`, `Agre`, `Stam`, `Cour` | `translation[(a + b) // 2]` — averaged |
| `Head`, `ArmLeg`, `Body`, `Tail` | no matrix; expression rule not proven (likely `min`) |

Examples with `allo.gen`:

| Gene | Alleles | Expressed |
| --- | --- | --- |
| skin type | (0, 0) | 0 |
| diet | (1, 1) | 2 (carnivore) |
| aggressiveness | (8, 7) | 80 |
| stamina | (2, 4) | 40 |
| courage | (5, 6) | 70 |

Rarity weights are 1–3 per allele (lower = rarer); they are the random
breeding weights. `SknTyp` is `3,3,2,2,2,2,1,1`; `Agre`/`Stam`/`Cour` are
`3,3,2,2,2,2,2,2,1,1`.

`anim_trans.dic` also lists the 20 dinosaur families (5 families:
HighQuadruped, LightQuadruped, HeavyBiped, LightBiped, LowQuadruped);
`graphic_trans.dic` holds reference widths `100,100,100,300,150`.

## Edition genes (27)

`Data/Edition/Genotype/*.gen` have 27 pairs, all bare. Across all 20 files
only slots **0-9 and 18** are ever non-zero:

| Slots | Values | Meaning |
| --- | --- | --- |
| 0-3 | 0..19 | species type (head, legs, body, tail) |
| 4/5 | 0..47 | head dummy position/type |
| 6/7 | 0..47 | body dummy position/type |
| 8/9 | 0..47 | tail dummy position/type |
| 10-17 | always `(0,0)` | unused/default |
| 18 | 0..9 | one trait, likely aggression (`allo` `(8,7)`) |
| 19-26 | always `(0,0)` | unused/default |

`kentro` and `stego` are the only files with dummy values: `(3,46)`,
`(3,46)`, `(3,46)` and `(4,47)`, `(4,47)`, `(4,47)`. The edition values do
**not** match the same-named design files pair-for-pair (e.g. design `allo`
has aggression `(8,7)` at index 15, edition `allo` at 18; design `anky` has
carapace values that the edition lacks), so this is an independent editor
data set, not a re-layout of the design genotype. Do not use these files at
runtime.

## Species selection (P2 import)

The saved 18-gene genotype selects a body mesh through gene 0 (head type):

```text
dominant allele = min(allele A, allele B)     # dominance rule from gene_trans
head type 0..19  ->  one base `<species>.gen` file
```

The 20 base design files in `Data/Design/Scenario/Genotype` (`allo.gen` ..
`velo.gen`) have exactly 20 distinct head types 0..19, so the mapping is
one-to-one and data-driven (no hardcoded species ids):

```text
0 allo   1 anky    2 brachio 3 coelo   4 deino
5 diplo  6 gigano  7 hypsilo 8 igua    9 kentro
10 pachy 11 parasau 12 peleca 13 proto 14 salta
15 stego 16 trex   17 trice  18 trood  19 velo
```

Hybrids use the dominant body plan; e.g. `[18,0]` is allo. The port reader is
`src/formats/dino_data.cpp` (`read_gen_head_type`); Cpg04 resolves all 20
saved dinosaurs to five species. Tests:
`tests/test_anim_trans.py::test_base_species_head_types_are_unique`.

## Reader

`tools/formats/gen.py`:

```text
python tools/formats/gen.py --dump <file.gen>
python tools/formats/gen.py --scan <dir>
```

Returns `gene_count` and the allele pairs; `--dump` also names the 18 design
genes. Tests: `tests/test_gen.py` (sample, 57+20-file scans, ranges).

`tools/formats/gene_trans.py` reads `simu_trans.dic` and exposes
`GeneTable.express(a, b)`:

```text
python tools/formats/gene_trans.py <simu_trans.dic>
```

Tests: `tests/test_gene_trans.py` (18 blocks, counts, min/average rules,
rarity weights).

## Evidence

- `game/%MAINDIR%/Data/Design/Scenario/Genotype/*.gen` (read-only).
- `Data/Edition/Genotype/*.gen`.
- `Data/Design/Genotype/simu_trans.dic` — translations, dominance matrices,
  rarities.
- `Data/Design/Genotype/anim_trans.dic` — 20 dinos / 5 families.
- `tools/formats/gen.py`, `gene_trans.py`, `tests/test_gen.py`,
  `tests/test_gene_trans.py`.
- Gene descriptions are the `//` comments in `allo.gen`/`anky.gen`.

## Open

- Phenotype mapping (gene value → mesh part/skin/carapace).
- Head/ArmLeg/Body/Tail expression rule (no matrix); the head-type gene now
  selects the base species mesh, but hybrid part assembly is not ported.
- Breeding/selection rules (rarity → random allele choice, mutations).
- The 9 extra edition genes.
- The dummy range discrepancy (0..39 documented vs 0..47 observed).
