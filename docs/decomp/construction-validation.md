# Construction validation and the `FENCE_INSERTION` state

Status: **research decoded; paddock-capacity model ported; enclosure tool
decoded; purchase/warning UI pending.** This is the game-logic companion to
`docs/platform/build.md` (port grid) and `docs/decomp/economy.md` (budget
API). Raw evidence: `notes/fence-insertion-recon.md`,
`notes/fence-construction-recon.md`, `notes/construction-validation.md`.

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

The index at `dinosaur+0x1e0` is an integer **size/cost class** in this path.
Runtime-confirmed on a live Cpg04 load: the need-reset `0x570fd0` logged
`[dinosaur+0x1e0]` = `1,2,2,2,0,0,1,0,1,1,2,0,1,1,1,1,1,1,0,0` for the 20
world dinosaurs. Correlated with the world order:

| Species | Individuals | Classes |
| --- | ---: | --- |
| coelo | 4 | 0, 0, 0, 0 |
| trood | 2 | 0, 0 |
| anky | 2 | 1, 1 |
| deino | 5 | 1, 1, 1, 1, 1 |
| allo | 7 | 2, 2, 2, 2, 1, 1, 1 |

The class is a per-individual size class (Allosaurus splits 4/3), not a pure
species constant. The table supports 0..3 (8/16/32/64). The value is the
**phenotype length category** (`saDinosaur+0x1e0` <- phenotype `+0x190`).
`FUN_00619830` looks up the body length and the head/tail proportion ratios,
scales each part by `body_length / part_length`, and `FUN_00617250` reduces it
to:

```text
head_ratio = mh * proportion[B0][head] + (1-mh) * proportion[A0][head]
tail_ratio = mt * proportion[B3][tail] + (1-mt) * proportion[A3][tail]
head = (int)(HeadLen[B0][A0] * (head_ratio / HeadLen[B0][A0] * body))
tail = (int)(TailLen[B3][A3] * (tail_ratio / TailLen[B3][A3] * body))
body = Part3Length[Body][B2][A2]
length = head + body + tail          # each part truncated in float32
class  = 0 if length <= 200, 1 if <= 620, 2 if <= 900, else 3
```

where `mh`/`mt` are the Part 2 Head/Tail merge values `[B][A] / 100` and
`proportion.dat` is `[head, arm, body, leg, tail]`. (`TranslatorConstants.ini`
`SizeCategory1..4MaxLength`; the biped flag `P+0x1b4` halves the body term
twice, which only affects odd lengths.) The sum of the individually truncated
parts matters: a single `body * (ratio + 1 + ratio)` expression gives 436/704
for the live 435/702 rows. All 20 live Cpg04 lengths reproduce exactly. The
full chain, the live values and the port implementation are in
`notes/phenotype-length-runtime.md` and `notes/paddock-size-class-runtime.md`.

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

`src/world/build.cpp` now has the paddock model (`Paddock`,
`paddock_capacity`, `paddock_cost`, `paddock_can_add/add`). To match the
original construction flow:

1. Treat each closed fence rectangle as a paddock with
   `capacity = (w/4.0f) * (h/4.0f)`.
2. Give each dinosaur a cost of `8 << index`, where `index` is its size class
   0..3 (runtime Cpg04 values are 0/1/2), not a constant.
3. Reject an insertion when the dinosaur is already inside or
   `capacity - used < cost`, show `WARNING_NO_ROOM_IN_FENCE`, and **keep the
   insertion state active**.
4. On success update `used`/`fill`, charge the store price to `DINOACHAT`,
   remove the store row, and place the dinosaur at the player's picked point
   inside the rectangle.
5. `0x535c97` is **not** a placement rule: it is the post-placement settle
   timer. After a successful insertion the game queues a 3-second type-3
   record in `spPark+0xbc` and hides the new scene node; after 1.5 s it
   reveals the node and leaves `FENCE_INSERTION`. Capacity and charging are
   already finished before this branch. See
   `notes/fence-insertion-recon.md` ("the placement settle timer").

### Port status (2026-10-26)

- Done: `Paddock`, `paddock_capacity` (`(w/cell)*(h/cell)`), the
  8/16/32/64 cost table, the duplicate check, `used`/`fill`, and
  `--selftest` coverage in `src/world/build.cpp`.
- Done: imported `.wld` dinosaurs fill their fence rectangles in record
  order under the capacity rule; Cpg04 = 9 paddocks, capacity 492.
- Done: **exact phenotype size class.** `DinoPhenotypeTable`
  (`src/formats/dino_data.{h,cpp}`) parses `proportion.dat` + the Part 2/3
  blocks; the import and buy paths use
  `length = body_length * (head_ratio + 1 + tail_ratio)` with thresholds
  200/620/900. Cpg04 `used` = **336** (retail exact); the live campaign save
  has the same genotypes as `Cpg04_Save.wld`. `DINO_DEBUG_PHENO=1` prints
  the per-dinosaur length/class.
- Done: the headless purchase core (`--buy-dino NAME`, optional
  `--buy-paddock N`): Market.ini lookup, full 18-gene `.gen` → phenotype
  class, capacity check (`WARNING_NO_ROOM_IN_FENCE`), budget check
  (`WARNING_NOT_ENOUGH_MONEY`), charge and spawn. Cpg04 Zarbosaurus: price
  4,900, budget 10,000 → 5,100, used 352 (336 + its class 1 cost).
- Pending: the store purchase UI and the `DINOACHAT` count/total bucket, the
  store-row removal (`FUN_00594270`), the `WARNING_NO_ROOM_IN_FENCE` panel
  path and the player pickup point.

## `FENCE_CONSTRUCTION` (enclosure / wall tool)

`OPEN_CONSTRUCTION_FENCE` opens the `FENCE_CONSTRUCTION` state
(`0x5265b0`, vtable `0x642b4c`, panel
`FENCE_CONSTRUCTION_AND_INFO_PANEL`). It has two modes:

- **two-click perimeter** (`+0x4a`): the first click stores the corner after
  the `0x58a190` busy/inside check; the second builds the sorted rectangle
  (`0x52bbf0`), previews it (`0x527150`, 16-edge limit) and applies it
  through `0x526f20`: `0x58a4e0` perimeter validation with margin `+0x50`,
  `0x581b90` cost, `0x52bed0` affordability, `0x585d20` CONSTRUCTION charge,
  and `0x53ac60` wall record (type 0x17, two corners at `+0x18..+0x24`).
  `0x581b90` is the perimeter price:
  `((x1+y1)-(x0+y0))*2/cell*price` (fence cells * per-cell price).
- **feature placement** (`+0x48`): a feature selected in the fence list
  (`LunchBox` and the observation-area toggle). The click validates through
  `0x5812b0` + `0x527150`, checks the budget, then spawns the `.bfi` actor
  (`0x53acd0`) and, for `AnimalBait`, the animal actor (`0x53b0d0`).

Both paths end in the `stPerimeter` / `stWall` model: `0x584090` builds the
rectangle's four `stWall` segments (`0x582210`), each rasterised to grid cells
(`0x584200`, `length/cell` 0x28-byte cell records, Bresenham walk). The
occupancy grid is one `u16` per world unit; `0x5849a0` stamps the wall type
into the high nibble (`u16 = (u16 & 0x0fff) | (type << 12)`) and clears it on
removal. Show arenas use the same constructor (`0x580f00` with the `.shw`
`Wall type`), so the enclosure tool and show placement share the wall model.
The port stores fence edges instead of rasterised cells.

Validation codes from `0x58a190`/`0x58a4e0`/`0x5812b0` are shown by
`0x52c2e0` as `&WARNING_BUSY_SPACE`, `&WARNING_UNEVEN_FLOOR`,
`&WARNING_INSIDE_PERIMETER`, `&WARNING_PERIMETER_TOO_SMALL` and
`&WARNING_ROAD_TOO_SHORT`.

Selling (`&REQUEST_SELL_FENCE`, `0x663d4c`) is part of the shared sell
handler. It refuses a paddock whose occupant vector
(`[fence+0x4c]+0x98`) is non-empty with
`&WARNING_SELLING_NON_EMPTY_FENCE`, otherwise refunds
`int(0x58acc0(park, fence) * park_sellback)` and removes it. The port
currently removes fences without the occupant gate or the refund. The panel title/price come from `0x5269e0`, the
`FENCE_STATS_*` values from `0x52c1f0`, and the feature list from
`0x526d70`. `N`/`J` rotate the placement (`+0x3c`); Esc leaves.

Port status: the port arms a two-click fence tool when the enclosure panel
opens and checks the cell, but it does not yet model the retail warning
codes, the perimeter price, the 16-edge preview limit, or the
LunchBox/observation feature path. Raw evidence:
`notes/fence-construction-recon.md`.

## Open

- ~~Runtime-dump `[dinosaur+0x1e0]`~~ **resolved:** size/cost class 0..3;
  live Cpg04 values `1,2,2,2,0,0,1,0,1,1,2,0,1,1,1,1,1,1,0,0`.
- ~~Runtime-dump `*(uint*)(landscape+0xc)`~~ **resolved:** `cell = 4` at
  runtime (all nine Cpg04 stFence ctors logged `cell=00000004`).
- ~~Decode the `0x535c97` 64-byte-record branch (`record+0x24 * 0.5 <
  record+0x20`)~~ **resolved:** it is the 1.5 s node-reveal settle timer for a
  successful placement, not a validator. Records are `spPark+0xbc` placement
  queue entries (types 3/4/5; `+0x20` elapsed, `+0x24` lifetime). The port can
  reveal immediately or mimic the 1.5 s delay; nothing else depends on it.
- ~~Size class chain~~ **solved and ported:** phenotype length with
  `proportion.dat` + Part 2/3 blends, thresholds 200/620/900. The live
  campaign save has the same genotypes as the `.wld`; Cpg04 `used` = 336 in
  the port. All 20 per-dinosaur lengths match the live ctor values exactly
  (`notes/phenotype-length-runtime.md`).
- ~~Whether path tiles charge `CONSTRUCTION`~~ **resolved (decompiled):**
  the `ROAD_CONSTRUCTION` commit (`0x52eb50`) charges
  `FUN_00585d20(cost)`, cost = `(|run| world units * GeneralParams +0x1a4)
  >> 2` = 10 per cell beyond the first. See
  `notes/road-construction-recon.md`.

## Evidence

- `notes/fence-insertion-recon.md` — full disassembly/decompilation log.
- `notes/fence-construction-recon.md` — `FENCE_CONSTRUCTION` state,
  validators, warning table, cost and wall-record path.
- `notes/campaign-unlock-runtime.md` — live Cpg04 trace: `cell=4`, the 20
  dinosaur size classes, and the campaign/progression `dino.dat`.
- `scripts/x32dbg/start-campaign.ps1` — reproducible campaign trace harness.
- `notes/construction-validation.md` — budget gate and first pass.
- `docs/decomp/economy.md` — budget setters and category mapping.
- `docs/platform/build.md` — port grid and placement rules.
