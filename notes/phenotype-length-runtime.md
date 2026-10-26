# Phenotype length and paddock size class

Status: **solved, runtime-confirmed 20/20 on Cpg04.** This supersedes the
"divergence from the `.wld`" section of `notes/paddock-size-class-runtime.md`:
the live genotypes do match the `.wld` exactly. The apparent mismatch was an
**ordering artifact** (see below).

## What was measured

`scripts/x32dbg/start-campaign.ps1 -GenotypeTrace -UnlockCampaigns -NoTrace`
now installs its loggers while the debuggee is stopped (commands sent while
it runs are only processed at the next break, so the old late block never
armed). New breakpoints:

| Address | Logged | Why |
| --- | --- | --- |
| `0x56FED0` | return address, genotype pointer, genes 0-3, `P+0x98/+0xa4/+0xa8`, `P+0x190` | `saDinosaur` ctor; fires once per dinosaur in file order |
| `0x570FD0` | `saDinosaur+0x1e0`, `+0x50`, part lengths | the old `DINORESET` logger |
| `0x619830` | runtime Part 3 Length cells: Head `(0,4)`, `(1,4)`, `(18,0)`, Body `(0,4)`/`(0,0)`, Tail `(0,4)`, `(1,4)`, `(18,0)` | proves the runtime tables are the file tables |

The ctor ABI is `FUN_0056FED0(phenotype /*[esp+4]*/, genotype /*[esp+8]*/, ...)`,
called from `0x5865BB` inside `FUN_00586520`; the world path is
`FUN_0053CD50` -> `FUN_00586520`. `FUN_0053CD50` passes the record's 18-gene
array at `drsDinosaur+0x3c`.

## Ordering artifact

`0x570FD0` (need reset) does **not** fire in world-record order; `0x56FED0`
does. Logging both in one run shows:

- the 20 ctor genotype pairs equal the `.wld` pairs, in file order;
- the 20 `DINORESET` rows are a different permutation of the same set.

The earlier note compared `.wld` row *i* with `DINORESET` row *i* and
concluded the live genotypes diverge for 10/20 rows. They do not.

## Runtime table cells

The Part 3 tables at translator `+0x308` (Head), `+0x314` (Body), `+0x318`
(Tail) were read inside `FUN_00619830`; they match
`tools/formats/simu_trans.py` exactly:

| Cell | Runtime | File |
| --- | ---: | ---: |
| Head `(B=4,A=0)` | 157 | 157 |
| Head `(B=4,A=1)` | 129 | 129 |
| Head `(B=0,A=18)` | 135 | 135 |
| Body `(B=4,A=0)` | 140 | 140 |
| Body `(B=0,A=0)` | 200 | 200 |
| Tail `(B=4,A=0)` | 206 | 206 |
| Tail `(B=4,A=1)` | 154 | 154 |
| Tail `(B=0,A=18)` | 193 | 193 |

## The part lengths are scaled proportions, not table values

`FUN_00619830` first stores the raw Length-table values at
`P+0x98..+0xa8`, computes the five part scales at `P+0xd4..+0xe4`, and then
its final loop **rewrites** `P+0x98` (length), `P+0xac` (width) and `P+0xc0`
(height) from `__ftol()` of x87 floats. The decompiler drops the operands of
that loop; the runtime values recover the formula:

```text
table_head = Length[Head][gene0.B][gene0.A]
table_tail = Length[Tail][gene3.B][gene3.A]
body       = Length[Body][gene2.B][gene2.A]
prop_head  = (proportion[gene0.A].head + proportion[gene0.B].head) / 2
prop_tail  = (proportion[gene3.A].tail + proportion[gene3.B].tail) / 2

head = (int)(table_head * (prop_head * (body / table_head)))   // float32
tail = (int)(table_tail * (prop_tail * (body / table_tail)))   // float32
length = head + body + tail
class  = length <= 200 ? 0 : length <= 620 ? 1 : length <= 900 ? 2 : 3
```

The division and multiplication must be done in **float32** in that order:
`prop * (body / table)` then `table * scale`. A plain
`(int)(prop * body)` gives 166 for row 1 (live 167), and rounding the total
gives 704 for row 11 (live 702).

`proportion.dat`: `u32 count`, then 5 floats per species in `anim_trans.dic`
`// Dinosaurs` order: head, arm, body, leg, tail. The length formula uses
columns 0 and 4.

## Twenty live rows (Cpg04)

`p98/pa4/pa8` read at `0x56FED0`, in `.wld` record order; prediction uses the
formula above.

| # | gene 0 | gene 2 | gene 3 | live head/body/tail | length | class |
| ---: | --- | --- | --- | --- | ---: | ---: |
| 1 | (1,1) | (1,1) | (1,1) | 167/226/213 | 606 | 1 |
| 2 | (0,0) | (0,0) | (0,0) | 224/200/318 | 742 | 2 |
| 3 | (0,0) | (0,0) | (0,0) | 224/200/318 | 742 | 2 |
| 4 | (0,0) | (0,0) | (0,0) | 224/200/318 | 742 | 2 |
| 5 | (18,18) | (18,18) | (18,18) | 48/53/69 | 170 | 0 |
| 6 | (18,18) | (18,18) | (18,18) | 48/53/69 | 170 | 0 |
| 7 | (0,4) | (0,4) | (0,4) | 158/140/194 | 492 | 1 |
| 8 | (3,18) | (3,18) | (3,18) | 51/53/68 | 172 | 0 |
| 9 | (18,0) | (18,0) | (18,0) | 127/126/182 | 435 | 1 |
| 10 | (18,0) | (18,0) | (18,4) | 127/126/156 | 409 | 1 |
| 11 | (0,4) | (0,0) | (0,4) | 225/200/277 | 702 | 2 |
| 12 | (3,3) | (18,3) | (3,3) | 54/53/68 | 175 | 0 |
| 13 | (1,4) | (4,4) | (1,4) | 75/80/85 | 240 | 1 |
| 14-18 | (4,4) | (4,4) | (4,4) | 91/80/95 | 266 | 1 |
| 19 | (3,3) | (3,3) | (3,3) | 54/53/68 | 175 | 0 |
| 20 | (3,3) | (3,3) | (3,3) | 54/53/68 | 175 | 0 |

Classes: `1,2,2,2,0,0,1,0,1,1,2,0,1,1,1,1,1,1,0,0` -> six class 0, ten class 1,
four class 2 -> `used = 6*8 + 10*16 + 4*32 =` **336**, matching the retail
`stFence` total. The port's previous species-bbox proxy gave 384.

Note row 13: the head gene is `(1,4)` and the result is 75, i.e. the **mean**
of the two proportions, not the pure allele-4 value (91). The mix is 50/50
for Head/Body/Tail; only ArmLeg uses the directional merge table.

## Reproduction

```powershell
.\.venv\Scripts\python.exe $env:TEMP\pheno_f32.py   # local scratch script
```

The Python oracle is the float32 formula above; it matches all 20 live rows.

## Evidence

- Runtime: `notes/x32dbg-campaign.out.log` (`CTOR`, `TBL`, `DINORESET` lines),
  `scripts/x32dbg/start-campaign.ps1` (`-GenotypeTrace`).
- Ghidra: `0x0056FED0` (ctor), `0x00586520` (create), `0x0053CD50` (world
  spawn), `0x00584ED0` (genotype reader), `0x00617250` and `0x00619830`
  (translator), `0x006189E0` (Part 3 table reader).
- Data: `Data/Design/Genotype/simu_trans.dic` Part 2/3,
  `proportion.dat`, `TranslatorConstants.ini`.
- Python oracle: `tools/formats/simu_trans.py`, `anim_trans.py`,
  `wld.py`.
