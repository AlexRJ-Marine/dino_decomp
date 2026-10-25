# Animal/employee profile recon (`.bni` / `.bei`) — 2026-10-25

Both are actor profiles: a fixed header, then a body interleaving numeric
records with length-prefixed ASCII asset/sound strings.

## `.bni` animals

5 files in `Data/Design/animal/`: Ostrich, Sheep, Goat, Pig, Cow.
33 mesh paths, all existing.

48-byte header (12 x u32):

```text
0 u32 variant (Cow 3, others 1)
1 u32 1
2 f32 1.0
3 f32 size (Sheep 1.0, Pig 1.5, Goat 2.1, Cow 2.5, Ostrich 4.0)
4 u32 limit 50
5 u32 food value (200..1000)
6 f32 quantity (0.25..0.5)
7 u32 food kind id = .bsi kind
8 f32 10.0
9 u32 0
10 u32 0
11 u32 1
```

| File | Variant | Size | Food value | Quantity | Food kind |
| --- | ---: | ---: | ---: | ---: | ---: |
| Ostrich | 1 | 4.0 | 200 | 0.25 | 5 |
| Sheep | 1 | 1.0 | 400 | 0.5 | 7 |
| Goat | 1 | 2.1 | 250 | 0.3 | 6 |
| Pig | 1 | 1.5 | 600 | 0.5 | 8 |
| Cow | 3 | 2.5 | 1000 | 0.5 | 9 |

Food kinds match `FoodSourceList`/.bsi exactly. Cow's food value 1000 vs
CowFood nutrition 800; others match.

Body assets: 5–12 `.mesh` paths (LOD1/2, parts, walk1/2) + texture directory.

## `.bei` employees

2 files in `Data/Design/employees/`: Nutritionist (kind 0), Guard (kind 1).
9 mesh paths, all existing; sounds `tweet`, `singing`.

60-byte header (15 x u32):

```text
0 u32 kind (Nutritionist 0, Guard 1)
1 u32 1
2 u32 1
3 f32 height 1.7
4 f32 walk speed (Guard 1.0, Nutritionist 2.0)
5 f32 action speed 2.5
6 u32 10
7 u32 2
8 u32 500
9..11 f32 50, 3, 100
12..14 u32 0, 0, 1
```

Body assets: Guard 4 meshes (`secu_*`), Nutritionist 5 (`feed_*`), texture
dir + texture (`gardien.tga`, `feed.tga`), sounds.

## Evidence

- `Data/Design/animal/*.bni`, `AnimalList.ini`.
- `Data/Design/employees/*.bei`, `EmployeeList.ini`.
- `tools/formats/actors.py`, `tests/test_actors.py`.
- `docs/formats/bni.md`, `docs/formats/bei.md`.
