# Dinosaur action-chooser target policy (node+0x3c) - vtables, GetTarget, port contract

Status: resolved for the ABI + two concrete classes; one open item (the per-action
target *factory* that fills `action+0x08`). All addresses are in
`DinoIsland.exe` (image base 0x00400000), Ghidra project `/DinoIsland.exe-ebea17`.

Follow-up to `notes/dino-action-chooser-recon.md` (P3 "policy vtable +0x14" open
item) and `docs/formats/bai.md`.

## 1. Node layout and the node constructor

`FUN_0056e860` is the **only** place that allocates a behavior node and copies
`node+0x3c`. It allocates 0x40 bytes from a 0x38-byte stack template:

```c
piVar2 = operator new(0x40);                  // 0x0056e860
*(byte *)(piVar2 + 0x08) = *param_3;          // +0x08 kind
piVar2[3] = *(int *)(param_3 + 0x04);         // +0x0c score
piVar2[4] = *(int *)(param_3 + 0x08);         // +0x10 distance2
piVar2[5] = *(int *)(param_3 + 0x0c);         // +0x14 percent
copy_ctor(param_3 + 0x14);  piVar2[6]  = &PTR_FUN_006254b8;   // +0x18 vector<...>
copy_ctor(param_3 + 0x24);  piVar2[10] = &PTR_FUN_00626ad4;   // +0x28 vector<...>
piVar2[0x0e] = *(int *)(param_3 + 0x30);      // +0x38 action instance
piVar2[0x0f] = *(int *)(param_3 + 0x34);      // +0x3c TARGET POLICY  <==
// then linked as a std::list node (next at +0x00, prev at +0x04)
```

So the node is:

```text
+0x00 next            +0x08 kind (byte, 1 = scored)
+0x04 prev            +0x0c score
+0x10 distance2       +0x14 percent (100)
+0x18 vector #1 (vtable 0x006254b8, begin/end/cap +0x1c/+0x20/+0x24)
+0x28 vector #2 (vtable 0x00626ad4, begin/end/cap +0x2c/+0x30/+0x34)
+0x38 action instance (its +0x04 = .bai 0xac record, its +0x08 = target policy)
+0x3c target policy  <== node+0x3c
```

Node list destruction: `FUN_0056d7e0` / `FUN_0056cde0`. It destroys the two
embedded vectors and frees the node, but **does not free the target**: the
target is owned by the action instance (`action+0x08`), not by the node.

### Where the target pointer comes from

Two template fillers:

```c
FUN_0056e190(chooser, list)                 // 0x0056e190
  for each action in list[+4..+8]:          // vector of action instances
      piVar4 = *(int **)(action + 8);       // target policy
      ...
      template+0x30 = action;               // node+0x38
      template+0x34 = piVar4;               // node+0x3c

FUN_0056e300(chooser, unused, target)       // 0x0056e300
  template+0x34 = target;                   // node+0x3c
```

`FUN_0056e190` is called from the two dinopark reaction entry points
`FUN_00521d10` (reaction 2) and `FUN_00521d80` (reaction 1) with the perceived
object's action list. `FUN_0056e300` is only reached from the show/attraction
side (`FUN_0056a9a0`). **The park always gets its node targets from
`action+0x08`.**

## 2. Target policy vtables

The installed vtables found by following the constructors:

| vtable | ctor | created by | +0x14 GetTarget | +0x18 radius | +0x28 index |
| --- | --- | --- | --- | --- | --- |
| `0x00642ffc` | `FUN_0056bfd0` (size 0x2c) | `FUN_0056a730` <- `FUN_0053acd0` (show element, types 0x16/0x17) | centre of the box at `[this+0x28]`: `x=([box+4]+[box+0xc])/2`, `y=([box+8]+[box+0x10])/2` | `*(int *)[this+0x0c]` (first dword of the second object) | name lookup on `[[this+0x28]+0x28]`: `LunchBox` -> `[[box+0x48]+0xc]`, `StaticBait` -> `[[box+0x74]+0xc]`, else `[[[box+0x68]+0x40]+0x1c]` |
| `0x006430ac` | `FUN_0056edc0` (dtor `FUN_0056ee40`) | `FUN_00569e00` | **-1 / -1** (no target) | `*(int *)[this+0x0c]` (`0x0056c8d0`) | same name lookup (`0x0056c910`) |
| `0x006430e4` | `FUN_0056f620` (dtor `FUN_0056f680`) | `FUN_0056ac80` <- `FUN_00521d10` / `FUN_00521d80` (dino reactions) | position of the actor at `[[this+0x38]+0x28]`: `x=[actor+0x08]`, `y=[actor+0x0c]` | `[actor+0x278]` | constant **11** |

Both target classes derive from `0x00642ffc`; `0x006430ac` is the intermediate
that overrides `+0x14` with the no-target sentinel, `0x006430e4` supplies the
real actor position. Ctor evidence: `FUN_0056bfd0` writes
`*this = &PTR_FUN_00642ffc`, `FUN_0056edc0` writes `*this = &PTR_LAB_006430ac`,
`FUN_0056f620` writes `*this = &PTR_LAB_006430e4`.

### Decompiled slots

`0x0056ee00` - `0x006430ac` slot +0x14 (no target, `RET 0x8`):

```c
void GetTarget(int *x, int *y) { *x = -1; *y = -1; }
```

`0x0056fe40` - `0x006430e4` slot +0x14 (`RET 0x8`):

```asm
56fe40  MOV EAX,[ECX+0x38]     ; owner context (chooser+0xa4)
56fe47  MOV EAX,[EAX+0x28]     ; -> world actor
56fe4a  MOV ECX,[EAX+0x08]     ; actor x
56fe4d  MOV [EDX],ECX          ; *outX
56fe53  MOV EAX,[EAX+0x0c]     ; actor y
56fe56  MOV [ECX],EAX          ; *outY
56fe58  RET 0x8
```

`0x0056c870` - `0x00642ffc` slot +0x14 (`RET 0x8`), box centre:

```asm
56c870  MOV EDX,[ECX+0x28]  ; box
56c874  MOV EAX,[EDX+0x0c]  \  *outX = ([box+0x0c] + [box+0x04]) / 2
56c877  MOV ESI,[EDX+0x04]  /
...
56c88b  MOV EAX,[ECX+0x10]  \  *outY = ([box+0x10] + [box+0x08]) / 2
56c88e  MOV EDX,[ECX+0x08]  /
56c89e  RET 0x8
```

`0x0056fe60` - `0x006430e4` slot +0x18 (radius):

```asm
56fe60  MOV EAX,[ECX+0x38]  ; owner
56fe63  MOV ECX,[EAX+0x28]  ; actor
56fe66  MOV EAX,[ECX+0x278] ; stamp radius
56fe6c  RET
```

`0x0056c8d0` / `0x0056c8e0` - `0x00642ffc`/`0x006430ac` slots +0x18/+0x1c:

```asm
56c8d0  MOV EAX,[ECX+0x0c]; MOV EAX,[EAX];    RET   ; dword 0
56c8e0  MOV EAX,[ECX+0x0c]; MOV EAX,[EAX+4];  RET   ; dword 1
```

`0x0056c910` - slot +0x28 (inventory index), string compares against
`"LunchBox"` (`0x0066303c`) and `"StaticBait"` (`0x00665068`).
`0x0056fe90` - `0x006430e4` slot +0x28: `return 11;`
`0x0056fea0` - slot +0x2c: `return (float)(int16)[actor+0x14];`
`0x0056c9a0` - slot +0x2c (base): name compare, returns bool-ish.

### Full slot order (base `0x00642ffc`, from `.rdata` at `0x00642ffc`)

```text
+0x00 0x0056c020  +0x04 0x0056c060  +0x08 0x0056c490  +0x0c 0x004cf3a0
+0x10 0x0056c8b0  +0x14 0x0056c870  +0x18 0x0056c8d0  +0x1c 0x0056c8e0
+0x20 0x0056c8f0  +0x24 0x0056c900  +0x28 0x0056c910  +0x2c 0x0056c9a0
```

`0x006430e4`: `+0x0c 0x0056f660` (returns `[this+0x38]`), `+0x10 0x0056c8b0`
(`GetPosition(out)` -> forwards to `+0x14` with `out, out+4`), `+0x14
0x0056fe40`, `+0x18 0x0056fe60`, `+0x1c 0x0056f670` (returns 50; **unused** by
the gate), `+0x20 0x0056fe70`, `+0x24 0x0056fe80` (returns 1), `+0x28
0x0056fe90` (11), `+0x2c 0x0056fea0`.

## 3. The two call sites (calling convention)

Both are `__thiscall` (this in ECX) with **two `int *` stack arguments and
`RET 0x8`**, so the engine signature is
`void GetTarget(int *outX, int *outY)`.

Availability gate `FUN_0056d980` (`0x0056d9bf..0x0056da1d`):

```c
target = node->target;                              // [EDI+0x3c]
if (target != lastTarget) {
    target->vtable[0x14](&tx, &ty);                 // GetTarget
    if (tx != -1) {
        dx = tx - actor->x; dy = ty - actor->y;     // actor x/y at +0x08/+0x0c
        dist2 = dx*dx + dy*dy;
    } else dist2 = 0;
    target->vtable[0x1c]();                         // result unused
    radius = target->vtable[0x18]();                // GetRadius
    lastTarget = target;
}
node->distance2 = dist2;                            // [EDI+0x10]
if (action->record->effect_class != 5) {
    node->kind = 0;
    if (action->enabled != 1) goto skip;
    if (target->actor != 0 && target->actor->+0x28 != 0 && ...) { linked filter }
    // dist2 * actor->[+0x1f4] / 100 compared with radius*radius
}
```

Scorer `FUN_0056e410` (`0x56e446..0x56e455`):

```asm
56e446  MOV ECX,[ESI+0x3c]     ; target
56e449  LEA EDX,[ESP+0x20]     ; &ty
56e44d  PUSH EDX
56e44e  LEA EDX,[ESP+0x20]     ; &tx
56e452  MOV EAX,[ECX]
56e454  PUSH EDX
56e455  CALL [EAX+0x14]        ; GetTarget(&tx,&ty)
```

and the threat term (`0x56e4d1`):

```c
if (tx != -1) {                                  // CMP EAX,-1
    cell  = world->grid[ty>>3][tx>>3];            // world = actor+0x20
    threat = -cell;
    if (FUN_00572dd0(chooser, tx, ty) == 1)      // ECX=chooser, actor box test
        threat += cell_actor - actor->[+0x2a0];
    score += need_term(2, threat / 10, needs);
}
```

Refresh policy: **the coordinates are not cached**. `+0x14` is re-evaluated
once per distinct target object per availability pass (once per tick, only when
the current action finished / the actor is alive). Only the derived
`distance²` is stored on the node (`node+0x10`).

## 4. Which actions use which target type

- `0x00642ffc` (box centre + name->index): the **food / bait** family. The
  `+0x28` slot returns the inventory index the `consume_flag` gate checks
  (`FUN_0056e0e0` -> `actor[0x214 + index*4] != 0`), and the names
  `LunchBox` / `StaticBait` are exactly the `consume_flag == 1` actions from
  `docs/formats/bai.md`: `Eat`, `EatBait`, `EatForSmallies`,
  `EatBaitForSmallies`, `DinoAttackForCarniv`. Created from the show-element
  factory (`FUN_0053acd0`, types 0x16/0x17) - this is the only place the class
  is constructed in this build, so the park side of that mapping is not
  runtime-confirmed yet.
- `0x006430ac` (-1/-1): **no-target / self actions** - anything that does not
  own a target object; the `-1` sentinel makes the scorer skip the threat term
  (matches `Sleep`, and the `Eat` case when the feeder is already the self
  target).
- `0x006430e4` (actor position, radius `actor+0x278`, index 11): the
  **visitor / obstacle reaction** nodes created by `FUN_00521d10` (reaction 2)
  and `FUN_00521d80` (reaction 1) through `FUN_0056ac80` -> `FUN_0056f620`.
  These are the only park-side concrete target class proven so far.

## 5. Port contract

Model only what the chooser reads:

```cpp
// node+0x3c
struct ActionTarget {
    enum Kind { None, Actor, Box } kind = Kind::None;
    int  x = -1, y = -1;      // engine: -1 = no target, threat term skipped
    int  radius = 0;          // slot +0x18
    int  item_index = 0;      // slot +0x28, index into actor[0x214 + i*4]
    float amount = 0.0f;      // slot +0x2c, gate skips inventory check if <= 0
    const void *actor = nullptr;  // raw field target+0x28 (linked/obstacle filter)
};

struct BehaviorNode {
    BehaviorNode *next, *prev;   // +0x00 / +0x04
    uint8_t kind;                // +0x08, 1 = scored
    int score;                   // +0x0c
    int distance2;               // +0x10
    int percent;                 // +0x14, 100 default
    ActionInstance *action;      // +0x38
    ActionTarget   *target;      // +0x3c
};
```

Rules to reproduce retail:

1. `GetTarget` is `__thiscall void(int *x, int *y)`; it may write `-1,-1`.
2. The availability gate must call it **once per distinct target pointer per
   pass** and cache `distance²` on the node.
3. Range test: `distance² * actor[+0x1f4] / 100 <= radius * radius`.
4. Threat term: only when `x != -1`; `world = actor+0x20`, 8-unit cells,
   row pointers at `world+0x10`, stride 0x20; `threat = -cell(tx,ty)`,
   then optionally `+= cell(actor) - actor[+0x2a0]`; then
   `need_term(SECU=2, threat/10)`.
5. `consume_flag`: if `amount > 0`, require `actor[0x214 + item_index*4] != 0`.

The port's `DinoActionLoop` currently passes `threat_score = 0`; with this
contract it only needs the `(x, y)` pair per action target plus the actor's
`+0x278`/`+0x1f4`/`+0x2a0` caches it already persists.

## 6. Corrections to earlier notes

- `0x0064302c` / `0x00643040` (score hooks `FUN_0056cb30` / `FUN_0056cc20`) are
  **behavior** vtables, not node vtables: `FUN_0056ca80` / `FUN_0056cb80`
  build them and both call `FUN_0056cde0`. Base behavior vtable is
  `0x00643094` (slot +0x10 = `FUN_0056e820`).
- `node+0x10` is written with `distance²` by the availability gate
  (`0x0056da24 MOV [EDI+0x10],ESI`). The "rest cost = node+0x10 / 100" line in
  `notes/dino-action-chooser-recon.md` therefore needs a recheck; the scorer
  reads `+0x10` off EBX (the chooser), not off the node.

## 7. Open

1. ~~The factory that fills `action+0x08`~~ - resolved in section 8
   (`FUN_00569640`, called only by `FUN_0056a9a0`); the class is chosen by the
   producer. The id-list producers (`behavior+0xf4`, `+0xb8`, `+0xc8`) remain
   open.
2. Waypoint / `ReachWaypoint` targets: no concrete vtable found. `0x00642ffc`
   (box) and `0x006430e4` (actor) are the only `GetTarget` implementations in
   the 0x0056c000-0x0056f000 cluster.
3. `FUN_0056f620`'s `+0x28` raw field is 0 for the actor target (base ctor is
   called with `p3 = 0`), so the linked/obstacle branch of the gate never fires
   for it - worth confirming at runtime with a trace of `0x0056da4e`.

## 8. Target factory (`action+0x08`) - resolved

`FUN_00569640` is the **only** writer of `action+0x08`:

```c
void __thiscall FUN_00569640(void *a, void *record, void *target) {
    *(u8 *)a       = 1;       // +0x00 enabled
    *(void **)(a+4) = record; // +0x04 .bai/asItemBehavior record
    *(void **)(a+8) = target; // +0x08 target policy
}
```

Its only caller is `FUN_0056a9a0` (call at `0x0056aa0e`), which attaches **one**
target to a whole action list at owner `P+0x24` (begin `P+0x28`, end `P+0x2c`):
for each definition it allocates a 0xc-byte action, calls `FUN_00569640`, pushes
it, then calls `FUN_0056e300` for the per-target node helper. The concrete
target class is selected by the **producer**, not by the `.bai` record:

| producer | target ctor | vtable |
| --- | --- | --- |
| `FUN_0056a730` <- `FUN_0053acd0` (show element types 0x16/0x17) | `FUN_0056bfd0` | `0x00642ffc` box/bait |
| `FUN_00569e00` <- `FUN_0056bc40` / `FUN_0056bf50` | `FUN_0056edc0` | `0x006430ac` no target |
| `FUN_0056ac80` <- `FUN_00521d10` / `FUN_00521d80` | `FUN_0056f620` | `0x006430e4` actor |

Correction: `FUN_0056e300` is not show-only; `FUN_0056a9a0` is its sole caller,
so the reaction path reaches it too.

### Reaction ordering

`FUN_00521d10`/`FUN_00521d80` pass the action-owner block `P` (not a target) to
`FUN_0056e190`; `P+0x24` already holds the previous `FUN_0056ac80` fill, which
carries a `0x006430e4` policy. `FUN_0056e190` reads `action+0x08` and calls its
vtable **+0x0c `GetOwner`** (not +0x14) to skip actions whose owner equals the
chooser owner. `FUN_0056ac80`/`FUN_0056a9a0` then refill the list (push only,
no clear), so the first nodes bound by a starter can use the previous fill's
target - a one-fill lag worth a runtime check.

### Port implication

- One `ActionTarget` per attach operation/list, shared by every action in it;
  the class comes from the producer. The port's per-action provider is an
  approximation for the mixed `{Eat, Sleep, ...}` subset.
- `ActionTarget` should expose `GetOwner()` (slot +0x0c, the `+0x38` owner
  context); `FUN_0056e190` skips an action whose target owner equals the
  chooser owner.
- The id-list producers (`behavior+0xf4`, `+0xb8`, `+0xc8`) are still open;
  the only writer in the 0x51f-0x521 cluster is `FUN_0055ad00` (perception
  memory at `P+0xa4`, vector `+0x1b4`, records `+0x34`), fed with
  `[controller+0x28]` from `FUN_0056ac80` at `0x0056afde`.

## 9. Port status

Implemented in `src/world/dino_actions.*`:

- `ActionTarget` (none/actor/box, `x`/`y` = -1 sentinel, radius, item index,
  amount) and `chooser_threat_score(...)`, which uses the `FUN_00572dd0`
  actor-threat-radius box (`|tx-x| <= r`, `|ty-y| <= r`) and the danger grid.
- `ActionThreatProvider` on `choose_action` / `DinoActionLoopConfig`; the loop
  calls it for every candidate before `score_action`.
- Viewer: imported dinosaurs map `Eat`/`EatForSmallies` to the nearest stocked
  food source (an inferred park target); other actions return 0. The factory
  that fills `action+0x08` and the visitor/obstacle reaction nodes remain open.

Tests: `--dino-actions` selftest prints `dino target box: -500`,
`dino target actor: -40`, `dino target none: 0`, `dino choose threatbox: Eat
8259` and `dino loop threat: Exploration`; `test_cpp_dino_action_chooser`
guards them.

## 10. Runtime confirmation (Cpg04, 20 dinosaurs)

`notes/target-vtable-runtime.md` (x32dbg trace, 32.4 s of live behaviour):

| class | vtable | scorer (`0x56e455`) | gate (`0x56d9db`) |
| --- | --- | --- | --- |
| no-target | `0x006430AC` | 414 hits (Exploration/RunAway/Sleep), `tx/ty=-1` | 139 |
| actor | `0x006430E4` | 159 hits, `DinoAttackForCarniv`, real `tx/ty` | 571, `DinoAttackForHerbiv` |
| box | `0x00642FFC` | 0 in the window | 446, `Eat` |

- `0x521d80` (dino reaction) fired **20 times**, every target
  `vt=0x006430E4`; `0x521d10` (visitor) fired 0. The direct reaction path is
  live even though the `+0xb8` list has no writer.
- `target+0x28` is 0 for no-target/actor; for box it is the show-element
  pointer. `target+0x38` is the owner context; `[[target+0x38]+0x28]` is the
  perceived actor that the actor class `+0x14` reads.
- At template-fill time `[action+0x08]` is a **heap wrapper**, not the class
  vtable; read the class at `node+0x3c` in the scorer/gate.
- Free Game IGUANO and the loaded `cppwrite` save have **0 dinos**, so the
  chooser never runs there; Cpg04 is the test park.

