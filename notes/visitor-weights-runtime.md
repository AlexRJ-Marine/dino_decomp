# Visitor weight vector — runtime evidence (2026-10-25)

Read-only recon of `DinoIsland.exe` under the headless x32dbg harness.
Reproduction script: `tools/recon/dump-visitor-weights.ps1`.

## What was dumped

The script sets three breakpoints:

| Address | Purpose | Log tag |
| --- | --- | --- |
| `0x00544110` | copies one parsed profile into the 0xEC-byte list record | `BVI_REC` |
| `0x0051f560` | writes `DAT_00675638` (the scorer weight pointer) | `GLOBAL_W_PTR` |
| `0x00522a10` | attraction scorer | `SCORE_HIT` |

At `0x0051f560` it also writes the argument pointer to
`C:\Tools\dino_dumps\wptr.bin` with `savedata` so the script can arm hardware
read breakpoints on the live record.

## Profile records

24 `BVI_REC` hits (one per `.bvi`), all at the same temporary address
(`0x0019FA7C`, or `0x0019FA68` when loading a save). Every hit logged the
same seven dwords at `temp+0xD0`:

```text
0 = 3F800000  1.0
1 = 3F800000  1.0
2 = 3F800000  1.0
3 = 43FA0000  500.0
4 = 43960000  300.0
5 = C53B8000  -3000.0
6 = 453B8000  3000.0
```

They match `.bvi` file offset `0x38..0x50` exactly. A scan of all 24 files
found a single distinct 7-float tuple, so this is global tuning data repeated
in every profile, not a per-personality value.

## Global pointer

`GLOBAL_W_PTR` (the argument to `FUN_0051f560`, i.e. the value stored in
`DAT_00675638`) pointed into the heap, e.g. `0x0A0CBED8` or `0x033FAF78`.
The six dwords there were `1, 1, 1, 500, 300, -3000`. The fifth profile field
(`P4`) was read at `ptr-0x9C` and the pointer itself at `+0x14`; both matched
the same block.

## The five variable weights

At the same breakpoint, dumping `ptr-0xAC .. ptr-0x98` gave:

```text
P0 (-0xAC) = 0           0.0
P1 (-0xA8) = 3F000000    0.5
P2 (-0xA4) = 3F800000    1.0
P3 (-0xA0) = BE4CCCCD   -0.2
P4 (-0x9C) = 0           0.0
```

That is SurferMan's file `0x1C..0x2C` (0.0, 0.5, 1.0, −0.2, 0.0), shifted by
`+8` in the record. `FUN_00544110` copies 13 dwords from the parsed temporary
record `+0xC` into the list record `+0xC`; those are file dwords 1..13
(offset `4..0x34`), so record `+0xC` = file `+4`, and record `+0x24` = file
`+0x1C`. The constant block is copied separately to `+0xD0`.

## Scorer details

`FUN_0059aef0` is `pow` (MSVC `_CIpow`), not a random helper. The double at
`0x006296a8` is `2.0` (`00 00 00 00 00 00 00 40`). In `FUN_00522a10` the
actor pointer is loaded into `EBP` (`MOV EBP,[EDI+0xC]`), so the `[EBP+0x48]`,
`[EBP+0x4c]`, `[EBP+0x50]` operands are actor need fields, not stack locals.
The random base is `rand() % (int)(W6 - W5) + W5`
(`FLD [W+0x18]; FSUB [W+0x14]; CALL ftol; rand; IDIV`).

## Negative result

A hardware read breakpoint on record `+0x24` (Surfer weight 0) and `+0x34`
(weight 4) was armed after park load. x32dbg reported both as set, but no
reads fired within 15 s in either an empty free-game park or a save loaded
from `run/save/slot001.sav` (two visitors). So the five variable weights are
not touched by the per-frame visitor AI; they are probably read at visitor
creation or in a slower need cycle.

## Caveats

- The headless harness reloads breakpoints from
  `C:\Tools\x64dbg\release\x32\headless\db\DinoIsland.exe.dd32`; the script
  deletes it before each run.
- Heap and stack addresses change per run; only the `.bvi` offsets and the
  constant values are stable.
- `savedata` takes a stack address (`esp+4`), not the pointer target
  (`[esp+4]`).
