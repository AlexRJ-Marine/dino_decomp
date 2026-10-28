# Fence wall parameters — margin, style index and cost (2026-10-28)

Status: **answered from the binary + retail `.bwi`/`.bbi` data.** The two-click
enclosure is the `Fence.bwi` wall style: **margin 4**, style index **0**,
**30 per perimeter cell**. The panel's "Observation Area" button is *not* a
wall style — it is a separate single-click placement of the `PointOfView`
building (4x4, construction cost 500). Raw evidence only; no `src/` change.

Complements `notes/fence-construction-recon.md` (state machine) and
`notes/wall-raster-recon.md` (raster/validators). Binary
`/DinoIsland.exe-ebea17`. All addresses VA.

## Questions asked

1. In the normal two-click wall/enclosure mode, what are the concrete values
   of `+0x50` (margin), `+0x4c` (index) and the price used by `0x581b90`?
2. What is the UI selection compared with `0x17` in `0x526930`, and where do
   the wall footprint/price live?
3. What does `SWITCH_OBSERVATION_AREA` change (`LAB_00531b60` /
   `LAB_00531b80`)? Is it a wall style with a different size/price?
4. What feeds `FENCE_FEATURE_PRICE` / `FENCE_NAME_INDICS` (`%u`, `%`), and
   what is shown in the default wall mode?

## Answers

| Item | Value | Proof |
| --- | --- | --- |
| `+0x50` margin | **4** (fixed) | ctor `0x52675e` = first wall record `+0x0c` = `Fence.bwi` byte 0x00 = 4. No runtime writer. |
| `+0x4c` wall style index | **0** (fixed) | ctor `0x526742`; passed to `0x581b90` as the config index and to `0x53ac60` as the WallList index. `WallList.ini` index 0 = `Fence.bwi`. |
| cell size (cost divisor) | **4** | `0x581bb9 DIV [config+0]`; config = element+0x0c = definition base D; `Fence.bwi` 0x00 = 4 |
| price per cell | **30** | `0x581bbb IMUL EAX,[ECX+0x10]`; config+0x10 = D+0x10 = element+0x1c = `Fence.bwi` 0x10 = 30 |
| 16x16-unit square cost | **450** | `(p2sum-p1sum)*2/4*30 = 30*2/4*30` |
| observation-area building | **`PointOfView`** (BuildingList index 3) | only `.bbi` containing `SWITCH_OBSERVATION_AREA`; tail key lands at record `+0x190` |
| observation footprint / price | **4x4 / 500** | `PointOfView.bbi` footprint 4x4, construction cost (file 0x15) = 500 |

## 1. Wall mode: `+0x4c`, `+0x50` and the `0x581b90` price

### 1.1 The two fields are written only in the constructor

`0x5265b0` (state ctor, vtable `0x642b4c`) writes both, once:

```asm
00526739 MOV ECX,[ESI+0x44]     ; this->tool  (+0x44)
00526742 MOV dword [ESI+0x4c],EDI   ; EDI = 0  -> wall style index
00526747 MOV EDX,[ECX+0x8]      ; tool+8 = pwdWorldManager
00526750 MOV EAX,[EDX+0x34]     ; pwdWorld data block
00526753 MOV ECX,[EAX+0x6c]     ; vector<wall def> begin (0xb4 stride)
00526756 MOV EDX,[ECX+0x0c]     ; first element +0x0c = .bwi file+0
0052675e MOV dword [ESI+0x50],EDX   ; margin = 4
```

- A full instruction walk of the state class ranges
  `0x526560..0x527480` and `0x52a5c0..0x52d500` finds **no other writer** of
  this-state `+0x50` or `+0x4c`. They are constructor constants for the life
  of the state.
- `+0x4c` is therefore *not* a "selected feature index"; it is the
  `WallList.ini` style index. `+0x50` is *not* the selected feature's
  footprint; it is the wall definition's first field (grid cell size).

### 1.2 `0x581b90` config = wall definition `+0x0c`

Both call sites build the same pointer:

```asm
; 0x52ad71 (live preview) and 0x526f75 (apply)
MOV EAX,[EBP+0x4c]      ; style index (0)
MOV EDX,[tool+8]        ; pwdWorldManager
LEA EAX,[EAX+EAX*4]     ; *5
MOV ECX,[EDX+0x34]      ; pwdWorld data
LEA EDX,[EAX+EAX*8]     ; *45
MOV EAX,[ECX+0x6c]      ; wall-def vector begin
LEA EAX,[EAX+EDX*4+0xc] ; element + idx*0xb4 + 0x0c = def base D
```

`0x581b90` (raw x86):

```asm
00581b90 MOV EAX,[ESP+4]    ; p1
00581b94 MOV ECX,[EAX+4]    ; p1.y
00581b97 MOV EDX,[EAX]      ; p1.x
00581b99 ADD ECX,EDX
00581b9b MOV EAX,ECX
00581b9d NEG EAX
00581b9f SHL EAX,0x1f
00581ba2 SUB EAX,ECX        ; EAX = -(p1.x+p1.y)
00581ba4 MOV ECX,[ESP+8]    ; p2
00581ba8 MOV EDX,[ECX+4]
00581bab ADD EAX,EDX
00581bad MOV EDX,[ECX]
00581baf MOV ECX,[ESP+0xc]  ; config
00581bb3 ADD EAX,EDX        ; p2sum - p1sum
00581bb5 SHL EAX,1          ; *2
00581bb7 XOR EDX,EDX
00581bb9 DIV dword [ECX]    ; / config[0] = cell size
00581bbb IMUL EAX,[ECX+0x10] ; * config[4] = price per cell
00581bbf RET
```

```text
cost = ((p2.x+p2.y) - (p1.x+p1.y)) * 2 / cell * price_per_cell
```

`0x52bbf0` sorts the two click points and subtracts 1 from both components of
the high corner before this call.

### 1.3 What `Fence.bwi` actually contains

`Data/Design/attractions/walls/Fence.bwi` (loader `0x53d9a0`, 0xb4-byte
records, definition base = element+0x0c = file base):

```text
file 0x00  u32   4        -> D+0x00  cell size / margin (0x50)
file 0x04  f32   8.0      -> D+0x04  height (Fence = 8.0)
file 0x08  u32   100      -> D+0x08  uniform
file 0x0c  u32   30       -> D+0x0c  cost class (10 wood / 15 / 30 plates)
file 0x10  u32   30       -> D+0x10  cost multiplier  (0x581b90 reads this)
file 0x14  u8    0        -> D+0x14  electrified
```

All 24 `.bwi` files have file 0x00 = **4** and file 0x10 = **30**; only the
height (0x04 = 2/4/6/8) and cost class (0x0c = 10/15/30) vary. Therefore the
two-click perimeter price is uniform across wall styles:
**`(sumdiff*2/4)*30`**.

Worked example (4x4 build cells = 16x16 world units, high corner = low+15):

```text
p1 = (100,100), p2 = (115,115)
cost = ((115+115) - (100+100)) * 2 / 4 * 30
     = 30 * 2 / 4 * 30 = 450        -> "Cost of enclosure: 450"
```

Minimum size (`0x58a4e0`, corrected reading of the raw x86):

```asm
0058a507 SUB EBP,ECX        ; p2.x - margin
0058a509 ADD EDI,ECX        ; p1.x + margin
0058a50b MOV EDX,EBP
0058a50d ADD EBX,ECX        ; p1.y + margin
0058a50f SUB EAX,ECX        ; p2.y - margin
0058a511 LEA ECX,[ECX*4-1]  ; 4*margin - 1
0058a518 SUB EDX,EDI        ; (p2.x-m)-(p1.x+m)
0058a51a CMP EDX,ECX
0058a51c JL  -> code 4 (WARNING_PERIMETER_TOO_SMALL)
```

The test is on the **inset** span: `dx - 2*margin >= 4*margin - 1`, i.e.
`dx >= 6*margin - 1`. With margin 4 the enclosure must be **>= 23 world units
across** (6 build cells at 4 units/cell); the scanned interior is then
`>= 4*margin` = 16 units wide. The old summary "width smaller than 4*margin-1"
in `notes/wall-raster-recon.md` is missing the `-2*margin`.

## 2. What the `0x17` check in `0x526930` is

```asm
00526939 MOV ECX,[0x006749e4]      ; UI selection manager
0052693f MOV EAX,[ECX+4]
00526946 MOV ECX,[EAX]             ; selection list at +8
00526949 ADD ECX,0x8
0052694c PUSH ECX
00526951 CALL 0x00447b90           ; ref selection id
00526956 CALL 0x00447c30           ; -> selection object
0052695f MOV EDX,[EAX]
00526963 CALL [EDX+0x4]            ; virtual slot 1 = class id
00526966 CMP EAX,0x17
```

`0x17` is a **UI class id**, fetched through `vtable+4`, not an element-type
enum. The class is created by `0x53ac60`, the wall/perimeter order builder
called from the apply path:

```c
FUN_0053ac60(park_world /*this*/, style_index, p1, p2):
    local = 0x17;
    FUN_00447bb0(&local);          // factory: class id 0x17
    obj = FUN_00447c30();
    obj+0x14 = style_index;        // WallList index
    obj+0x18/+0x1c = p1.x/p1.y;
    obj+0x20/+0x24 = p2.x/p2.y;
    FUN_005e7310();                // notify listeners / register the order
    FUN_00447b30();
    return obj;
```

Caller `0x526f20` (perimeter apply) passes `state+0x4c` as `style_index`
(`0x526fc2..0x526fd6`); the same index is the `0x581b90` config index. The
object is then made the current UI selection (`0x41a0e0`) and `0x5269e0`
refreshes the panel from it. On re-entry, `0x526930` sees that class id and
takes the "selected enclosure" branch. `0x52bbb0` stores the object at
`+0x78` and sets `+0x48 = 1`.

No wall footprint or price is stored in that object; it holds only the style
index and the two corners. The footprint/margin and price live in the
**`.bwi` wall definition** (see 1.3), loaded from
`Data/Design/attractions/walls/WallList.ini` + `<Name>.bwi` by `0x53d9a0`
into `pwdWorld` (`tool+8 -> +0x34`) at `+0x6c` (0xb4 stride).

Related definition tables in the same `pwdWorld` block:

```text
+0x6c  0xb4  wall defs     WallList.ini + *.bwi
+0x7c  0x1d8 feature defs  FeatureList.ini + *.bfi
+0x8c  0x1f4 building defs BuildingList.ini + *.bbi
```

## 3. `SWITCH_OBSERVATION_AREA` is a building placement, not a wall style

The ctor resolves the panel switch `0x662efc` and installs:

```text
[widget+0x60]+0x20 = LAB_00531b60
[widget+0x60]+0x00 = LAB_00531b80
```

```asm
00531b60 MOV EAX,[ESP+8]      ; new state
00531b64 TEST EAX,EAX
00531b66 JNZ  ret
00531b68 MOV EAX,[ESP+4]      ; widget
00531b6c LEA ECX,[EAX+0x20]   ; &widget->name  ("SWITCH_OBSERVATION_AREA")
00531b6f PUSH ECX
00531b70 MOV ECX,[EAX+0x5c]   ; -> fence state
00531b73 CALL 0x0052b760

00531b80 MOV EAX,[ESP+4]      ; widget
00531b84 MOV DL,[ESP+8]
00531b88 TEST DL,DL
00531b8a MOV ECX,[EAX+0x5c]   ; -> fence state
00531b8d JZ  0x0052bb10       ; unchecked -> clear price widget
00531b8f ADD EAX,0x20
00531b92 PUSH EAX
00531b93 CALL 0x0052b960      ; checked -> set price widget
```

State fields touched (`0x52b760` / `0x52b960` / `0x52bb10`):

| Offset | Meaning |
| ---: | --- |
| `+0x54` | feature (`*.bfi`) preview active — set to 0 by `0x52b760` |
| `+0x5c` | observation/building preview active — set to 1 by `0x52b760` |
| `+0x60` | **building definition index** in the 0x1f4 `BuildingList.ini` table |
| `+0x84` | `FENCE_FEATURE_PRICE` widget — written by `0x52b960`, cleared by `0x52bb10` |

`0x52b760(state, &widget->name)`:

1. `+0x54 = 0`, `+0x5c = 1`.
2. Scans the 0x1f4 table for the record whose string at **record+0x190**
   equals the widget name (`strcmp 0x597c30`), stores its index in `+0x60`.
3. Builds the preview rect from the selected enclosure (`+0x78`) and copies
   building fields `+0x0c`, `+0x10`, `+0x18c` into the ground preview.

`0x52b960(state, &widget->name)`:

1. Same lookup.
2. Formats `"%u"` of **record+0x24** into `+0x20`, copies to `+0x2c`, then
   `0x5bb940(FENCE_FEATURE_PRICE, ...)`.

`0x52bb10` clears the price widget (`text = ""`).

Click paths (`0x52b0d0`):

- `+0x48 == 0` -> two-click perimeter (`+0x4a`), uses `+0x50`/`+0x4c`.
- `+0x48 != 0` -> `0x52b242`; if `+0x54` (feature) the `.bfi` feature path;
  else if `+0x5c` (observation) a **single-point building placement**:
  `0x58a730(scene, point, building_def)` validate -> cost
  `[building_def+0x18]` -> budget -> `0x53b410(pwdWorldManager,
  building_index=+0x60, &point, +0x78)` creates a class 0x18 (building
  order) object (`obj+0x14/+0x18` = point, `+0x1c` = building index,
  `+0x20` = selected enclosure `+0x78`, `+0x30` = the other point arg).

Neither handler writes `+0x4c` or `+0x50`; the two-click wall margin/price
are untouched.

### Which building is it?

`buildinglist.ini` order: 0 `SmallSeats`, 1 `MediumSeats`, 2 `LargeSeats`,
3 `PointOfView`, ... The 0x1f4 loader `0x53fb00` writes the first tail key
string to record `+0x190` (`0x54070a..0x54076d` reads the 8 tail strings at
`+0x190, +0x19c, +0x1a8, ...`). Of the 39 `.bbi` files, **only
`PointOfView.bbi`** contains `SWITCH_OBSERVATION_AREA`:

```text
0x26a "DATA\graphics\TEXTURES\buildings"   -> record +0x178
0x2cc "SWITCH_OBSERVATION_AREA"            -> record +0x190
0x306 "&SwitchFenceObservationArea"        -> record +0x19c
```

So the switch resolves to **PointOfView**, index **3**:

```text
footprint 4 x 4 world units
construction cost (file 0x15 = def+0x18 = element+0x24) = 500
visitor price (file 0x19 = def+0x1c) = 5
category 5 (PointOfView)
```

`0x52b960` displays **500** in `FENCE_FEATURE_PRICE`; `0x58a730`/`0x53b410`
place a 4x4 PointOfView inside the selected enclosure.

The localized text confirms the wiring: `&SwitchFenceObservationArea` =
"Observation Area" (`ParkEng.loc` line 374); tutorial text (line 1992) says
the "Observation Post" button appears in the Enclosure Panel after the first
enclosure is built.

## 4. Panel text: `FENCE_FEATURE_PRICE`, `FENCE_NAME_INDICS`, `%u` and `%`

Ctor widget map (corrected: `FENCE_FEATURE_PRICE` is `+0x84`,
`FENCE_NAME_INDICS` is `+0xa0`):

```text
+0x84 FENCE_FEATURE_PRICE      +0x88 selected order object (0 initially)
+0x8c FENCE_CONSTRUCTION_AND_INFO_PANEL
+0x90 FENCE_STATS_DINO_QUANTITY_INDICS_BACK_CHIFFRE
+0x94 FENCE_STATS_OCCUPATION_INDICS_BACK_CHIFFRE
+0x98 FENCE_STATS_CARNI_DISTRIBUTION_INDICS_BACK_CHIFFRE
+0x9c FENCE_STATS_HERBI_DISTRIBUTION_INDICS_BACK_CHIFFRE
+0xa0 FENCE_NAME_INDICS
```

Writers:

| Widget | Writer | Value |
| --- | --- | --- |
| `FENCE_FEATURE_PRICE` (+0x84) | `0x52a970` (feature row checked) | `"%u"` of `.bfi` record `+0x1c` = file 0x19 price (`LunchBox` = **100**) |
| | `0x52b960` (observation checked) | `"%u"` of building `+0x24` = def+0x18 = construction cost (`PointOfView` = **500**) |
| | `0x52bb10` (observation unchecked) | empty string |
| `FENCE_NAME_INDICS` (+0xa0) | `0x5269e0` | `[paddock+4]` name string |
| 4 stat widgets (+0x90/+0x94/+0x98/+0x9c) | `0x5269e0` | `"%u"` + `"%"` of `0x57e720` stats |

`0x5269e0` is **not** the price/title formatter; it is the name+stats refresh
(called on enter when the selection is a class-0x17 order, and after a
perimeter apply). It:

1. `0x52bbb0` -> `+0x78 = selection`, `+0x48 = 1`.
2. `FENCE_NAME_INDICS = [paddock+4]` (`0x526a30..0x526a3d`).
3. scale = `0x58c460(park economy, 39600.0f)`; `0x57e720(paddock, scale,
   out[4])`.
4. four times: `sprintf("%u", out[i])`, append `"%"` (`0x663038`), set widget
   `+0x90/+0x94/+0x98/+0x9c` (`0x526a92..0x526d54`).

Concrete strings in the default wall mode:

- **Name**: the selected paddock's name; the default name key
  `&DefaultFenceName` (`0x666104`) localizes to **"Enclosure"**
  (`ParkEng.loc` line 1007). `FUN_00590440` initializes the name bank's fence
  slot from that key. With no enclosure selected, `0x5269e0` is not called
  and the widget keeps its previous text (empty on a fresh panel; the INI
  section has no `TEXT`).
- **Price**: nothing in the two-click path writes `FENCE_FEATURE_PRICE`; it
  stays empty. The cost is shown in the message line instead:
  `&MESSAGE_FENCE_PERIMETER_COST` (`0x66343c`) = **"Cost of enclosure: "**
  (`ParkEng.loc` line 1259), then `"%u"` of the `0x581b90` cost
  (`0x52ad9b..0x52af13`); e.g. `"Cost of enclosure: 450"`.
- **Stats**: four `"<n>%"` values (dino quantity / occupation / carni / herbi
  sufficiency widgets).

## Port implications (no code changed)

- Default enclosure constants: **margin 4**, wall style `Fence` (WallList
  index 0), **30 per perimeter cell**, cost
  `((x1+y1)-(x0+y0))*2/4*30` after the retail sort/max-1.
  A 16x16-unit (4x4-cell) enclosure costs **450**, not 1,500.
- The retail minimum is `dx >= 6*margin-1 = 23` units across each axis
  (`dx - 2*margin >= 4*margin-1`); with 4-unit-aligned corners that is a
  **6-cell minimum span** (the port currently accepts a 2-cell span).
- `SWITCH_OBSERVATION_AREA` should not be modelled as a fence style: it is a
  single-click `PointOfView` building (4x4, construction cost 500) placed
  inside the selected enclosure, with `FENCE_FEATURE_PRICE` showing 500.
- The two-click fence itself never writes `FENCE_FEATURE_PRICE`; its cost is
  the `"Cost of enclosure: <n>"` message line.

## Open / inferred

- The `.bwi` field at **file 0x10 = 30** is proven as the `0x581b90`
  multiplier, but its design label is not proven. `docs/formats/bwi.md`
  labels it "visibility/build time" and calls file 0x0c the construction
  cost; the perimeter formula does **not** read file 0x0c (that is config[3]).
  The cost class 10/15/30 is still used elsewhere (wall record/sell paths) —
  not re-traced here.
- The `bwi.md` evidence line "build path `0x0052b2dd`/`0x0052b342` passes
  wrapper+0x1c (= wall+0x0c)" is a **misattribution**: both addresses are in
  the `.bfi` feature branch of `0x52b0d0` (click `+0x48`/`+0x54`), where
  `EDI = feature element+0xc` and `[EDI+0x1c]` is the `.bfi` price
  (file 0x19). It is the LunchBox/bait price (100 for LunchBox), not a wall
  construction cost.
- `0x526930` compares the class id returned by the selection object's
  `vtable+4`. The object is the class-0x17 order created by `0x53ac60`; the
  class descriptor itself was not dumped, but the factory call and the
  comparison agree.
- The switch callback passes `widget+0x20` as the lookup string. Reading that
  as the overlay widget's **section key** (`SWITCH_OBSERVATION_AREA`) is
  inferred: the loader writes that key to record+0x190 and the retail lookup
  has to match for the button to work. No live dump of `widget+0x20` was made.
- The show-perimeter state (the branch that uses
  `&MESSAGE_SHOW_PERIMETER_COST`, state id `0xd`) was not located; it is the
  likely user of a non-zero style index (`Show.bwi` = 1).
- No retail run was made in this pass. A live build of one enclosure would
  confirm the 450 example and the 23-unit minimum; the data + x86 above are
  self-consistent without it.

## Evidence

- Ghidra: `0x5265b0` ctor (`0x526742`, `0x52675e`); `0x526930` enter;
  `0x5269e0` name/stats; `0x52a970` feature price; `0x52bb10` price clear;
  `0x52b760` observation preview; `0x52b960` observation price;
  `0x52b0d0` click; `0x52b242` feature/observation branch; `0x526f20` apply;
  `0x581b90` cost; `0x58a4e0` min-size; `0x58a730` observation validator;
  `0x53ac60` class-0x17 order; `0x53b410` class-0x18 order; `0x53d9a0`
  `.bwi` loader; `0x53ed30` `.bfi` loader; `0x53fb00` `.bbi` loader
  (`0x54070a..0x54076d` tail strings); `0x53d4d0`/`0x53a400`
  `pwdWorldManager` world-data load; `0x590440` default name bank.
- Data: `Data/Design/attractions/walls/WallList.ini` (24; index 0 `Fence`),
  `Fence.bwi` (0x00=4, 0x04=8.0f, 0x08=100, 0x0c=30, 0x10=30),
  `Data/Design/buildings/BuildingList.ini` (index 3 `PointOfView`),
  `PointOfView.bbi` (footprint 4x4, file 0x15=500, strings
  `SWITCH_OBSERVATION_AREA` / `&SwitchFenceObservationArea`),
  `Data/Overlay/ParkEng.loc` (Observation Area / DefaultFenceName Enclosure /
  "Cost of enclosure: "), `FreeGame.ini` (Budget 75000).
- Cross-refs: `notes/fence-construction-recon.md`,
  `notes/wall-raster-recon.md`, `docs/formats/bwi.md`, `docs/formats/bbi.md`,
  `docs/formats/bfi.md`.
