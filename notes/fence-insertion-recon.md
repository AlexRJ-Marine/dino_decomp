# FENCE_INSERTION validator (`0x535be0`) — dinosaur → paddock placement

Status: **research decoded; port implementation pending.** The
`&WARNING_NO_ROOM_IN_FENCE` condition is a **paddock capacity** test, not a
cell-mask build rule. Complements `notes/construction-validation.md` (budget
gate) and `docs/decomp/economy.md`.

Method names below are descriptive (the binary has no symbols); only
`FENCE_INSERTION` and the `stFence.cpp` assert path are recovered strings.

## Class and call sites

| Item | Address / value |
| --- | --- |
| State ctor | `0x535b10` — `FUN_0052bd10(0x12, "FENCE_INSERTION")` |
| vtable | `0x642d80`: slot 0 `0x535b40` dtor, slot 1 `0x535b70` enter, slot 2 `0x535bb0` leave, slot 3 `0x535be0` validate/apply |
| Warning string | `&WARNING_NO_ROOM_IN_FENCE` at `0x6647cc` |
| Budget gate | `0x52bed0` (see `notes/construction-validation.md`) |
| stFence spawn | world loader `0x53d0b0` → `0x586920` → `0x588d50` → ctor `0x57dc60` |

`0x535b10` sets `[esi] = 0x642d80` and `[esi+0x50] = arg`. Slot 1
(`0x535b70`) calls the base init, copies `[[[esi+0x44]+0x10]+0x14]+0xbc`
into `this+0x58`, calls `FUN_0052cc10(this+0x40, 4)`, clears `this+0x48` /
`this+0x4c` and stores 1 through `*(*(this+0x44)+0x50)`. `this+0x44` is the
shared construction tool; `this+0x50` the selected list/store object.

The `0x53cfb0` fence-record loader is a **different** object
(`0x5868f0 → 0x588ac0 → 0x579440`, a 0x154-byte `spPark.cpp` allocation). It
does not reach the `stFence` ctor.

## Validate/apply branches

The method has three entry branches selected by `tool+0x3d` (`0x535bf2`),
`this+0x48` (`0x535c97`) and `this+0x38` (`0x535cfe`). They converge on the
final capacity/placement check (`0x535e54..0x535e6c`):

```asm
00535e54  MOV EAX,[ESI+0x4c]        ; dinosaur object created from the selection
00535e57  MOV ECX,[ESI+0x54]        ; fence object created from the list row
00535e5a  MOV EDI,[EAX+0x34]        ; dinosaur
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

### Failure path (corrected)

`0x536017..0x5360fa` is the capacity/placement failure path:

- builds the `&WARNING_NO_ROOM_IN_FENCE` string (`0x6647cc`) and calls
  `FUN_0040bfe0` to localise/format it;
- copies the result into `this+0x2c`;
- calls `FUN_005e3d70` with `ECX = [tool+0x48]` (the message/panel object);
- cleans up the tool/list selection and returns.

There is **no** `[this+0x18] = 5` and no leave call on this path; the
insertion state stays active and the player can try again.

By contrast, the early selection failures at `0x535c83` and `0x535ce4` do
set `[this+0x18] = 5` and call `[edx+8]` (slot 2 = leave `0x535bb0`). The
park-registration branch at `0x535f43` also sets state 5 + leave.

## `stFence::CanAddActor` (`0x57eb50`)

```text
if (dinosaur is found in fence+0x98 .. fence+0x9c)   // vector<dinosaur*>
    return false;                                    // already registered
free = *(float*)(fence+0x7c) - *(float*)(fence+0x80);
cost = *(float*)(fence + 0x84 + dinosaur[0x1e0]*4);
return free >= cost;
```

The occupant vector is a `vector<dinosaur*>` with **4-byte** elements. The
search is a `std::find` unrolled four dwords at a time (`SAR ESI,4` at
`0x57eb65`, `ADD EDX,4` per element, then a 0..3 dword tail at
`0x57eb9b..0x57ebbb`). It is **not** a 16-byte record array.

Field math (`0x57ebc3..0x57ebde`):

```asm
MOV EAX,[EDI+0x1E0]
FLD  dword ptr [ECX+0x7C]       ; capacity
FSUB dword ptr [ECX+0x80]       ; minus used
FCOMP dword ptr [ECX+EAX*4+0x84]; compare with cost[index]
```

The index at `dinosaur+0x1e0` is an integer in this path. **Runtime-confirmed:**
a live Cpg04 run logged `DINORESET` for all 20 world dinosaurs at
`0x570fd0` with values

```text
f1e0: 1, 2, 2, 2, 0, 0, 1, 0, 1, 1, 2, 0, 1, 1, 1, 1, 1, 1, 0, 0
```

i.e. six class 0, ten class 1 and four class 2 dinosaurs (the table supports
0..3). At 8/16/32/64 that is a real `used` total of **336** for Cpg04, not
160. The port's imported-dino placement currently passes cost index 0 for
every dinosaur (`paddock_add(..., 0)`), which is a known deviation; the index
is a size class and must be set from the dinosaur's size, not a constant.

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
`+0x20`. The third ctor argument is a **pointer** to the cell value; the
constructor dereferences it (`uVar3 = *param_4`) and divides in **float**.
The same cell value is used by the wall rasteriser (`stWall.cpp`,
`FUN_00584200`); its caller is `stPerimeter::AddSegment` (`0x582210`). The
port's 4-unit grid (`floor(wx/4)`) is the working value.

**Cell-size confirmation (retail data):** all 14 `drsFence` records in the
seven parseable campaign `.wld` files have inclusive world spans
(`x1-x0+1`, `y1-y0+1`) divisible by 4, with no exceptions:

```text
(24,24) x5   (32,32) x3   (32,24)   (16,32)   (32,48)   (32,16)
(24,48)      (24,40)
```

That gives integer cell capacities for every retail paddock at `cell = 4`
(e.g. Cpg04 `(452,312)-(467,343)` = 4 x 8 cells = 32), which closes the
"landscape +0xc" question by data geometry.

**Runtime confirmation:** a live Cpg04 run (`scripts/x32dbg/start-campaign.ps1`)
logged the `stFence` ctor for all nine fences with

```text
FENCE this=... p0=... p1=... cellptr=... cell=00000004
      x0=000001C4 y0=00000138 x1=000001D3 y1=00000157
```

i.e. `cell = 4` and the first rectangle `(452,312)-(467,343)` exactly as in
the `.wld`. No further doubt on the denominator.

`stFence::AddActor` (`0x57e8a0`) appends the dinosaur pointer to `+0x98` and
does:

```text
used(+0x80) += cost_table[dinosaur+0x1e0]
fill(+0x78)  = used / capacity
```

**Load-time placement does not use this gate.** In the live Cpg04 run,
`0x57eb50` was armed before the world load and produced **no hits** while the
20 world dinosaurs were created (the nine `stFence` ctors did run). The
capacity check is therefore the manual-insertion / store-purchase rule
(`FENCE_INSERTION`), not the campaign world-load placement path; loaded
dinosaurs are placed directly into their paddocks.

## `stFence::PlaceActor` (`0x57ec00`)

Two modes, selected by the fifth argument:

- **`param_5 == 1`** (the `0x535be0` purchase path): the caller has already
  computed the placement point from the construction tool (`0x5fc4d0`) and
  passes it in. The function clamps the point into the fence rectangle,
  randomises the direction (0..359 degrees) and marks cells. There is no
  random-point search.
- **`param_5 == 0`** (other callers): a 50-iteration random-point loop
  exists, using `max(dino+0x54, dino+0x58)/100` as the inset, but its success
  flag is initialised to 1 at `0x57ee44` and never cleared. The failure
  branch at `0x57f1c0` is unreachable (Ghidra removes it), and the assert
  block at `0x57ec22/0x57ec39/0x57ec3f` cannot be entered because the loop
  back-edge only continues while the counter is below 0x32. The function
  always returns 1.

The `0x57f220` cell test (`byte[column[x] + y*2] & 0xf0`) controls local scan
behaviour but **does not gate the return value**. The port's "random
buildable cell with retry" is therefore a deliberate approximation, not a
retail rule; the retail placement point is the player's pick.

## Purchase / park branches

`0x535ed4` checks `tool+0x3c` (store purchase flag). When set:

```text
cost = tool+0x40                              ; store price
FUN_00585cc0([[tool+0x10]+0x14]+0x120, cost)  ; budget -= cost, count++, total += cost
FUN_00594270([tool+0x18], tool+0x38)          ; remove the sold spDinosaurStore row
tool+0x38 = -1
```

`FUN_00585cc0` sets `category+0x4 = 1`, `budget+0x8 -= cost`,
`category+0x9c += 1`, `category+0xa0 += cost` (the DINOACHAT bucket).
`FUN_00594270` is the `spDinosaurStore` removal (assert path
`spDinosaurStore.cpp` @ `0x6662a4`); its argument is the store row index,
not the cost.

When `tool+0x3c == 0` and the selected list object's `+0x1c` is also 0, the
code instead calls `FUN_0058f430` (park registration) and finishes without
`AddActor`. So the state handles both a store purchase and an already-owned
park dinosaur; it is not purchase-only.

Otherwise the fence path runs: `FUN_0057e8a0` (AddActor), `FUN_0057f250`
(placement/occupancy side effects), `FUN_0051f950`, `FUN_0058f210`, then
`this+0x48 = 1` and clears `[[this+0x4c]+0x30]+0x10`.

## First branch and `this+0x48`: the placement settle timer (solved)

`0x535beb..0x535c94` (`tool+0x3d`) walks a **4-byte-element** vector at
`[[tool+0x10]+0x14]+0x9c`, calls `0x57ead0` (the same 4-byte membership scan)
against `[tool+0x34]`, then calls `0x5f6fb0` (a generic 4-dword vector find)
over `record+0xa8..+0xac`. If the find returns the end it sets state 5 and
leaves. This is a **list/duplicate selection check**; there is no position or
footprint data in it.

`0x535c97..0x535cf4` (`this+0x48`) is **not a validator and does not affect
paddock capacity**. It is the second half of a successful placement: the
preceding code (the `0x535f4f` branch) has already run `stFence::AddActor`,
`FUN_0057f250` and `FUN_0058f210`, stored `1` in `this+0x48`, and cleared
`[[this+0x4c]+0x30]+0x10`. From the next frame on, this branch walks the
park's **64-byte placement-record vector** (`SAR EAX,6`) at `[this+0x58]`,
matches `record+0x2c` against the placed dinosaur's `saDinosaur`
(`[[this+0x4c]+0x34]`), and tests:

```asm
FLD  float [record+0x24]     ; duration (3.0 s for a type-3 record)
FMUL float [0x62544c]        ; 0.5f
FCOMP float [record+0x20]    ; elapsed time
; 0.5*duration < elapsed  ->  reveal + leave
MOV  EAX, [[this+0x4c]+0x30] ; gpkParkScene node of the drsDinosaur
MOV  byte [EAX+0x10], 1      ; node enabled/visible
MOV  [this+0x18], 5          ; leave FENCE_INSERTION
CALL [vtable+8]
```

The record container is created by `spPark`'s ctor `0x5874a0`:
`park+0xbc = FUN_0058ddc0(park+0x3c, park+0x9c)`. `0x58ddc0` builds an object
with three vectors of 0x40-byte records at +0x04, +0x1c and +0x2c. The record
producers are:

| builder | type `+0x3c` | `+0x24` | fields |
| --- | ---: | ---: | --- |
| `FUN_0058f210` | 3 | 3.0 | `+0x2c` = `saDinosaur`; `+0x00..+0x0c` = footprint w/h and centre; `+0x14..+0x1c` = centre |
| `FUN_0058f430` | 4 | 4.0 | `+0x2c` = `saDinosaur`, `+0x30` = source `stFence`, `+0x34` = target `stFence`, `+0x10..+0x1c` = two points |
| `FUN_0058f670` | 5 | 0.5 | `+0x2c` = `saDinosaur`, `+0x10/+0x14` = one point |

The consumer (the function around `0x58e9e9`) accumulates the frame delta into
`record+0x20`; when the accumulated time crosses half of `record+0x24` it runs
the type action, and when it crosses the full `+0x24` it sets `record+0x28 = 1`
and copies the record into the history vector at container +0x30. Type 3 does
nothing (it is a pure 3-second timer); type 4 performs the deferred fence
transfer (`0x57e980` source removal, `0x57e8a0` AddActor, `0x57f250` placement)
at 2 s; type 5 repositions/re-footprints the dinosaur via `0x570e70` at 0.25 s.

`[this+0x4c]+0x30` is the dinosaur's **gpkParkScene node**, created by
`0x53cd50` -> `0x5f4220` -> `0x507990`; the base ctor `0x508740` initialises
byte +0x10 to 1, and the scene-graph traversal (`0x5f54cf..0x5f54e5`,
`0x5fc225..0x5fc23c`) skips nodes whose `+0x10` is 0. So the placement path
**hides the new dinosaur for the first half of its 3-second placement record
(1.5 s), then reveals it and leaves the state**. The leave handler `0x535bb0`
forces the flag back to 1 if a placement is still pending, so the model can
never stay hidden after the state exits.

Port consequence: capacity/charging do not depend on this branch. If the port
wants retail timing parity it can hide the new model for 1.5 s (or simply show
it immediately, since the port has no click-to-spawn latency). No 64-byte
record needs to be modelled for correctness.


## Port recipe (P4)

1. `paddock_capacity = (w_world / 4.0f) * (h_world / 4.0f)` (float division).
2. `cost(index) = 8 << index`, index from `dinosaur+0x1e0`; use 0 while the
   index semantics are unconfirmed (fresh actor value).
3. Insert only if the dinosaur is not already in the paddock and
   `capacity - used >= cost`; otherwise show `WARNING_NO_ROOM_IN_FENCE` and
   **stay in the insertion state**.
4. Add: `used += cost`, `fill = used / capacity`.
5. On a store purchase: `DINOACHAT count++`, `total += price`,
   `budget -= price`, remove the store row.
6. Placement point: the player's picked point on the ground inside the
   paddock (a random buildable cell is only a stand-in).

## Open

- ~~Runtime-dump `[dinosaur+0x1e0]` at `0x57ebc3`~~ **resolved:** the field is
  a size/cost class 0..3. A live Cpg04 load at `0x570fd0` produced six 0s, ten
  1s and four 2s for the 20 dinosaurs; see above. The remaining work is to
  derive the class in the port's imported-dino placement instead of always
  using index 0.
- ~~Runtime-dump `*(uint*)(landscape+0xc)` to confirm the cell size is 4~~
  **confirmed both by retail geometry and at runtime:** all 14 `.wld` fences
  have inclusive spans divisible by 4, and the live `stFence` ctors logged
  `cell=00000004`.
- ~~Identify the `0x535c97` 64-byte-record type and its `+0x20`/`+0x24` floats~~
  **resolved:** the records are the park's placement/move queue
  (`spPark+0xbc`, container `0x58ddc0`); `+0x20` is elapsed time, `+0x24` the
  record lifetime (3/4/0.5 s), and the branch is the 1.5 s settle/reveal
  timer, not a validator.
- Whether path tiles charge the CONSTRUCTION budget (only the wall path and
  the dinosaur store price are proven budget setters).

## Evidence

- Ghidra: `0x535b10` ctor, `0x535b40` dtor, `0x535b70` enter, `0x535bb0`
  leave, `0x535be0` validate; `0x57dc60` stFence ctor; `0x57e8a0`
  AddActor; `0x57ead0` membership; `0x57eb50` CanAddActor; `0x57ec00`
  PlaceActor; `0x57f220` cell test; `0x585cc0` budget setter;
  `0x594270` store removal; `0x5e3d70` warning panel; `0x5874a0` spPark ctor
  (`+0xbc` record container); `0x58ddc0` container ctor (three 0x40-stride
  vectors); `0x58f210`/`0x58f430`/`0x58f670` record builders; the consumer
  around `0x58e9e9` (time accumulation, half-life action, full-life retire);
  `0x58ef40`/`0x58f090` record save/load; `0x53cd50` -> `0x5f4220` ->
  `0x507990` node creation; `0x508740` node base ctor (`+0x10 = 1`);
  `0x5f54cf`/`0x5fc225` scene-graph visibility tests.
- Strings: `FENCE_INSERTION` `0x6647bc`,
  `&WARNING_NO_ROOM_IN_FENCE` `0x6647cc`, `Last Chance for placement`
  (`stFence.cpp` assert), `stPerimeter.cpp` `0x665f30` (caller),
  `stWall.cpp` `0x665f64` (rasteriser), `spDinosaurStore.cpp` `0x6662a4`.
- Data: `Data/Design/Scenario/Cpg04/Cpg04_Save.wld` fences
  `(452,312)-(467,343)`, `(460,356)-(491,403)`, ... (`.world.fences`).
