# Park-loader economy guard `0x58b1b0`: what `BL` really is

Status: solved statically (Ghidra `DinoIsland.exe-ebea17`, image base 0x400000).
Answers the open item in `notes/hud-budget-source-recon.md` (lines 92-98) and
`notes/NOW.md` ("retail's load skips the restore for version > 0x17 when the
`BL` flag is set"). No runtime check was needed; one optional watch is listed
in section 8.

## Answer

- `BL` is **not** a save field, an autosave/manual flag, an editor flag or a
  return value. It is the **third stack argument of the park reader
  `FUN_0058aed0`** (`this` = `spPark`), i.e. a *call-site mode argument*:
  `0` = the stream is a `.sav` (self-contained park), `1` = the stream is a
  `.wld` campaign world file (world only; the park/economy block is absent).
  It is loaded once, at `0x58af45 MOV EBX,[ESP+0xA4]`, and never written again.
- `EDI` at the guard is the **file format version** (the first `u32` of the
  file), i.e. `FUN_0058aed0`'s second stack argument. Confirmed: it is read at
  `0x58aee1 MOV EDI,[ESP+0xA0]` and is used for every layout test in the
  function (`0x3`, `0x6`, `0x11`, `0x17`, `0xe`, `0x34`, `0x2d`).
- **Retail manual LOAD of a version-53 `.sav` => `BL = 0` => `JZ 0x58b1b9`
  taken => `FUN_00585240` runs => the budget IS read/restored.** The skip
  branch is unreachable for a `.sav`.
- The skip is real but belongs to the **campaign `.wld` load**
  (`FUN_00403e10` -> `0x0051e6c0(reader, version, 1)`), where `BL = 1` and the
  retail `.wld` version is 50-53, i.e. `> 0x17` => the economy/tail block is
  correctly not consumed because a `.wld` does not contain it.
- Redundancy: `FUN_00585240` **already self-gates** on the same flag
  (`0x585240 MOV AL,[ESP+0xC]; TEST AL,AL; JNZ ret`), so even without the
  outer guard a `.wld` load never touches the budget. For `BL = 1` the only
  material effect of the whole `0x58b1b0` guard is skipping the 12-byte
  `spPark+0x148` tail read at `0x58b1e9`.
- Therefore the `26000` HUD after a `12345` save load is **not** explained by
  this guard. See section 8 for the remaining candidates.

## 1. The guard (bytes + pseudo-C)

```
0058b1a9 8bcd            MOV ECX,EBP                 ; ECX = spPark (local copy of ECX entry)
0058b1ab e8 00040000    CALL 0x0058b5b0             ; reload/relink pass
0058b1b0 84db            TEST BL,BL                  ; loader flag (arg3 & 0xff)
0058b1b2 7405            JZ  0x0058b1b9              ; flag == 0  -> read
0058b1b4 83ff17          CMP EDI,0x17                ; flag != 0 : version <= 0x17 ?
0058b1b7 7733            JA  0x0058b1ec              ;   no  -> skip whole block
0058b1b9 8b8d 20010000  MOV ECX,[EBP+0x120]         ; ECX = spPark->economy
0058b1bf 53              PUSH EBX                    ; arg3 = flag
0058b1c0 57              PUSH EDI                    ; arg2 = version
0058b1c1 56              PUSH ESI                    ; arg1 = reader
0058b1c2 e8 79a0ffff    CALL 0x00585240             ; economy read  (+0x08 = budget)
0058b1c7 8b8d c4000000  MOV ECX,[EBP+0xC4]
0058b1cd 53              PUSH EBX
0058b1ce 57              PUSH EDI
0058b1cf 56              PUSH ESI
0058b1d0 e8 ab9a0000    CALL 0x00594c80             ; FUN_0058c210 + FUN_0058c620
0058b1d5 83ff0e          CMP EDI,0xE
0058b1d8 7212            JC  0x0058b1ec              ; version < 14 -> no tail
0058b1da 8b06            MOV EAX,[ESI]
0058b1dc 6a01            PUSH 0x1
0058b1de 81c5 48010000  ADD EBP,0x148
0058b1e4 6a0c            PUSH 0xC
0058b1e6 55              PUSH EBP
0058b1e7 8bce            MOV ECX,ESI
0058b1e9 ff5008          CALL [EAX+0x8]              ; read(spPark+0x148, 0xC)
0058b1ec 5f 5e 5d 5b     POP EDI/ESI/EBP/EBX
0058b1f0 81c4 88000000  ADD ESP,0x88
0058b1f6 c20c00          RET 0xC
```

```c
/* FUN_0058aed0, this = spPark (ECX) */
if (flag == 0 || version <= 0x17) {
    FUN_00585240(spPark + 0x120, reader, version, flag);   /* economy  */
    FUN_00594c80(spPark + 0x0c4, reader, version, flag);   /* two sub-readers */
    if (version >= 0x0e)
        reader->read(spPark + 0x148, 0x0c, 1);             /* camSwitcher tail */
}
```

The same flag is tested once more, earlier in the same function:

```
0058affb 84db            TEST BL,BL
0058affd 7500..          JNZ 0x0058b051
        (flag == 0)      read count + N x [read u32 -> factory -> vtable+0x10]
0058b051 83ff17          CMP EDI,0x17
0058b054 77..            JA  0x0058b0b2              ; flag!=0 && version>0x17 -> skip
        (flag != 0,      read count + FUN_005761b0/FUN_00576270/FUN_00576260
         version<=0x17)
```

That block is the campaign prefix / objective list, which
`docs/formats/sav.md` already documents as ".sav only" - the exact same
`flag && version > 0x17` rule.

## 2. Containing function and signature

`FUN_0058aed0` = `spPark` park-block reader (MSVC 6 thiscall, `RET 0xC`):

| | |
| --- | --- |
| entry | `0x0058aed0` |
| prototype | `void __thiscall spPark_Read(Reader *stream, uint version, char campaignFlag)` |
| `this` (ECX) | `spPark` - the caller passes `[simManager+0x14]` (`0x586ae7`) |
| arg1 `[ESP+4]` | `Reader *stream` -> `ESI` (`0x58aed9 MOV ESI,[ESP+0x98]`) |
| arg2 `[ESP+8]` | `uint version` -> `EDI` (`0x58aee1 MOV EDI,[ESP+0xA0]`) |
| arg3 `[ESP+0xC]` | `char campaignFlag` -> `EBX` (`0x58af45 MOV EBX,[ESP+0xA4]`) |

Frame proof: `SUB ESP,0x88` (0x58aed0), `PUSH EBX/EBP/ESI` (0x58aed6-8),
`PUSH EDI` (0x58aee0); at 0x58af45 ESP is `E-0x98`, so `[ESP+0xA4] = E+0xC`
= third stack argument. `EBX` is pushed at entry and popped at 0x58b1ef but is
*re-loaded* from the argument at 0x58af45; there is no other write to EBX in
the function (whole-function scan: only 0x58aed6 `PUSH EBX`, 0x58af45
`MOV EBX,[ESP+0xA4]`, 0x58b1bf `PUSH EBX`, 0x58b1cd `PUSH EBX`, 0x58b1ef
`POP EBX`).

Caller `FUN_00586ad0` (`0x00586ad0`, `this` = SimulationManager):

```
00586ad0 53                PUSH EBX
00586ad1 8b5c2408          MOV  EBX,[ESP+0x8]     ; arg1 = reader
00586ad5 55                PUSH EBP
00586ad6 8b6c2414          MOV  EBP,[ESP+0x14]    ; arg3 = flag
00586ada 56                PUSH ESI
00586adb 8b742414          MOV  ESI,[ESP+0x14]    ; arg2 = version
00586adf 57                PUSH EDI
00586ae0 83fe02            CMP  ESI,0x2
00586ae3 8bf9              MOV  EDI,ECX           ; this = sim manager
00586ae5 720b              JC   0x586af2
00586ae7 8b4f14            MOV  ECX,[EDI+0x14]    ; ECX = spPark
00586aea 55                PUSH EBP               ; arg3 = flag
00586aeb 56                PUSH ESI               ; arg2 = version
00586aec 53                PUSH EBX               ; arg1 = reader
00586aed e8de430000        CALL 0x0058aed0
```

Caller `FUN_0051e6c0` (`0x0051e6c0`, `this` = daMainApplication /
"APPLI" object, `ESI`):

```
0051e6c0 53                PUSH EBX
0051e6c1 8b5c240c          MOV  EBX,[ESP+0xC]     ; arg2 = version
0051e6c5 55 56             PUSH EBP / PUSH ESI
0051e6c7 83fb0b            CMP  EBX,0xB
0051e6ca 57                PUSH EDI
0051e6cb 8bf1              MOV  ESI,ECX
0051e6cd 722f              JC   0x51e6fe
0051e6cf 8a44241c          MOV  AL,[ESP+0x1C]     ; arg3 = flag  ([ESP+0x1C] = E+0xC)
0051e6d3 84c0              TEST AL,AL
0051e6d5 7527              JNZ  0x51e6fe           ; flag != 0 -> no scenario reset
0051e6d7 8b8eb4000000      MOV  ECX,[ESI+0xB4]    ; scenario context
0051e6dd e89ef50300        CALL 0x0055dc80
0051e6e2 8b8eb4000000      MOV  ECX,[ESI+0xB4]
0051e6e8 e893f20300        CALL 0x0055d980
0051e6ed 8b442414          MOV  EAX,[ESP+0x14]    ; arg1 = reader
0051e6f1 8b8eb4000000      MOV  ECX,[ESI+0xB4]
0051e6f7 53                PUSH EBX               ; version
0051e6f8 50                PUSH EAX               ; reader
0051e6f9 e812f00300        CALL 0x0055d710        ; apply scenario defaults
0051e6fe 8b4c241c          MOV  ECX,[ESP+0x1C]    ; arg3 = flag
0051e702 8b7c2414          MOV  EDI,[ESP+0x14]    ; arg1 = reader
0051e706 51                PUSH ECX               ; flag
0051e707 8b4e0c            MOV  ECX,[ESI+0xC]     ; world manager
0051e70a 53                PUSH EBX               ; version
0051e70b 57                PUSH EDI               ; reader
0051e70c e84fde0100        CALL 0x0053c560        ; world block
0051e711 8b54241c          MOV  EDX,[ESP+0x1C]    ; flag
0051e715 8b4e08            MOV  ECX,[ESI+0x8]     ; SimulationManager
0051e718 52                PUSH EDX               ; flag
0051e719 53                PUSH EBX               ; version
0051e71a 57                PUSH EDI               ; reader
0051e71b e8de430000        CALL 0x00586ad0
```

So the flag is decided **three levels above** the guard and is never derived
from the byte stream.

## 3. Caller chain up to the LOAD GAME / park-load entry

```
daMainApplication::Run  FUN_00402460 (0x402460, this = app, ECX)
  0x402538  if (app[0x48].state == 1 && app[0x48].b10)      ; menu start request
  0x4025e6    CALL 0x00403a80                               ; build ParkAppli
  0x4025ee    MOV ECX,[app[0x48]+0x24]                      ; start mode
  0x4025f1    CMP ECX,0x2
  0x4025f8    CALL 0x00402f10   ; mode == 2  -> "load existing save" variant
  0x4025ff    CALL 0x00403e10   ; else       -> new park (free game / campaign)
```

(Selector identification already recorded in `notes/park-load-static.md` line
23-24 and 213: `+0x24` = load-vs-new selector.)

`FUN_00402f10` = save load. Prologue `0x402f1e XOR EBX,EBX` -> flag is the
constant 0 for the whole function:

```
00403127 53                PUSH EBX                     ; arg3 flag = 0 (persistent push)
00403185 8b442418          MOV ECX,[ESP+0x18]           ; version read from the file
00403189 51                PUSH ECX                     ; arg2 version
0040318a 8b4d40            MOV ECX,[EBP+0x40]           ; this = appli
0040318d 52                PUSH EDX                     ; arg1 reader (opened "save/<name>")
0040318e e8..              CALL 0x0051e6c0
```

`FUN_00403e10` = new park; its campaign branch is the only flag = 1 site:

```
00403e10 ...              (scenario init: FUN_0051d1d0, FUN_0051dad0, FUN_0051da20)
004040d8 8b4c2410          MOV ECX,[ESP+0x10]           ; read version slot
004040dc 6a01              PUSH 0x1                     ; arg3 flag = 1
004040de 8d542440          LEA EDX,[ESP+0x40]           ; reader
004040e2 51                PUSH ECX                     ; arg2 version
004040e3 8b4d40            MOV ECX,[EBP+0x40]
004040e6 52                PUSH EDX                     ; arg1 reader
004040e7 e8..              CALL 0x0051e6c0
```

`FUN_00402bc0` = save/autosave wrapper (`save/auto/auto.sav`); it re-reads the
current file before writing and also passes 0:

```
00402da1 8b4c2414          MOV ECX,[ESP+0x14]           ; version
00402da5 57                PUSH EDI                     ; arg3 flag = 0 (XOR EDI,EDI at 0x402bcb)
00402da6 8d542448          LEA EDX,[ESP+0x48]           ; reader
00402daa 51                PUSH ECX                     ; arg2 version
00402dab 8b4d40            MOV ECX,[EBP+0x40]
00402dae 52                PUSH EDX                     ; arg1 reader
00402daf e8..              CALL 0x0051e6c0
```

## 4. Possible sources of `BL` at `0x58b1b0` (complete - only three call sites exist)

`list_xrefs` gives exactly one caller for `0x58aed0` (0x586aed), one for
`0x586ad0` (0x51e71b) and three for `0x51e6c0` (0x402daf, 0x40318e, 0x4040e7),
so this table is exhaustive.

| # | instruction | value of `BL` | file opened | meaning |
| --- | --- | --- | --- | --- |
| 1 | `0x402da5 PUSH EDI` (EDI = 0 from `0x402bcb XOR EDI,EDI`) in `FUN_00402bc0` | 0 | `save/` + `save/auto/auto.sav` | save/autosave wrapper; parks block present |
| 2 | `0x403127 PUSH EBX` (EBX = 0 from `0x402f1e XOR EBX,EBX`) in `FUN_00402f10` | **0** | `save/<name>.sav` (the LOAD GAME selection) | **manual / autosave LOAD** |
| 3 | `0x4040dc PUSH 0x1` in `FUN_00403e10` (campaign branch) | 1 | campaign record path (`CpgNN_Save.wld`, `docs/formats/wld.md`) | world-only file; no park/economy block |

Nothing else can set it: inside `FUN_0058aed0` the only write to EBX is
`0x58af45`, which reads the stack slot of argument 3.

`BL` is therefore **not**: a parameter of the save loader proper, a return
value, a local derived from the file, the autosave/manual distinction, or an
editor flag. It is a constant chosen by the caller (`0` for `.sav`, `1` for
`.wld`).

## 5. `EDI` = version (confirmed)

- Loaded once at `0x58aee1 MOV EDI,[ESP+0xA0]` = stack argument 2.
- The callers obtain that argument from the **first dword of the file**:
  `FUN_00402f10` `0x402cb5-0x402cc6` (`reader->read(&local_64,4,1)`), then
  `if (10 < version) read(&extra,4,1)`; `FUN_00403e10` same pattern
  (`0x4040xx`, `docs/formats/wld.md`); `FUN_00402bc0` `0x402cb5`.
- Every layout branch in `FUN_0058aed0`/`FUN_00585240` compares `EDI`
  (`0x3`, `0x6`, `0x11`, `0x17`, `0xE`, and inside the reader `0xB`, `0x34`,
  `0x2D`), and `docs/formats/sav.md` / `wld.md` already identify that first
  dword as the version (`50-53` retail).

## 6. Operational answers

| load | flag at 0x58b1b0 | version | economy `+0x120/+0x08` | `spPark+0x148` tail |
| --- | --- | --- | --- | --- |
| retail **manual LOAD** of a v53 `.sav` (LOAD GAME panel -> `FUN_00402f10`) | 0 | 53 > 0x17 | **read (budget restored)** | read |
| **autosave** `.sav` via the same `.sav` path | 0 | 50-53 | **read** | read |
| campaign **`.wld`** load (`FUN_00403e10`, new-game/campaign chapter) | 1 | 50-53 > 0x17 | **skipped** (block absent from the file) | skipped |
| old `.wld`, version <= 0x17 | 1 | <= 23 | would be attempted (legacy layout) | skipped if < 14 |

Note the guard's `version` test is *only* about the legacy file layout: with
`BL = 1` and a modern `.wld` the park block simply is not in the stream, so
reading it would desynchronise the reader.

**Bonus (important for the port): the guard is redundant for the budget.**
`FUN_00585240` re-tests the same flag itself:

```
00585240 8a44240c        MOV AL,[ESP+0xC]      ; arg3 = flag
00585244 53 55 56 57     PUSH EBX/EBP/ESI/EDI
00585248 8bf9            MOV EDI,ECX           ; economy
0058524a 84c0            TEST AL,AL
0058524c 0f8524050000    JNZ 0x00585776        ; flag != 0 -> return
00585252 8b6c2418        MOV EBP,[ESP+0x18]    ; arg2 = version
00585256 83fd0c          CMP EBP,0xC
00585259 0f8217050000    JC  0x00585776        ; version < 12 -> return
0058525f 8b742414        MOV ESI,[ESP+0x14]    ; reader
00585263 6a01            PUSH 1
00585265 8d4f08          LEA ECX,[EDI+0x8]     ; &budget
...                      read(economy+0x08, 4, 1)
```

So the budget is read **iff `flag == 0 && version >= 12`**, no matter what the
outer guard at `0x58b1b0` does. `FUN_00594c80`'s two callees
(`FUN_0058c210`, `FUN_0058c620`) self-gate identically; the only read that
depends solely on the outer guard is the 12-byte `spPark+0x148` block.

## 7. `header_a` / `header_c` (`docs/formats/sav.md`)

These are **file bytes inside the park block** (`tools/formats/sav_manual.py`:
`block+0x04 = header_a = 3` for an autosave, `0` for a manual save;
`block+0x0C = header_c` float, `0.0` vs `10.0`). They are consumed by the park
block reader, *not* by the guard: `BL` is already fixed at `0x58af45`, before
the first park-block read, and never re-derived. Consequences:

- `header_a/header_c` decide whether retail **accepts/lists** the file
  (patching `3 -> 0` and `0.0 -> 10.0` makes an autosave loadable through
  LOAD GAME, `notes/sav-autosave-manualized.png`); they cannot influence
  `BL` or the 0x58b1b0 decision.
- They are orthogonal to this note: a manualized autosave is still a `.sav`,
  so it still goes through `FUN_00402f10` with `BL = 0` and its economy is
  read.

## 8. What is left (and the one runtime watch, if wanted)

Static analysis cannot explain the `26000` HUD; the flag question above is
settled. Two remaining candidates, in order:

1. The `.sav` scenario reset in `FUN_0051e6c0` (`0x51e6d7-0x51e6f9`,
   `FUN_0055d710`) runs *before* the park block is read, so it cannot overwrite
   the restored budget - but a later scenario/economy writer in the same load
   could. Check callers of `FUN_0055d8e0` (`0x55d904`) / `FUN_00561bb0`
   (`0x5620fa`) that are reachable from `FUN_0051e6c0`.
2. The observed load was a campaign `.wld` start (flag 1, economy skipped, the
   scenario budget survives: `FUN_0055d8e0` writes `scenario+0x7c`).

If a runtime confirmation is still wanted, the single decisive point is:

- breakpoint `0x58b1b0`, expression `(ebx & 0xff) | (edi << 8)` - low byte =
  flag, high word = version. `0` low byte = `.sav` (budget restored),
  `1` low byte = `.wld` (budget skipped).
- equivalent upstream watch: hardware read on the stack slot at
  `0x58af45` (`[esp+0xa4]`), or breakpoint `0x4040dc` / `0x403127` to see
  which call site fires.
- to read the live value: at `0x58b1b9` (read path) `ecx` = the economy and
  `[ecx+8]` = the budget; at `0x58b1ec` the block was skipped.

## Evidence

- `0x58aed0` `SUB ESP,0x88`; `0x58aed6-0x58aee1` argument loads;
  `0x58af45 MOV EBX,[ESP+0xA4]`; `0x58b1f6 RET 0xC`.
- `0x58b1a9-0x58b1e9` guard and gated reads; `0x58affb-0x58b0b2` second gate.
- `0x586ad0-0x586aed` `FUN_00586ad0` -> `FUN_0058aed0` (ECX = `[sim+0x14]`).
- `0x51e6c0-0x51e71b` `FUN_0051e6c0` (scenario reset gated by
  `version > 0xB && flag == 0`).
- Call sites: `0x402daf` (flag 0), `0x40318e` (flag 0, `0x402f1e XOR EBX,EBX`),
  `0x4040e7` (`0x4040dc PUSH 0x1`).
- Selector: `0x4025ee-0x4025ff` in `FUN_00402460`.
- `0x585240-0x585259` `FUN_00585240` self-gate.
- Prior repo evidence reused: `notes/park-load-static.md` (selector),
  `docs/formats/sav.md` (`.sav` vs `.wld` block split, versions 50-53),
  `docs/formats/wld.md` (`0x00403e10` = `.wld` loader, campaign flag 1),
  `notes/hud-budget-source-recon.md` (economy field), `tools/formats/sav_manual.py`.

