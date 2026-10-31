# Show-arena wall grid: is there a grid -> mesh renderer? (2026-10-31)

Read-only runtime probe on the retail `DinoIsland.exe` through the windowed
x32dbg harness (`run-test/`, virtual input). No game files were modified; no
tracked screenshot was written.

**Question.** `FUN_005849a0` stamps every show-perimeter cell with class 7
(`DAT_006757b4 << 12 = 0x7000`). Does any function *read* such a marked cell
and instantiate the wall mesh (the "grid-to-mesh renderer"), or is the stamp
occupancy-only?

**Answer (negative, now measured).** A 16-bit hardware read watchpoint on a
show-perimeter cell `(420,416)` fired on **3 accesses in 60 s**, all of them the
marking code itself (`FUN_005849a0`'s read-modify-write at `0x584a12`/`0x584a23`,
trapped after the instruction at `0x584a19`/`0x584a27`). No renderer, scene
builder, or other consumer read the cell during the arena build or the park
loop. The class-7 stamp is occupancy-only. It is **not** the wall-mesh path.

The cell never carries the mesh in the first place: `cell+0x00` is
`*(wallRecord+8)`, which at runtime is `0x64` = 100 for both the fence and the
show walls. That is the `.bwi` `field_08` (uniform 100, see
`docs/formats/bwi.md`), **not** a mesh pointer. `notes/show-live-actor-recon.md`
section 7 ("mesh/resource pointer (`wallRecord+8`)") is wrong on that point.

## Commands run

```powershell
# census of every stamped wall point (auto-run log breakpoint)
.\scripts\x32dbg\show-wall-grid.ps1 -Mode Mark -CampaignRow 3 -UnlockCampaigns `
  -ParkTimeoutSec 180 -SimSeconds 15 -Log notes\harness\show-wall-grid-mark.log

# hardware read watchpoint on the first show cell (420,416)
.\scripts\x32dbg\show-wall-grid.ps1 -Mode Hw -CampaignRow 3 -UnlockCampaigns `
  -ParkTimeoutSec 180 -HwSeconds 60 `
  -CellXMin 420 -CellXMax 499 -CellYMin 416 -CellYMax 451 `
  -Log notes\harness\show-wall-grid-hw6.log
```

New harness script (committed with this report):
`scripts/x32dbg/show-wall-grid.ps1` (`-Mode Mark|Hw`). It runs the normal
Campaigns -> Cpg04 -> mission Start click sequence with virtual input, installs
the probes before the Start click, and takes no screenshots (see the harness
notes below for why).

## Static anchors (Ghidra `/DinoIsland.exe-ebea17`)

```asm
005849a0  PUSH ECX ...                       ; ECX = stWallCell, kept all function
005849f4  MOV ESI,[ECX+8]                    ; pair vector begin
005849fa  MOV EAX,[0x006757b4]               ; 7 (wall class)
00584a09  SHL EAX,0xc                        ; 0x7000
00584a12  AND word [EBP+EDI*2],0xfff         ; EBP = rows[x], EDI = pair.y
00584a1c  LEA EBP,[EBP+EDI*2]                 ; EBP = cell address
00584a23  OR  word [ESI+EDI*2],AX            ; the class-7 stamp
```

At `0x584a12`: `ECX` = cell object, `EBP` = `rows[x]`, `EDI` = y, `ESI` = x,
`EAX` = `0x7000`, cell address = `EBP + 2*EDI`. `wret=00584504` in the logs is
the return address into `FUN_00584200` (the `CALL 0x005849a0` is at `0x5844ff`).
Hardware data breakpoints trap after the access, so the two trap addresses are
`0x584a19` (after the AND) and `0x584a27` (after the OR).

## Run 1 - stamp census (`show-wall-grid-mark.log`)

`1304` stamps in `326` cell records, two wall records:

| wall record | points | cells | rect (world units) | where |
| ---: | ---: | ---: | --- | --- |
| `0x098B11AC` | 1072 | 268 | 9 rectangles: (376..407, 372..403), (420..443, 504..551), (452..467, 312..343), (456..487, 356..403), (456..479, 464..487), (512..543, 372..403), (512..543, 444..459), (572..595, 380..403), (608..631, 364..403) | the 9 Cpg04 fences |
| `0x098B1260` | **232** | 58 | **(420..499, 416..451)** | the **show arena** |

- The show record rect is exactly `2*(80+36) = 232` perimeter points, all
  `wcls=0x7000`, `wval=0` on the first stamp, `wmesh=0x64` (100),
  `wf24=0x098B1260` (the show wall record), `wf20=0x0A6A1410` (the grid).
- The grid is one `mcGrid` (`FUN_0061c1d0`: width, height, float cell size,
  `rows` at +0x10). The rasteriser and the dino footprint validator index
  `rows[world_x]` directly, so the Cpg04 grid cell is 1 world unit and the
  show rect needs no scaling. The 9 fences are distinct rectangles; no fence
  point falls inside the show rect, so the `(420..499, 416..451)` filter is
  unambiguous.
- **The arena is built at Cpg04 park load even though the show is closed**:
  the show wall record and its 232 stamps are present in the same world-import
  pass as the fences.

## Run 2 - hardware read watchpoint (`show-wall-grid-hw6.log`)

The stop was a real break condition on the first show stamp
(`esi>=0x1A4 && esi<=0x1F3 && edi>=0x1A0 && edi<=0x1C3`; x64dbg bare numbers
are hex, see below):

```text
CELLSTOP|wx=000001A4|wy=000001A0|wrow=0E9B64C0|wval=0|wcls=00007000
        |wobj=0E8598B0|wmesh=00000064|wf20=0A6A1410|wf24=098B1260
        |wret=00584504|t=642897531
cell row=0x0E9B64C0 y=0x1A0 -> cell=0x0E9B6800
Hardware breakpoint at 0E9B6800 set!
```

`bph 0x0E9B6800, r, 2` (16-bit read/write). The only hits in the 60 s window:

```text
CELLREAD|rcip=00584A27|rret=098B1260|rs4=00000009|rs8=00000000|rsC=0E4C69C0|rval=7000     |t=642898093
CELLREAD|rcip=00584A19|rret=098B1260|rs4=00000014|rs8=00000000|rsC=0AE8DA50|rval=70000000 |t=642898109
CELLREAD|rcip=00584A27|rret=098B1260|rs4=00000014|rs8=00000000|rsC=0AE8DA50|rval=70007000 |t=642898125
PARKLOOP|dt=00000000|t=642899015
Hardware breakpoint deleted!
```

Interpretation:

- `0x584A27` = the instruction after `OR word [esi+edi*2],AX` at `0x584A23`;
  `0x584A19` = the instruction after `AND word [ebp+edi*2],0xfff` at `0x584A12`.
  Both are **`FUN_005849a0` itself** (ECX still the cell; `[esp]` holds the show
  wall record `0x098B1260`, not a return address).
- The cell `(420,416)` is the corner shared by edge 1 (start) and edge 4 (end),
  so it is stamped twice: hit 1 is the first stamp (cell `0x7000`, next cell 0
  -> dword `0x00007000`), hits 2-3 are the second stamp (cell cleared by the
  AND -> `0x00000000` while cell `(420,417)` already holds `0x7000` ->
  `0x70000000`, then `0x70007000`). This confirms the address and the class-7
  value exactly.
- `PARKLOOP` is the one-shot `FUN_0058c2f0` park-clock call site `0x58c2fe`
  (`dt=0`: the mission briefing was still up). It fired 1.5 s after the stop,
  i.e. the arena build happens **inside** the park frame loop, and the watch
  stayed armed for the rest of the world import. The script then sent one
  `DIK_ESCAPE` (probe harness); `run-test/dino_hook.log` records
  `virtual keys: pressed=01` / `pressed=-` and the keyboard device captured, so
  the briefing hand-off was delivered. No later copy of the cell was read.
- Nothing else read the cell for the remaining ~57 s of park loop.

## Static cross-check: every known grid-class consumer is occupancy/logic

`search_code` for the class test (`0xf000` / `>> 0xc`) over the whole image,
plus the known validators:

| function | role | reads the cell to... |
| --- | --- | --- |
| `FUN_0055af20` | dino footprint visitor (`FUN_0055af70`) | `class = cell>>12`; 0 free, 1 free only on a low-11-bit match, else blocked |
| `FUN_0058a390` | construction "busy" scan context | `TEST byte [cell+1],0xf0`; 1 = BUSY |
| `FUN_0058d550` | road placement validator | accepts only `(cell & 0xf000) == 0x4000` at both ends |
| `FUN_0058cf90` | road grid visitor | class road (`DAT_00675a60`) or 0; else busy |
| `FUN_00570f30`, `FUN_00571a80` | dino danger-grid stamp/unstamp | class/flags, writes back |
| `FUN_0061c460`, `FUN_0061c1d0` | `mcGrid` clear / ctor | class 9 preserved |

None of them instantiates geometry. This matches the watchpoint result: the
grid is a logic/occupancy structure (`mcGrid`, CORE/Inc/Grid/mcGrid.cpp), not a
display source.

Consequence for the port: do not model a grid-cell -> wall-mesh renderer. The
wall pieces are the `stWallCell` records (`FUN_00584700`) created per segment
by `FUN_00584200`; the grid stamp is only the occupancy commit. Where the
*visual* wall comes from (the `.bwi` record's `string[4]` mesh, e.g.
`fence_show_lod1.mesh`) is still OPEN; it is not reached through the grid.

## Caveats

- One cell was watched. A renderer that only ever touches a different cell
  would be missed, but `(420,416)` is a perimeter corner, is stamped twice
  (edge 1 and edge 4), and any per-segment or per-corridor display pass that
  maps the grid would have to resolve that cell. The build-time and per-frame
  windows are both covered.
- The `AND` of the *resume* instruction was not trapped (x64dbg steps over the
  removed software breakpoint before it applies DR7); the `OR` on the same
  instruction pair and the second stamp prove the watchpoint was live.
- The Esc is sent after `PARKLOOP`; the retail briefing may still cover the
  scene, but the scene build is not deferred by the modal (the arena itself is
  built during the loop, as `PARKLOOP` dt=0 at the stop shows).

## Harness facts learned (reusable)

1. **x64dbg bare numbers are hexadecimal.** `{d:420}` = 1056; `a==0xA` and
   `0n` fail ("Error when evaluating break condition"). Spell decimal bounds as
   hex (`0x1A4` etc.) in conditions. This was the cause of the first failed
   watchpoint run.
2. **A breakpoint command runs even when the break condition is false**; only a
   true condition stops the debuggee. The command's log line therefore cannot
   be used as a "stop" signal. Key on a coordinate that only a true hit can
   carry (the first in-range `CELLSTOP`).
3. **Commands sent while the debuggee runs are queued** until the next stop.
   Always `pause` (and let it take effect) before `bph`/`bc`/`SetBreakpoint*`.
4. **Never call PrintWindow/ShowWindow while the debuggee is stopped** at a
   breakpoint: the window thread is frozen and the call can hang the harness
   (this timed out one run). Use posted messages only while waiting; take
   screenshots only while running.
5. The hardware watchpoint must be re-armed from a real stop; the first
   *recording* starts at the instruction after the stop point.

## Files

- `notes/harness/show-wall-grid-mark.log` - census (1304 stamps, 326 cells).
- `notes/harness/show-wall-grid-hw6.log` - final watchpoint run (3 reads, 60 s).
- `notes/harness/show-wall-grid-hw5.log` - 25 s watchpoint run, same 3 reads.
- `notes/harness/show-wall-grid-hw3.log` / `hw4.log` - failed condition/detection
  attempts kept for the harness lessons above; `hw.log` and `hw2.log` are the
  pre-fix attempts (`hw2` timed out in PrintWindow and has no log).
