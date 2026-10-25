# `.bvi` visitor profile

Status: **solved** for the header, personality ids and asset table; the five
preference weights and the tail bytes are partial.

## Inventory

24 files in `Data/Design/visitors/` (2.0–2.1 KB each), listed by
`VisitorList.ini`:

| Id | Name (file) | VisitorList label | Files |
| ---: | --- | --- | --- |
| 0 | Surfer | `SURFER` | Man/Woman |
| 1 | Hooligan | `HOOLIGAN` | Man/Woman |
| 2 | Parent | `PARENTS` | Man/Woman |
| 3 | Blase | `BLASES` | Man/Woman |
| 4 | Old | `RETRAITE` | Man/Woman |
| 5 | Riche | `RICHE` | Man/Woman |
| 6 | Scientist | `SCIENTIFICS` | Man/Woman |
| 7 | Fan | `AMOUREUX` | Man/Woman |
| 8 | Spd | `SPD` | Man/Woman |
| 9 | Kid | `ENFANTS` | Man/Woman/2/2/3/3 |

24 files, 624 mesh references, all paths exist.

## Header (96 bytes, 24 × 32-bit)

| Offset | Type | Meaning |
| ---: | --- | --- |
| 0 | u32 | personality id (see table) |
| 4 | u32 | 1 |
| 8 | u32 | 1 |
| 12 | f32 | height (1.7 m in all files) |
| 16 | f32 | walk speed (1.2–2.5) |
| 20 | u32 | 10 |
| 24 | u32 | 2 |
| 28..48 | 5 × f32 | preference weights (−1..1); semantics open |
| 48..95 | 12 × u32 | header tail (semantics open: 576, 2.0, 1.0×3, 500, 300, −3000, 3000, 0, 0, 2) |

### Personalities

| Id | Name | Walk speed | Preferences |
| ---: | --- | ---: | --- |
| 0 | Surfer | 2.5 | 0.0, 0.5, 1.0, −0.2, 0.0 |
| 1 | Hooligan | 2.2 | −0.5, 1.0, 0.6, 0.0, 0.0 |
| 2 | Parent | 2.0 | 0.5, −0.8, 0.6, 0.2, 0.3 |
| 3 | Blase | 1.5 | 0.0, 0.3, 0.5, −0.5, 0.2 |
| 4 | Old | 1.2 | 0.5, −1.0, 0.1, 0.3, 0.7 |
| 5 | Riche | 2.0 | 0.3, 0.3, 0.5, −0.2, 0.3 |
| 6 | Scientist | 1.8 | 0.0, 0.0, 0.5, 0.6, 0.5 |
| 7 | Fan | 2.0 | 0.7, 0.0, 0.4, −0.6, 0.4 |
| 8 | Spd | 2.0 | 0.4, −0.5, 0.4, 0.0, 0.4 |
| 9 | Kid | 2.5 | 0.6, 0.0, 0.5, 0.0, 0.4 |

Man and Woman variants have identical stats; only the mesh paths/texture
differ. Children (`Kid*2`, `Kid*3`) also share the Kid stats.

## Strings

After the header: 28 length-prefixed ASCII strings.

- 26 `.mesh` paths: `standing` LOD1/LOD2, `walk1` LOD1/LOD2, `walk2`
  LOD1/LOD2, `seated` LOD1 ×2, `run1/2/3` LOD1 ×2, `happy1/2/3` LOD1 ×2,
  `unhappy1/2/3` LOD1 ×2.
- `DATA\graphics\TEXTURES\actors\people\customers` (directory).
- A texture file (`fan.tga`, `hooli.tga`, …).

The repeats are deliberate: the actor has two slots per animation state
(e.g. `seated` twice), not duplicate data.

## Tail

After the last string, 91–113 bytes remain. It is mixed binary, not a float
array (the first bytes decode as a 4-byte code followed by floats). It likely
holds need rates/attraction preferences; not decoded.

## Reader

`tools/formats/bvi.py`:

```text
python tools/formats/bvi.py --dump <file.bvi>
python tools/formats/bvi.py --scan <visitors-dir>
```

Tests: `tests/test_bvi.py` (FanMan values, 24-file scan, 624 mesh paths).

## Evidence

- `Data/Design/visitors/*.bvi` (read-only).
- `Data/Design/visitors/VisitorList.ini` — 10 profile types, 24 names.
- `tools/formats/bvi.py`, `tests/test_bvi.py`.

## Open

- Meaning of the five preference weights (likely attraction categories).
  Visitor AI (`docs/decomp/visitor-ai.md`) uses six global preference weights
  (`0x522a10`, `DAT_00675638`); `.bvi` supplies five per-profile weights, so
  the index mapping is still open.
- Tail bytes (needs/rates).
- How personality ids select visitor spawn probabilities.
