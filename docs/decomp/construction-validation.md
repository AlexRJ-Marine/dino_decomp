# Construction validation and the `FENCE_INSERTION` state

Status: **research decoded; port implementation pending.** This is the
game-logic companion to `docs/platform/build.md` (port grid) and
`docs/decomp/economy.md` (budget API). Raw evidence:
`notes/fence-insertion-recon.md`, `notes/construction-validation.md`.

Method names are descriptive; the binary has no symbols. Only
`FENCE_INSERTION` and the `stFence.cpp` / `spDinosaurStore.cpp` assert paths
are recovered strings.

## Two independent checks

| Gate | Function | Failure |
| --- | --- | --- |
| affordability | `FUN_0052bed0` | `&WARNING_NOT_ENOUGH_MONEY` |
| paddock capacity | `stFence::CanAddActor` `0x57eb50` | `&WARNING_NO_ROOM_IN_FENCE` |

`FUN_0052bed0` reads the park budget through
`[[[[this+0x44]+0x10]+0x14]+0x120]+8` and compares it with the cost argument.
It does **not** deduct; the caller's build path does (walls:
`FUN_00585d20` with the `.bwi` `+0x0c` cost; dinosaurs:
`FUN_00585cc0` with the store price).

## `FENCE_INSERTION` (`pvwStateFenceInsertion`)

`0x535b10` is the state constructor: `FUN_0052bd10(0x12, "FENCE_INSERTION")`,
vtable `0x642d80`. The virtual slots are dtor `0x535b40`, enter `0x535b70`,
leave `0x535bb0` and validate/apply `0x535be0`.

The state places a **dinosaur** into a paddock. It handles both a store
purchase (when `tool+0x3c` is set) and an already-owned park dinosaur:

1. Validate with `stFence::CanAddActor`.
2. Compute the placement point with `stFence::PlaceActor`.
3. For a store purchase: charge the price (`FUN_00585cc0`) and remove the
   store row (`spDinosaurStore`, `FUN_00594270`).
4. Register the dinosaur in the paddock (`stFence::AddActor`, `0x57e8a0`)
   or in the park (`FUN_0058f430`, when `tool+0x3c == 0` and the list
   object's `+0x1c == 0`).

## Warning path (corrected)

The capacity/placement failure (`0x535e68` / `0x535ece` → `0x536017`) does
**not** set state 5 and does **not** leave the state. It builds
`&WARNING_NO_ROOM_IN_FENCE` (`0x6647cc`), copies the formatted text to
`this+0x2c`, calls `FUN_005e3d70([tool+0x48], ...)` (the message/panel
object), cleans up the tool/list selection and returns. The insertion state
stays active.

The early selection failures (`0x535c83`, `0x535ce4`) and the park branch
(`0x535f43`) do set `[this+0x18] = 5` and call `[edx+8]` (slot 2 = leave
`0x535bb0`).

## Paddock capacity

`stFence` stores its capacity model at:

```text
+0x78  float  fill ratio (used / capacity)
+0x7c  float  capacity
+0x80  float  used
+0x84  float  cost for index 0 = 8
+0x88  float  cost for index 1 = 16
+0x8c  float  cost for index 2 = 32
+0x90  float  cost for index 3 = 64
+0x98  vector<dinosaur*>  occupants (4-byte elements)
```

The constructor `0x57dc60` derives capacity from the `drsFence` start/end
rectangle (`+0x18`/`+0x20` in the world record) and the cell size:

```text
capacity = ((x1 - x0 + 1) / cell) * ((y1 - y0 + 1) / cell)   // float division
```

The cell size is passed as a pointer (third constructor argument) and
dereferenced. The same value is used by the wall rasteriser (`stWall.cpp`,
`FUN_00584200`); its caller is `stPerimeter::AddSegment` (`0x582210`). The
port's 4-unit grid (`floor(wx/4)`) is the working value.

`CanAddActor` (`0x57eb50`):

```text
if dinosaur is already in the occupant vector: return false
free = capacity - used
cost = cost_table[dinosaur[0x1e0]]
return free >= cost
```

The occupant vector uses 4-byte pointers and a 4-wide unrolled scan; it is
**not** a 16-byte record array. `AddActor` (`0x57e8a0`) appends the dinosaur,
adds its cost to `used` and stores `fill = used / capacity`. `CanAddActor`
therefore rejects both a full paddock and a duplicate insertion.

The index at `dinosaur+0x1e0` is an integer in this path. A freshly
constructed actor has it zeroed (`FUN_004bc690`, `0x4bc9e8`), which selects
cost 8. On the `saDinosaur` save layout (`0x573590`) the same offset is a
`0x18`-byte vector cursor/end pointer, and attribute id 29 (`0x572620`) returns
it as a stat. Its range and meaning are **unconfirmed**; use index 0 until a
runtime dump settles it.

## Placement point

`stFence::PlaceActor` (`0x57ec00`) has two modes:

- `param_5 == 1` (the `0x535be0` path): the caller already computed the point
  from the construction tool (`FUN_005fc4d0`); the function clamps it into
  the fence rectangle and randomises the direction (0..359 degrees). There is
  no random-point search.
- `param_5 == 0` (other callers): a 50-iteration random-point loop exists,
  but its success flag is set to 1 at `0x57ee44` and never cleared. The
  failure branch is unreachable and the function always returns 1. The
  `0x57f220` cell test does not gate the return value.

The port's "random buildable cell with retry" is a deliberate approximation;
the retail point is the player's ground pick. Map the cell test to the `.raw`
occupation mask (0 = buildable, 255 = blocked/water) if a fallback point is
needed.

## Purchase branch

When `tool+0x3c` is set:

```text
cost = tool+0x40                              ; store price
FUN_00585cc0(budget_category, cost)           ; budget -= cost, count++, total += cost
FUN_00594270(dinosaur_store, tool+0x38)       ; remove the sold store row
tool+0x38 = -1
```

`FUN_00585cc0` is a generic category setter: `category+0x4 = 1`,
`budget+0x8 -= cost`, `category+0x9c += 1`, `category+0xa0 += cost`. The
dinosaur-purchase row is the `DINOACHAT` bucket from
`docs/decomp/economy.md`. `FUN_00594270`'s second argument is the **store row
index**, not the cost.

## Port implications

`src/world/build.cpp` currently has no paddock-capacity model. To match the
original construction flow:

1. Treat each closed fence rectangle as a paddock with
   `capacity = (w/4.0f) * (h/4.0f)`.
2. Give each dinosaur a cost of `8 << index` (index 0 = 8 until the runtime
   field is confirmed).
3. Reject an insertion when the dinosaur is already inside or
   `capacity - used < cost`, show `WARNING_NO_ROOM_IN_FENCE`, and **keep the
   insertion state active**.
4. On success update `used`/`fill`, charge the store price to `DINOACHAT`,
   remove the store row, and place the dinosaur at the player's picked point
   inside the rectangle.
5. The 64-byte-record spatial test at `0x535c97` is still undecoded; do not
   claim full parity for paddock placement until it is resolved.

## Open

- Runtime-dump `[dinosaur+0x1e0]` at `0x57ebc3` to confirm the cost index
  range and meaning.
- Runtime-dump `*(uint*)(landscape+0xc)` at `0x57dc60` entry to confirm the
  cell size is 4.
- Decode the `0x535c97` 64-byte-record branch (`record+0x24 * 0.5 <
  record+0x20`).
- Whether path tiles charge `CONSTRUCTION` (only the wall path and the
  dinosaur store price are proven budget setters).

## Evidence

- `notes/fence-insertion-recon.md` — full disassembly/decompilation log.
- `notes/construction-validation.md` — budget gate and first pass.
- `docs/decomp/economy.md` — budget setters and category mapping.
- `docs/platform/build.md` — port grid and placement rules.
