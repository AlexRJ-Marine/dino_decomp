# Paddock size class: phenotype length and runtime values

Status: **solved and ported.** The class chain and the hybrid scale are both
explained; the port matches retail Cpg04 exactly (`used = 336`).

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
if (P+0x1b4 == 0) {           // 0 = biped (families 2/3)
    P+0x04 = P+0x98 + P+0xa4 / 2;    // half the body twice
    P+0x08 = P+0xa8 + P+0xa4 / 2;
} else {                      // non-biped
    P+0x04 = P+0x98 + P+0xa4;        // head + body
    P+0x08 = P+0xa8;                 // tail
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

## Why hybrids differ from the raw table

The live campaign save (`run/save/auto/auto.sav`, 53,338 bytes, 20
`drsDinosaur` records) has the **same 18-gene genotypes** as
`Cpg04_Save.wld` (20/20, verified with `wld.read_sav_world`). There is no
genotype mutation — the hybrid lengths differ because the raw Part 3 lookup is
then **scaled per part**.

`FUN_00619830` computes the scales at `0x619969..0x619a05` and applies them in
the loop at `0x619a0d..0x619a39`:

```text
head_scale = merge_blend(proportion[parent].head) * body_length / head_length
body_scale = 1
tail_scale = merge_blend(proportion[parent].tail) * body_length / tail_length
```

The raw part length does not cancel in the runtime code: the apply loop
rewrites each part as `table * scale` from floats, truncated. Expressing the
same operations directly in float32 (the order matters) reproduces **all 20
live Cpg04 values exactly**:

```text
head_ratio = mh * proportion[B0][head] + (1-mh) * proportion[A0][head]
tail_ratio = mt * proportion[B3][tail] + (1-mt) * proportion[A3][tail]
                      // mt uses [B3] first (0x6199E0 swap)
head = (int)(HeadLen[B0][A0] * (head_ratio / HeadLen[B0][A0] * body))
tail = (int)(TailLen[B3][A3] * (tail_ratio / TailLen[B3][A3] * body))
body = BodyLen[B2][A2]
length = head + body + tail
```

`mh`/`mt` are the Part 2 merge values `[B][A] / 100`. `proportion.dat` columns
are `[head, arm, body, leg, tail]`; the head ratio uses column 0 and the tail
ratio column 4. The class length is the sum of the three **individually
truncated** parts, not `body * (ratio + 1 + ratio)` with a final rounding:
the latter gives 436/704 for rows 9/11 where the live/port values are
435/702. Earlier `DINORESET` orderings hid this because 0x570FD0 does not fire
in world-record order; the 0x56FED0 ctor logger does. See
`notes/phenotype-length-runtime.md` for the 20-row proof.

The port implements this exact formula (`DinoPhenotypeTable`) and reports
Cpg04 `used = 336`.

## Port implementation

`src/formats/dino_data.{h,cpp}` now exposes `DinoPhenotypeTable`:
`read_dino_phenotype()` parses `proportion.dat`, the Part 2 Head/Tail merge
blocks and the Part 3 Head/Body/Tail Length blocks from `simu_trans.dic`;
`size_class()` implements the formula above with the retail thresholds
`200/620/900` (class 3 above 900). `paddock_size_class()` in
`src/world/build.cpp` now uses those thresholds.

- The imported `.wld`/`.sav` dinosaurs use
  `phenotype_table.size_class(record.genotype)`; the species-bbox proxy
  remains only as a fallback when the table fails to load.
- `--buy-dino` reads the full 18-gene `.gen` with `read_gen_genotype()` and
  uses the same class.
- Cpg04: **`paddocks: 9 capacity=492 used=336`**, matching retail exactly.
  `DINO_DEBUG_PHENO=1` prints the per-dinosaur genotype, length and class.
- Tests updated: `test_cpp_wld_park_dinosaurs`, `test_cpp_buy_dino`
  (`used=336`, or `352` after the Zarbosaurus purchase).

## Evidence

- `FUN_0056FED0` saDinosaur ctor (copies 0x1b8 phenotype bytes to `+0x50`).
- `FUN_00617250` disassembly `0x6174bb..0x6174ff` (length and class).
- `FUN_00619830` (five part lengths, part->gene switch, lookups).
- `FUN_0057EB50` (`0x57ebc3` reads `+0x1e0`).
- `FUN_00619830` scale block `0x619969..0x619a05` and the apply loop
  `0x619a0d..0x619a39` (part length *= scale).
- `TranslatorConstants.ini` keys at translator `+0x244..+0x250`
  (`docs/formats/translator-constants.md`).
- Runtime: `scripts/x32dbg/start-campaign.ps1` (Cpg04 mission 1) and its
  generated, gitignored `notes/x32dbg-campaign.out.log` `DINORESET` lines;
  the 20-value table above is the durable copy. The live 53 KB campaign save
  was parsed with `wld.read_sav_world` (20 dinosaurs + 9 fences + 57
  buildings); its genotypes match `Cpg04_Save.wld`.
- Port: `src/formats/dino_data.{h,cpp}` (`DinoPhenotypeTable`),
  `src/world/build.cpp` (`paddock_size_class`), `src/viewer/main.cpp` (import
  and buy paths), `tests/test_viewer_cpp.py` (`used=336`).
