# `.bni` animal profile

Status: **solved** for the header, asset list and food link; some numeric
fields are partial.

## Inventory

5 files in `Data/Design/animal/` (438–648 bytes), one per `AnimalList.ini`
entry (Ostrich, Sheep, Goat, Pig, Cow). 33 mesh paths, all existing.

## Header (48 bytes, 12 × 32-bit)

| Offset | Type | Meaning |
| ---: | --- | --- |
| 0 | u32 | variant (Cow 3, others 1) |
| 4 | u32 | 1 |
| 8 | f32 | 1.0 |
| 12 | f32 | size (Sheep 1.0 … Ostrich 4.0) |
| 16 | u32 | limit (50 in all) |
| 20 | u32 | food value (200–1,000) |
| 24 | f32 | quantity (0.25–0.5) |
| 28 | u32 | food kind id (matches `.bsi`) |
| 32 | f32 | 10.0 |
| 36 | u32 | 0 |
| 40 | u32 | 0 |
| 44 | u32 | 1 |

| File | Variant | Size | Food value | Quantity | Food kind |
| --- | ---: | ---: | ---: | ---: | ---: |
| OstrichAnimal | 1 | 4.0 | 200 | 0.25 | 5 |
| SheepAnimal | 1 | 1.0 | 400 | 0.5 | 7 |
| GoatAnimal | 1 | 2.1 | 250 | 0.3 | 6 |
| PigAnimal | 1 | 1.5 | 600 | 0.5 | 8 |
| CowAnimal | 3 | 2.5 | 1,000 | 0.5 | 9 |

Food kinds match `docs/formats/bsi.md` exactly. Cow's `food_value` is 1,000
while CowFood's nutrition is 800; the others match.

## Assets

The body interleaves numeric records with length-prefixed strings:

- 5–12 mesh paths: LOD1/LOD2, part1/part2, walk1/walk2 (variants differ).
- One texture directory: `DATA\graphics\TEXTURES\actors\animals`.

The number of meshes varies (Sheep 5, Cow 6, Pig 12 with repeated parts).

## Reader

`tools/formats/actors.py`:

```text
python tools/formats/actors.py --animal <file.bni>
python tools/formats/actors.py --scan-animals <animal-dir>
```

Tests: `tests/test_actors.py` (Cow values, food-kind cross-check, 5-file
scan, all mesh paths).

## Open

- Meaning of `variant` (Cow 3 vs others 1), `limit` = 50, `speed` = 10.0,
  `flag` = 1.
- Why Cow's food value differs from CowFood's nutrition.
- Numeric records interleaved with the mesh strings.
