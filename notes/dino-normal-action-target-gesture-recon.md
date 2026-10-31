# Normal-action executor: gesture field, Exploration point, Victory gesture

Date: 2026-10-31. Binary `/DinoIsland.exe-ebea17` (SHA256
`7ba16f38db4fe9461e700e6005e29a37c0d7a6d9441a233880a7c4405bbd717c`), read-only
Ghidra pass. No runtime run in this pass; runtime lines quoted from the existing
(gitignored) probe logs.

Scope: the three open items behind `FUN_0056f7a0` (normal-action executor) and
`FUN_0056eec0` case 5. Cross-checks for `notes/dino-move-record-actions.md` and
`docs/decomp/dinosaur-actions.md` are in section 5.

## 0. Answer in one line

1. **`element+0x40` is `params[8]` (the last .bai element dword, a float)** and
   has nothing to do with gestures. The executor's `[ESI+0x40]` is a field of the
   executor/target-policy object (`behaviour+0xa0`), written at runtime only by
   `FUN_0056fb70` (the `special_action == 5` attack-gesture picker). Normal
   actions emit a **shared per-behaviour gesture**, not a per-element one.
2. **The Exploration action point is a fresh random point inside the dino's
   paddock rect**, computed inline in case 0:
   `x = minX + rand()%(maxX-minX-7) + 4`, same for `y`. It is not the dino
   position, not a cell centre, not an action-list field. RunAway instead uses
   the **minimum-cost cell centre** of the same rect.
3. **Victory's gesture is `Victory.bai` element[0 or 1] `bones[0]` = 26 or 35**,
   duration the shared sentinel `[0x006430a8] = 0x7f7fc99e`, mode 2, self,
   allowTurn 0, instant 1. Element 0 is used when the actor is in the
   show/attraction cast list (`FUN_00583230`), element 1 otherwise. Portable:
   with no cast list the retail answer is element 1 -> kind 35. No park
   class-6 availability path was found; the `+0x1c` gate mechanism is open
   (its result is recorded as unused in `notes/dino-target-vtable-recon.md`),
   so Victory is not scored in a park (negative result).

## 1. `.bai` element `+0x3c` / `+0x40` are `params[7]` / `params[8]`

`FUN_0053e060` builds one 0x58-byte element record per element. The tail of the
per-element loop (0x53eb30-0x53ec18) is nine consecutive 4-byte reads whose
destinations are, **in file order**:

| # | file field | destination | store site |
| ---: | --- | --- | --- |
| 0 | `params[0]` | `element+0x54` | `0x53eb59` |
| 1 | `params[1]` | `element+0x2c` | `0x53eb63` |
| 2 | `params[2]` | `element+0x14` | `0x53eb79` |
| 3 | `params[3]` | `element+0x18` | `0x53eb8f` |
| 4 | `params[4]` | `element+0x1c` | `0x53eba5` |
| 5 | `params[5]` | `element+0x20` | `0x53ebbb` |
| 6 | `params[6]` | `element+0x24` | `0x53ebd1` |
| 7 | `params[7]` | `element+0x3c` | `0x53ebe7` |
| 8 | `params[8]` | `element+0x40` | `0x53ebf3` |

```text
0053ebdd MOV EAX,[EDI]              ; element base
0053ebdf MOV EDX,[ESP+0x1c]         ; stream
0053ebe3 PUSH 0x1
0053ebe5 PUSH 0x4
0053ebe7 LEA ECX,[ESI+EAX*1+0x3c]   ; element+0x3c = params[7]
0053ebf0 CALL [EDX+0x8]
0053ebf3 MOV EAX,[EDI]
0053ebf5 MOV EDX,[ESP+0x1c]
0053ebf9 PUSH 0x1
0053ebfb PUSH 0x4
0053ebfd LEA ECX,[ESI+EAX*1+0x40]   ; element+0x40 = params[8]
0053ec06 CALL [EDX+0x8]
```

The only consumer of the pair is the tail of `FUN_00569a00` (element execute) at
`0x569d6f`:

```c
iVar9 = *(int *)(*(int *)(param_1 + 4) + 0xa0) + element_index * 0x58;  // element
FUN_0056ddb0(*(undefined4 *)(iVar9 + 0x3c), *(undefined4 *)(iVar9 + 0x40));
```

`FUN_0056ddb0(percent, value)` early-returns when `percent == 100`; otherwise it
pushes `percent` into `obj+0x14`, `value` into `obj+0x24`, and scales
`obj+0xc = obj+0xc * percent / 100`. It is the animation blend/ramp request, not
a move kind.

### Real `.bai` values (all 64 files, 204 elements)

```text
params[7] : 1 (x1), 5 (x12), 25 (x24), 50 (x13), 75 (x13), 100 (x141)  -> percent
params[8] : 0 (x141), 5.0 (x13), 10.0 (x13), 20.0 (x36), 30.0 (x1)     -> float
```

No element carries `0x12..0x17` in `params[8]`. Other element fields were also
checked and rejected: `time` 0..300, `duration` 0/2/4/.../60, `n_bones` 0..6.

## 2. What actually fills the executor's `+0x40` (the gesture kind)

`FUN_0056f7a0` is not a function on the `.bai` element. `ESI = ECX` at
`0x56f7a4` is the executor/target-policy object (`this`); the element node is
`EBP` from the `[this+0x18]` vector (0x18-byte node: `+0x00/+0x04` distance gate
squared, `+0x08` done byte, `+0x0c` element context, `+0x10` state, `+0x14`
action object). State 2 reads its own object:

```text
0056fa3b MOV EDX,[ESI+0x38]      ; target/action descriptor (its +0x28 = saDinosaur)
0056fa3e MOV EAX,[ESI+0x40]      ; gesture kind  <== NOT the .bai element
0056fa41 TEST EAX,EAX
0056fa43 MOV ECX,[EDX+0x28]
0056fa46 MOV [ESP+0x18],ECX      ; actor
0056fa4a LEA EBX,[ECX+0x2c8]     ; mover
0056fa50 JZ 0x56fadc             ; kind 0 -> nothing to play
0056fa56 CMP byte [ESI+0x36],0   ; already-played flag
0056fa5c MOV EDX,[EDI+0x4bc]     ; actor clip id
0056fa62 CMP EDX,0x30 / JL 0x9 / JG 0xf   ; gate: 0x30 or 9..0xf
0056fa71 PUSH EAX
0056fa74 CALL 0x00575de0        ; duration = table[phenotype+0x1a8+0x1bc+kind*4]
0056fa84 PUSH 0 ; PUSH 0        ; allowTurn = 0, instant = 0
0056fa88 PUSH EDX               ; kind
0056fa89 LEA EAX,[EBX+0xc]      ; target = mover position (self)
0056fa8c PUSH 0x2               ; mode 2
0056fa8e PUSH ECX               ; duration
0056fa90 PUSH EAX ; PUSH EAX
0056fa93 CALL 0x00573f40
0056fa9c MOV byte [ESI+0x36],1  ; played
```

The executor object is embedded at `behaviour+0x60`
(`LEA ECX,[ESI+0x60]` at `0x569e81`, constructor `FUN_0056edc0`, vtable
`0x006430ac`), so

```text
executor+0x40  ==  behaviour+0xa0
```

Initialised in the behaviour constructor `FUN_00569e00` at `0x569e92`
(`MOV [ESI+0xa0],ECX`, the 3rd stack argument) - the same value that becomes
`executor+0x3c` (the action container used by `FUN_0056f2f0`/`FUN_0056f460`).

### The only runtime writer: `FUN_0056fb70`

A byte scan of `0x56b000..0x570000` for every `MOV [reg+0x40], r32/imm32` store
(mod=01 disp8 and mod=10 disp32 forms) finds exactly three, all in
`FUN_0056fb70`:

| address | instruction | value |
| --- | --- | --- |
| `0x56fdab` | `MOV [ESI+0x40],EBP` | `0x12/0x13/0x14/0x16/0x17` (or `0x33`) |
| `0x56fdd3` | `MOV [ESI+0x40],0` | none |
| `0x56fde3` | `MOV [ESI+0x40],EBX` | paired `0x12/0x13/0x14/0x16/0x17/0x18/0x33` |

`FUN_0056fb70` is the `special_action == 5` (ObstAttack) gesture emitter (called
only from `FUN_0056f560` at `0x56f5a5`). It:

1. finds the node whose `+0x14 == action`, sets `node+0x10 = 3` (done),
   `[esi+0x36] = 0`, `[esi+0x4] = 0`, `[action+0x14] = 0`;
2. picks the attack gesture from the actor five weights at `actor+0x288`
   (`rand()%[actor+0x29c]`, cumulative walk): group 0 -> 9/10/11 by
   `actor+0x1e0` vs `target+0x1e0`, groups 1..4 -> `0xc/0xd/0xe/0xf`, `>4` ->
   `0x33`; that one is emitted at `0x56fc94` (ret `0x56FC99`) to the target actor;
3. maps the target facing state (`FUN_00575be0` = front/left/right/back) to the
   second kind: `0 -> 0x33`, `1 -> 0x16/0x14`, `2 -> 0x18/0x12`,
   `3 -> 0x17/0x13`; if `FUN_00575cc0(kind)` says the clip cannot play, the
   left-kind becomes `rand()%2 + 0x13`;
4. `rand()%actor+0x288[group]` vs `rand()%actor+0xb4` (plus a further
   `rand()%actor+0x270` test) decides `EBP` / `0` / `EBX` -> `[esi+0x40]`.

So `[esi+0x40]` is a **shared last-attack-gesture slot on the behaviour**. The
executor state 2 re-emits whatever is in it; it is not per-element and has no
`.bai` source. This matches the probe `ret=0x56FA98` kinds
`0x12 (8), 0x13 (24), 0x14 (14), 0x16 (1), 0x17 (2)` - all values only
`FUN_0056fb70` writes.

**Port consequence:** do not look for a per-element gesture in `BaiElement`.
Either model `behaviour+0xa0` as a 0/"last attack gesture" slot updated by the
ObstAttack selector, or keep the state-2 emission at 0/off for normal actions.
There is no `.bai` field to import.

## 3. Exploration action point (case 0, `0x56ef12`)

Case 0 state 0 computes the point inline from a rect, then calls
`FUN_005741c0(mover, float[2]{x,y})` at `0x56ef84`:

```text
0056ef19 MOV ECX,[EBX+0x38]      ; target/action descriptor
0056ef1c MOV EBP,[ECX+0x1c]      ; rect
0056ef1f MOV EDX,[EBP+0x1c]      ; minX
0056ef26 CALL 0x00597319         ; rand()
0056ef2b MOV ECX,[EBP+0x24]      ; maxX
0056ef32 SUB ECX,EDX
0056ef35 SUB ECX,0x7             ; maxX-minX-7
0056ef38 IDIV ECX
0056ef3e LEA ECX,[EAX+EDX+4]     ; X = minX + rand()%(maxX-minX-7) + 4
0056ef42 MOV EDX,[EBP+0x20]      ; minY
0056ef4d CALL 0x00597319         ; rand()
0056ef52 MOV EBP,[EBP+0x28]      ; maxY
0056ef59 SUB EBP,ECX
0056ef60 SUB EBP,0x7
0056ef63 IDIV EBP
0056ef69 LEA EAX,[ECX+EDX+4]     ; Y = minY + rand()%(maxY-minY-7) + 4
0056ef5c FILD [ESP+0x18] / 0056ef76 FILD [ESP+0x14]   ; int -> float
0056ef84 CALL 0x005741c0         ; (mover, &{X,Y})
```

Rect layout (identical to the paddock bounds rect):

```text
rect+0x1c = minX   rect+0x20 = minY   rect+0x24 = maxX   rect+0x28 = maxY
```

**That rect is the dino's paddock rect.** The constructor installs the very same
pointer as the path object confinement rect (`0x569e98-0x569e9f`):

```c
// FUN_00569e00, arg1 = target/action descriptor, this = behaviour
FUN_0055ac30(this + 0xa4 /* path */, arg2 /* occupancy grid */, *(undefined4 *)(arg1 + 0x1c) /* paddock rect */);
```

`FUN_0055ac30` stores it at `path+0x1c0`; `FUN_0055af70` (the mover step guard)
compares against `[path+0x1c0]+0x1c/+0x20/+0x24/+0x28` - same rect, same fields.
So the Exploration target is a uniform random point in the paddock rectangle,
4 units inside each edge, integer coordinates, not cell-snapped, with no
reachability/occupancy check.

Not the alternatives:

- Not the dino position: the first `0x27` record (kind `0x27`, mode 2, target
  `mover+0xc`) is the self record; the second `0x27`, the `0x34` marker and the
  fixed `0x20` all use the random point (`FUN_005741c0` param 2).
- Not a cell centre: RunAway `FUN_0056f200` -> `FUN_00577570` (minimum-cost cell
  scan over the same rect) then `x = x*8 + 4`, `y = y*8 + 4`, clamped to the
  rect. Probe confirms: `0x35` cell markers at `380.0/376.0` (both 4 mod 8);
  Exploration `0x20` targets at `393.0/380.0` and `384.0/408.0` (not 4 mod 8).
- Not `behaviour+0x30/+0x34`: those are written only by `FUN_0056f200` (RunAway,
  called from case 1 at `0x56efca`), never in case 0.
- Not an action-list or `.bai` field: every case-0 visit re-rolls the point.

Port recipe (one step per Exploration start):

```text
span_x = maxX - minX - 7;  if (span_x <= 0) span_x = 1;   // retail has no guard
x = minX + rand() % span_x + 4;
y = minY + rand() % (maxY - minY - 7) + 4;
queue 0x27 self (table dur), 0x27 to point (table+0.5), 0x34 to point,
      0x20 to point (X[0x23c]), 0x20 self (5..20 s)
```

Retail `IDIV` would fault when `maxX-minX <= 7`; any RNG is acceptable for the
port (the invariants are the rect, the +4 inset and the integer range).

## 4. Victory (case 5, `0x56f152`)

```text
0056f152 MOV ECX,[ESI+0x10]      ; node state
0056f157 JZ 0x56f172             ; state 0
0056f159 CMP ECX,0x3 / JNZ epilogue
0056f15e CMP [EDI+0x4b4],0       ; actor busy -> skip
0056f168 PUSH EAX ; CALL 0x56c260 ; unregister

0056f172 MOV EAX,[EBX+0x38]      ; descriptor
0056f175 PUSH EDI                ; actor (saDinosaur of the element action)
0056f176 MOV ECX,[EAX+0xcc]      ; cast/context list holder
0056f17c CALL 0x583230           ; 1 = actor is in ctx+0x50..+0x54

0056f181 MOV EDX,[ESI+0xc]       ; element context
0056f184 DEC AL / NEG AL / SBB EAX,EAX / NEG EAX
                                 ; EAX = (ret==1) ? 0 : 1        <== element index
0056f188 MOV EDX,[EDX+4]         ; .bai 0xac record
0056f191 MOV EDX,[EDX+0xa0]      ; element array base (stride 0x58)
0056f1a8 MOV EAX,[EDX+EAX*8+8]   ; EAX*8 == 0x58*idx -> elements[idx]+0x08
0056f1ac MOV EDX,[EAX]           ; bones vector begin -> bones[0]   <== kind
0056f1ae MOV EAX,[0x006430a8]    ; 0x7f7fc99e (3.4e38)              <== duration
0056f18b PUSH 0 ; 0056f18f PUSH 1 ; PUSH EDX ; PUSH 2 ; PUSH EAX ;
0056f1b7 PUSH ECX ; PUSH ECX ; CALL 0x573f40
0056f1c0 MOV [ESI+0x10],3        ; node done
```

`FUN_00583230(this, actor)` is a plain membership test over a 4-dword-stride list
at `this+0x50..this+0x54` (returns 1 if found, else 0).

Real file: `Victory.bai` has `special_action = 6` and exactly two elements (no
events, params beyond `100` unused):

```text
element[0]  time 0  dur 0.0  bones (26,)   -> kind 0x1a
element[1]  time 0  dur 0.0  bones (35,)   -> kind 0x23
```

Emitted record:

```text
kind = 26 (actor in cast list) or 35 (not in list)
mode = 2, duration = 0x7f7fc99e (float 3.4e38 = wait forever / until cancelled)
target = mover position (self), allowTurn = 0, instant = 1
```

`[0x006430a8]` is a shared .rdata float constant sitting between the
`0x006430ac` vtable and following data (`0x006430a4 = 0x0056e820`,
`0x006430a8 = 0x7f7fc99e`, `0x006430ac = 0x0056ee80`); Sleep `0x1e` uses the
same value.

**Portability:** yes, with a caveat.

- Without a show/attraction cast list, `FUN_00583230` returns 0 on an empty list,
  so retail behaviour is element 1 -> **kind 35**. That is the correct fallback
  for the port today.
- The kind 26 branch only happens when the dino is registered in the show cast
  (the list the show/attraction controller fills). It needs that list to be
  reproduced later, but the record shape does not depend on it.
- "Forever" means the record never times out; it is ended by the next
  queue/cancel, exactly like Sleep `0x1e`.

## 5. Cross-check with the two documents

`notes/dino-move-record-actions.md`:

- line 79/85: case 5 duration `[0x006430A8]` is right, but the **kind** is
  `Victory.bai` element[0|1] `bones[0]` (26/35), selected by `FUN_00583230`,
  not a gesture from the context table.
- line 79: case 0 "`0x27` mode 2 2.5 s target=action point (inline push,
  `X[0x258]+0.5`)" is confirmed; the point is the random paddock-rect point
  above (section 3).

`docs/decomp/dinosaur-actions.md`:

- line 492 ("The source of `node+0x40` ... is still open"): **resolved and
  corrected** - `+0x40` is not a node field. It is `executor+0x40` =
  `behaviour+0xa0`, initialised in `FUN_00569e00` (`0x569e92`) and written at
  runtime only by `FUN_0056fb70` (`0x56fdab/0x56fdd3/0x56fde3`). Nothing in
  `.bai` feeds it.
- line 446 ("`0x34` marker to the action point"): confirmed; the point is the
  random paddock-rect point, recomputed on every case-0 visit.
- line 447 ("`0x35` marker to the steering cell"): confirmed; the cell is the
  **minimum-cost cell** of the paddock rect (`FUN_00577570`), then `*8+4`.

## 6. Open (cheap probes)

1. Constructor value of `behaviour+0xa0`. `FUN_00569e00` copies its 3rd stack
   argument there (`0x569e92`), the same pointer stored to `executor+0x3c` (the
   action container), so it is not 0 at construction. The state-2 `TEST EAX,EAX`
   therefore looks like it can pass with a stale pointer until `FUN_0056fb70`
   runs. Probe: breakpoint at `0x0056fa3e`, print `ESI+0x40` and `ESI+0x3c`;
   condition `[ESI+0x40] > 0x40`.
2. Rect identity for park dinos. `FUN_0053cfb0` passes `world+0x158` as the
   descriptor (`FUN_0051fe20` arg1) and `world+0x174` as the second argument;
   the descriptor `+0x1c` field is the same slot, so the rect value is
   `[world+0x174]`. The layout and the `FUN_0055ac30` hand-off prove the *same*
   object is `path+0x1c0`; confirm per dino by dumping `[behaviour+0x98]+0x1c`
   and `[behaviour+0xa4+0x1c0]` and comparing pointers on one Cpg04 dino.
3. `Victory` never ran in the available probe (`notes/dino-move-record-actions.md`
   line 85, "not hit"), so kind 26/35 and the sentinel are static-only. Probe:
   log the `a5` argument at `0x56f1bb`; expect 35 in a park without a cast list.
