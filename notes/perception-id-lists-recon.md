# Perception id lists on the actor+0x270 behaviour block

Status: **consumers decoded and corrected; no runtime producer exists in this
build.** All addresses are `DinoIsland.exe` (base 0x400000), Ghidra project
`/DinoIsland.exe-ebea17`. Follow-up to `notes/perception-reaction-recon.md`
and `notes/dino-target-vtable-recon.md` section 8.

Method note: the three "lists" are *container sub-objects* (vptr + begin/end/
cap), not bare vectors, so a plain data-flow grep is not enough. Every
`mov [reg+disp], reg` in `.text` was enumerated instruction by instruction
(PE mapped image, capstone, 0x401000-0x623f34) for `disp` in
{0xb8,0xbc,0xc8,0xcc,0xf4,0xf8} and the candidates were resolved to their
enclosing Ghidra function. The result is the table in section 2: **no store
to any of the six displacements exists in the AI/park-behaviour range**, and
the only stores found anywhere belong to unrelated classes (ParkView UI,
mcLib containers, etc.).

## 1. Reached object and its layout

The object the whole reaction path hangs off is **not** a per-actor
`apDinosaurBehavior`: it is the block reached as `action_instance + 0x270`.

Chain (all verified by decompilation):

```text
FUN_0053cfb0   (.bai action-record factory)
  uVar1 = FUN_005868f0(.bai record block, ...)     ; parsed .bai data
  uVar2 = FUN_0051fe20(uVar1, ...)                 ; 0x274-byte action instance
                                                      pushed into manager+0x14
FUN_0051fe20
  obj = operator new(0x274)                        ; ParkBehavior.cpp:0x280
  obj = FUN_0056bc40(uVar1, ...)                   ; ctor
FUN_0056bc40  (0x56bc40)
  FUN_00569e00(a2..a7)                             ; sub-object ctors
  obj[0x9c] = a2       ; 0x0056bc69  ==>  obj+0x270 = a2 (.bai block)
```

So `*(action_instance + 0x270)` is the parsed `.bai` behaviour block. Layout
(I = int dword, B = byte):

```text
+0x014 B  state/stage (==1 gates the park reaction passes)
+0x0d6 B  visitor-reaction enabled
+0x0d8 B  "run chooser A maintenance"  (FUN_0056bcb0/FUN_0056ab00)
+0x0d9 B  ) either one -> FUN_0051f8d0
+0x0da B  )
+0x0db B  scan the +0x108 list and stop reactions on visible entries
+0x0b4 I  container vptr        +0x0b8 I begin  +0x0bc I end  +0x0c0 I cap
+0x0c4 I  container vptr        +0x0c8 I begin  +0x0cc I end  +0x0d0 I cap
+0x0f0 I  container vptr        +0x0f4 I begin  +0x0f8 I end  +0x0fc I cap
+0x104 I  container vptr        +0x108 I begin  +0x10c I end  +0x110 I cap
```

Container encoding proof: `FUN_0053d4d0` (0x53d54a..0x53d57b) does
`LEA ECX,[ESI+0xb8]` + `CALL FUN_00520e70`, and `FUN_00520e70` is

```c
void init(void *p) { p[0]=0; p[1]=0; p[2]=0; /* + byte */ }
```

so `+0xb4` is the vptr and `+0xb8/+0xbc/+0xc0` are begin/end/cap. The same
template dtor shape appears in `FUN_00544330` (`*this = vptr; free(this[1])`).
Vtable values seen for this template family: `0x00642f04..0x00642f50`,
`0x00642fb4..0x00642fc4`, `0x006435a0..` etc. (dense, 4-byte staggered).

`FUN_00569e00` (the 0x274 instance ctor) creates four more containers at
`+0x04 / +0x14 / +0x24 / +0x38` and embeds a no-target policy at `+0xa4`
(`FUN_0056edc0`, vtable `0x006430ac`).

## 2. Writers: there are none

| list | container | writer | pushed value | trigger | consumer |
| --- | --- | --- | --- | --- | --- |
| visitor reaction, `+0xf4` | vptr `+0xf0`, begin/end `+0xf4`/`+0xf8`, cap `+0xfc` | **none** (0 store sites) | would be `E+0x08` (scene-entity actor ptr) | would be per-tick `FUN_0051fab0` stage 4 / `FUN_0051f810` | `FUN_0051fab0` @0x51fc96 -> `FUN_00521d10`; `FUN_0051f810`; cleanup `FUN_0051f8d0` |
| dino reaction, `+0xb8` | vptr `+0xb4`, begin/end `+0xb8`/`+0xbc`, cap `+0xc0` | **none** (0 store sites) | would be `E+0x08` | would be `FUN_0051fab0` stage 2 / `FUN_0051f950` | `FUN_0051fab0` @0x51fb7e -> `FUN_00521d80`; `FUN_0051f950` |
| known targets, `+0xc8` | vptr `+0xc4`, begin/end `+0xc8`/`+0xcc`, cap `+0xd0` | **none** (0 store sites) | would be `E+0x08` | would be per-tick | `FUN_0051fa10` -> `FUN_0055ae30` + `FUN_00521dd0` (stop) |
| `+0xdb` scan list, `+0x108` | vptr `+0x104`, begin/end `+0x108`/`+0x10c` | **none** (0 store sites) | would be `E+0x08` | would be per-tick | `FUN_0051fab0` @0x51fcfc -> `FUN_00521d60` |

The only vptr installs at `+0xb4`/`+0xc4` in the whole image are in the
`.bai` structure ctor `FUN_0053d4d0` (0x53d55a `= 0x00642f18`, 0x53d57b
`= 0x00642f14`) and `FUN_0053d6d0` - i.e. the containers exist because the
data model defines them, not because the AI writes them.

Whole-image store census at those displacements (4-byte, index=0,
non-`[esp+...]`): 0xb8 -> 40 hits, 0xbc -> 37, 0xc8 -> 43, 0xcc -> 48,
0xf4 -> 12, 0xf8 -> 12. Resolving every one of them to its function gives
ParkView / UI element pointers (`0x00523eed`, `0x00523f2b`, `0x005236cd`,
`0x00527615`...), mcLib containers (`0x498f4d`, `0x4bc146`, ...) and
`0x0055xxxx` actor code - **none of them is the `+0x270` block**. The only
`+0xb4`/`+0xc4` vptr installs in the whole image are `FUN_0053d4d0` and
`FUN_0053d6d0` (`.bai` ctor), `FUN_00587af0`/`FUN_005874a0` (`+0xf0`,
`FUN_005875e4`) and `FUN_005277xx` UI code.

Consequence: no code ever adds an element, so both guards are false. The
zero state itself is *inferred*: every ctor of this family uses
`FUN_004052c0(size,1,0,1,file,line)` while the vector-growth path uses `...,3,...`, and that 4th argument is the only difference; if it were not a
zeroing allocator the `begin != end` guards would read heap garbage, so
zero-init is the only consistent reading (one debugger read of
`[beh+0xb8]`/`[beh+0xbc]` settles it). With empty lists:

- `FUN_0051fa10` (known targets, `+0xc8`) never runs its body,
- the dino branch `+0xb8` -> `FUN_00521d80` never fires,
- the visitor branch `+0xf4` -> `FUN_00521d10` never fires,
- `FUN_0051f8d0` / the `+0x108` scan never fire.

`FUN_0055ae30` (perception-memory forget, `ECX = actor+0xa4`, arg = entry) has
exactly two xrefs, both inside these dead bodies (0x51fa80, 0x51f924).
The only live perception-memory *insert* is `FUN_0055ad00`, reached solely
from `FUN_0056ac80` (i.e. **only when a reaction actually starts**), which is
another reason the loop is closed: no start -> no remember -> no list.

## 3. Consumers (corrected ABI)

`FUN_0051fab0(ECX = manager, dt)` is the per-tick driver. It iterates two
0x274-instance vectors (`manager+0x14` and `manager+0x24`, both filled by
`FUN_0051fe20/0x51ff90/0x520060/0x5202c0/0x520550`) and one
"behaviour slot" vector `manager+0x04`:

```c
// 0x51fad1  stage 1 - known targets
beh = *(instance + 0x270);
if (beh->+0x14 == 1 && beh->+0xc8 != beh->+0xcc)
    FUN_0051fa10(manager, beh, dt);

// 0x51fb23  stage 2 - dino reaction list
beh = *(instance + 0x270);
if (beh->+0x14 == 1 && beh->+0xb8 != beh->+0xbc)
    for each slot E in manager+0x04:
        if (find(beh->+0xb8, beh->+0xbc, E->+0x08) != end)
            FUN_00521d80(E, beh, instance);          // ECX=E, arg1=beh, arg2=instance
// 0x51fbbb  stage 3 - instance->vtable[+0xc](dt)
// 0x51fbe2  clear every E->+0x08->+0x4f9
// 0x51fc30  stage 4 - visitor reaction list (same shape, +0x14/+0xd6/+0xf4)
//           -> FUN_00521d10(E, beh, instance)
// 0x51fcbe  stage 5 - +0xd8 -> FUN_0056bcb0/FUN_0056ab00 on the instance
// 0x51fcdd  stage 6 - +0xdb -> for id in beh->+0x108: if (E->+0xc)->+0x15==0
//                                  FUN_00521d60(E)
// 0x51fd3e  stage 7 - +0xd9||+0xda -> FUN_0051f8d0(manager, E)
// 0x51fd63  every E gets (**(E->vtable+0xc))(dt)
```

`E` (an element of `manager+0x04`) is the 0x18-byte **behaviour slot** built by
`FUN_00521b30` and registered by `FUN_0051f5c0`:

```text
+0x00 vptr 0x00642abc     +0x04 grid (FUN_00572ec0/FUN_00572fa0)
+0x08 owner actor         +0x0c chooser A (visitor, vptr 0x00643040)
+0x10 chooser B (dino, vptr 0x0064302c)
+0x14 state: 3 idle, 1 dino reaction, 2 visitor reaction
```

Reaction starters (both `__thiscall`, `RET 0x8`, i.e. two stack args):

```c
// FUN_00521d10  visitor, state 2  (0x521d10)
void start(E, beh /*arg1*/, P /*arg2*/) {
    E->+0x14 = 2;
    FUN_0056d7d0(E->+0x0c, beh);        // chooser->+0x10 = beh; chooser->+0x15 = 0
    E->+0x0c->+0x04 = (char *)P + 4;    // 0x00521d35
    FUN_0056e190(E->+0x0c, P + 0x24);   // build nodes from P's action list
    FUN_0056ac80(P, E->+0x0c);          // ECX=P, arg=chooser
}
// FUN_00521d80  dino, state 1 - identical with E->+0x10
// FUN_00521d60(E)  state 2 -> 0, FUN_0056b550(E->+0x0c, 1)
// FUN_00521dd0(E)  state 1 -> 0, FUN_0056b550(E->+0x10, 1)
```

`P` = the 0x274 action instance. `FUN_0056ac80` (asAttractionBehavior.cpp)
then: appends the chooser to `P+0x1c`, reads `chooser->+0x28` = the **actor**,
copies the 0xac records of `*(P+0x268)+0x10..+0x14` into new instances, builds
the target with `FUN_0056f620(ctx, chooser, P+0xa4)` (vtable `0x006430e4`,
i.e. the already-ported `ActionTarget`), sets `chooser->+0x24 = P+0x48`,
`chooser->+0x20 = 0`, calls `FUN_0055ad00` (perception-memory remember) and
`actor->+0x4cc = P + 0xa4`.

`FUN_0051fa10(ECX=manager, beh, dt)` is the *stop* side of the dino list:

```c
for each slot E in manager+0x04:
    if (find(beh->+0xc8, beh->+0xcc, E->+0x08) != end) {
        FUN_0055ae30(actor+0xa4, E->+0x08);   // forget in perception memory
        FUN_00521dd0(beh);                    // stop dino reaction       <-- dead: list empty
    }
```

`FUN_0051f810` / `FUN_0051f950` (`ECX=manager, beh, actor`) are the *start*
side driven from the entity tick `FUN_0051e6c0` (which calls
`FUN_0051f950(*(e+0x4c), *(e+0x54))` and `FUN_0051f810(*(e+0x5c), *(e+100))`
for every scene entity, 0x51e7xx). Same find -> `0x521d10` / `0x521d80`.
They are dead for the same reason.

## 4. The `+0x15` byte is a chooser flag, not actor visibility

Correction to `notes/perception-reaction-recon.md`: `E+0x0c` is **chooser A**,
not a visual object, and `+0x15` is a chooser field, not a hidden flag:

```c
FUN_0056cc90 (chooser base ctor, vptr 0x00643094)
    +0x0c list sentinel  +0x14 byte = 1  +0x15 byte = 0  +0x28 = actor
FUN_0056d7d0 (0x56d7d0, called by both starters)
    *(u8 *)(this + 0x15) = 0;
    *(int *)(this + 0x10) = arg;       // perceived block
```

So `(chooser+0x15) == 0` reads "target not acquired / lost". It is cleared on
every (re)assignment, so the `FUN_0051f8d0` body would stop a reaction that
was just started unless the chooser update raises it. Measured: the whole
image has 23 byte stores at displacement 0x15, **none** inside the AI range
0x51d000-0x525000, so no park-side code ever raises this flag - consistent
with the whole path being inert.

The single genuinely live visibility-ish byte on the entity side is
`E->+0x08->+0x4f9`, cleared once per tick by `FUN_0051fab0` (0x51fbfd).

## 5. Despawn / removal path

`FUN_0051f720(ECX=manager, entry)` is the removal path (ParkBehavior.cpp:0x16d):

```c
i = find(manager+0x04, entry);
beh = *(*(entry + 0x10) + 0x10);          // one owner block
if (beh) FUN_00521dd0(slot_of(beh));      // stop dino reaction
beh = *(*(entry + 0x0c) + 0x10);
if (beh) FUN_00521d60(slot_of(beh));      // stop visitor reaction
FUN_00521bf0(entry);                      // ~apBehavior base
free(entry);                              // memmove out of manager+0x04 vector
```

No entry is ever erased from `+0xb8/+0xc8/+0xf4` because none is ever added.

## 6. Port contract

1. Model `beh+0xb8/+0xc8/+0xf4/+0x108` as **always-empty** lists. Do not
   invent a producer; retail cannot fill them (whole-image store census).
2. Keep the reaction state machine and the *consumers* as dead-but-correct
   structure guards:
   `if (state == 1 && !list.empty()) ...` - they never run.
3. The reaction entry points to port are the **direct** ones, called with
   `(slot, perceived_block, action_instance)`:
   `start_visitor()` -> state 2 on `slot+0xc`; `start_dino()` -> state 1 on
   `slot+0x10`; `stop_visitor/dino` -> state 0 + `chooser_stop(chooser, 1)`.
4. Chooser fields to reproduce: `+0x04 = perceived_block+4`, `+0x10 =
   perceived_block`, `+0x15 = 0` on start, `+0x28 = actor`, `+0x24 =
   perceived_block+0x48`, `+0x20 = 0`; node list built from
   `perceived_block+0x24`; target from `perceived_block+0xa4` (already ported
   as `ActionTarget`, vtable `0x006430e4`).
5. Perception memory (`actor+0xa4`): only `FUN_0055ad00` (add, vector `+0x1b4`
   of 4-byte entries + 0x10-byte records at `+0x34`, caller
   `FUN_0056ac80`) and `FUN_0055ae30` (remove, callers are the dead bodies).
   So in the port memory is only written when a reaction starts.
6. Do **not** gate anything on `beh+0x14==1` alone without also checking the
   lists - retail checks both, and the list test always fails.

## 7. Open

- Why the `.bai` block carries reaction tables that nothing fills: either the
  tables are filled by an out-of-process editor path (`.bai`/`.bbi` tools) or
  the retail build has the perception-producer feature disabled. A runtime
  breakpoint on `0x51fb2f` (read of `beh+0xb8`) in a loaded retail park would
  confirm the lists are empty at 0x0 == 0x0 (cheap, one breakpoint).
- Whether `0x56ac80` is also the show path (its asserts are
  `asAttractionBehavior.cpp`); if so the park reaction reuses the show action
  controller verbatim, which the port already has.

## 8. Evidence index

| claim | evidence |
| --- | --- |
| `+0x270` = `.bai` block | `0x56bc69`, `FUN_0051fe20` <- `FUN_0053cfb0` <- `FUN_005868f0` |
| container = vptr/begin/end/cap | `0x53d54a..0x53d57b` + `FUN_00520e70`; dtor `FUN_00544330` |
| no producers | full `.text` store census (section 2); only vptr installs at `+0xb4/+0xc4` are `FUN_0053d4d0`/`FUN_0053d6d0` |
| start ABI | `0x521d10`, `0x521d80`, call sites `0x51fb9c`, `0x51fcb4` |
| state/stop | `0x521d60` (2->0), `0x521dd0` (1->0) |
| chooser layout | `FUN_0056cc90`, `FUN_0056cb80/0x56ca80`, `FUN_0056d7d0` |
| slots built/registered/removed | `FUN_00521b30`, `FUN_0051f5c0` (0x18 alloc, push `+4..+0xc`), `FUN_0051f720` |
| per-frame driver | `FUN_0051fab0`, `FUN_0051e6c0` |
