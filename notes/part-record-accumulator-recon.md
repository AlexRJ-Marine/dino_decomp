# Phenotype part-record accumulator (Ghidra)

Status: **solved.** The 23-slot pass reads Part 6 only; the last five slots are
byte-identical duplicates of slots 5, 10, 11, 16 and 17. Skin and carapace are
separate Part 7/8 additions.

## `0x619a60` additive pass

```text
for s in 0..22:
    part = T[0xb8 + s*4]          // body-part index
    idx  = P[0xe8 + s*4]          // Part-6 row, 0 = absent
    if idx == 0: continue
    f    = P[0xac + part*4] / T[0xa4 + part*4]   // part width / reference
    row  = T[0x84] + idx*0x10    // int[18] at row+4
    for k in 0..6:  acc[k]   += row[k]   * f
    sel = T[0x114 + part*4]      // {8,9,10,18,11} for Head,Arm,Leg,Body,Tail
    if sel != 18:   acc[sel] += row[sel] * f
    for j in 0..5:  acc[12+j] += row[12+j] * f
```

`T[0x114..0x127]` is **five** ints, one per body part, not 23.
`T[0xb8]` is a hard-coded 23-entry part map (`FUN_0061a230`):

| Slots | Part | Source |
| --- | --- | --- |
| 0..5 | Head | head dummy row cols 0..5 |
| 6..9 | Body | body dummy row cols 0..3 |
| 10 | Arm | body dummy row col 4 |
| 11 | Leg | body dummy row col 5 |
| 12..17 | Tail | tail dummy row cols 0..5 |
| 18..22 | 0,1,2,4,4 | **copies** of 5, 10, 11, 16, 17 |

The copies are literal at `0x617ad1` (slots `+0xfc/+0x110/+0x114/+0x128/
+0x12c` to `+0x130/+0x134/+0x138/+0x13c/+0x140`), so those five rows are
counted twice with the same ratio. This is the first suspect for the +2/+3
Intelligence residuals.

## `0x619b80` percentage pass

Same 23-slot walk over Part 6 table 1 (`T[0x90]`, data `T[0x94]`), no width
ratio:

```text
for k in 0..6:  pct[k]   += row[k]   - 100
if sel != 18:   pct[sel] += row[sel] - 100
for j in 0..5:  pct[12+j] += row[12+j] - 100
```

Initial values: `acc[0..17] = 0`, `pct[0..17] = 100`.

## Application (`0x619d90` / `0x619c70` / `0x619fb0`)

```text
P[+0x18] = trunc(P[+0x18] * pct[0]/100 + 1e-4) + trunc(acc[0])
P[+0x2c] = P[+0x2c] * pct[2]/100 + acc[2]*0.01
P[+0x30] = P[+0x30] * pct[2]/100 + acc[2]*0.01
P[+0x38] = ... pct[3] / acc[3]
P[+0x3c] = ... pct[4] / acc[4]
P[+0x40] = ... pct[5] / acc[5]
P[+0x48] = ... pct[6] / acc[6]
P[+0x4c] = ... pct[7] / acc[7]           // pct[7] never fed by sel
P[+0x50] = ... pct[8] / acc[8]
P[+0x54] = ... pct[9] / acc[9]
P[+0x58] = ... pct[10] / acc[10]
P[+0x5c] = ... pct[11] / acc[11]
P[+0x68] = ... pct[12] / acc[12]
P[+0x6c] = ... pct[13] / acc[13]
P[+0x70] = ... pct[14] / acc[14]
P[+0x74] = ... pct[15] / acc[15]
P[+0x78] = ... pct[16] / acc[16]
P[+0x194] = trunc(P[+0x194] * 100 / pct[6] + 1e-4) - trunc(acc[17])
```

`pct[1]` and `pct[17]` are never read. `P+0x1c`, `+0x34` and `+0x64` are only
clamped. The final clamp is Part 9 min/max (`T[0x418]` / `T[0x474]`).

## Skin and carapace

Not part of the 23 slots. `0x617250` adds them directly:

- skin: index `P[0x174] = Part7_map[gene10 pair][gene11 pair]` (8×3);
  Part 7 table 0 (`T[0x44]`) × `P[0xa4]/T[0xa0]` into `acc`, table 1
  (`T[0x54]`) minus 100 into `pct`.
- carapace: same with genes 12/13, Part 8 map (`T[0x34]`, 6×3), tables
  `T[0x64]`/`T[0x74]`, index stored at `P[0x178]`.

The ratio is body length `P[0xa4] / T[0xa0]` (the Part 7 "582" value).

## Row selection

Part-6 row = `Part5[pair][type][ Part4[pair][pos][col] - 1 ]`:

- `pos` = genes 4/6/8 (head/body/tail dummy position);
- `type` = genes 5/7/9 (dummy type);
- pairs `(4,5)`, `(6,7)`, `(8,9)` from `0x6488c8 = [4,6,8]`,
  `0x6488d4 = [5,7,9]`, translated through the 18 gene tables `T+0x1b8`.

## Confidence

- Certain: 23 iterations, null test, 0x10 record stride, 7/1/6 split, five
  selectors `{8,9,10,18,11}`, 23-entry part map, five literal slot copies,
  `value-100`, constants, field map, `+0x194` asymmetry, Part 9 clamp.
- High: Part 6 = the two 50×18 tables, skin = Part 7, carapace = Part 8.
- Medium: body-part order (from `0x619830`/Part 3) and whether the duplicate
  slots are intentional. The numeric attribution of the +2/+3 rows needs the
  full part-width chain.

## Evidence

- `0x619a60`, `0x619b80`, `0x619c70`, `0x619d90`, `0x619fb0`, `0x61a230`.
- `0x617ad1` (slot copies), `0x617b66`/`0x617c59` (skin/carapace).
- Constants `0x626a78` (1e-4), `0x626a7c` (0.01), `0x6256b8` (100.0),
  `0x6488c8`/`0x6488d4` (gene pairs), `0x625484` (0.01).
- `docs/formats/translator-constants.md`, `notes/translator-recon.md`,
  `tools/formats/simu_trans.py`, `tools/formats/gene_trans.py`.
