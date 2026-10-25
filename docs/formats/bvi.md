# `.bvi` visitor profile

Status: **solved** for the header, personality ids, asset table and the
constant attraction-scoring block; the consumer of the five variable weights
and the tail bytes are partial.

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
| 28..48 | 5 × f32 | preference weights (−1..1); not the scorer vector (see below) |
| 48 | u32 | 576 |
| 52 | f32 | unknown per-profile scale (1.0–2.5; not walk speed) |
| 56..84 | 7 × f32 | **constant tuning block**: 1, 1, 1, 500, 300, −3000, 3000 |
| 84..95 | 3 × u32 | 0, 0, 2 |

### Header fields 0x38..0x50

These seven floats are **identical in all 24 files**:

```text
0x38  1.0        W0  appeal-array term scale
0x3c  1.0        W1  attr10 term scale
0x40  1.0        W2  attr11 term scale
0x44  500.0      W3  price/ticket penalty
0x48  300.0      W4  per-tile distance penalty
0x4c  -3000.0    W5  random-base lower bound
0x50  3000.0     W6  random-base upper bound (code uses W6 − W5 = 6000)
```

At runtime `FUN_0051f560` sets `DAT_00675638` = profile record `+0xD0`, which
is this block; `FUN_00522a10` reads W0..W5 and uses W6 − W5 for the random
range. Dumped live under x32dbg as
`3F800000 3F800000 3F800000 43FA0000 43960000 C53B8000` (plus `453B8000`).
See `docs/decomp/visitor-ai.md` for the corrected score formula.

### In-memory record

`FUN_00544110` copies each parsed profile into a `0xEC`-byte record. The
five variable weights land at record `+0x24..+0x34` (not `+0x1C..+0x2C`);
the constant block lands at `+0xD0..+0xE8`. Runtime check with SurferMan:
record `+0x24..0x34` = `0.0, 0.5, 1.0, −0.2, 0.0`, exactly file `0x1C..0x2C`.

The consumer of the five variable weights is still open. One strong lead:
`FUN_0058abf0` aggregates visitor needs by `visitor+0x68` (the attraction
category index) using `+0x48`/`+0x4c`/`+0x50`, and the scorer reads the
attraction's per-category array at `+0x18` with that index. The five profile
weights are therefore likely the per-category weights that choose or bias
`+0x68`, but a hardware read breakpoint on record `+0x24` produced no hits in
an empty free-game park or a loaded two-visitor save (15 s window), so the
read may happen only at visitor creation/deserialization, or the values may
be editor-only. The scorer does **not** read them.

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

After the last string, 91–113 bytes remain. The prefix is structured:

```text
u32  5
f32  200, 400, 600, 800, 1000      five thresholds
f32  0.8, 9.6, 1.9, 22.8, 4.0, 48.0   three (rate, 12×rate) pairs
u32  14
...  ~57 bytes of mixed binary (not float-aligned; purpose open)
```

The 200..1000 thresholds and the rate pairs are identical in the samples
checked (Blase, Fan); they look like need/patience bands rather than
per-profile data. The trailing mixed bytes are still undecoded.

## Reader

`tools/formats/bvi.py`:

```text
python tools/formats/bvi.py --dump <file.bvi>
python tools/formats/bvi.py --scan <visitors-dir>
```

Tests: `tests/test_bvi.py` (FanMan/Surfer values, 24-file scan, 624 mesh
paths, and the constant scoring block).

## Evidence

- `Data/Design/visitors/*.bvi` (read-only).
- `Data/Design/visitors/VisitorList.ini` — 10 profile types, 24 names.
- `FUN_00544110` — profile → 0xEC list record (weights at `+0x24`, constants
  at `+0xD0`).
- `FUN_0051f560` — publishes the constant block as `DAT_00675638`.
- `FUN_0058abf0` — aggregates visitor needs by `visitor+0x68` category.
- `notes/visitor-weights-runtime.md` — live x32dbg dump.
- `tools/formats/bvi.py`, `tests/test_bvi.py`.

## Open

- Consumer of the five variable weights at file `0x1C..0x2C` (record
  `+0x24..+0x34`). They are **not** the attraction-scoring vector; the
  decompiled scorer uses the constant block instead. Likely need/attraction
  category weights read at visitor creation.
- Meaning of the per-profile scale at `0x34` (1.0–2.5) and the `576` at
  `0x30`.
- Tail prefix meaning (thresholds/rates) and the trailing mixed bytes.
- How personality ids select visitor spawn probabilities.
