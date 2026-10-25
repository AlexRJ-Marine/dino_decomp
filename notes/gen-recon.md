# Genotype recon (`.gen`) — 2026-10-25

## Files

- `Data/Design/Scenario/Genotype/` — 57 files, **all 18 genes**; 20 carry the
  French documentation header, 37 are bare allele pairs (`allo2.gen`, …).
- `Data/Edition/Genotype/` — 20 files, **all 27 genes**, bare (editor data).

## Format

Text, Windows-1252. First token = gene count; then two ints per gene.
`//` comments are documentation only.

`allo.gen` (design) header comments:

```text
--00-- ALLOSAURUS
Hauteur (m) 5; Longueur (m) 12; Poids (Kg) 2000
Marche B; Alimentation C
Vitesse Marche (km/h) 8; Vitesse Course (km/h) 40
Intelligence 1 .. 1.9
```

18 genes (allele A, allele B):

```text
 0 head type            5 head dummy type     10 skin type
 1 legs type            6 body dummy pos      11 skin subtype
 2 body type            7 body dummy type     12 carapace type
 3 tail type            8 tail dummy pos      13 carapace subtype
 4 head dummy pos       9 tail dummy type     14 diet (0 H / 1 C)
                                             15 aggressiveness 0..9
                                             16 stamina 0..9
                                             17 courage 0..9
```

`allo.gen` values: 0 for all phenotype genes, diet (1,1), aggressiveness
(8,7), stamina (2,4), courage (5,6).

## Observed ranges (57 design files)

```text
head/legs/body/tail type          0..19
dummy positions/types             0..47   (comments say 0..39)
skin type/subtype                 0..7 / 0..2
carapace type/subtype             0..5 / 0..2
diet                              0..1
aggressiveness/stamina/courage    0..9
```

## Edition set

20 files, 27 genes each; all bare. The design genes are not at the same
indices (edition `allo.gen` has `(8,7)` at index 18, not 15). Extra 9 genes
undecoded; editor-only.

## Expression (`simu_trans.dic`)

18 blocks, each: translation count/values, optional N×N dominance matrix,
rarity count/weights. Allele counts:

```text
Head/ArmLeg/Body/Tail 20 (no matrix)
HDDi/HDTy/BDDi/BDTy/TDDi/TDTy 48
SknTyp 8; SknSub 3; CarTyp 6; CarSub 3
Diet 2; Agre/Stam/Cour 10
```

Rules:

- `HDDi`, `HDTy`, `BDDi`, `BDTy`, `TDDi`, `TDTy`, `SknTyp`, `CarTyp`,
  `Diet`: `translation[min(a,b)]` (lower allele dominant).
- `SknSub`, `CarSub`, `Agre`, `Stam`, `Cour`:
  `translation[(a+b)//2]` (averaged).
- `Head`/`ArmLeg`/`Body`/`Tail`: no matrix; rule unproven (likely min).

`allo.gen`: diet (1,1) → 2; aggression (8,7) → 80; stamina (2,4) → 40;
courage (5,6) → 70.

Rarity weights are 1–3 (lower = rarer). `anim_trans.dic` lists 20 dinos and
5 families; `graphic_trans.dic` reference widths `100,100,100,300,150`.

## Evidence

- `tools/formats/gen.py` (reader/scan), `tests/test_gen.py` (57+20 files).
- `tools/formats/gene_trans.py`, `tests/test_gene_trans.py` (18 blocks,
  dominance/average rules, rarities).
- `docs/formats/gen.md` — spec and gene table.
- Raw files: `game/%MAINDIR%/Data/Design/Scenario/Genotype/*.gen`.

