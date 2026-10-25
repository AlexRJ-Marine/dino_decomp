# `anim_trans.dic` / `proportion.dat` recon (2026-10-25)

Read-only parse of the genotype dictionaries. No writes to `game/` or
`original/`.

## Files

```text
anim_trans.dic   5727 bytes  20 dinos / 5 families / 51 clips / 4 x 20x20
graphic_trans.dic  89 bytes  5 reference widths 100 100 100 300 150
proportion.dat    404 bytes  u32 20 + 20 x 5 floats
```

Both are cp1252 text. Sections are introduced by `//` comments. The first
three sections have an integer count followed by that many names; the four
tables have no count, just 20 rows of 20 integers.

## Validation

- 20 dinosaurs, alphabetical: `allo anky brachio coelo deino diplo gigano
  hypsilo igua kentro pachy parasau peleca proto salta stego trex trice trood
  velo`.
- 5 families: HighQuadruped, LightQuadruped, HeavyBiped, LightBiped,
  LowQuadruped.
- 51 animations, `Neutral` .. `Slip`. Every one exists as
  `Data/Graphics/Actors/Dinos/animations/<family>/<family>_<name>.anim` for
  all five families (51 each, 255 files).
- Four tables: Head, ArmLeg, Body, Tail. All 20×20, diagonal 0.
- Head/Body/Tail: only 0 and 50.
- ArmLeg: 0, 1, 50, 99.

## ArmLeg structure

```text
A: allo coelo deino gigano hypsilo pachy peleca trex trood velo
B: anky brachio diplo igua kentro parasau proto salta stego trice
```

- A→B = 1 (except trex→stego = 99)
- B→A = 99
- A→A = 50
- B→B = 50 (except anky→trice = 1)

The matrix is asymmetric in 200 of 380 off-diagonal cells. This looks like a
directional merge/retarget verdict, but the loader and consumer have not been
decompiled. The only xrefs to the file paths are the park-application setup
(`FUN_0051d1d0` at `0x51d436` and `0x51d50a`), which stores the strings at
`park+0x34` and `park+0x48`; the actual text load is indirect.

`TranslatorConstants.ini` classifies dinos by size:

```text
MaxQuadrupedRange    150
MinHeightForHeavyBiped 400
MinWeightForHeavyBiped 1500
SizeCategory1MaxLength 200
SizeCategory2MaxLength 620
SizeCategory3MaxLength 900
SizeCategory4MaxLength 3200
```

That is probably how a dino gets one of the five animation families and how
the hybrid lab decides which parts can be merged.

## proportion.dat

`u32 20` then 20 × 5 little-endian floats, same species order as
`anim_trans.dic`. Column 3 is exactly 1.0 in every record (leg reference).

The other columns match visible part sizes: T-rex arm 0.235, Diplodocus tail
2.093, Brachiosaurus head 1.1375, Triceratops head 0.6573. Read as
`[head, arm, body, leg, tail]`; the order is inferred from the data until the
loader is decompiled.

## Evidence

- `Data/Design/Genotype/anim_trans.dic`, `graphic_trans.dic`,
  `proportion.dat`, `TranslatorConstants.ini`.
- `Data/Graphics/Actors/Dinos/animations/*`.
- `tools/formats/anim_trans.py`, `tests/test_anim_trans.py`,
  `docs/formats/anim-trans.md`.

## Open

- Code that reads the four matrices and what 1/50/99 mean.
- Whether A/B are size classes, animation classes, or merge direction.
- `graphic_trans.dic` index order.
