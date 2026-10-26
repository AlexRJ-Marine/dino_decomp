# Genotype graphic tables recon (2026-10-26)

Read-only analysis of `Data/Design/Genotype/SkinCarapaceData.ini` and
`AdornmentData.ini`.

## SkinCarapaceData.ini

Plain text, `//` comments. Token stream:

```
DATA\graphics\TEXTURES\actors\elements_sup\bonus\peau\
22
void
bonus01.tga
bonus10.tga
...
DATA\graphics\TEXTURES\actors\elements_sup\bonus\armure\
16
void hurt.tga blood.tga
armor_01_s.tga armor_01_s_hurt.tga armor_01_s_blood.tga
...
```

- Skins: 22 names, index 0 = `void`, 21 `bonusNN.tga` files.
- Carapaces: 16 rows x 3 textures (`nomina`, `hurt`, `blood`); row 0 is
  `void`, `hurt.tga`, `blood.tga`.
- All non-void textures exist under the two access paths.

The 22 skin indices are exactly the unique values of the `simu_trans.dic`
Part 7 map (8 skin types x 3 subtypes, type 0 = 0,0,0). The 16 carapace
indices are exactly the unique values of the Part 8 map (6 x 3, type 0 =
0,0,0). That ties the translated genotype to concrete textures.

## AdornmentData.ini

Plain text, `//` comments. Token stream:

```
49
horn1\horn_l_01_lod1.mesh
horn1\horn_l_01_lod2.mesh
... (49 LOD pairs)
4
4
claw1\claw_xl_01.mesh
claw1\claw_l_01.mesh
claw1\claw_m_01.mesh
claw1\claw_s_01.mesh
... (4 types x 4 sizes)
```

- 49 LOD pairs: horns (3), dorsal plates, feathers (feath1/2,
  doubfeath1/2), ankyball, protoceratops/triceratops collars,
  parasaurolophus horn.
- Three entries repeat the `_lod1` path twice (the file says "No LOD needed
  for these 3 types").
- 4 claw types x 4 sizes = 16 models.
- All 114 model paths exist under `Data/Graphics/Actors/elements_sup/`.

This is the graphic table for the `.gen` dummy genes 4-9 (head/body/tail
dummy position/type), which `simu_trans.dic` Part 4/5/6 translate into dummy
type ids and influence values.

## Translator link

The translator config (`FUN_0051d1d0`) stores the paths:

```
+0x50 AdornmentData.ini
+0x54 DATA\Graphics\Actors\elements_sup\
+0x58 SkinCarapaceData.ini
```

The phenotype part-record cache at translator `+0x84` (0x10-byte entries) and
`+0x94` fits these two lists (49/16 entries, index by the Part 7/8 maps and
the dummy type ids), but the exact loader call was not pinpointed.

## Tooling

- `tools/formats/genotype_assets.py` — both readers.
- `tests/test_genotype_assets.py` — counts, spot entries, asset existence,
  Part 7/8 index ranges.
- `docs/formats/genotype-assets.md` — spec.

## Open

- Translator offsets for the two tables.
- Claw indexing (dummy type vs separate gene).
- LOD selection rule.
