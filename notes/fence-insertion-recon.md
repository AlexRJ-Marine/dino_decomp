# FENCE_INSERTION validator (`0x535be0`) — dinosaur → paddock placement

Status: **decoded**. The `&WARNING_NO_ROOM_IN_FENCE` condition is a **paddock
capacity** test, not a per-cell construction rule. Complements
`notes/construction-validation.md` (budget gate) and `docs/decomp/economy.md`.

## Class and call site

| Item | Address / value |
| --- | --- |
| State ctor | `0x535b10` — `FUN_0052bd10(0x12, "FENCE_INSERTION")` |
| vtable | `0x642d80` (slot 0 `0x535b40` dtor, slot 1 `0x535b70` enter, slot 2 `0x535bb0` leave, slot 3 `0x535be0` validate/apply) |
| Warning string | `&WARNING_NO_ROOM_IN_FENCE` at `0x6647cc` |
| Budget gate | `0x52bed0` (already documented in `notes/construction-validation.md`) |
| Park spawn wrapper | `0x586920` → `0x588d50` → ctor `0x57dc60` |
| World fence record → spawn | `0x53d0b0` / `0x53cfb0`, call at `0x53d0df` / `0x53cfdf` |

`0x535b10` sets `[esi] = 0x642d80`, `[esi+0x50] = arg`. Slot 1 (`0x535b70`)
calls the base init, copies `[[[esi+0x44]+0x10]+0x14]+0xbc` into `this+0x58`,
calls `FUN_0052cc10(this+0x40, 4)`, clears `this+0x48`/`this+0x4c` and sets
`[[this+0x44]+0x50] = 1`.

`this+0x44` is the shared construction tool; `this+0x50` the selected
list/store object. The method is called with one stack argument (`RET 4`).

## Common validate/apply path

The method has three entry branches selected by tool bytes `+0x3d`, `+0x48`
and `this+0x38`. All of them converge on the same final check
(`0x535e54..0x535e6c`, bytes from the live binary):

```asm
00535e54  MOV EAX,[ESI+0x4c]        ; record created from the tool selection
00535e57  MOV ECX,[ESI+0x54]        ; fence object created from the list row
00535e5a  MOV EDI,[EAX+0x34]        ; dinosaur object
00535e5d  MOV ECX,[ECX+0x4c]        ; stFence
00535e60  PUSH EDI
00535e61  CALL 0x0057eb50           ; stFence::CanAddActor
00535e66  TEST AL,AL
00535e68  JZ 0x00536017             ; -> WARNING_NO_ROOM_IN_FENCE
...
00535ec7  CALL 0x0057ec00           ; stFence::PlaceActor
00535ecc  TEST AL,AL
00535ece  JZ 0x00536017             ; -> WARNING_NO_ROOM_IN_FENCE
```

On failure the state is set to 5 (`[esi+0x18] = 5`) and virtual slot `+8` is
called; that is the warning dialog path. On success the optional purchase
branch runs and then `stFence::AddActor` registers the dinosaur.

## `stFence::CanAddActor` (`0x57eb50`)

```text
if (dinosaur is found in fence+0x98 .. fence+0x9c)   // 16-byte records, any of 4 dwords
    return false;
free = *(float*)(fence+0x7c) - *(float*)(fence+0x80);
cost = *(float*)(fence + 0x84 + dinosaur[0x1e0]*4);
return free >= cost;
```

Exact assembly of the field math (`0x57ebc3..0x57ebde`):

```asm
MOV EAX,[EDI+0x1E0]
FLD  dword ptr [ECX+0x7C]       ; capacity
FSUB dword ptr [ECX+0x80]       ; minus used
FCOMP dword ptr [ECX+EAX*4+0x84]; compare with cost[class]
```

The occupant vector at `+0x98` is a `vector<int*>` in 16-byte groups; the scan
compares four dwords per group (`0x57eb71..0x57eb99`) and then the tail
(`0x57eb9b..0x57ebbb`). Being already registered returns **false** (i.e. the
same dinosaur cannot be inserted twice).

## Capacity and cost table

`stFence` ctor `0x57dc60` (decompiled):

```text
capacity(+0x7c) = ((p1.x - p0.x + 1) / cell) * ((p1.y - p0.y + 1) / cell) * 1.0
used(+0x80)     = 0
fill(+0x78)     = 0
cost table:
    +0x84 = 8.0f
    +0x88 = 16.0f
    +0x8c = 32.0f
    +0x90 = 64.0f
```

`p0`/`p1` are the `drsFence` start/end rectangle at file record `+0x18` /
`+0x20`; `cell` is `*(uint*)(runtime_landscape_record + 0xc)`, passed as the
third ctor argument. The same value is the wall-rasteriser cell size in
`stWall` (`0x584200`). The port's 4-unit build grid (`floor(wx/4)`) is the
working value; a runtime dump of this field would close the last doubt.

`stFence::AddActor` (`0x57e8a0`) appends the dinosaur pointer to `+0x98` and
does:

```text
used(+0x80) += cost_table[dinosaur[0x1e0]]
fill(+0x78)  = used / capacity
```

So `+0x1e0` is a **cost class** (0..3) in this path. Note: the `saDinosaur`
serializer (`0x573590`) treats `+0x1e0` as a vector end pointer, and attribute
id 29 (`0x572620`) returns it as a stat. The fence path only makes sense for
a small integer (the table has four entries). Runtime confirmation is listed
as the one open item below; the port uses class 0 (cost 8), which is what the
zero-initialising actor ctor (`0x4bc9e8`) produces.

## `stFence::PlaceActor` (`0x57ec00`)

- Up to 50 random attempts (`CMP [ESP+0x4C],0x32`, `0x57ec1b`).
- Random point inside the fence rectangle, inset by 1/100 of the longer side
  (`0x57ec8c..0x57ed34`): `x = rand() % (max_x - 2*pad - min_x) + min_x + pad`,
  likewise `y`; random direction 0..359 degrees.
- Each candidate is tested with `0x57f220` against the pathmap cell
  (`byte[column[x] + y*2]` high nibble). Blocked candidates retry.
- After 50 failures the assert string `Last Chance for placement` fires; the
  function still returns 1, so no extra failure mode is exposed.
- On success it calls `FUN_00571f00`.

Port equivalent: pick a random point in the paddock rectangle, accept when the
`.raw` occupation mask cell is buildable (0), retry; do not gate on mask
sub-classes.

## Purchase branch (success)

`0x535ed4` checks `tool+0x3c`. When set:

```text
cost = tool+0x40
FUN_00585cc0([[[tool+0x10]+0x14]+0x120], cost)   ; budget spend + category counters
FUN_00594270([[[tool+0x10]+0x18]], tool+0x38)    ; spDinosaurStore::Remove(store index)
tool+0x38 = -1
```

`FUN_00585cc0` decrements `budget+0x8`, increments category count `+0x9c` and
category total `+0xa0`. `FUN_00594270` is `spDinosaurStore` removal (assert
path `spDinosaurStore.cpp`). This confirms `0x535be0` places a **purchased**
dinosaur into a paddock.

Then it either registers the dino into the park (`FUN_0058f430` /
`0x51f950` / `0x58f210`) or into the fence (`FUN_0057e8a0`, `FUN_0057f250`),
marks `this+0x48 = 1`, and clears `[[this+0x4c]+0x30]+0x10`.

## The first branch (`tool+0x3d != 0`) is not the cell rule

`0x535beb..0x535c94` only handles a pre-selected footprint: it walks a vector
of 16-byte records at `[[tool+0x10]+0x14]+0x9c`, finds a record whose dword is
already present in `[tool+0x34]`'s 16-byte vector (`FUN_0057ead0`), then calls
`FUN_005f6fb0` (a generic 4-dword vector find) and warns if the returned
pointer is the end. This is UI selection bookkeeping, not a per-cell
buildability test.

## Port recipe (P4)

1. `paddock_capacity = (w_world/4) * (h_world/4)` for the fence rectangle.
2. `cost(dino) = 8 << class`, class 0 default.
3. Insert only if the dino is not already in the paddock and
   `capacity - used >= cost`; otherwise raise `WARNING_NO_ROOM_IN_FENCE`.
4. Add: `used += cost`, `fill = used/capacity`.
5. Counters: `DINOACHAT` (or the store category) `+count`, `+cost`; remove the
   store row and deduct the store price.

## Open

- Runtime-dump `[dino+0x1e0]` at `0x57ebc3` to confirm the cost class range
  (0..3) and whether it is a size class rather than attribute 29.
- Confirm the runtime landscape record `+0xc` is 4 (the port's grid) by
  dumping it at `0x57dc60` entry.

## Evidence

- Ghidra: `0x535b10` ctor, `0x535b40` dtor, `0x535b70` enter, `0x535bb0`
  leave, `0x535be0` validate; `0x57dc60` stFence ctor; `0x57e8a0`
  AddActor; `0x57ead0` vector membership; `0x57eb50` CanAddActor;
  `0x57ec00` PlaceActor; `0x57f220` cell test; `0x585cc0` budget setter;
  `0x594270` store removal.
- Strings: `FENCE_INSERTION` `0x6647bc`,
  `&WARNING_NO_ROOM_IN_FENCE` `0x6647cc`, `Last Chance for placement`
  (`stFence.cpp` assert).
- Data: `Data/Design/Scenario/Cpg04/Cpg04_Save.wld` fences
  `(452,312)-(467,343)`, `(460,356)-(491,403)`, ... (`.world.fences`).
