# Dino action table, move emitters and eating - static pass

Date: 2026-10-30. Read-only Ghidra recon (`/DinoIsland.exe-ebea17`).
Cross-checks `notes/dino-mover-runtime-probe.md` (60 s Cpg04 probe) and
`notes/dino-mover-tick-recon.md`. Nothing in `src/` was touched.

Status: **Q1 solved** (and it overturns the "Actions.bad is not the source"
idea); Q2 mapped for every call site; Q3/Q4 answered from code; Q5 is a
recipe with an explicit Open list.

All `rec+NN` below are offsets inside the **in-memory** 0x81c record that
`phenotype+0x1a8` points to. File offsets are labelled `f[i]`.

---

## 1. The action table: `FUN_00615e80` + the 0x81c records

### 1.1 What the translator opens

`FUN_00586200` builds `saSimuGenotypeTranslator` with `FUN_00615e80(param_2+0x18)`
(0x586244), where `param_2 = [[appli+0xc]+0x34] + 0x110` (0x51d4cc). So the
three inputs are the *config* strings at **cfg+0x128, cfg+0x12c, cfg+0x130**
(the same cfg object whose +0x1a0/+0x1a8 are passed to `spPark`).

Order in the ctor (0x615f67..0x6165ab):

| input | consumer | kind |
| --- | --- | --- |
| `cfg+0x128` (`param_2[0]`) | `FUN_006180e0` (0x615f7d) | `simu_trans.dic`, 10 parts |
| `cfg+0x130` (`param_2[2]`) | `FUN_00408800(str,1)` (0x615fe5), callback `LAB_00616970` | text tables |
| `cfg+0x12c` (`param_2[1]`) | `FUN_00417d10(path,"rb")` (0x6164f1 / `FUN_00417800`) | **the 0x81c table** |

```text
00615f99 MOV EDI,dword ptr [EDI + 0x8]      ; EDI = param_2, [EDI+8] = param_2[2]
00615fa8 CMP EDI,EBP / JZ ...                ; copy string
00615fda LEA EAX,[ESP + 0x18] / PUSH 0x1
00615fe1 LEA ECX,[ESP + 0x44] / CALL 0x00408800   ; parse the text file
...
00616462 MOV EDX,0x64be2c                    ; "rb"
006164a9 MOV EDX,dword ptr [ESP + 0x8d4]     ; param_2
006164b8 MOV EDI,dword ptr [EDX + 0x4]       ; param_2[1]  <-- binary path
006164f1 ... CALL 0x00417d10(local_8b8-3, local_8b8, 0, 1)  ; path, "rb"
```

`FUN_00417800` is the mcLib stream open: `FUN_00597a44(path, mode)` like
`fopen`, plus endian/version. Reads go through `FUN_00417e70(dest, size,
count)` and the vtable slot `+8`.

### 1.2 The record build (0x616550..0x6166c6)

```text
0061655d PUSH 0x4 / PUSH dest / CALL [EDX+8]   ; count = first dword of the file
0061656a MOV EBX,[ESP + 0x34]                  ; EBX = count
0061659a LEA EDX,[ESP + 0xb4]                  ; &template record
006165a1 LEA EDI,[ESI + 0x3b0]                 ; vector.begin
006165a8 PUSH EBX / PUSH EDI / CALL 0x0061a5b0 ; resize to `count` records, stride 0x81c
006165c6 MOV EAX,dword ptr [EDI]               ; *piVar1 = vector.begin
             CALL [EDX+8] (rec+0x08, 4, 1)
             CALL [EDX+8] (rec+0x00, 4, 1)
             CALL [EDX+8] (rec+0x04, 4, 1)
0061661a PUSH 0x33 / PUSH 0x8 / CALL 0x00417e70 (rec+0x0c , 8,  0x33)
00616645 PUSH 0x33 / PUSH 0x4 / CALL 0x00417e70 (rec+0x354, 4,  0x33)
0061664b..00616671 loop 51 x: CALL 0x00417e70 (rec+0x420 + i, 4, 5)
00616688 CALL 0x00417e70 (rec+0x1bc, 4, 0x33)   ; <-- the 51 durations
00616693 MOV EDX,0x33
0061669d FLD  [0x0062588c]                     ; 1.0f
006166a3 FDIV [EAX + ECX + 0xffffff30]         ; / rec[0x1bc + i*4]
006166ab FSTP [EAX + ECX - 0x4]                ; rec[0x288 + i*4] = 1/dur[i]
```

`FUN_0061a5b0` is only the MSVC6 `vector::insert`/`_Insert_n` helper for
0x81c-byte elements - it is **not** the builder (the audit's "FUN_00615e80 ->
FUN_0061a5b0 builds the table" is right only in the sense of the resize).

### 1.3 Record field map (file order -> in-memory order)

File order per block: 3 dwords, 51 pairs, 51 dwords, 51x5 dwords, 51 dwords.
Byte footprint = `3*4 + 51*8 + 51*4 + 51*5*4 + 51*4 = 1848 = 462 f32`.
Plus the 4-byte count: `4 + 5*1848 = 9244` = **exactly
`Data/Design/Actions.bad`**. Its first dword is `5`.

| reads | rec offset | meaning |
| --- | --- | --- |
| f[1] | +0x000 | 463.65 / 199.49 / 47.04 / 31.34 / 71.06 (family anchor x) |
| f[2] | +0x004 | 522.85 / 316.54 / 358.19 / 123.00 / 96.92 (anchor y) |
| f[0] | +0x008 | 0.0024 / 0.0053 / 0.005 / 0.0189 / 0.0103 |
| f[3..104] | +0x00c + kind*8 (forward), +0x010 (lateral) | 51 kinds |
| f[105..155] | +0x354 + i*4 | unknown (51) |
| f[156..410] | +0x420 + i*20 | 51 x 5 params |
| **f[411..461]** | **+0x1bc + kind*4** | **the 51 record durations** |
| derived | +0x288 + kind*4 | `1.0f / dur[kind]` |

`FUN_00575de0(mover, kind)` (0x575de0, returns `float10`):

```c
kind 1: (P[0xa4] * X[0x14] * 0.01) / needs[0x54]     // walk stride / walk speed
kind 2: (P[0xa4] * X[0x1c] * 0.01) / needs[0x58]     // run  stride / run speed
else   : X[0x1bc + kind*4]                            // <-- verbatim float
```

so there is **no scale factor, no blend and no per-species attribute**: the
turn duration is the loaded float.

### 1.4 Why the old note read 1.0

`0x1bc` is a *record* offset, not a file offset. The value at file float 111
(which the earlier pass printed as "0.5, 0, 0, 1.0, 1.0, ...") lands at
`rec+0x36c` (`0x354 + (111-105)*4`), i.e. in the second array - that is why
"dur[3] = dur[4] = dur[5] = 1.0 in all families" was wrong. The real duration
array is the **last 51 floats** of each block.

### 1.5 Which record a dino uses (`FUN_00617250`, 0x617250)

```c
/* family selection, 0x617278..0x6172a4 */
if (table[param_1+0x3c0][legs.alleleB][legs.alleleA] < 0x32)
    P[0x6b] = Part10[legs.alleleA];       /* simu_trans.dic part 10 */
else
    P[0x6b] = Part10[legs.alleleB];
if (P[0x6b] == 2 || P[0x6b] == 3) P[0x6d] = 0; else P[0x6d] = 1;   /* 0 = biped */
/* size category 0..3 from P[0x244..0x24c]; bipeds snap to Heavy/LightBiped */
if (P[0x6d] == 0) P[0x6b] = (P[0x64 /*sizeCat*/] < 2) + 2;
/* 0x617d2c */
iVar5      = *(int *)(translator + 0x3b0) + P[0x6b] * 0x81c;
P[0x6a]    = iVar5;                       /* byte 0x1a8 = X, the action table */
P[0x68]    = DAT_00676ef4[0x40+P[0x64]*4] / (P[0x29] * *(float*)(iVar5+0x134) * 0.01f);
```

So `phenotype+0x1a8` = `translator[0x3b0] + family*0x81c`;
`phenotype+0x1ac` = family 0..4; `phenotype+0x1b4` = 0 for the two biped
families (record +0x1ec/0x1f0 logic is unrelated to this flag).

### 1.6 The per-family duration table (measured from the retail file)

`dur[k]` = `X[0x1bc + k*4]` = `Actions.bad` block floats[411+k]:

| family (index) | dur[3] | dur[4] | dur[5] | dur[9] | dur[0x19] | dur[0x1c] | dur[0x20] | dur[0x27] |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 0 HighQuadruped | 1.5 | 1.5 | 2.0 | 2.0 | 2.0 | 2.0 | 2.0 | 2.0 |
| 1 LightQuadruped | 2.0 | 2.0 | 1.5 | 2.0 | 2.0 | 1.5 | 2.0 | 2.0 |
| 2 HeavyBiped | 2.0 | 2.0 | 2.0 | **1.8** | 2.0 | 1.5 | 2.0 | 2.0 |
| 3 LightBiped | **1.2** | **1.2** | **1.5** | 1.2 | 2.0 | 1.5 | 1.5 | 2.0 |
| 4 LowQuadruped | 2.0 | 2.0 | 1.5 | 1.5 | 1.5 | 1.5 | 2.0 | 2.0 |

Full arrays (k = 0..50), for the port:

```text
HighQuadruped:  1,1.5,1.5,1.5,1.5,2,2,2,2,2,2,2,1.2,2,1,2,2,1.5,1.5,1.2,1.2,2,1.5,1.5,1.5,2,1.5,2,2,2,2,2,2,1.5,2,2,1.5,2,2,2,1.5,2,2,2,2,2,2,2,2,1.2,1.7
LightQuadruped: 1,1,1,2,2,1.5,2,2,1.7,2,2,2,1.5,2,1.5,1.2,1.5,1.5,2,1.5,1.5,1.5,1.2,1.2,1.2,2,1.3,1.5,1.5,1.5,2,2,2,1,1.5,2,1.2,2,2,2,2,2,1.3,2,1.7,2,2,2,1.3,0.8,1.5
HeavyBiped:     1,2,2,2,2,2,2,2,2,1.8,2,2,1.2,1.5,1.5,1.5,2,1.5,1.2,1.2,1.5,2,1,1,1,2,1.3,1.5,1.5,1.5,2,1.6,2,0.8,1.2,2,1.2,2,1.5,2,1.5,2,1.5,1.7,1.7,1.7,1.7,2,1.5,1,1.2
LightBiped:     1,1,1.2,1.2,1.2,1.5,1.5,2,1.2,1.2,2,2,1.2,1.2,2,1,2,1.2,1,1,1,1,1,1,1,2,0.8,1.5,1.5,1.5,2,1.5,1.5,0.7,0.8,2,1.2,1.5,1.5,2,1.2,1.6,1,1.7,1.7,1.7,1.7,1.5,1.3,1.2,1
LowQuadruped:   1,2,2,2,2,1.5,2,1.5,2,1.5,1.5,1.5,1.5,2,1,1.5,1.5,1.5,1.5,1.5,1.5,1.5,1.5,1.5,1.5,1.5,1.2,1.5,1.5,1.5,1.5,1.5,2,0.8,1.2,2,1.2,2,1.5,2,1.2,2,1.5,1.7,1.7,1.7,1.7,1.5,1.6,1.2,1.5
```

Cross-check against the probe (this is the independent confirmation):

| probe observation | table row |
| --- | --- |
| 15/20 dinos: kind3 = kind4 = 1.2, kind5 = 1.5 | LightBiped |
| 4 dinos: 2.0 / 2.0 / 2.0 | HeavyBiped |
| 1 dino: 2.0 / - / 1.5 | LightQuadruped |
| one kind-9 record of **1.8 s** on 0x0B3AA010 | HeavyBiped `dur[9] = 1.8` - and that dino is in the 2.0-turn group |
| 0x19 records of 2.0 s | `dur[0x19] = 2.0` |
| 0x27 record of 2.0 s | `dur[0x27] = 2.0` |

Consequence for the port: **turn duration is selected by the dino's animation
family, not by species**. 20 species collapse onto 5 rows.

Open: the probe logged "1.4" for the 0x1c/0x20 records while the table says
1.5 (0x3FC00000). Cheapest check: grep the raw log for `dur=3FB33333`; if it
really is 1.4, a per-element duration (`element+0x1d8`, see `FUN_00520880`
kind 7 below) is overriding the table.

---

## 2. Park move-record emitters

`FUN_00573f40(pos, target, dur, mode, kind, instant, allowTurn)` is
`__thiscall` on the mover with the extra leading `Vec2* position` (probe §2).
Two executors carry almost all 0x573f40 traffic; both are **per-frame**
`Update` slots reached from a vtable, not from `FUN_00569660`.

### 2.1 `FUN_0056eec0` - no-target/item action executor (vtable slot `0x006430b4`)

Entry: `SUB ESP,0x1c; MOV EBX,ECX` (no frame pointer, so Ghidra did not make
it a function). It loops the element vector `[this+0x18]..[this+0x1c]`
(last element first), takes `dino = [element+0x14]->0x28`, and switches on the
element's **move profile** `[element+0xc]->[+4]->[+0x60] - 1` via the jump
table at `0x0056f1e0`:

| prof+0x60 | arm | what it emits |
| ---: | --- | --- |
| 1 | 0x56ef12 -> 0x56ef19 | `FUN_005741c0(mover, target)`: see below (0x27 + 0x20 + 0x20 lunge) |
| 2 | 0x56ef8e -> 0x56efc8 | 0x35 marker to `[this+0x30]/[this+0x34]`, then the 0x19 hold |
| 3 | 0x56f087 | 0x1c pose record or the 0x1d append |
| 4,5 | 0x56f1c7 | nothing |
| 6 | 0x56f152 | gesture from the profile kinds list with the 0x7f7fc99e sentinel |

Records (arg order `pos, target, dur, mode, kind, instant, allowTurn`):

```text
; 0x56f02c  (prof+0x60 == 2, every frame)
PUSH 1 / PUSH 1 / PUSH 0x35 / PUSH 1 / PUSH 0 / LEA tgt / LEA pos
  -> { pos, (this+0x30, this+0x34), 0, mode 1, kind 0x35, inst 1, allow 1 }
; 0x56f07d  (after it; skipped when mover+0x1f4 == 0x19)
dur = FUN_00575de0(0x19)
  -> { pos, pos, dur[0x19], mode 2, kind 0x19, 0, 0 }
; 0x56f0ff  (prof+0x60 == 3, element state 3, dino+0x2b8 == 3,
;            mover+0x1f4 != 0x1c and mover+0x1f0 != 0x1c)
  -> { pos, pos, FUN_00575de0(0x1c), mode 2, kind 0x1c, 0, 0 }
; 0x56f130  (prof+0x60 == 3, other state) - append, not queue+start
  -> append { pos, pos, FUN_00575de0(0x1d), mode 2, kind 0x1d, 0, 0 }
; 0x56f1bb  (prof+0x60 == 6)
kind = *(int*)([prof+0xa0] + idx*0x58 + 8);   idx = 11*prof->? (see code)
  -> { pos, pos, *(float*)0x006430a8, mode 2, kind, 0, 0 }
     *(float*)0x006430a8 == 0x7f7fc99e  (the "hold until replaced" sentinel)
```

`FUN_005741c0` (0x5741c0) is the lunge/return list the probe saw as
`(0x27, 2.0), (0x20, 1.4/1.5), (0x20, 5..20 s)`:

```c
FUN_00573f40(pos,pos, X[600]=X[0x258]=dur[39], 2, 0x27, 0, 0);
append {target, dur[39]+0.5, mode 2, kind 0x27, 0, 1};
if (state == 0) FUN_00574ad0(pos, 1);
insert {target, 0, mode 1, kind 0x34, inst 1, allow 1};
FUN_00573dc0(target, target, X[0x23c]=dur[32], 2, 0x20, 0, 0);
d = rand()%16 + 5.0;
FUN_00573dc0(target, pos, d, 2, 0x20, 0, 1);
```

### 2.2 Actor-target action executor (0x56f7e0..0x56fbxx, no Ghidra boundary)

Same grammar, different class (ctor `FUN_0056F620`, vtable `0x006430e4`,
`[+0x38]` = actor). Emits:

```text
; 0x56f94a  behind a squared-distance gate (dist2 vs [EBP], [EBP+4])
kind = 0x34 + (dino[0x2b8] != 2)          ; 0x34 walk / 0x35 run
  -> { pos, point, 0, mode 1, kind, inst 1, allow 1 }
[EBP+0x10] = 1                            ; element started
; 0x56f98a  (element finished; after FUN_0056df40/FUN_0056e7f0)
  -> { pos, pos, 0, mode 2, kind 0, 0, 0 }        ; idle / stop
; 0x56f9fd  (abort path)
  -> { pos, pos, 0, mode 0, kind 0, 0, 0 }        ; mode 0 = cancel, [EBP+0x10]=2
; 0x56fa93  (gesture; kind = element+0x40, non-zero)
  -> { pos, pos, FUN_00575de0(kind), mode 2, kind, 0, 0 }
```

Distance gate (0x56f887): `dx*dx + dy*dy` compared with `[EBP]` and
`[EBP+4]`, which are built at 0x56f83f..0x56f863 from
`DAT_00675708[ (actor+0x1e0)[...] ]` times the actor scale; `0x56f610` sets
`DAT_00675700 = 0.1`.

### 2.3 `FUN_0056c3b0` region (object path; same two records)

```text
; 0x56c65f  same shape as 0x56f94a: {pos, point, 0, mode 1, 0x34/0x35, 1, 1}
; 0x56c70e  {pos, pos, 0, mode 2, kind 0, 0, 0}  after FUN_0056df40/FUN_0056e7f0/FUN_0056e7d0
```

### 2.4 Choice / special-action class

```text
; FUN_0056e670 (0x56e799)  action list exhausted:
{ pos, pos, 0, mode 2, kind 0, 0, 0 }
; FUN_0056fb70 (0x56fc94)  special_action == 5 random idle/gesture
kind = {9,10,11,12,13} or 0x0b..0x0f chosen from dino[0x288+n] counts
  -> { actor1.pos, actor2.pos, FUN_00575de0(kind), mode 2, kind, 0, 1 }
; then, on one branch, FUN_00573f40(pos,pos,0,mode 0,kind 0,0,0) to cancel
```

This is the "0x0b..0x0f idle/random arm" the probe saw; it is *not*
`FUN_00569660`, it is `FUN_0056fb70` (the two share the `+0x288` count array).

### 2.5 Visitor eat controller

```text
; FUN_00520880
0x5209d6  { pos, pos, 0, mode 2, kind 0, 0, 0 }                    (finish)
0x520a91  { pos, pos, *(float*)(element+0x1d8), mode 2, kind 7, 0, 0 }
0x520b85  { pos, pos, *(float*)(element+0x204), mode 2, kind 0x12, 0, 0 }
```

Matches the probe: the 6 firing controllers are visitor-side, no kind 8.

---

## 3. The per-frame 0x34/0x35 re-issue

The re-issue is in the **executors, not the mover**: both 2.1 (0x56f02c) and
2.2 (0x56f94a) run once per frame over the element list and emit the marker
each time the element is not yet in state 3. Per-frame count matches the
probe (883 / 911 / 643 identical records at ~16 ms).

What ends it: the state byte `element+0x10` is set to 1 on emit and to
**3 on arrival** (`0x56f94f`, `0x56f1c0`, `0x56f9f6`), and the branch
`TEST EDX,EDX / JNZ 0x56ef95` (state != 0) / `0x56f95b..` (state 3) only
emits once. In 2.2 the squared-distance gate also suppresses the marker as
soon as the actor is inside the radius.

Duplicate handling in `FUN_00573f40` (0x573f40, decompiled):

```c
n = (state == 2 && mover[0x1f4] && mover[0x1f0]) ? 2 : 1;
if (n < (queue.end - queue.begin)/0x18)
    FUN_00576150(queue.end, queue.end, queue.begin + n*0x18, &instant, 0);  /* insert copy */
if (state == 1) {                                     /* cancel arrival test */
    k = queue[0].kind;
    if (k != 3 && k != 4 && k != 5) queue[0].mode = 0;
} else if (state == 2) {
    idx = (mover[0x1f4] && mover[0x1f0]) ? 1 : 0;
    queue[idx].mode = 0;
}
if ((mover[0x1f4] == 0x1e || mover[0x1f4] == 0x15) && kind != 0x1c && mover[0x64] == 0) {
    append { pos, X[0x22c] = dur[28], mode 2, kind 0x1c, 0, 0 };
    if (state == 0) FUN_00574ad0(pos, 1);
}
if (kind != 0) {
    if (kind == 0x35 && needs[0x58] == 0.0f) kind = 0x34;     /* run speed 0 */
    append { target, dur, mode, kind, instant, allowTurn };
    if (state == 0) FUN_00574ad0(pos, 1);
    mover[0x65] = (kind == 8);
}
```

So duplicates never accumulate beyond one pending copy: the executing record
is switched to mode 0 (instant arrival) and the queue is capped at 1 (or 2
while a turn is pending). That is why the probe saw thousands of *calls* but
only ~238 distinct `(kind,target)` instructions.

---

## 4. Dino eating

`FUN_00576e50(food, amount)` is the **food-source drain** (0x576e50):

```c
food->0x00 = 1;  *food->0x04 = 1;  food->0x16 = 1;
if (food->0x08 < amount) amount = food->0x08;      /* clamp to remaining */
food->0x08 -= amount;
src = food->0x0c;                                  /* {rate, capacity} */
if (food->0x08 >= src[1]*0.66666f) { food->0x10 = 0; }
else if (food->0x08 >= src[1]*0.33333f) { food->0x10 = 1; }
else if (food->0x08 > 0) { food->0x10 = 2; }
else { food->0x10 = 3; }                           /* empty */
return amount * src[0];                            /* nutrition delivered */
```

Called from exactly two sites, both in `FUN_00569a00` (0x569aae, 0x569ae8),
the **element effect executor**:

```c
prof = [action+4] -> +0xa0 + idx*0x58;         /* same profile table as the moves */
ate  = 0;
for each kind in prof->kinds:                  /* vector at prof+8 .. prof+0xc */
  if (kind == 8) {                             /* Eat */
     ate = 1;
     amount = (float)*(int*)(dino + 0x288);    /* dino needs bucket [0] */
     tgt = [[[action+8]->0x28]->0x28]->0x28;   /* the target actor/feature */
     if (name == "LunchBox")   FUN_00576e50(amount);
     else if (name == "StaticBait") FUN_00576e50(amount);
     else (*(tgt->vtable + 8))(amount);        /* generic consume */
     FUN_005727b0(0x17, (int)amount, 100, 0);  /* need/attribute delta */
  }
```

So a park dino *does* have a depletion path, but it is **effect-side**: it
requires the action's move-profile kinds list to contain kind **8**. It never
becomes a move record, so the "kind 8 record" the port expected does not
exist. In Cpg04 no element profile carried kind 8, hence 0 kind-8 records, no
`SetLevel`, and `0x571190` never firing.

`0x00580e60 saFoodSource::SetLevel` has **one** caller:
`FUN_00522300` at 0x522335 (the visitor eat state machine) - the source is
`ctrl+0x38`, `+0x54/+0x58/+0x5c` the level; the probe's 6 controllers all had
0 there.

`FUN_00571190` (0x571190) is the **action completion / end-of-action**:

```c
switch (dino[0x2c0 /*action id*/]) {
  case 9:  dino[0x264] = dino[0x84];  break;
  case 10: dino[0x280] = dino[0x88];  break;
  case 0xb:dino[0x284] = dino[0x8c];  break;
  case 0xd:dino[0x29c] = dino[0xb0]; dino[0x2a0] = dino[0x94];
           copy 5 dwords dino[0x84..] -> dino[0x288..]; break;
  case 0x10/0x14/0x1a/0x1b:  walk speed, run speed, perception, ...
  case 0x1c: dino[0x2a8] = 0;
}
if (dino[0x2c0] != 0x1f) { dino[0x2c0] = 0x1f; dino[0x260] = 0;
                           dino[0x515] = 1; dino[0x4d] = 1; }
```

and its only per-frame caller is `saDinosaur::Update` `FUN_00571540`:

```text
00571a02 CMP EAX,0x1f          ; action id == 0x1f (idle)?
00571a06 JZ  0x571a2c          ; yes -> never call completion
00571a08 FLD  [ESI+0x260]      ; action countdown
00571a0e FSUB [ESP+0x58]       ; -= dt
00571a12 FST  [ESI+0x260]
00571a23 JZ   0x571a2c         ; still > 0
00571a27 CALL 0x571190
```

The probe logged `act = 0x1F` (`dino+0x2c0`) and `cd = 0` (`dino+0x260`) on all
3381 records: the park actions in that mission never assign an action id, so
`0x571190` is gated off at the first compare. That also means a park dino never
applies an action's end-of-action need/speed effects - the port must not
invent an "action finished -> apply deltas" step for these mission actions.

---

## 5. Port recipe

1. **Duration source.** Build 5 duration arrays from
   `Data/Design/Actions.bad` floats `411..461` of each of the 5 blocks
   (`dur[kind]`), and the 51 derived `inv_dur[kind] = 1/dur[kind]`.
   `DinoMover::durationFor(kind)`:
   `kind 1 -> strideWalk/walkSpeed`, `kind 2 -> strideRun/runSpeed`,
   `else if kind <= 50 -> dur[kind]`, `else` -> **out of table** (see Open).
   Turn records use `dur[3]`, `dur[4]`, `dur[5]`; the sentinel record uses
   `0x7f7fc99e`.
2. **Family.** Add `phenotype.family` (0..4) computed exactly as §1.5 (legs
   allele -> `simu_trans.dic` part 10, biped snap to 2/3, size category),
   and `phenotype.action_table = &tables[family]`. The port currently keys
   turns off a constant 1.0 s; replace it with `tables[family].dur[kind]`.
3. **Move list representation.** Keep the mover queue as-is (0x18 records,
   FIFO), and model the *emitter* separately:
   `struct MoveIntent { kind, mode, dur, Vec2 target, bool instant, bool allowTurn; }`
   - executors emit `1..3` intents per element event, then stop while
     `element.state != 3`;
   - the walk/run marker is `{pos, cell, 0, mode 1, kind 0x34/0x35, 1, 1}`
     and is **re-emitted every frame** until `element.state = 3`;
   - in `queue()` reproduce `FUN_00573f40`: mode-0 the executing record when
     `state==Moving` and `kind not in {3,4,5}`; cap the queue at 1 (2 with a
     pending turn) by replacing the pending copy; downgrade `0x35 -> 0x34`
     when run speed is 0.
4. **Marker resolution.** `0x34 -> kind 1`, `0x35 -> kind 2` at the first
   waypoint (`FUN_00574ad0` step 5); mode-1 arrival is an exact float compare,
   and on failure the mover re-issues a marker to `mover+0x14/+0x18`
   (`FUN_00575ac0`) - keep that or the dino can stall short of the cell.
5. **Eat.** Drop "one kind-8 move record + consume one level". Instead, when
   an element's move profile kinds list contains 8, call
   `food->drain(amount)` with `amount = dino.needs[0]` (clamped to the
   remaining level) and add the nutrition delta to attribute 0x17
   (0..100). Food state is `{level +0x08, state +0x10, rate/capacity +0x0c}`
   with the 2/3 - 1/3 thresholds. No `SetLevel` for dinos.
6. **Action completion.** Only when `dino.action_id != 0x1F` and the
   `dino+0x260` countdown crosses 0 - i.e. never for the park mission actions
   used in Cpg04. Do not apply completion deltas unconditionally.

### Open (with the cheapest probe)

1. **Kinds > 50.** `X[0x1bc + kind*4]` runs past the 51-float array into the
   `1/dur` array. The probe has `actk = 0x39` (57) with `sd = 2.0` on a dino
   whose `1/dur[6]` is 0.5-0.67. Probe: log `mover+0x1f4` and `mover+0x30`
   together at 0x5752c7 and read the matched pair; if kinds 0x33..0x3d really
   execute, add the observed values as a table.
2. **"1.4" durations** for 0x1c/0x20 (probe) vs 1.5 in the file. Grep
   `notes/harness/dino-mover-runtime.log` for `dur=3FB33333`; if present, find
   the element-duration override (`element+0x1d8`, as used by the kind-7
   record at 0x520a91).
3. **Executing record identity for `0x27`/`0x20`.** Whether the probe's
   `(0x27, 2.0), (0x20, 1.5), (0x20, 5..20)` came from `FUN_005741c0`
   (profile 1) or from the actor executor. Probe: log the return address at
   0x573f40 for one 0x27 record.
4. **Exact entry point of the 0x56f7e0 executor.** Ghidra has no function
   boundary; create one at 0x56f7e0 (or find the vtable slot written by
   `FUN_0056F620`'s class) and re-check the case-3/6 arms.
5. **Which class the probing dinos used.** The two identical 0x35 records per
   frame (596+594 profiles) suggest two elements (or two executors) per dino.
   Probe: log `[esp]` (caller) at 0x573f40 and count distinct return
   addresses per dino per frame.

---

## 6. Evidence read this pass

Decompiled: `FUN_00586200`, `FUN_0051d1d0`, `FUN_00615e80`, `FUN_0061a5b0`,
`FUN_00617250`, `FUN_00575de0`, `FUN_00575410`, `FUN_00573f40`,
`FUN_00569a00`, `FUN_00576e50`, `FUN_00571190`, `FUN_005741c0`,
`FUN_0056fb70`, `FUN_0056e670`, `FUN_00417d10`, `FUN_00417e70`,
`FUN_00417800`.
Disassembly: 0x6164f1..0x6166c6 (table build), 0x56eec0..0x56f1c7 (executor +
jump table 0x56f1e0), 0x56f7e0..0x56fb43, 0x56c630..0x56c713,
0x56e780..0x56e7a3, 0x5209a0..0x520b85, 0x571a00..0x571a30.
Data: `Data/Design/Actions.bad` (5 x 462 f32; `f[411..461]` = durations),
`Data/Design/Genotype/simu_trans.dic` part 10 (family), `anim_trans.dic`
(51 clip names = the 51 table slots).
Constants: `0x006430a8 = 0x7f7fc99e`, `0x0062588c = 1.0f`, `0x00675700 = 0.1`
(`FUN_0056f610`).
