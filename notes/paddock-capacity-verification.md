# Paddock capacity verification (P4a)

Checks the ported `stFence` capacity model against the retail campaign data.
Rule (`0x57dc60` / `0x57eb50`, see
`docs/decomp/construction-validation.md`):

```text
capacity = ((x1-x0+1) / cell) * ((y1-y0+1) / cell)   // float, cell = 4
cost     = 8 << index       # 8 / 16 / 32 / 64
can_add  = not already inside && capacity - used >= cost
add      = append, used += cost, fill = used / capacity
```

## Cpg04 (isl04) — the only campaign world with dinosaurs

`Cpg04_Save.wld` has 9 fence rectangles, 20 `drsDinosaur` records and
41,397 bytes. The save stores no dinosaur position, so the port fills
paddocks in record order (first fit) with cost index 0 (8 each):

| Paddock | Rectangle | Capacity | Used | Fill | Dinosaurs |
| ---: | --- | ---: | ---: | ---: | --- |
| 0 | (452,312)-(467,343) | 32 | 32 | 1.000 | 4 |
| 1 | (460,356)-(491,403) | 96 | 96 | 1.000 | 12 |
| 2 | (572,380)-(595,403) | 36 | 32 | 0.889 | 4 |
| 3 | (512,444)-(543,459) | 32 | 0 | 0 | 0 |
| 4 | (376,372)-(407,403) | 64 | 0 | 0 | 0 |
| 5 | (512,372)-(543,403) | 64 | 0 | 0 | 0 |
| 6 | (420,504)-(443,551) | 72 | 0 | 0 | 0 |
| 7 | (456,464)-(479,487) | 36 | 0 | 0 | 0 |
| 8 | (608,364)-(631,403) | 60 | 0 | 0 | 0 |
| **Total** | | **492** | **160** | **0.325** | **20** |

The viewer prints `paddocks: 9 capacity=492 used=160` after
`dinos: 20/20 spawned ...`.

## Other campaigns

| World | Version | Fences | Capacity | Dinosaurs |
| --- | ---: | ---: | ---: | ---: |
| `Cpg01_Save.wld` | 51 | 0 | 0 | 0 |
| `Cpg02_Save.wld` | 51 | 2 | 112 | 0 |
| `Cpg03_Save.wld` | 50 | 1 | 36 | 0 |
| `Cpg04_Save.wld` | 53 | 9 | 492 | 20 |
| `Cpg05_Save.wld` | 52 | 2 | 72 | 0 |
| `Cpg06_Save.wld` | 51 | 0 | 0 | 0 |
| `Cpg07_Save.wld` | 2 | — | — | legacy, not parsed |
| `Cpg08_Save.wld` | 51 | 0 | 0 | 0 |

Only Cpg04 is constrained, and 160/492 is well within capacity, so the
retail data does not contradict cost index 0. The cost classes for larger
dinosaurs remain unconfirmed (`dinosaur+0x1e0` is open).

## No stored paddock link

The `drsDinosaur` fields `+0x24/+0x28/+0x2c` are sequential registry ids
(`63824/63826/63825` etc.), not fence object ids (`3, 63529, 63575, ...`), so
the world block does not say which paddock a dinosaur was in. The port's
record-order first fit is a documented convention, not recovered data.

## Evidence

- `tests/test_viewer_cpp.py::test_cpp_wld_park_dinosaurs` asserts the
  summary line; `test_cpp_selftest` covers the capacity/cost/duplicate/fill
  maths (29 checks).
- `src/world/build.cpp` (`paddock_capacity`, `paddock_cost`,
  `paddock_can_add`, `paddock_add`) and `src/viewer/main.cpp` (import fill).
- `docs/platform/build.md`, `docs/decomp/construction-validation.md`.
