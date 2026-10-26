# Laboratory / general-params recon (2026-10-26)

Read-only analysis of `Data/Design/laboratory/Laboratory.ini` and
`Data/Design/generalparams/GeneralParams.bgd`.

## Ten design databases

`FUN_0053d4d0` opens the ten design databases in this order (config struct
+0x80..+0xa4):

```
[0] walls            0x53d9a0
[1] actions          0x53e060
[2] features         0x53ed30
[3] buildings        0x53fb00
[4] landscape objs   0x5407f0
[5] visitors         0x540c70
[7] food sources     0x5420d0
[6] employees        0x541940
[8] animal           0x542920
[9] general params   0x542f80
```

## Laboratory.ini

Small INI, four block families. All values:

```
GENERAL   MaxUpgradeLevel 4, MaxBeamStrength 3
BEAM 1    6 genes, 100% survival, 10 damage
BEAM 2    2 genes, 100% survival, 20 damage
BEAM 3    2 genes, 100% survival, 40 damage
LEVEL 1   no reproduction, 0 modifications, 100% clone fail, beam 0, 4 slots
LEVEL 2   reproduction, 2 modifications, 40% clone fail, beam 1, 8 slots
LEVEL 3   reproduction, 4 modifications, 20% clone fail, beam 2, 8 slots
LEVEL 4   reproduction, 20 modifications, 0% clone fail, beam 3, 8 slots
COST      breed 2000, clone 4000, mutation 1000/2000/3000,
          rare/uncommon/common allele 1000/500/250
```

The four level blocks and three beams are contiguous 1-based indices; the
reader validates them against `MaxUpgradeLevel` / `MaxBeamStrength`.

## GeneralParams.bgd

170-byte packed record read by `0x542f80`:

```
0x00 f32 3000.0
0x04 f32 1800.0
0x08 f32 3.0
0x0c f32 1.5
0x10 string ""            (len 1 + NUL)
0x15 string "rocket"
0x20 string "trumpet"
0x2c string "startBlast"
0x3b string "whitle"      (read as a separate single string)
0x46 f32 8.0
0x4a f32 120.0
0x4e f32 40.0
0x52 u32 10
0x56 f32 60.0
0x5a f32[16]  3 3 3 3 / 3 2 2 2 / 3 2 2 2 / 3 2 2 2
0x9a f32[4]   6.0 10.5 14.0 20.0
```

Runtime offsets from the loader: floats to +0x110/+0x114/+0x118/+0x11c; the
four sounds to a 4-entry string vector; `whitle` to +0x144/+0x14c; then
+0x154 (8.0), +0x150 (120.0), +0x1a0 (40.0), +0x1a4 (10), +0x1f8 (60.0),
the 4x4 grid at +0x1a8..+0x1c4 and the four floats at +0x1e8..+0x1f4.

The strings are the celebration/firework sound set (`rocket`, `trumpet`,
`startBlast`, `whitle`). The numeric meanings are still hypotheses.

## Tooling

- `tools/formats/laboratory.py` — reuses `overlay.parse_ini`.
- `tools/formats/general_params.py` — packed reader.
- `tests/test_laboratory.py`, `tests/test_general_params.py`.
- `docs/formats/laboratory.md`, `docs/formats/general-params.md`.

## Open

- Numeric meanings in GeneralParams.bgd (times/rates/distances/ramp).
- Upgrade price source (not in Laboratory.ini).
- Whether `RadiationDamage` is a probability or a stat hit.
