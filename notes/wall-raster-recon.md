# Wall rasteriser / occupancy grid — raw evidence (2026-10-28)

Status: **Q1-Q4 answered from disassembly.** Three corrections to the existing
notes:

1. `0x5849a0` does **not** stamp the cell's wall type (cell `+0x14`) nor the
   `stWall` type (`+0x10`); it stamps the **global** `DAT_006757b4`, which the
   spPark ctor sets to `7`. The per-cell/per-wall type fields are dead in this
   path.
2. Flags `0` and `1` are **both add**; the remove flag is `2` (set by the cell
   destructor `0x584850`). There is no writer of flag `1` in the wall path.
3. Buildings **do** stamp the grid (`FUN_00586b80`, type `1`), so the premise
   "maybe buildings don't stamp" is false. `FUN_00586b80` does not touch the
   `scene+0x34`/`scene+0x50` rect lists at all.

Binary `/DinoIsland.exe-ebea17`. All addresses are VA.

## Q1 — what `0x584200` rasterises

### `stWall` ctor `0x584200` (called by `0x582210`, which is called by `0x584090`)

Signature (MSVC thiscall): `stWall(this, point* p0, point* p1, int* cellSize,
grid*, u16 wallType)`.

Fields written:

```asm
0058420d MOV EAX,[ESI]          ; p0.x
00584213 MOV [EBX],EAX          ; +0x00
00584215 MOV ECX,[ESI+4]
00584218 MOV [EBX+4],ECX        ; +0x04 p0.y
0058421b MOV EDX,[EDI]          ; p1.x
00584221 MOV [EBX+8],EDX        ; +0x08
00584224 MOV EAX,[EDI+4]
00584236 MOV [EBX+0xc],EAX      ; +0x0c p1.y
0058422b MOV [EBX+0x10],ECX     ; +0x10 wallType (param 5)
0058423e MOV dword [EBX+0x14],0x64353c   ; vtable
```

`+0x18/+0x1c/+0x20` is a `vector<stWallCell*>` (begin/end/cap).

Cell count:

```asm
00584249 FILD dword [ESI]        ; p0.x
00584252 FILD dword [ESI+4]      ; p0.y
00584259 FILD dword [EDI]        ; p1.x
0058425b FILD dword [EDI+4]      ; p1.y
...                              ; dx, dy
0058427e FSQRT                   ; sqrt(dx*dx+dy*dy)
00584284 FADD float [0x62588c]   ; + 1.0f   (0x62588c = 0x3f800000)
0058428a FILD qword [ESP+0x1c]   ; cell size (int64)
0058428e FDIVP
00584290 CALL 0x005982d4         ; __ftol (truncate toward zero)
```

So **`count = trunc((len + 1.0f) / cell)`**. For `len=16, cell=4`:
`17/4 = 4.25 -> 4` cells.

`FUN_005ffa50` at `0x5842aa` is the vector resize; it fills all `count`
entries with the value `0` (`MOV dword [ESP+0x54],0` before the call), i.e. the
cell-pointer array is zero-filled. The loop `0x5842bf..0x58430b` then allocates
`count` x `0x28`-byte cells and stores them at `stWall+0x18[i]`.

### Cell record (`0x584700`, 0x28 bytes)

```asm
00584722 MOV dword [ESI+0x04],0x642f74   ; vtable
0058470e MOV [ESI+0x08],0                 ; pair vector begin
00584711 MOV [ESI+0x0c],0                 ; pair vector end
0058471f MOV [ESI+0x10],0                 ; pair vector capacity
00584729 MOV [ESI+0x14],ECX               ; wall type (4th explicit ctor arg)
00584734 MOV [ESI+0x18],0                 ; never read again
0058473d MOV [ESI+0x1c],0                 ; add/remove flag
00584740 MOV [ESI+0x20],EDX               ; grid
00584730 MOV [ESI+0x24],EAX               ; cell-size source object
00584743 MOV EDI,[ESI+0x08]
00584737 LEA EAX,[ECX+1]                  ; capacity = cellSize+1 pairs
```

Initial pair-vector capacity is `cell_size + 1` (5 pairs for cell=4), because
one cell can hold `cell_size` interior points plus the boundary point.
`cell+0x00` = `*(int*)(cellSizeSource+0x08)` (an owner id; semantics open).

### Dispatch: four cases

```asm
0058432d MOV ECX,[ESI]       ; start.x
00584333 MOV EAX,[EDI]       ; end.x
00584335 MOV EDI,[ESI+4]     ; start.y
0058433c CMP ECX,EAX
0058434a JNZ 0x0058436f
0058434c CMP EDI,EDX
0058434e JNZ 0x00584389

; start == end: single callback
0058435d PUSH EAX            ; cellptr
0058435e PUSH EDI            ; y
0058435f PUSH ECX            ; x
00584360 LEA ECX,[ESP+0x34]  ; this = raster context
00584364 CALL [0x00643538]   ; FUN_00584520

; start.x == end.x (vertical)
00584395 MOV ECX,[ESP+0x68]  ; this = grid
00584399 CALL 0x0055d600

; start.y == end.y (horizontal)
0058437b MOV ECX,[ESP+0x68]  ; this = grid
0058437f CALL 0x0055d590

; else Bresenham 0x5843a3..0x5844e5, same callback per step
```

`0x55d590` (horizontal, **loops x at fixed y**):

```asm
0055d59c CMP EDX,ESI         ; sort x0,x1
0055d5aa CMP ESI,[ESP+0x10]
0055d5b6 MOV EDI,[ESP+0x1c]  ; y
0055d5ba TEST EDI,EDI
0055d5be CMP EDI,[EBX+4]     ; 0 <= y < grid.height
0055d5c3 TEST ESI,ESI
0055d5c7 CMP ESI,[EBX]       ; 0 <= x < grid.width
0055d5d7 PUSH ECX            ; cellptr = grid.rows[x] + y*2
0055d5d8 PUSH EDI            ; y
0055d5d9 PUSH ESI            ; x
0055d5da MOV ECX,EBP         ; this = raster context
0055d5dc CALL [EAX]          ; callback
```

`0x55d600` is the same with the roles swapped (**loops y at fixed x**). The
Bresenham path calls the same callback with `(x, y, grid.rows[x]+y*2)`.

### Callback `0x584520` — index by Euclidean distance, append the point verbatim

```asm
00584520 MOV AL,[ECX+0x1c]       ; context "started" flag
00584523 SUB ESP,0x8
00584526 TEST AL,AL
00584528 JNZ 0x0058453c
0058452a FILD dword [ESP+0xc]    ; x (arg1)
0058452e MOV byte [ECX+0x1c],1
00584532 FSTP float [ECX+0x8]    ; origin.x
00584535 FILD dword [ESP+0x10]   ; y (arg2)
00584539 FSTP float [ECX+0xc]    ; origin.y

00584544 MOV EDX,[ECX+0x10]      ; index
0058454c LEA ESI,[EDX+1]         ; index+1
0058454f FSUB [ECX+0x8]          ; dx
00584556 FSUB [ECX+0xc]          ; dy
0058456f FSQRT                   ; dist
00584575 FILD qword [ESP+4]      ; (double)(index+1)   ([ESP+8] = 0)
00584579 FMUL [ECX+4]            ; (index+1) * cellSize
0058457c FCOMPP                  ; compare (index+1)*cellSize with dist
00584580 TEST AH,0x41
00584583 JZ 0x00584590           ; (index+1)*cell > dist -> keep index
00584585 MOV EAX,[ECX+0x14]      ; count
00584588 DEC EAX
00584589 CMP EDX,EAX
0058458b JZ 0x00584590           ; index == count-1 -> keep index
0058458d MOV [ECX+0x10],ESI      ; index = index+1

00584590 MOV EDX,[ESP+0x10]      ; x
00584594 MOV EAX,[ESP+0x14]      ; y
00584598 MOV [ESP+4],EDX         ; local pair = (x, y)
0058459c MOV [ESP+8],EAX
005845a0 MOV EAX,[ECX+0x18]      ; stWall
005845a3 MOV ECX,[ECX+0x10]      ; index
005845a6 LEA EDX,[ESP+4]         ; &pair
005845aa PUSH EDX                ; &pair
005845ab MOV EDX,[EAX+0x18]      ; stWall->cells.begin
005845ae MOV ECX,[EDX+ECX*4]     ; cells[index]
005845b1 CALL 0x005848c0         ; append pair
```

Raster-context layout (built on the stack of `0x584200` at `[ESP+0x28]`):

```text
+0x00 vtable 0x643538
+0x04 float cell size
+0x08 float origin.x   (first in-bounds callback point)
+0x0c float origin.y
+0x10 int   index
+0x14 int   cell count
+0x18 stWall*
+0x1c byte  started
```

`0x5848c0` is a plain vector append of the 8-byte `(x,y)` pair to
`cell+0x8/+0xc/+0x10`, growth = doubling (`newCapacity = 2*count`), old buffer
freed through `0x405400`. It does **not** transform or offset the point.

**Answer to "1 or 4 units thick":** the wall occupies exactly the 1-unit-wide
Bresenham/integer line. The `cell_size` only groups consecutive line points
into `stWallCell` records (a run of up to `cell_size` world units **along** the
segment); it is not thickness. There is no left/right offset. The vector of a
cell contains the raw integer map-unit coordinates of that run.

### Concrete example: (100,200) -> (116,200), cell=4

`len=16`, `count=trunc((16+1)/4)=4`. Horizontal branch `0x55d590` visits
`x=100..116` inclusive at `y=200` (17 callback calls). Index increments when
`(index+1)*4 <= distance` and `index != 3`:

| cell | index threshold | points | pairs |
| ---: | --- | --- | ---: |
| 0 | dist 0..3 | (100,200) (101,200) (102,200) (103,200) | 4 |
| 1 | dist 4..7 | (104,200) (105,200) (106,200) (107,200) | 4 |
| 2 | dist 8..11 | (108,200) (109,200) (110,200) (111,200) | 4 |
| 3 | dist 12..16 | (112,200) (113,200) (114,200) (115,200) (116,200) | 5 |

Stamped map units: **17 u16 grid words** (4 `stWallCell` records),
`(100..116, 200)`, all with `u16 = (old & 0x0fff) | 0x7000`
(`DAT_006757b4 = 7`). The last cell record absorbs the remainder (5 pairs);
this is not a bug: `index` is clamped at `count-1`.

Notes:
- Cell grouping is **segment-relative** (origin = first in-bounds callback
  point), not aligned to the 4-unit world grid. A segment starting at x=101
  groups 101..104, 105..108, ...
- The points are the integer points on the inclusive line; the four `0x584090`
  edges share corners, so a closed perimeter stamps each corner twice
  (idempotent, same type).
- Diagonal segments: the Bresenham step advances one major-axis unit, while the
  cell index advances on Euclidean distance, so a 4-unit diagonal cell holds
  ~3 points, not 4.
- Out-of-grid points are skipped by the axis/Bresenham bounds tests before the
  callback; the count is still computed from the full segment length.

### Who calls `0x584090`

```text
0x584090 <- 0x580f00 (xref 0x580fcc)
0x580f00 <- 0x57dc60 (xref 0x57dc89, stFence ctor: capacity
                     ((x1-x0+1)/cell)*((y1-y0+1)/cell), wall type arg)
0x580f00 <- 0x579440 (xref 0x579469, show/attraction ctor)
```

So the player fence path is `drsFence` -> `stFence` `0x57dc60` -> `0x580f00`
-> `0x584090` (4 sides) -> `stWall` -> rasteriser. `0x57dc60` is called from
`0x588d50` (xref 0x588d9d).

## Q2 — `0x5849a0` add/clear semantics

```asm
005849a1 MOV EAX,[ECX+0x1c]     ; flag
005849a5 MOV EBX,[ECX+0x8]      ; pairs begin
005849a9 MOV ESI,[ECX+0xc]      ; pairs end
005849ad SUB ESI,EBX
005849af SAR ESI,3              ; pair count
005849b2 TEST EAX,EAX
005849b8 JZ 0x005849ed          ; flag 0 -> ADD
005849ba CMP EAX,1
005849bd JZ 0x005849ed          ; flag 1 -> ADD
; ---- CLEAR (anything else) ----
005849c5 MOV EDX,[ECX+0x8]
005849cb MOV EDI,[EDX+EAX*8+4]  ; pair.y
005849cf MOV EBX,[EBX+0x10]     ; grid.rows
005849d2 LEA EDX,[EDX+EAX*8]
005849d5 MOV EDX,[EDX]          ; pair.x
005849d7 MOV EDX,[EBX+EDX*4]    ; rows[x]
005849da AND word [EDX+EDI*2],0xfff
; ---- ADD ----
005849fa MOV EAX,[0x006757b4]   ; GLOBAL type, not cell+0x14
00584a09 SHL EAX,0xc
00584a12 AND word [EBP+EDI*2],0xfff
00584a23 OR  word [ESI+EDI*2],AX
```

- The add branch runs for the **whole byte-vector** `(end-begin)/8` pairs; it
  stamps every pair of the cell record, i.e. every integer point of the wall
  line. Nothing else is stamped.
- Add = `(old & 0x0fff) | (DAT_006757b4 << 12)`; the low 12 bits survive.
- Clear = `old & 0x0fff` (high nibble zeroed, low 12 bits survive).
- `DAT_006757b4` is 0 in the file; the spPark ctor `0x5874a0` writes `7`
  (`DAT_006757b4._0_2_ = 7` at `0x587974`). Road = `DAT_00675a60` = 4; a third
  value `DAT_00675754` = 6 is used by the separate `0x57d990` stamp below.
- **Flag 1 is not "remove".** The cell ctor `0x584700` writes 0; the cell
  destructor `0x584850` writes 2 before calling `0x5849a0`:

```asm
00584850 ... MOV dword [param_1+0x1c],2 ; then CALL 0x5849a0
```

`0x5849a0` has exactly two callers: `0x584200` (ctor, flags 0) and `0x584850`
(dtor, flag 2). No writer of flag 1 found in this build; if set externally it
would behave as add.

## Q3 — validator scans `0x58a390`, `0x58a190`, `0x58a4e0`, `0x5812b0`

`0x58a390` is the callback of a stack "scan context" (vtable `0x6435b8`):

```text
+0x00 vtable 0x6435b8
+0x04 int  result code (0 ok, 1 busy, 2 uneven)
+0x08 grid*     (scene+0x60)
+0x0c terrain*  (scene+0x74)
+0x10 float reference height
```

Occupancy test (only the high nibble, no low-12-bit test):

```asm
0058a39a MOV EAX,[EBP+8]              ; grid
0058a39d MOV ECX,[EAX+0x10]           ; rows
0058a3a0 MOV EAX,[ECX+EDX*4]          ; rows[x]
0058a3a3 MOV ECX,[ESP+0x14]           ; y
0058a3a7 TEST byte [EAX+ECX*2+1],0xf0 ; (u16 & 0xf000)
0058a3ac JZ 0x0058a3bd
0058a3ae MOV EAX,1
0058a3b3 MOV [EBP+4],EAX              ; code 1 = BUSY
0058a3b6 RET 0xc
```

Terrain/slope test (bilinear `u16` height at `x*cell`, `y*cell`, `/256`,
compare with the reference height at the scan origin):

```asm
0058a4a9 FMUL float [0x00626ca8]  ; * 0.00390625 (1/256)
0058a4b2 FSUB float [EBP+0x10]    ; - reference
0058a4b5 FABS
0058a4b7 FCOMP float [0x00643598] ; 0.01f (0x3c23d70a)
0058a4bf TEST AH,0x41
0058a4c2 JNZ 0x0058a4d4           ; |delta| <= 0.01 -> ok, return 0
0058a4c4 MOV dword [EBP+4],2      ; code 2 = UNEVEN_FLOOR
0058a4cb MOV AL,1
```

So `0x58a390` rejects any nonzero high nibble (any of types 1, 4, 6, 7, ...
all count as occupied) and accepts only type 0; the low 12 bits are ignored.
The slope test is an **absolute height equality** test at 1/256-unit
resolution, tolerance 0.01 world units — not a gradient threshold. `0.01f`
accepts `<= 0.01`, rejects `> 0.01`.

Wrappers:

- `0x58a190(scene, point*, margin)` builds the context, then scans the square
  `x = p.x .. p.x+margin-1`, `y = p.y .. p.y+margin-1` and returns context+4
  (`0x58a37c MOV EAX,[ESP+0x2c]`). Before the scan it loops the `scene+0x50`
  pointer vector; each element `-> +0x1c -> rect +0x1c/+0x20/+0x24/+0x28`. If
  the whole margin square is strictly inside such a rect it returns **3**
  immediately (`0x58a1fd MOV EAX,3`). So codes: 1 busy, 2 uneven, 3 inside
  perimeter.
- `0x58a4e0(scene, p1*, p2*, margin)` first rejects too-small rects:

```asm
0058a511 LEA ECX,[ECX*4-1]      ; 4*margin - 1
0058a518 SUB EDX,EDI            ; (p2.x-margin) - (p1.x+margin)
0058a51a CMP EDX,ECX
0058a51c JL 0x0058a713          ; -> EAX=4 (PERIMETER_TOO_SMALL)
```

  then scans `x = arg2.x+margin .. arg3.x-margin`, `y = arg2.y+margin ..
  arg3.y-margin` (the **inset interior**, not the border line) with the same
  context and returns context+4 (`0x58a705 MOV EAX,[ESP+0x2c]`). This assumes
  arg2 is the low corner and arg3 the high corner; `0x52bbf0` is the caller's
  two-point sorter (it also applies `max-1`), but which output gets the low
  corner was not disambiguated from its float code.
- `0x5812b0(scene, point*, orientation, size*)` uses **only** the `scene+0x34`
  pointer vector (records: `+0x04 min x, +0x08 min y, +0x0c max x, +0x10 max
  y`). It translates the point by `[scene+0x1c]->+0x1c/+0x20`, swaps w/h for
  orientation 1/3, and returns 1 on overlap:

```asm
00581307 MOV EDI,[ECX+0x34]      ; vector begin
00581312 MOV EAX,[ECX+0x38]      ; vector end
0058131c SAR EAX,2               ; pointer count
00581330 MOV EDX,[ECX+4]         ; rec.minx
00581336 MOV EBP,[ECX+8]         ; rec.miny
00581339 MOV ECX,[ECX+0x10]      ; rec.maxy
0058133c CMP ESI,EDX
00581340 CMP ESI,EBX             ; interval overlap x
00581364 MOV dword [ESP+0x10],1  ; overlap -> return 1
```

There is **no** grid/occupancy access and **no** slope test in `0x5812b0`. It
is the feature (LunchBox / observation-area) overlap test.

`0x52c2e0` maps the returned codes to the retail strings: 1
`WARNING_BUSY_SPACE` (0x66350c), 2 `WARNING_UNEVEN_FLOOR` (0x6634f4), 3
`WARNING_INSIDE_PERIMETER` (0x6634d8), 4 `WARNING_PERIMETER_TOO_SMALL`
(0x6634b8), 5 `WARNING_ROAD_TOO_SHORT` (0x6634a0). Decompiled switch
confirmed.

## Q4 — building construction does stamp

Placement chain: the construction helper `0x53d1a0` calls
`0x586950` (a thin wrapper) -> `0x589100`. `FUN_00589100` dispatches on a type
id read at `[definition+0x6c]`:

```text
7 -> 0x595d30   8 -> 0x594d30   9 -> 0x595bf0   0xa -> 0x595c70
0xb -> 0x5958b0 5 -> 0x5959b0   6 -> 0x590130
0xc -> 0x586b80 ; and cases 0,1,2,3,4 -> 0x586b80
```

`FUN_00586b80` (generic footprint object ctor) hardcodes the grid type:

```asm
00586b93 MOV word [ESI+0x2e],0x1     ; type 1, hardcoded
...
00586c66 MOV CX,word [ESI+0x2e]      ; = 1
00586c6a MOV EBX,[ESI+0x44]          ; grid
00586c77 MOV word [ESP+0x14],CX      ; context+0x04 type
00586c7f MOV dword [ESP+0x18],EBX    ; context+0x08 grid
00586c85 MOV dword [ESP+0x10],0x643558 ; vtable
...
00586cc0 LEA ECX,[ESP+0x1c]          ; context
00586cc4 CALL [EDX]                  ; 0x5779a0
```

Footprint: `x = [obj+0x08] .. [obj+0x10]`, `y = [obj+0x0c] .. [obj+0x14]`
(set from the `.bfi` record at `+0x40`, with the orientation swap at
`0x586c37..0x586c60`). `0x5779a0`:

```asm
005779a0 MOV AX,word [ECX+4]         ; type
005779a7 MOV ECX,[ECX+8]             ; grid
005779b7 AND word [EDX+EDI*2],0xfff
005779c3 SHL EAX,0xc
005779c9 OR  word [EDX+EDI*2],AX
```

So every building cell becomes `(old & 0x0fff) | 0x1000`. The callback returns
with `AL=0` (`005779d1 XOR AL,AL`), so the caller's break-on-nonzero never
fires and the whole footprint rect is stamped.

Removal: `0x586e80` and `0x586f80` build the same `{vtable 0x643558, type 0,
grid}` context and call `0x5779a0` for every footprint cell, which clears the
high nibble (`type 0` -> `old & 0x0fff`).

`FUN_00586b80` never references `scene+0x34` or `scene+0x50`; its only
registration call is `0x572fa0` (`0x586c10`), which inserts the object into a
container at `[registry+0x04]` (`0x5e40e0`/`0x5e81f0`) and writes
`[entry+0x14]`. It does not append to the two obstacle rect vectors. Therefore:
- buildings block later walls/perimeters through the **grid** high nibble
  (`0x58a390` code 1), not through the rect lists;
- with `0x58a190`/`0x58a4e0` they are seen by the cell scan;
- with `0x5812b0` (feature placement) they are seen only if something else put
  their rect into `scene+0x34`, which the building ctor does not do.

Adjacent grid writer (not part of the stWall path): `0x57d990` stamps
`DAT_00675754 << 12` (global = 6) **and** then clears bits 0..10 and ORs the
16-bit value at `[context+0x0a]`, or clears the high nibble when the flag byte
at `[context+0x08]` is non-zero. Its context is `{vtable 0x6432a0, grid*,
byte flag, u16 value}` (layout `0x57d730`: `+0` vtable, `+4` grid, `+8` flag,
`+0x0a` value) and `0x57d730` rasterises either a filled rect, a line, or an
axis line selected by `[source+0x24]` (0/1/4). Called by
`0x57d6f0/0x57d720` (flag 1 = clear) and
`0x57da20/0x57dad0/0x57dc30` (flag 0 = stamp). The object's role (fence
area/id marker vs. preview) is **not** proven yet.

## Open

- `cell+0x00` owner id = `*(int*)(cellSizeSource+0x08)`; the cell-size source
  is the pointer passed through `0x580f00`/`0x584090`; its `+8` meaning is
  unknown.
- `stWall+0x10` and cell `+0x14` (the passed wall type) are not read by
  `0x5849a0`; what consumes them is unknown (possibly only the show arenas /
  renderer).
- `0x57d730`/`0x57d990` type-6 marker: shape, call site and whether it fills
  the paddock interior or draws the line are unresolved.
- Low 12 bits of the grid word: `0x57d990` writes an 11-bit value; no reader
  found in the validators. Semantics open.
- `0x58a4e0` assumes arg2 is the low corner and arg3 the high corner;
  `0x52bbf0` sorts the two click points (and subtracts 1 from the max), but a
  runtime probe would confirm the exact last column/row.
- The four `0x584090` edges are inclusive on both ends, so edges share corner
  cells; harmless with a single type, but relevant if the stamp ever becomes a
  counter.

## Evidence

- `0x584200` (stWall raster ctor), `0x584520` (raster context callback),
  `0x5848c0` (pair append), `0x584700` (cell ctor), `0x584850` (cell dtor,
  flag 2), `0x5849a0` (grid stamp), `0x584090`/`0x582210` (perimeter/AddSegment),
  `0x55d590`/`0x55d600` (axis walks), `0x55d...` Bresenham at
  `0x5843a3..0x5844e5`, `0x5844ef..0x584507` (final stamp loop).
- `0x58a190`/`0x58a4e0`/`0x58a390`/`0x5812b0`, `0x52c2e0` (warnings),
  `0x52bbf0` (rect sorter), `0x52b0d0`/`0x526f20` (fence tool apply).
- `0x5779a0` (type stamp), `0x5779e0` (secondary footprint rasteriser whose
  context type is 0, i.e. clear), `0x586b80` (building ctor, type 1),
  `0x586e80`/`0x586f80` (building clear), `0x589100` (spPark dispatcher).
- `0x57d730`/`0x57d990` (type-6 marker), `0x57dc60` (stFence ctor),
  `0x580f00` (perimeter ctor), `0x588d50` (stFence creation).
- Constants: `0x62588c` = 1.0f, `0x6254a0` = 0.0f, `0x626ca8` = 1/256,
  `0x643598` = 0.01f. Globals: `0x6757b4` = 7 (wall, written by `0x5874a0`),
  `0x675754` = 6, `0x675a60` = 4 (road).
- `notes/fence-construction-recon.md`, `docs/decomp/construction-validation.md`.
