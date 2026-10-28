# Fence info panel + sell/remove — binary recon (2026-10-28)

Status: **click→panel path, the four stat formulas, the name edit/commit and the
whole sell/remove path are decoded from the binary.** Remaining uncertainty is
listed in §D (Open). Binary `/DinoIsland.exe-ebea17` (Ghidra MCP), all
addresses VA, Ghidra image base 0x00400000. No `src/` change.

Complements: `notes/fence-construction-recon.md` (tool state machine, cost,
wall raster), `notes/fence-wall-params-recon.md` (§4 panel text),
`notes/fence-selected-state-recon.md` (selected-wall pass, runtime capture),
`notes/fence-insertion-recon.md` (capacity), `notes/world-objects-recon.md`
(`drsFence` save layout), `notes/campaign-world-instantiation.md`
(`0x53d0b0` instantiation).

**Lane collision:** `notes/fence-selected-state-recon.md` and
`notes/fence-construction-recon.md` are clean; I edited neither. §C records
four corrections to them.

---

# 0. Short answers

| Question | Answer |
| --- | --- |
| Which state opens the info panel for a built enclosure? | **The fence construction state itself** — state id `0x0f`, class `pvwStateFenceConstruction` (`FENCE_CONSTRUCTION`), ctor `0x5265b0`, vtable `0x642b4c`. There is **no separate fence info state** (no `FENCE_SELECTED` string exists; its siblings `DINOSAUR_SELECTED`/`BUILDING_SELECTED`/`LUNCHBOX_SELECTED`/`CAGE_SELECTED`/`VISITOR_SELECTED` do exist). |
| Which panel? | `FENCE_CONSTRUCTION_AND_INFO_PANEL` (`0x663000`) — one ROOT panel that holds BOTH halves: `FENCE_STATS_PANEL` (info: name + 4 numbers) and `FENCE_CONSTRUCTION_PANEL` (title/price/LunchBox/Observation switches). |
| The four numbers | count / occupation % / carnivore food sufficiency % / herbivore food sufficiency % — exact formulas in §A4. |
| Sell UI | HUD switch button `OPEN_SELLING` ("Sell") at `(579,572)` 25×25 in 800×600 → state `0x14` `SELLING` → click a fence → `REQUEST_PANEL` confirmation → `0x533de0` removal. |
| Sell value | `0x58acc0(park+0x14, stFence)` = `stFence::GetSellValue()` (perimeter build cost **+ feature values**) **+** attached-actor construction costs. |
| Refund | `ftol(value * budget[+0x98])`, `budget = spPark(+0x120)`, `+0x98` = `[CONTEXT] SellBackFactor` (default 1.0; `FreeGame.ini` 1.0/0.5/0.2). |
| Non-empty gate | `stFence+0x98 != stFence+0x9c` → `&WARNING_SELLING_NON_EMPTY_FENCE`, no removal, state stays. |
| 16-edge invariant | **Does not exist.** The `>= 16` test in `0x527150` is on `stFence+0xd8..+0xdc` = the paddock's **LunchBox** list (max 15 feeding dishes), not a park wall vector. |

---

# A. Selection info panel

## A1. Click path (proven end to end)

The default park state is **id 5, name `NEUTRAL`** (ctor `0x539e60`,
name string `0x664efc`, vtable `0x642ea8`, object size 0x48):

| slot | address | role |
| --- | --- | --- |
| +0x00 | `0x539e80` | dtor |
| +0x04 | `0x539eb0` | enter |
| +0x08 | `0x53a0d0` | leave |
| +0x0c | `0x5391c0` | **update** (consumes the pending click) |
| +0x14 | `0x52be00` | **click** |
| +0x1c | `0x52be60` | keys |
| +0x28..+0x34 | `0x53b760`/`0x53b920`/`0x53be00`/`0x53c170` | extended hooks |

```asm
; 0x52be00  NEUTRAL::Click(arg)
0052be05 TEST EAX,EAX ; JNZ +0x1b
0052be0b MOV  ECX,[ESI+0x40]      ; the pick record
0052be0e CALL 0x0052caa0          ; run the six pick categories
0052be13 MOV  byte [ESI+0x38],1   ; pending-click flag
; 0x5391c0  NEUTRAL::Update()
005391c0 MOV  AL,[ECX+0x38]
005391c5 JZ   ret
005391c7 MOV  byte [ECX+0x38],0
005391cb CALL 0x0052bff0          ; dispatch by hit category
```

`FUN_0052bff0` is the shared **hit → state** dispatcher. It reads the pick
record (`state+0x40`) and tests six category blocks, each a `{u32 unused,
begin, end, cap}` vector whose `begin/end` are at `+0x04/+0x08`:

| block | flag bit | result state | state name |
| --- | --- | --- | --- |
| `+0x08` | `0x01` | `6` | `DINOSAUR_SELECTED` |
| `+0x1c` | `0x02` | `0x0d` | `CONSTRUCTION_KIT` (`0x663c98`) |
| **`+0x2c`** | **`0x04`** | **`0x0f`** | **`FENCE_CONSTRUCTION` (fence)** |
| `+0x3c` | `0x08` | `FUN_0052c5c0` | — (lunchbox/cage path) |
| `+0x4c` | `0x10` | `0x0b` | `BUILDING_SELECTED` |
| `+0x5c` | `0x20` | `0x0a` | `VISITOR_SELECTED` |

The fence branch (`0x52c10b..0x52c170`):

```asm
0052c10b MOV ECX,[EAX+0x30]      ; fence hit vector begin
0052c10e MOV EDX,[EAX+0x34]      ; end
0052c111 CMP ECX,EDX  ; JZ next
0052c115 TEST byte [EAX],0x4 ; JZ next
0052c11a MOV EAX,[EAX+0x30]
0052c11d MOV EDX,[EAX]           ; first id
0052c11f MOV EAX,[ESI+0x44]      ; the shared construction tool
0052c122 PUSH EDX
0052c123 MOV ECX,[EAX+0x8]       ; tool+0x8 = the graphics scene
0052c126 CALL 0x0053c2d0         ; scene registry lookup by id
0052c12b MOV EDX,[EAX]           ; object vtable
0052c12d MOV ECX,EAX
0052c12f CALL [EDX+0x10]         ; object -> selection id
0052c132 PUSH EAX
0052c137 CALL 0x00447b10         ; set the UI focus to that id
0052c14c MOV EDX,[ESI]           ; the current state's vtable
0052c150 MOV dword [ESI+0x18],0x0f   ; next state = 15
0052c157 CALL [EDX+8]                ; current state leaves
```

So the "selected object" is resolved by **`0x53c2d0(list, id)`**:

```c
int FUN_0053c2d0(int list, int id) {
  for (p = *(int**)(list+0x14); p != *(int**)(list+0x18); p++)
    if (*(int*)(*p + 0x10) == id) return *p;
  return 0;
}
```

with `list = [tool+0x8]` (graphics scene) and `*p` = scene object, id at
`sceneObject+0x10`. The resolved object's `vtable+0x10` result is stored as the
**UI focus** (`0x447b10`). The fence state's enter then reads it back with
`0x447b90`/`0x447c30` (see §A2), so `state+0x88` is the same object.

The pick itself: `FUN_0052caa0(pick) = [pick+0x18]` is the picker; for flag
bit 2 it calls `FUN_005f6070(picker, &result)` which walks the pickable vector
at `picker+0x50..+0x54` and calls `obj->vtable+0x1c(picker+0x20 /*ray*/,
result)` (the category blocks carry no ray themselves).

**Result:** clicking a fence post in `NEUTRAL` → state `0x0f`
`FENCE_CONSTRUCTION`, which *is* the info panel. (Runtime capture already
confirms the panel opens on a fence-post click: `fence-selected-state-recon.md`
§"Retail capture cross-check".)

### State table (from the factory `FUN_00529120`, ids read inside each ctor)

| id | ctor | name (string VA) |
| ---: | --- | --- |
| 5 | `0x539e60` | `NEUTRAL` (`0x664efc`) |
| 6 | `0x5394d0` | `DINOSAUR_SELECTED` (`0x664ee8`) |
| 8 | `0x5386d0` | `CAGE_SELECTED` (`0x664e30`) |
| 9 | `0x537040` | `LUNCHBOX_SELECTED` (`0x664ccc`) |
| 0x0a | `0x5391f0` | `VISITOR_SELECTED` (`0x664e40`) |
| 0x0b | `0x536160` | `BUILDING_SELECTED` (`0x664b40`) |
| 0x0c | `0x532590` | `CONSTRUCTION_MAIN` (`0x663cb4`) |
| 0x0d | `0x5314c0` | `CONSTRUCTION_KIT` (`0x663c98`) |
| 0x0e | `0x52ced0` | `PREDEFINED_SHOW` (`0x6635a8`) |
| **0x0f** | **`0x5265b0`** | **`FENCE_CONSTRUCTION` (`0x663024`)** |
| 0x10 | `0x52de80` | `ROAD_CONSTRUCTION` (`0x663784`) |
| 0x11 | `0x52f1a0` | `BUILDING_CONSTRUCTION` (`0x6639e4`) |
| 0x12 | `0x535b10` | `FENCE_INSERTION` (`0x6647bc`) |
| 0x13 | `0x534430` | `DINOSAUR_PURCHASE` (`0x6647a8`) |
| **0x14** | **`0x532ae0`** | **`SELLING` (`0x663cf4`)** |

`FUN_0052bff0` is called from the *update* of every construction state
(`0x52d5ee` PREDEFINED_SHOW, `0x52e9d1` ROAD_CONSTRUCTION, `0x530631` id 0x11,
`0x536ce6` BUILDING_SELECTED, `0x5391cb` NEUTRAL) and from the fence state's own
click (`0x52b0bd` → `FUN_0052bff0` after `0x52b0d0`), i.e. the same dispatcher
serves "click something else while a tool is open".

## A2. Panel, widgets and the fill routine

Ctor `0x5265b0` = `FUN_0052a6d0(0x0f, "FENCE_CONSTRUCTION", tool)` then resolves
widget objects (each `FUN_0040c040(name,0,1)` → `FUN_005bacd0` →
`FUN_004139d0`). Exact store addresses:

```text
0x52660e  +0x8c  FENCE_CONSTRUCTION_AND_INFO_PANEL   (0x663000)  <- panel root
0x526642  +0x84  FENCE_FEATURE_PRICE                 (0x662fec)
0x526676  +0xa0  FENCE_NAME_INDICS                   (0x662fd8)
0x5266aa  +0x90  FENCE_STATS_DINO_QUANTITY_INDICS_BACK_CHIFFRE      (0x662fa8)
0x5266d1  +0x94  FENCE_STATS_OCCUPATION_INDICS_BACK_CHIFFRE         (0x662f7c)
0x526712  +0x98  FENCE_STATS_CARNI_DISTRIBUTION_INDICS_BACK_CHIFFRE (0x662f48)
0x52673c  +0x9c  FENCE_STATS_HERBI_DISTRIBUTION_INDICS_BACK_CHIFFRE (0x662f14)
0x526742  +0x4c  = 0
0x52675e  +0x50  = [[[tool+0x8]+0x34]+0x6c]+0xc     (cell size 4)
```

Panel lifetime (this is the *only* show/hide):

```asm
; 0x5269c0  leave
MOV ECX,[ESI+0x8c] ; PUSH 0 ; CALL 0x005bbeb0     ; panel hidden
; 0x5269ee..0x5269f6  (inside the info fill)
MOV ECX,[EBP+0x8c] ; PUSH 1 ; CALL 0x005bbeb0     ; panel shown
```

`0x5bbeb0(widget, on)` is the widget show/hide (`vtable+0x4c` command `1`/`0`).

**The fill routine is `0x5269e0`** (`fence-wall-params-recon.md` §4 calls it
"name/stats refresh"; it is the whole info-panel fill). Its only two callers are
the enter `0x526930` (at `0x52697c`) and the successful perimeter apply
`0x526f20` (at `0x526ffe`) — **there is no per-frame refresh**, the numbers are
a snapshot taken when the panel opens.

```c
void opvFenceInfoFill(state) {          /* 0x5269e0 */
  FUN_0052bbb0(state);                  /* +0x78 = UI focus object, +0x48 = 1 */
  FUN_005bbeb0(state->panel_8c, 1);     /* show */
  focus = 0x447c30(0x447b90(...));      /* the wall object */
  state->+0x88 = focus;
  0x5bb940(state->nameWidget_a0, *(char**)(focus->+0x4c + 4));  /* stFence name */
  scale = 0x58c460( [[[[tool+0x10]+0x14]+0xc4]+4], 39616.0f );
  0x57e720(*(int*)(focus->+0x4c), scale, out4);   /* stFence::GetStats */
  set(+0x90, "%u"   out[0]);
  set(+0x94, "%u%%" out[1]);
  set(+0x98, "%u%%" out[2]);
  set(+0x9c, "%u%%" out[3]);
}
```

The widget stores are at `0x526ab6` (+0x90), `0x526b9a` (+0x94), `0x526c7e`
(+0x98), `0x526d4a` (+0x9c); `"%"` is `0x663038`, `"%u"` is `0x64d894`.
`[focus+0x4c]` = the **`stFence`** (set by `drsFence::Set`, §B3), so
`stFence+0x04` = name data, `stFence+0x0c` = name length.

### Panel layout (`ConstructionInterface.Ini`)

```text
[FENCE_CONSTRUCTION_AND_INFO_PANEL]  ROOT, X=550 Y=10 147x352, ALIGN_RIGHT/DOWN, VISIBLE=0
  [FENCE_STATS_PANEL]                0,0 143x200      <- the "info" half
    [FENCE_NAME_PANEL]               0,0 143x27   texture LAB_NAME_PANEL, HINT &Name
      [FENCE_NAME_INDICS] EDIT 7,5 128x14  TEXT_JUSTIFY=CENTER, HINT &Name
    [FENCE_STATS_DINO_QUANTITY_PANEL]     35,44  28x28  &FenceDinoQuantity
      ...[_INDICS_BACK_CHIFFRE]           65,47  43x20  (child 4,3 36x13)
    [FENCE_STATS_OCCUPATION_PANEL]        35,73  28x28  &FenceOccupationPercentage
    [FENCE_STATS_CARNI_DISTRIBUTION_PANEL]35,102 28x28  &CarniFoodSufficiency
    [FENCE_STATS_HERBI_DISTRIBUTION_PANEL]35,131 28x28  &HerbiFoodSufficiency
  [FENCE_CONSTRUCTION_PANEL]         15,176 113x174   <- the "construction" half
    title / top / mid1 / mid2 / bottom strips, [FENCE_FEATURE_PRICE] 18,143
    [SWITCH_FENCE_LUNCHBOX]  43,45  25x25
    [SWITCH_OBSERVATION_AREA] 43,98 25x25
```

Both halves live in the same root and are `VISIBLE=1`; the state never hides
one of them — the panel is shown/hidden as a whole. So the *same* panel is the
"construction tool" UI and the "selected paddock" UI, exactly as its name says.

## A3. `stFence::GetStats` (`0x57e720`) — the four values

`int out[4]; stFence::GetStats(this=ECX, float scale, out)`:

```text
out[0] = (*(int*)(fence+0x9c) - *(int*)(fence+0x98)) >> 2      ; occupant count
out[1] = (int) (*(float*)(fence+0x78) * 100.0f)                ; fill %  (0x6256b8=100)

herb = 0; carn = 0;
for each dino* d in [fence+0x98 .. fence+0x9c):                ; 4-byte elements
    v = scale * *(float*)(d + 0x6c);
    if (*(int*)(d + 0x200) == 1)  carn += v; else herb += v;    ; 1 = carnivore

foodHerb = 0; foodCarn = 0;
for each feature* f in [fence+0xd8 .. fence+0xdc):             ; the LunchBox list
    def = *(int*)(f + 0xb0);                                   ; .bsi food record
    if (!def) continue;
    n = (# of the 10 bytes at f+0x60 + i*8, i=0..9, that are 0)
    if (*(int*)(def + 0x0c) < 5) foodHerb += n * *(float*)(def+0x04) * *(float*)(def+0x00);
    else                         foodCarn += n * *(float*)(def+0x04) * *(float*)(def+0x00);

out[2] = (carn == 0) ? 100 : (int)(foodCarn * 100 / carn);
out[3] = (herb == 0) ? 100 : (int)(foodHerb * 100 / herb);
```

Evidence: the whole function disassembled at `0x57e720..0x57e899`; the two
`FADDP` variants select the accumulator (`diet==1` → `FADDP ST1,ST0` at
`0x57e78e`, else `FADDP ST2,ST0` at `0x57e792`); the guards are
`FCOMP 0x6254a0 (0.0f)`; the divides are `FMUL 100.0f` + `FDIV` + `__ftol`
(`0x5982d4`, `FISTP`).

Field semantics:

| field | meaning | evidence |
| --- | --- | --- |
| `stFence+0x98..+0x9c` | `vector<dinosaur*>` occupants (4-byte elems) | `0x57e8a0` AddActor push, `0x57e980` RemoveActor erase, `0x533312` sell gate |
| `stFence+0x78` | fill = used/capacity (0..1, float) | `0x57e8a0`/`0x57e980`: `[0x80] = used ± cost[dino+0x1e0]`, `[0x78] = used/[0x7c]` |
| `stFence+0x7c` | capacity (rect cells) | `0x57dc60` ctor |
| `stFence+0x84..+0x90` | cost table 8/16/32/64 by `dino+0x1e0` | ctor, `0x57e980` |
| `dino+0x6c` | per-dino **rate** used as the food demand term | used only as `scale*d`; the same offset is the need-decay rate in `dino-action-chooser-recon.md` §238 |
| `dino+0x200` | diet: 1 = carnivore, 0 = herbivore | `gen-recon.md` "diet (0 H / 1 C)"; `dinosaur-action-loop.md` `+0x200 == 0` |
| `stFence+0xd8..+0xdc` | `vector<feature*>` = the paddock's **LunchBox** dishes | written/read by `0x57e630` (removal, gated on tag `"LunchBox"` `0x66303c`) and `0x527150`/`0x527030` (cap) |
| `feature+0xb0` | the `.bsi` food-source record | `docs/formats/bsi.md` header: +0x00 nutrition, +0x04 quantity, +0x0c kind (0..4 vegetarian, 5..9 carnivorous) |
| `feature+0x60 + i*8` | 10 feeding slots; a **zero first byte** counts as one available portion | the `CMP byte [EAX],BL` loop at `0x57e7e3`; semantics Open (§D) |

Units: `out[0]` = animals, `out[1]` = percent of **capacity units** (not area),
`out[2]`/`out[3]` = percent; the "%" is appended by the UI, and an empty bucket
(the paddock has no herbivore / no carnivore) reads **100 %**.

### The `scale` argument

```asm
00526a42 MOV EDX,[EBP+0x44]      ; tool
00526a45 PUSH 0x471ab000         ; 39616.0f
00526a4a MOV EAX,[EDX+0x10]
00526a4d MOV ECX,[EAX+0x14]      ; spPark
00526a50 MOV EDX,[ECX+0xc4]      ; spPark+0xc4 = the 8-byte schedule pair
00526a56 MOV ECX,[EDX+0x4]       ; its second half (0x58c1c0 object)
00526a59 CALL 0x0058c460         ; x * *(float*)(this+0x14)
```

* `0x58c460(this, x) = x * *(float*)(this+0x14)`.
* `spPark+0xc4` is created in the spPark ctor `0x5874a0` as
  `FUN_00594b80(param_4, param_3)` (file `spScheduleManager.cpp`), which stores
  `[+0x04] = FUN_0058c1c0(*param_2, 9)` and `[+0x00] = FUN_0058c480([+0x04],
  param_3)`; `FUN_0058c480` itself calls `0x58c460`.
* So `scale = 39616.0f * *(float*)([[spPark+0xc4]+4] + 0x14)` = a schedule
  rate. The exact meaning of that float is **Open** (§D).

Worked example with the retail Cpg04 rect `(452,312)-(467,343)`
(`stFence+0x1c` rect, cell 4): capacity = `4*8 = 32`, cost units 8/16/32/64.

## A4. `FENCE_NAME_INDICS` — the paddock name

* Field: an mcString at **`stFence+0x00`** — `stFence+0x04` = `char*` data,
  `stFence+0x0c` = length. Read by the fill as `[[focus+0x4c]+0x4]`.
* Default: generated by `FUN_00590ac0(nameBank, stFence)` from
  `0x588d50` right after the ctor. Its base string is `[bank+0x18]`, seeded from
  `&DefaultFenceName` = **"Enclosure"** (`ParkEng.loc` 1007/1008); the routine
  appends `sprintf("%u", n)` and increments `n` until the name is unique among
  the names in the bank list at `[bank+0x5c]`. So the paddocks are
  "Enclosure1", "Enclosure2", … in creation order. If `stFence+0x04` already
  holds a name, that one is the base instead.
* **Edit/commit**: `FENCE_NAME_INDICS` is `TYPE = "EDIT"`. The ctor installs two
  event callbacks on the widget (`[widget+0x60]` = event table):

```text
0x5267f0 -> [widget+0x60]+0x24   ; 0x5267f0
0x526880 -> [widget+0x60]+0x04   ; 0x526880
```

  Both do the same copy and differ only in the trigger test:

```asm
; 0x5267f0(eventObj, evType, param)      commit when evType==0x0a && param==0x0d
005267fc CMP EAX,0xa   ; JNZ ret
00526801 CMP [ESP+0x10],0xd ; JNZ ret
00526808 MOV EDX,[EBP+0xa0]   ; FENCE_NAME_INDICS widget
0052680e MOV ECX,[EBP+0x88]   ; selected object
00526816 MOV EAX,[EDX+0x2c]   ; the EDIT text buffer
00526819 MOV ECX,[ECX+0x4c]   ; stFence
0052681f LEA ESI,[ECX+4]      ; &stFence->name.data
...      copy the buffer into stFence+0x04, set stFence+0x0c = length
00526862 MOV ECX,[EBP+0xa4] ; CALL [vtable+0xc]   ; refresh (see Open)
; 0x526880  same copy, entered when its byte param == 0 (focus-out/validate)
```

  So the name is committed on the EDIT's validate event `(0x0a, 0x0d)` and on
  the `+0x04` slot with a zero byte; both write `stFence+0x04/+0x0c`.
* **Not persisted**: `drsFence` save `0x5e8910` writes only the placed base
  (`object_id`, `field_14`, start/end, waypoints), `class_tag`, `field_14`,
  start/end, `field_44`, `field_48` — **no name** (see
  `world-objects-recon.md` §3 and the loader `0x5e93e0`). On load `0x588d50`
  runs `0x590ac0` again, so the names are regenerated from the same
  "Enclosure%d" counter in load order. A player rename survives only until the
  park is reloaded.

## A5. Open / close

| action | code | effect |
| --- | --- | --- |
| open (click a paddock) | `0x52be00`→`0x5391c0`→`0x52bff0`→state `0x0f`→`0x526930`→`0x5269e0` | `0x5bbeb0(panel,1)` shows the panel |
| open (construction tool) | `OPEN_CONSTRUCTION_FENCE` → same state; enter only fills when the focus is a `drsFence` | same |
| close `Esc` | `0x52be60` (keys): `Esc` → `[state+0x18] = 5` + `vtable+8` (`0x5269c0` → `0x5bbeb0(panel,0)`) | panel hidden, back to `NEUTRAL` |
| close by clicking empty ground | `0x52b0d0` arg==0 → … ; or `FUN_0052bff0` last branch `if (state->+0x04 != 5) { state->+0x18 = 5; leave }` | panel hidden |
| close on sell / putback | the sell confirmation deletes the paddock; `PutBack` (`PUTBACK_PANEL_BUTTON`) leaves the state | panel hidden by leave |

`state->+0x04` is the *current* state id (set by `0x5e4540`) and `state+0x18`
is the id the state machine switches to after the slot-`+8` call — so the
"next state" mechanism is `[+0x18] = id; call vtable[2]`.

**Side effect worth knowing for the port:** the info fill calls `0x52bbb0`
first, which sets `state+0x48 = 1` (feature-placement mode) and
`state+0x78 = focus`. The click handler tests `[ESI+0x48]` *before* the
perimeter logic (`0x52b0f3`), so a click that follows the info-panel open takes
the feature branch. Whether retail intends this (a "the paddock is the target"
hint) or it is a bug is **Open**; it is a faithful behaviour to reproduce if
the port wants parity.

---

# B. Sell / remove a fence

## B1. UI path (proven)

`notes/fence-construction-recon.md` §"Selling a fence" described the *handler*;
the trigger is the HUD, not the fence panel:

```text
ParkInterface.Ini
  [GAUGE_PANEL]            ROOT  X=69  Y=569 618x31  ALIGN_RIGHT/DOWN
    [WARNING_INFO_PANEL]   503,0 46x31  TEXTURE WARNING_INFO_PANEL_IMAGE
      [OPEN_SELLING]       7,3 25x25  HINT "&OpenSelling" ("Sell")
                           BTN_SUB_TYPE = SWITCH, BTN_START_STATE = UP
```

Absolute screen rect **`(579,572)` 25x25** (centre 591,584) at 800x600 —
matches `docs/platform/ui-coordinates.json` (`park-hud / OPEN_SELLING`).

Wiring: the park interface ctor `FUN_00527d30` resolves `OPEN_SELLING`
(`0x663248`) at `0x5280f1..0x528135` and installs its `+0x24` event callback
`0x528f90`:

```asm
; 0x528f90 (switch ON, arg2 == 2 and button id 0x2d)
00528fb5 MOV ECX,[ESI+0x10]        ; the state machine
00528fb8 MOV [ECX+0x18],0x14       ; next state = 20 SELLING
00528fc2 CALL [EDX+8]              ; transition
; (switch OFF, arg2 == 1)

00529045 MOV EDX,[ESI+0x10]
00529048 MOV [EDX+0x18],0x5        ; back to NEUTRAL
00529054 CALL [EAX+8]
```

State `0x14` `SELLING` (ctor `0x532ae0`, vtable `0x642d30`):

| slot | address | role |
| --- | --- | --- |
| +0x00 | `0x532b90` | dtor |
| +0x04 | `0x532c40` | enter |
| +0x08 | `0x532c90` | leave |
| +0x0c | `0x532cc0` | **click/update** (the whole sell dispatcher) |

```c
/* 0x532c40 enter */
st->+0x90 = *([tool+0xc] + 0x40);
st->+0x94 = *(*([tool+0x10] + 0x14) + 0x120);   /* spPark+0x120 = the budget */
FUN_005c0430(st->panel, 2);                     /* OPEN_SELLING switch = DOWN */
FUN_0052cc20();
**(int**)(tool + 0x50) = 2;                     /* input mode 2 */
/* 0x532c90 leave */
FUN_005c0430(st->+0x8c, 1);   /* switch back UP */
FUN_005f8830(st->+0x90);
**(int**)(tool + 0x50) = 0;
```

`0x532cc0` is the shared sell dispatcher for **dino / feature / show / fence /
building / road** (one category per pick block). The fence one is block 2
(`pick+0x30..0x34`), entered at `0x532ee`, and builds
`&REQUEST_SELL_FENCE` ("Sell this enclosure for " + `"%d"` refund) into
`state+0x74`, then shows the confirmation:

```asm
00533496 PUSH 0 ; PUSH 0
0053349a PUSH EBP              ; the state
0053349b PUSH 0x533a70         ; <-- the fence "validate" callback
005334a0 PUSH EBX              ; &state+0x74 (the text)
005339e5 MOV EDX,[EBP+0x44]
005339e8 MOV ECX,[EDX+0x4c]    ; tool+0x4c = the request-panel manager
005339eb CALL 0x005e3ba0       ; 0x5e3ba0(mgr,text,cb,state): show REQUEST_PANEL
```

`0x5e3ba0(mgr,text,cb,state,0,0)` = `0x5e3d70(text,0,0)` (show) +
`[mgr+0x1c] = cb`, `[mgr+0x24] = state`. The dialog is `[REQUEST_PANEL]`
(`ParkInterface.Ini:1818`) with `[REQUEST_VALIDATE_ICON]` at screen
`(370,356)` 28x28 (centre 384,370) and `[REQUEST_CANCEL_ICON]` at `(400,356)`
(centre 414,370).

Validate → `0x533a70` → `JMP 0x533de0` (the removal, §B3). Cancel → no
callback, the state stays.

The other categories for reference (all in the same function):

| branch | message | request callback → removal |
| --- | --- | --- |
| `0x532d9c` dino | `&REQUEST_SELL_DINO` `0x663df8` | `0x533a00` (credit → `0x53a900` delete dino → `0x53b3d0`) |
| `0x532ea7` | `&WARNING_SELLING_UNAVAILABLE_DINO` `0x663dd4` | — |
| `0x532fc9` feature | `&REQUEST_SELL_FEATURE` `0x663dbc` | `0x533a50` → `0x533ac0` |
| `0x5330e2` | `&WARNING_SELLING_UNAVAILABLE_FEATURE` `0x663d94` | — |
| `0x53318e` show | `&REQUEST_SELL_SHOW` `0x663d80` | `0x533a60` → `0x533f30` (per-waypoint `0x53b250`) |
| `0x5332ab` | `&WARNING_SELLING_RUNNING_SHOW` `0x663d60` | — |
| **`0x533326` fence** | **`&REQUEST_SELL_FENCE` `0x663d4c`** | **`0x533a70` → `0x533de0`** |
| `0x5334a6` | `&WARNING_SELLING_NON_EMPTY_FENCE` `0x663d28` | — (returns) |
| `0x5335e1` building | `&REQUEST_SELL_BUILDING` `0x663d10` | `0x533a80` → `0x534040` |
| `0x533820` road | `&REQUEST_SELL_ROAD` `0x663cfc` | `0x533a90` → `0x5341b0` |

## B2. Base value and refund

`0x58acc0(economy, object)`:

```c
int FUN_0058acc0(int this /* = spPark+0x14 */, int *object) {
  int value = object->vtable[+0x14]();                 /* (A) */
  for (each rec in *(int**)(this+0xb0) .. *(int**)(this+0xb4))
     if (*(int*)(*rec + 0xc4) == object)                /* attached to this fence */
         value += *(int*)(*(int*)(*rec + 0x40) + 0x18); /* (B) its construction cost */
  return value;
}
```

**(A) `stFence::GetSellValue`, vtable `0x6432ac` slot `+0x14` = `0x581710`:**

```asm
00581710 MOV EDX,[ECX+0x1c]        ; the paddock rect object
00581715 MOV EAX,[EDX+0x10]
00581718 MOV ESI,[EDX+0x20]        ; y0
0058171b MOV EAX,[EAX]
0058171d MOV EAX,[EAX+0x18]
00581720 MOV EAX,[EAX]
00581722 MOV EDI,[EAX+0x24]        ; -> the wall definition
00581725 MOV EAX,[EDX+0x1c]        ; x0
00581728 ADD ESI,EAX               ; x0 + y0
0058172a..00581731 NEG/SHL31/SUB   ; = -(x0+y0)
00581733 MOV ESI,[EDX+0x28]        ; y1
00581736 ADD EAX,ESI
00581738 MOV ESI,[EDX+0x24]        ; x1
0058173b ADD EAX,ESI               ; (x1+y1) - (x0+y0)
0058173f SHL EAX,1
00581741 DIV dword [EDI]           ; / cell size      ([EDI]   = 4)
00581746 IMUL EAX,[EDI+0x10]       ; * price per cell ([EDI+0x10] = 30)
0058174a..00581764                 ; for each feature: += [[feat+0x28]+0x1c]
```

```text
(A) = ((x1 + y1) - (x0 + y0)) * 2 / cell * pricePerCell   ; the perimeter cost,
    + Σ over stFence+0x34..+0x38 of  [[feature+0x28] + 0x1c]
```

* `cell` / `pricePerCell` are the `Fence.bwi` fields at file `0x00` = 4 and
  file `0x10` = 30 (`docs/formats/bwi.md`, `fence-wall-params-recon.md`).
* The rect lives in the object at `stFence+0x1c`: `+0x1c` x0, `+0x20` y0,
  `+0x24` x1, `+0x28` y1 — the **same four ints** the sell's ground-removal loop
  walks in 12-unit steps (§B3) and the same ones the build cost sorts
  (`0x52bbf0`).
* The feature addend uses the *feature record's* `+0x1c` (the `.bfi` price,
  `fence-wall-params-recon.md` §4: LunchBox = 100).
* Worked example, the Cpg04 paddock `(452,312)-(467,343)`:
  `((467+343)-(452+312))*2/4*30 = 46*2/4*30 = 690`, + 100 per LunchBox.

**(B)** the attached-actor term: every record in `spPark+0xb0..+0xb4` whose
`[record+0x00]+0xc4 == stFence` contributes `[[record+0x40]+0x18]`. The same
`+0xc4 == stFence` link is used by the graphics deletion in `0x53abc0`
(§B3), so this is the "everything built on this paddock" value (the
`PointOfView` observation area = 500 is the obvious case,
`docs/formats/bbi.md`: def+0x18 = construction cost).

**Refund** (identical in the UI display and in the credit):

```asm
; display  0x5333db..0x533409
005333db MOV EDX,[EBP+0x54]        ; the drsFence
005333e1 MOV EAX,[EDX+0x38]        ; drsFence+0x38 = the stFence
005333e4 MOV EDX,[ECX+0x10]        ; spPark
005333e8 MOV ECX,[EDX+0x14]        ; spPark+0x14 = the economy
005333eb CALL 0x0058acc0
005333fa FILD [ESP+0x14]           ; (float)(int)value
005333fe FMUL float [EAX+0x98]     ; * budget->+0x98  (EAX = [EBP+0x94] = spPark+0x120)
00533404 CALL 0x005982d4           ; __ftol
00533410 PUSH 0x64c6cc             ; "%d"
```

```c
/* 0x585c40  the credit */
ECX = budget;  EAX = __ftol((float)*(int*)arg * *(float*)(ECX+0x98));
[ECX+0xb4]++;  [ECX+0xb8] += EAX;  [ECX+0x8] += EAX;  [ECX+0x4] = 1;
```

`spPark+0x120` is the budget object (`FUN_00585150`, 0x224 bytes, created in the
spPark ctor `0x5874a0`), fields: `+0x08` cash, `+0x98` **sell-back factor**,
`+0xb4` number of sales, `+0xb8` total sale income, `+0x04` dirty.
`0x585bd0(budget, cash, factor)` sets `+0x08`/`+0x98`.

Source of the factor: `[CONTEXT] SellBackFactor` in the scenario INI
(`FreeGame.ini`, `MissionBase.ini`): `FUN_00409510("CONTEXT", key, 1.0f)` in
`FUN_00561bb0` (`0x5620f0`, difficulty 0/1/2 → `SellBackFactor` /
`…Medium` / `…Hard`) writes it through `0x585bd0`. **Default 1.0**;
Free Game 1.0 / 0.5 / 0.2 (easy/normal/hard).

## B3. Removal (`0x533de0`) — exact order

`0x533a70` = `ECX = arg; JMP 0x533de0` (the request dialog passes the state).

```c
void FUN_00533de0(SELLING_state *st) {       /* EDI = st */
  drsFence *sel = st->+0x54;
  stFence  *f   = sel->+0x4c;

  /* 1. remove the feeding dishes' food-source actors */
  for (each feature in *(int**)(f+0x30) .. *(int**)(f+0x38)) {   /* 4-byte ptrs */
      if (0x405740(*(int*)(*(int*)(feature+0x28)+0x28), "Lunchbox" /*0x6630d0*/) == 0)
          0x53c4d0([tool+0x8] /*scene*/, feature);
  }
  /* 2. CREDIT THE BUDGET (refund, see B2) */
  value = 0x58acc0(*(int*)(spPark+0x14), sel->+0x38 /* = the stFence */);
  0x585c40(*(int*)(spPark+0x120) /*budget*/, value);
  /* 3. delete the graphics + the world object */
  0x53abc0([tool+0x8] /*scene*/, sel);
  /* 4. de-stamp the ground one 12-unit cell at a time */
  rect *r = *(rect**)(f + 0x1c);                 /* f->+0x1c = the rect object */
  for (i = r->+0x20 (y0); i < r->+0x28 (y1); i += 12)
    for (j = r->+0x1c (x0); j < r->+0x24 (x1); j += 12)
        0x53b400(scene, &(float){i, j});
}
```

Step-by-step evidence:

1. `0x533df4..0x533e57`: `ESI = stFence+0x30`, so `begin = stFence+0x34`,
   `end = stFence+0x38`, stride 4; class tag via `[[feature+0x28]+0x28]`;
   compare with `0x6630d0` ("Lunchbox"); call `0x53c4d0(scene, feature)`.
   `0x53c4d0` walks `scene+0x70..+0x74`, finds the object whose definition
   category `[[obj+0x28]+0x40]+0x6c == 0x0b` and whose `[[obj+0x28]+0xc8] ==
   feature` (the `.bsi` food-source actor, cf. `employee-ai-recon.md` §"Kind
   0xb"), selects it (`0x447b10`), queues the delete (`0x447c20`), clears the
   focus (`0x447b30`), then `0x520810([[obj+0x28]])` + `0x41b360`.
2. `0x533e59..0x533e77`: value + `0x585c40` credit — **before** any deletion.
3. `0x533e7c..0x533e86`: `0x53abc0(scene, sel)`:
   ```c
   for each obj in scene+0x70..+0x74:
       g = obj->+0x28; kind = g->def(+0x40)->+0x6c;
       if ((kind == 5 || kind == 6) && g->+0xc4 == sel->+0x38)
           { 0x447b10(obj->vtable+0x10()); 0x447c20(); 0x447b30(); }  /* delete */
   0x447b10(sel->vtable+0x10()); 0x447c20(); 0x447b30();               /* delete sel */
   ```
   `0x447c20` = `0x41b240(selection->+0x2c, selected)` = **push into the
   deletion queue** (not an immediate free); the actual virtual destructors run
   when the queue is flushed.
4. `0x533e8b..0x533f1f`: `r = *(int**)(sel+0x4c) + 0x1c`… as quoted above, then
   `0x53b400(scene, &pt)` → `0x5f6a30(scene, pt)` → `0x5fcc70(scene, pt, 0)`
   (`gpkEffectManager.cpp`) per 12-unit cell.

### What frees the grid / walls

* The `drsFence` dtor `0x5e87c0` frees **only** its `+0x28` string and calls the
  base dtor `0x5e7300`; it does **not** free `+0x4c` (the `stFence`), `+0x50`
  (the ground `gatAttraction`), `+0x54` (the actor) or `+0x38`. So deleting the
  world object alone does not clear the occupancy grid.
* The `stFence` lives in spPark: `0x588d50` pushes it into **both**
  `spPark+0x50..+0x58` and `spPark+0xa0..+0xa8` (`spPark.cpp:0x271`). Its dtor
  chain is base `0x580f00` → `0x584090` (`stPerimeter`, vtable `0x643530`:
  dtor `0x5841a0`), which calls `0x582180`:

  ```text
  stPerimeter dtor 0x5841a0 -> 0x582180
    for each wall* in perimeter+0x10..0x14:
        0x5845c0(wall)          ; "destroy the cells", then resets wall+0x14's
                                ; vtable and frees the vector at wall+0x18
    vtable resets, vector frees
  0x5845c0(wall)
    for each cell* in wall+0x18..0x1c:
        0x584850(cell)          ; cell+0x1c = 2 ; 0x5849a0() ; free the (x,y) list
        free cell, null the slot
  0x584850(cell) -> 0x5849a0(cell)
    0x5849a0: flag = cell+0x1c
              cells = cell+0x08..0x0c    (8-byte {x,y})
              grid  = [[cell+0x20]+0x10]
              flag 0/1 : u16 = (u16 & 0x0fff) | (DAT_006757b4 << 12)   ; stamp
              else     : u16 &= 0x0fff                                 ; CLEAR
  (the wall sub-object dtor 0x5846a0 only frees the now-empty cell vector)
  ```

  So the occupancy nibble is cleared **only** when the `stFence` (hence its
  `stPerimeter` walls) is destroyed, per cell, with `0x5849a0` taking the
  `flag != 0/1` branch. `0x5841a0` → `0x582180` → `0x5845c0` is the only
  reachable unstamp path (customers of `0x5849a0` are exactly `0x584200` build
  and `0x584850` clear).

* The **ground quad is part of the fence graphic**, not a separate object:
  `gatAttraction` ctor `0x5ec7e0` builds the 2-texture quad from the rect, and
  `gatFence` (`0x623640`) is pushed into `scene+0x50` and `scene+0xac`
  (`notes/fence-ground-beam-preview-recon.md` §1.2). Deleting the kind-5/6
  actors therefore removes mesh + beams + sand floor together.
* Dirty flags: `0x5f4e70` (AddAttraction) sets `scene+0xec = 1` and the render
  pass `0x5f5450` recomposes the basemap/landscape (`0x5fc3a0` compose,
  `0x5fb210` second manager) then clears it (same note, §1.4). The sell path
  does **not** set it itself; the removal path's scene calls / next frame do.
  There is **no** separate minimap flag in this path (the minimap is a render
  view, `MAP_RENDERVIEW`).
* Budget: credited **before** the deletions (§B3 step 2).

### Dinosaurs

Removal frees **nothing** and blocks on nothing beyond the gate: a fence with a
non-empty `+0x98` vector is never removed (§B4). There is no "orphan the dinos"
branch in `0x533de0`; the only dino handling in the whole function is the
`feature` loop of step 1 (food sources). Selling a dino (`0x533a00`) is the
mirror image: `0x585c40` credit → `0x53a900` (delete the dino) → `0x53b3d0`
(remove the scene effect at the dino's point).

## B4. The non-empty gate

```asm
; fence branch, 0x53330f
MOV EAX,[EAX+0x4c]        ; drsFence+0x4c = stFence
ADD EAX,0x98
MOV ECX,[EAX]   ; begin
MOV EDX,[EAX+4] ; end
CMP ECX,EDX
JNZ 0x5334a6              ; NOT EMPTY -> warning path
; ... empty: build &REQUEST_SELL_FENCE, ask, refund, remove
```

`0x5334a6..0x53356b` = localise `&WARNING_SELLING_NON_EMPTY_FENCE`
(`0x663d28`, text **"The enclosure cannot be sold: It still contains
dinosaurs."**), copy into `state+0x74`, then
`0x5e3d70(msg,0,0)` with `ECX = [tool+0x48]` (the message panel) and `RET 4` —
**no state change, no removal, no refund**; the sell mode stays armed.

Same gate reused by `stFence::CanAddActor`/`RemoveActor` for capacity
(`0x57eb50`/`0x57e980`).

## B5. Which attached actors go away

| actor | removed by | link test |
| --- | --- | --- |
| fence graphic + wall meshes + light beams | `0x53abc0` → kind 5/6 | `sceneActor->+0x28` (`the game object`) `+0xc4 == stFence` |
| paddock sand floor (`gatAttraction` quad) | same (it is the same `gatFence` object) | — |
| ground sand floor of a **loaded** paddock | `0x53b400`/`0x5f6a30`/`0x5fcc70` per 12-unit cell | rectangle from `stFence+0x1c` |
| `.bsi` food-source actors (kind `0x0b`) | `0x53c4d0` per feature with tag `"Lunchbox"` | `[[obj+0x28]+0xc8] == feature`, `def+0x6c == 0x0b` |
| the `LunchBox` `.bfi` dish itself | **not in `0x533de0`** | — (Open, §D) |
| the `PointOfView` observation-area `drsBuilding` | **not in `0x533de0`**; it is only *valued* in §B2(B) | link `+0xc4 == stFence` |
| the `stFence` (walls + occupancy cells + dino list + dish list) | the deferred deletion of the `drsFence` is **not** enough (`0x5e87c0` does not free it); the `stFence` must come out of the spPark lists | Open (§D) |

`0x53acd0` / `0x53b0d0` are the **creators** of the `.bfi` feature actor and of
the AnimalBait actor; the sell path does **not** call them or their inverse
pair. It calls `0x53c4d0` (food source) and `0x53abc0` (graphics) only.

## B6. The `>= 16` invariant — correction

`0x527150` (the perimeter-preview / validate hook, vtable `+0x30`):

```asm
0052715a MOV EDI,[ESP+0x14c]      ; the new feature record
00527167 MOV EAX,[EDI+0x28]       ; its class tag
0052716e MOV ECX,0x66303c ; compare  -> "LunchBox"
00527195 MOV EAX,[EBP+0x88]       ; the selected drsFence
0052719b MOV ECX,[EAX+0x4c]       ; stFence
0052719e ADD ECX,0xd8             ; stFence+0xd8 = the LunchBox vector
005271a9 SUB EDX,ESI ; SAR EDX,2
005271ae CMP EDX,0x10
005271b1 JC  0x5271c5
005271b6 MOV EAX,0x6 ; RETURN      ; silently blocked, no warning string
```

`0x527030` (state vtable `+0x28`, invoked when a fence-feature row is acted on)
does the same test on the `.bfi` record class tag (`[[tool+0x8]+0x34]+0x7c +
index*0x1d8 + 0x34`) and shows `&WARNING_TOO_MANY_LUNCHOXES_IN_FENCE`
(`0x663048`, "There are too many feeding dishes in the enclosure!"); the same
string is used from the click path at `0x52b472..0x52b4a7`.

So the invariant is **"at most 15 LunchBoxes per paddock"** on
`stFence+0xd8..+0xdc`, *not* a park wall-segment vector. There is **no**
16-edge limit on the perimeter itself, and nothing in the sell path touches a
wall-count. (`fence-construction-recon.md` §"Perimeter preview" attributed
this to `park+0xd8`; the base register is `stFence`, §C3.)

---

# C. Corrections to the earlier fence notes

1. **`0x5269e0` is not a "title/price formatter".** It is the full info-panel
   fill: show panel, resolve the focus, set `FENCE_NAME_INDICS`, compute and set
   the four stat widgets. `FENCE_FEATURE_PRICE` is written by
   `0x52a970`/`0x52b960`/`0x52bb10` only (`fence-wall-params-recon.md` §4 has
   this right; `fence-construction-recon.md` §"Enter / refresh" does not).
2. **`+0x50` is not "the selected feature footprint size / margin" from the
   ctor args**: `0x52675e` stores `[[[tool+0x8]+0x34]+0x6c]+0xc` (the cell size,
   4). The `+0x50` read in `0x52b0d0` is that same 4.
3. **The 16-entry cap is on `stFence+0xd8`, not `park+0xd8`** (§B6), and it caps
   LunchBoxes (max 15), not wall edges.
4. **`FENCE_STATS_*` widget/offset map** (`fence-construction-recon.md` §"Bound
   panel widgets") is correct, but the *values* are assigned count / occupation /
   **carni** / **herbi** in that order — `+0x98` = CARNI, `+0x9c` = HERBI
   (`0x526c7e`, `0x526d4a`). Both are **food sufficiency**, not distribution
   counts, and an empty bucket reads 100.
5. **Selection is not handled by a separate info state or by `0x53c2d0` alone**:
   `0x53c2d0` is a generic registry lookup; the *state switch* is
   `FUN_0052bff0` (tabulated in §A1).

---

# D. Open

1. **Who destroys the `stFence`.** `0x533de0` queues the `drsFence` for deletion
   but the `drsFence` dtor `0x5e87c0` frees neither `+0x38` (stFence), `+0x3c`,
   `+0x50` (ground) nor `+0x54`. The `stFence` sits in spPark's `+0x50` and
   `+0xa0` vectors; the erase side of `0x588d50` was not located. Until it is,
   the *unconditional* grid unstamp (`0x5841a0`→`0x582180`→`0x5845c0`→
   `0x5849a0`) is proven as the mechanism but its trigger in the sell flow is
   not. Runtime check: build a paddock, sell it, read the occupancy `u16` of a
   wall cell (expected `nibble` cleared) and `spPark+0x50`/`+0xa0` counts.
2. **`state+0xa4`** in the name-commit callbacks (called as `[+0xa4]->vtable+0xc`
   after the rename) is never written by the fence ctor (object size 0xa8,
   `0x529470`), so it is either zero-filled (retail would fault on a rename) or
   set by a path not found. Needs a live rename (type a name, press Enter) with
   a breakpoint on `0x5267f0`.
3. **`feature+0x60 + i*8` "10 slots"**: proven to be a 10-entry, 8-byte-stride
   array whose zero first byte counts as an available portion
   (`0x57e7e3..0x57e7ec`); the writer and the exact portion semantics were not
   traced.
4. **The `scale` rate**: `39616.0f * [scheduleObj+0x14]`, `scheduleObj =
   [[spPark+0xc4]+4]` (`spScheduleManager.cpp`, `0x594b80`/`0x58c1c0`). The
   meaning of `+0x14` (and hence the absolute sufficiency numbers) is open. A
   runtime read of `[[spPark+0xc4]+4]+0x14` on a real park would close it.
5. **Which actors carry `+0xc4 == stFence`** (the set deleted by `0x53abc0` and
   valued by `0x58acc0(B)`): read live. The candidates are the `gatFence`, the
   feature graphics and the `PointOfView` building.
6. **Does the `.bfi` LunchBox dish leave with the paddock?** `0x533de0` only
   removes the kind-0xb food source; the dish actor (if it is not a kind 5/6
   actor with `+0xc4 == stFence`) would leak. Live check: place a LunchBox,
   sell the empty paddock, count the scene actors.
7. **`state+0x48 = 1` side effect** of the info fill (§A5): retail parity for
   the click *after* opening the panel.
8. **`0x53c2d0` registry element class**: `list = [tool+0x8]`, vector
   `+0x14..+0x18`, id at element `+0x10`. The `drsFence` id is at `+0x14`, so
   the registry element is probably the scene object, not the drs object; the
   exact class was not dumped.

---

# E. Evidence

Ghidra (`DinoIsland.exe-ebea17`):

- Click/panel: `0x539e60` NEUTRAL ctor (id 5, `NEUTRAL` `0x664efc`, vtable
  `0x642ea8`), `0x52be00` click, `0x5391c0` update, `0x52caa0` pick runner,
  `0x5f6070` fence pick category, `0x52bff0` dispatcher (fence branch
  `0x52c10b..0x52c170`, state `0x0f` at `0x52c150`), `0x53c2d0` registry
  lookup; `0x5265b0` fence state ctor, `0x526930` enter, `0x5269c0` leave,
  `0x5269e0` info fill, `0x5bbeb0` widget show/hide, `0x5bb940` set text,
  `0x5267f0`/`0x526880` name commit, `0x590ac0` name bank, `0x590440` default
  name bank.
- Stats: `0x57e720` (formulas), `0x58c460` scale, `0x58c1c0` schedule object,
  `0x594b80` schedule pair, `0x5874a0` spPark ctor (`+0xc4`, `+0x120`,
  `DAT_006757b4 = 7`), constants `0x6256b8 = 100.0f`, `0x6254a0 = 0.0f`,
  `0x62544c = 0.5f`.
- stFence fields: `0x57dc60` ctor, `0x57e8a0` AddActor, `0x57e980` RemoveActor,
  `0x57e630` remove feature, `0x581710` GetSellValue (vtable `0x6432ac` slot
  `+0x14`), `0x57e720` GetStats.
- Sell: `0x528f90` OPEN_SELLING callback, `0x532ae0`/`0x532c40`/`0x532c90`/
  `0x532cc0` SELLING state, `0x533326` fence branch, `0x5333db` refund display,
  `0x533496` request show, `0x533a70`/`0x533de0` removal, `0x5334a6` non-empty
  warning, `0x5e3ba0`/`0x5e3d70` request panel, `0x447c20`→`0x41b240` deletion
  queue, `0x53abc0` graphics delete, `0x53c4d0` food-source delete,
  `0x53b400`→`0x5f6a30`→`0x5fcc70` ground effect.
- Value: `0x58acc0`, `0x585c40`/`0x585bd0`, `0x585cc0`/`0x585d20` budget
  categories, `0x5874a0`.
- Grid removal: `0x5841a0` (stPerimeter dtor) → `0x582180` → `0x5845c0` →
  `0x584850` → `0x5849a0`; `0x584200` build; `0x584700` cell ctor.
- Instantiation/links: `0x53d0b0`, `0x5e8820` (`drsFence::Set`: `+0x4c` stFence,
  `+0x50` ground, `+0x54` actor, `+0x38` stFence, `+0x3c` actor, `+0x40`),
  `0x5e93c0`, `0x5e72e0`, `0x5e87a0`/`0x5e87c0` (drsFence ctor/dtor),
  `0x5e87c0` does not free the stFence.

Data:

- `Data/Overlay/ConstructionInterface.Ini`: 1447 `FENCE_CONSTRUCTION_AND_INFO_PANEL`,
  1461 `FENCE_STATS_PANEL`, 1487 `FENCE_NAME_INDICS` (`TYPE = EDIT`),
  1558/1586 quantity, 1604/1632 occupation, 1649/1677 carni, 1694/1722 herbi,
  1740 `FENCE_CONSTRUCTION_PANEL`, 1842 `SWITCH_FENCE_LUNCHBOX`,
  1864 `SWITCH_OBSERVATION_AREA`, 1876 `FENCE_FEATURE_PRICE`.
- `Data/Overlay/ParkInterface.Ini`: 570 `OPEN_PADDOCK_LIST_PANEL`,
  615 `WARNING_INFO_PANEL`, 630 `OPEN_SELLING` (SWITCH), 1818 `REQUEST_PANEL`,
  2038 `REQUEST_VALIDATE_ICON`, 2051 `REQUEST_CANCEL_ICON`, 663 `MAP_PANEL`.
- `Data/Overlay/ParkEng.loc`: 263/264 `&OpenSelling` = "Sell";
  1007/1008 `&DefaultFenceName` = "Enclosure"; 1268/1269
  `&WARNING_SELLING_NON_EMPTY_FENCE` = "The enclosure cannot be sold: It still
  contains dinosaurs."; 1352/1353 `&WARNING_TOO_MANY_LUNCHOXES_IN_FENCE`;
  1364/1365 `&REQUEST_SELL_FENCE` = "Sell this enclosure for ".
- `Data/Design/Scenario/FreeGame.ini` `[CONTEXT]`: `SellBackFactor=1.0`,
  `…Medium=0.5`, `…Hard=0.2`.
- `docs/platform/ui-coordinates.json` (park-hud `OPEN_SELLING` at 579,572;
  dialog-request validate 370,356 / cancel 400,356).
- `docs/formats/bsi.md` (nutrition +0x00, quantity +0x04, kind +0x0c; kinds 0-4
  vegetarian, 5-9 carnivorous), `docs/formats/bwi.md` (Fence.bwi: cell 4,
  price 30), `docs/formats/bbi.md` (def+0x18 = construction cost).
