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
paddocks in record order (first fit). The cost class comes from the species
rest-mesh size (<300 cm = 0, 300–899 = 1, 900–1399 = 2, ≥1400 = 3):

| Paddock | Rectangle | Capacity | Used | Fill | Dinosaurs |
| ---: | --- | ---: | ---: | ---: | --- |
| 0 | (452,312)-(467,343) | 32 | 32 | 1.000 | 3 |
| 1 | (460,356)-(491,403) | 96 | 96 | 1.000 | 3 |
| 2 | (572,380)-(595,403) | 36 | 32 | 0.889 | 1 |
| 3 | (512,444)-(543,459) | 32 | 32 | 1.000 | 3 |
| 4 | (376,372)-(407,403) | 64 | 64 | 1.000 | 2 |
| 5 | (512,372)-(543,403) | 64 | 64 | 1.000 | 3 |
| 6 | (420,504)-(443,551) | 72 | 64 | 0.889 | 5 |
| 7 | (456,464)-(479,487) | 36 | 0 | 0 | 0 |
| 8 | (608,364)-(631,403) | 60 | 0 | 0 | 0 |
| **Total** | | **492** | **384** | **0.780** | **20** |

The viewer prints `paddocks: 9 capacity=492 used=384`.

## Retail classes vs the port approximation

A live Cpg04 run logged `[dinosaur+0x1e0]` at `0x570fd0` for the 20 world
dinosaurs:

```text
retail: 1,2,2,2,0,0,1,0,1,1,2,0,1,1,1,1,1,1,0,0   -> used 336
port:   1,2,2,2,0,0,2,0,2,2,2,0,1,1,1,1,1,1,0,0   -> used 384
```

The three differences are hybrid dinosaurs whose head type resolves to a
large species but whose body/leg genes make them smaller. The class is a
function of the **individual phenotype length**, not the head species; the
port uses the species rest-mesh bbox until the genotype translator is
ported. Cpg04 still fits (384/492), and the port's per-paddock fills never
exceed capacity.

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

Only Cpg04 is constrained. The port's 384/492 (species-bbox classes) is well
within capacity; the retail 336/492 uses the individual phenotype classes.
The exact per-individual class is open until the genotype translator is
ported.

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

## Update 2026-10-26: exact phenotype class

The species-bbox approximation above is superseded. The class is
`length = body_length * (head_ratio + 1 + tail_ratio)` (proportion.dat
blended by the Part 2 merge + Part 3 Body length); the port implements it in
`DinoPhenotypeTable` and Cpg04 now reports `used = 336`, matching retail.
See `notes/paddock-size-class-runtime.md`. This note is kept as the draft
state that led to the exact model.
