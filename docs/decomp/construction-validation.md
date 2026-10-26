# Construction validation and the `FENCE_INSERTION` state

Status: **budget gate, store charge and paddock-capacity validator decoded.**
This is the game-logic companion to `docs/platform/build.md` (port grid) and
`docs/decomp/economy.md` (budget API). Raw evidence:
`notes/fence-insertion-recon.md`, `notes/construction-validation.md`.

## Two independent checks

A construction action goes through two unrelated gates:

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

The state is the "place a purchased dinosaur into a paddock" flow:

1. Validate with `stFence::CanAddActor`.
2. Find a spot with `stFence::PlaceActor`.
3. Charge the store price (`FUN_00585cc0`) and remove the store row
   (`spDinosaurStore`, `FUN_00594270`).
4. Register the dinosaur in the paddock (`stFence::AddActor`, `0x57e8a0`).

Any failure sets state 5 and calls the warning virtual, which shows
`&WARNING_NO_ROOM_IN_FENCE` (`0x6647cc`). There is no per-cell buildability
rule in this path; the cell mask is only used to choose a spawn point.

## Paddock capacity

`stFence` stores its capacity model at:

```text
+0x78  float  fill ratio (used / capacity)
+0x7c  float  capacity
+0x80  float  used
+0x84  float  cost class 0 = 8
+0x88  float  cost class 1 = 16
+0x8c  float  cost class 2 = 32
+0x90  float  cost class 3 = 64
+0x98  vector<dinosaur*>  occupants (16-byte record groups)
```

The constructor `0x57dc60` derives capacity from the `drsFence` start/end
rectangle (`+0x18`/`+0x20` in the world record) and the grid cell size:

```text
capacity = ((x1 - x0 + 1) / cell) * ((y1 - y0 + 1) / cell)
```

`cell` is the third constructor argument, the same cell-size value used by
the wall rasteriser (`stWall`, `0x584200`). The port's 4-unit grid
(`floor(wx/4)`) is the working value.

`CanAddActor` (`0x57eb50`):

```text
if dinosaur is already in the occupant vector: return false
free = capacity - used
cost = cost_table[dinosaur.cost_class]
return free >= cost
```

`AddActor` (`0x57e8a0`) appends the dinosaur, adds its cost to `used` and
stores `fill = used / capacity`. `CanAddActor` therefore rejects both a full
paddock and a duplicate insertion.

## Spawn point

`stFence::PlaceActor` (`0x57ec00`) makes up to 50 random attempts:

- random point inside the fence rectangle, inset by 1 % of the longer side;
- random direction 0..359 degrees;
- candidate cells are tested with `0x57f220` (pathmap cell flags); blocked
  cells retry;
- after 50 failures the `Last Chance for placement` assert fires, but the
  function still returns success, so the observable failure is still the
  capacity check.

The port equivalent is the `.raw` occupation mask: accept a cell when the
mask is 0 (buildable land) and reject 255 (blocked/water).

## Purchase branch

When the tool's purchase flag (`+0x3c`) is set:

```text
cost = tool+0x40                              ; store price
FUN_00585cc0(budget_category, cost)           ; budget -= cost, count++, total += cost
FUN_00594270(dinosaur_store, tool+0x38)       ; remove the sold store row
tool+0x38 = -1
```

`FUN_00585cc0` is a generic category setter: `budget+0x8 -= cost`,
`category+0x9c += 1`, `category+0xa0 += cost`. The dinosaur-purchase row is
the `DINOACHAT` bucket from `docs/decomp/economy.md`.

## Port implications

`src/world/build.cpp` currently has no paddock-capacity model. To match the
original construction flow:

1. Treat each closed fence rectangle as a paddock with
   `capacity = (w/4) * (h/4)`.
2. Give each dinosaur a cost of `8 << size_class` (class 0 = 8 until the
   runtime field is confirmed).
3. Reject an insertion when the dinosaur is already inside or
   `capacity - used < cost`, and surface `WARNING_NO_ROOM_IN_FENCE`.
4. On success update `used`/`fill`, charge the store price to `DINOACHAT`,
   and place the dinosaur at a random buildable cell inside the rectangle.

## Open

- Runtime-dump `[dinosaur+0x1e0]` at `0x57ebc3`: the cost table has four
  entries, but the `saDinosaur` serializer uses `+0x1e0` as a vector end
  pointer. The live object passed to the fence path may be an actor/definition
  whose `+0x1e0` is a small size class. Until confirmed, the port uses class 0
  (cost 8), which is the value produced by the zero-initialising actor ctor
  (`0x4bc9e8`).
- Runtime-dump `*(uint*)(landscape+0xc)` at `0x57dc60` entry to confirm the
  cell size is 4.
- Whether path tiles charge `CONSTRUCTION` (only the wall path and the
  dinosaur store price are proven budget setters).

## Evidence

- `notes/fence-insertion-recon.md` — full disassembly/decompilation log.
- `notes/construction-validation.md` — budget gate and first pass.
- `docs/decomp/economy.md` — budget setters and category mapping.
- `docs/platform/build.md` — port grid and placement rules.
