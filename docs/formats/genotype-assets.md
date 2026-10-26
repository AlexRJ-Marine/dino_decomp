# Genotype graphic tables: skins, carapaces, adornments

Status: **solved structurally.** Both tables parse exactly and every
referenced texture/model exists. The indices line up with the
`simu_trans.dic` Part 7/8 maps.

## `SkinCarapaceData.ini`

`Data/Design/Genotype/SkinCarapaceData.ini` — plain text, `//` comments.

```text
DATA\graphics\TEXTURES\actors\elements_sup\bonus\peau\
22
void
bonus01.tga
...
DATA\graphics\TEXTURES\actors\elements_sup\bonus\armure\
16
void
hurt.tga
blood.tga
armor_01_s.tga
armor_01_s_hurt.tga
armor_01_s_blood.tga
...
```

- **Skins**: one access path, then a count (22) and 22 texture names.
  Index 0 is `void` (no bonus skin); 1..21 are `bonusNN.tga`.
- **Carapaces**: one access path, then a count (16) and **three** textures
  per carapace — `nomina` (base), `hurt`, `blood`. Index 0 is
  `void, hurt.tga, blood.tga`; 1..15 are `armor_01_s` (small/medium/large),
  `armor_02_*`, `armor_03_*`, `armor_04_*`, then `frank24`, `momie24`,
  `darkmaul`.

The skin list has exactly the 22 unique indices produced by the Part 7 map
(`8 types × 3 subtypes`, type 0 = `0,0,0`). The carapace list has the 16
unique indices of the Part 8 map (`6 × 3`, type 0 = `0,0,0`). So:

```text
skin texture      = skins[ skin_map[skin_type][skin_subtype] ]
carapace textures = carapaces[ carapace_map[carapace_type][carapace_subtype] ]
```

All non-`void` textures exist under the two access paths.

## `AdornmentData.ini`

`Data/Design/Genotype/AdornmentData.ini` — plain text, `//` comments.

```text
49                      number of head/body/tail adornment LOD pairs
horn1\horn_l_01_lod1.mesh
horn1\horn_l_01_lod2.mesh
...                     (49 pairs = 98 lines)
4                       number of claw types
4                       sizes per claw type
claw1\claw_xl_01.mesh
claw1\claw_l_01.mesh
claw1\claw_m_01.mesh
claw1\claw_s_01.mesh
...                     (4 types x 4 sizes = 16 lines)
```

- 49 LOD pairs cover horns (`horn1..horn3`), dorsal plates, feathers
  (`feath1/2`, `doubfeath1/2`), `ankyball`, the two collars and the
  parasaurolophus horn. Three entries are explicitly "no LOD needed": they
  repeat the same `_lod1` path twice (`feath2`, `doubfeath2`).
- 4 claw types × 4 sizes (`xl/l/m/s`) = 16 claw models.

All 114 model paths are relative to
`Data/Graphics/Actors/elements_sup/` and exist.

These are the models the `.gen` dummy genes 4–9 select: gene 4/5 = head
dummy position/type, 6/7 = body, 8/9 = tail; `simu_trans.dic` Part 4/5 turn a
dummy position into a type id (0..47) and Part 6 supplies the influence
values. The Part 4/5 comments call these "dummies", and the model list here is
the "adornment" (dummy) graphic table.

## Reader

`tools/formats/genotype_assets.py`:

```text
python tools/formats/genotype_assets.py SkinCarapaceData.ini
python tools/formats/genotype_assets.py AdornmentData.ini
```

`read_skin_carapace` returns `skin_dir`, `skins[22]`, `carapace_dir`,
`carapaces[16][3]`. `read_adornment` returns `pairs[49]`, `claw_types`,
`claw_sizes`, `claws[4][4]` and the flat `meshes` list. Both validate the
counts and reject trailing tokens. Tests: `tests/test_genotype_assets.py`
(counts, spot entries, every texture/model exists, Part 7/8 indices in range).

## Evidence

- `Data/Design/Genotype/SkinCarapaceData.ini`, `AdornmentData.ini`
  (read-only).
- `Data/Graphics/Actors/elements_sup/` — all 114 models exist.
- `Data/Graphics/Textures/actors/elements_sup/bonus/{peau,armure}/` — all
  textures exist.
- `docs/formats/simu-trans.md` Part 7/8 maps.
- `tools/formats/genotype_assets.py`, `tests/test_genotype_assets.py`.

## Open

- Which translator table (`+0x84` / `+0x94`) receives the two lists; the
  file contents and index ranges fit the part-record cache, but the loader
  call has not been pinpointed.
- Whether the claw table is indexed by the dummy type or by a separate gene.
- The exact usage of the LOD pair (LOD1/LOD2 by camera distance).
