# Paddock size class: phenotype length and runtime values

Status: **chain solved and runtime-confirmed.** The only open point is the
live hybrid genotype (see "Divergence from the `.wld`" below).

## Chain

`stFence::CanAddActor` (`0x57eb50`) indexes the cost table
`fence+0x84[4]` with `saDinosaur+0x1e0` (`0x57ebc3`). The value is a 0..3
size class:

```text
saDinosaur+0x1e0  <- phenotype block +0x190
phenotype block   = 0x1b8-byte translator output, copied to saDinosaur+0x50
                    by FUN_0056FED0 (`param_1 + 0x14` is +0x50 bytes)
phenotype+0x190   <- FUN_00617250 (size-category comparison)
```

`FUN_00617250` (disassembly `0x6174bb..0x6174ff`) computes:

```text
length = P+0x04 + P+0x08
P+0x00 = length
if (length <= translator+0x244) class = 0
else if (length <= translator+0x248) class = 1
else if (length <= translator+0x24c) class = 2
else class = 3
```

and `P+0x04`/`P+0x08` are the head/body/tail part lengths:

```text
if (P+0x1b4 == 0) {           // non-biped (families 2/3 are biped)
    P+0x04 = P+0x98 + P+0xa4;        // head + body
    P+0x08 = P+0xa8;                 // tail
} else {
    P+0x04 = P+0x98 + P+0xa4 / 2;    // biped: half the body twice
    P+0x08 = P+0xa8 + P+0xa4 / 2;
}
length = P+0x04 + P+0x08
```

So the metric is **head + body + tail** (bipeds count `body/2` twice, which
differs only by the integer rounding of an odd body length).

The three lengths are written by `FUN_00619830` as part-3 length lookups:

```text
part = 0 Head -> gene 0
part = 1 Arm  -> gene 1
part = 2 Leg  -> gene 1
part = 3 Body -> gene 2
part = 4 Tail -> gene 3
length[part] = Part3Length[part][alleleB][alleleA]
```

The five lengths land at `P+0x98, +0x9c, +0xa0, +0xa4, +0xa8`; therefore the
class uses **Head (`+0x98`), Body (`+0xa4`) and Tail (`+0xa8`)**.

Thresholds (`Data/Design/TranslatorConstants.ini`, stored at translator
`+0x244..+0x250`):

| key | value |
| --- | ---: |
| `SizeCategory1MaxLength` | 200 |
| `SizeCategory2MaxLength` | 620 |
| `SizeCategory3MaxLength` | 900 |
| `SizeCategory4MaxLength` | 3200 |

## Runtime values

`scripts/x32dbg/start-campaign.ps1` breaks at `0x570fd0` (saDinosaur need
reset) on a Cpg04 load and logs `[ecx+0x1e0]`, `[ecx+0x50]` (length),
`[ecx+0xe8]` (`P+0x98` head), `[ecx+0xf4]` (`P+0xa4` body), `[ecx+0xf8]`
(`P+0xa8` tail) and `[ecx+0x204]` (`P+0x1b4`). The 20 retail dinosaurs:

| # | class | length | head | body | tail | +0x1b4 |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 1 | 606 | 167 | 226 | 213 | 1 |
| 2 | 2 | 742 | 224 | 200 | 318 | 0 |
| 3 | 2 | 742 | 224 | 200 | 318 | 0 |
| 4 | 2 | 742 | 224 | 200 | 318 | 0 |
| 5 | 0 | 170 | 48 | 53 | 69 | 0 |
| 6 | 0 | 170 | 48 | 53 | 69 | 0 |
| 7 | 1 | 492 | 158 | 140 | 194 | 0 |
| 8 | 0 | 172 | 51 | 53 | 68 | 0 |
| 9 | 1 | 435 | 127 | 126 | 182 | 0 |
| 10 | 1 | 409 | 127 | 126 | 156 | 0 |
| 11 | 2 | 702 | 225 | 200 | 277 | 0 |
| 12 | 0 | 175 | 54 | 53 | 68 | 0 |
| 13 | 1 | 266 | 91 | 80 | 95 | 0 |
| 14 | 1 | 266 | 91 | 80 | 95 | 0 |
| 15 | 1 | 266 | 91 | 80 | 95 | 0 |
| 16 | 1 | 266 | 91 | 80 | 95 | 0 |
| 17 | 1 | 266 | 91 | 80 | 95 | 0 |
| 18 | 1 | 266 | 91 | 80 | 95 | 0 |
| 19 | 0 | 175 | 54 | 53 | 68 | 0 |
| 20 | 0 | 175 | 54 | 53 | 68 | 0 |

Every row satisfies `class = category(head + body + tail)` with the thresholds
above. Row 1 is the `+0x1b4 = 1` branch; its body is even, so both formulas
give 606.

## Divergence from the `.wld`

The `.wld` genotype list predicts rows 1-6, 8 and 12 exactly (including the
hybrid `(0,4)` head/body in row 7: 158 and 140). Ten of the 20 rows differ,
e.g. row 7's tail is 194 in the live game but the `.wld` `(0,4)` Part 3/merge
composition gives 206; rows 9-11 and 13 also differ. The class still matches
for 19/20 rows (row 11 is class 2 live vs 1 from the file).

So the live park does **not** use the `.wld` genotypes unchanged for every
record (the mission/store path may recombine them), or the hybrid table is
composed by more than a plain two-allele blend. Do not treat the `.wld`
genotype as the final phenotype for the size class until the live genotype is
traced.

## Port guidance

`src/world/build.cpp::paddock_size_class` currently uses 300/900/1400. The
retail thresholds are **200/620/900** (class 3 above 900). Do not simply swap
the numbers: the port feeds the function a **mesh bounding-box maximum**, not
the phenotype `head + body + tail` length. The correct port input is the
phenotype length (Part 3 Length tables, part->gene `[0,1,1,2,3]`). Until that
is ported, the species-bbox proxy remains an approximation, and the port's
Cpg04 `used` (384) will not match retail (336).

## Evidence

- `FUN_0056FED0` saDinosaur ctor (copies 0x1b8 phenotype bytes to `+0x50`).
- `FUN_00617250` disassembly `0x6174bb..0x6174ff` (length and class).
- `FUN_00619830` (five part lengths, part->gene switch, lookups).
- `FUN_0057EB50` (`0x57ebc3` reads `+0x1e0`).
- `TranslatorConstants.ini` keys at translator `+0x244..+0x250`
  (`docs/formats/translator-constants.md`).
- Runtime: `notes/x32dbg-campaign.out.log` `DINORESET` lines,
  `scripts/x32dbg/start-campaign.ps1` (Cpg04 mission 1).
