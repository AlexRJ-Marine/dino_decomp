# `.bei` employee profile

Status: **solved** for the header, assets and sounds; some fields partial.

## Inventory

2 files in `Data/Design/employees/` (521 and 580 bytes), listed by
`EmployeeList.ini`:

| Kind | File | Walk speed | Action speed |
| ---: | --- | ---: | ---: |
| 0 | Nutritionist | 2.0 | 2.5 |
| 1 | Guard | 1.0 | 2.5 |

Both are 1.7 m and use the same sound set (`tweet`, `singing`).

## Header (60 bytes, 15 × 32-bit)

| Offset | Type | Meaning |
| ---: | --- | --- |
| 0 | u32 | employee kind (Nutritionist 0, Guard 1) |
| 4 | u32 | 1 |
| 8 | u32 | 1 |
| 12 | f32 | height 1.7 |
| 16 | f32 | walk speed (1.0 / 2.0) |
| 20 | f32 | action/run speed 2.5 |
| 24 | u32 | 10 |
| 28 | u32 | 2 |
| 32 | u32 | 500 |
| 36 | f32 | 50 |
| 40 | f32 | 3 |
| 44 | f32 | 100 |
| 48..59 | u32 | 0, 0, 1 |

## Assets

The body interleaves numeric records with length-prefixed strings:

- Guard: `secu_standing_lod1`, `secu_walk1_lod1`, `secu_walk2_lod1`,
  `secu_action1_lod1` meshes.
- Nutritionist: `feed_standing_lod1`, `feed_walk1_lod1`, `feed_walk2_lod1`,
  `feed_walk3_lod1`, `feed_action_lod1` meshes.
- Texture directory + texture (`gardien.tga`, `feed.tga`).
- Sounds: `tweet`, `singing`.

All 9 mesh paths exist.

## Reader

`tools/formats/actors.py`:

```text
python tools/formats/actors.py --employee <file.bei>
python tools/formats/actors.py --scan-employees <employees-dir>
```

Tests: `tests/test_actors.py` (Guard/Nutritionist values, 2-file scan, assets
exist).

## Open

- Meaning of the header fields at 24–59 (10, 2, 500, 50, 3, 100, 0, 0, 1).
- How the two sound names are triggered.
- Behaviour/needs for guards and nutritionists (AI side).
