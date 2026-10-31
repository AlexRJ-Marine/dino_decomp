# Eat arrival gate and bite amount (`0x0056c490`, `0x0056f7a0`)

Date: 2026-10-31. Read-only Ghidra pass on the retail
`DinoIsland.exe` (SHA256 `7ba16f38db4fe9461e700e6005e29a37c0d7a6d9441a233880a7c4405bbd717c`).
No runtime run; the Cpg04 port test reproduces the logged values.

## Question

The port gated the LunchBox Eat on `feeder.radius` (`.bfi` width/2) and
drained the whole remaining quantity. What are the retail proximity gate and
the amount passed to `FUN_00576e50`?

## Answer in one line

- **Gate** (`0x0056c490`, object/box executor): `r = .bai field_18 × mode`,
  arrival iff `0.8·r² ≤ d² ≤ 1.2·r²`. For `Eat`/`EatBait`
  (`field_18 = 3.1`, `field_1c = 3`): `r = 3.1 × 0.01 × body part length
  (dino+0xf4)`. For `EatForSmallies`/`EatBaitForSmallies`
  (`field_18 = 2.5`, `field_1c = 4`): `r = 2.5 × (size_radius + 1)`.
- **Bite amount** (`FUN_00569a00`): `(float)*(int*)(dino+0x288)`. In a fresh
  park `+0x288` is the first of the five `+0x9c..+0xac` stat slots copied
  from the phenotype, i.e. `+0x4c` = `attack_head`. The Cpg04 anky's
  `attack_head = 30`.

## 1. Object/box executor `0x0056c490`

Ghidra has no function boundary; the code runs `SUB ESP,0x178` … `RET 4` at
`0x0056c844`. State dispatch at `0x0056c4f9` uses the table
`{0x0056c500, 0x0056c718, 0x0056c7c4, 0x0056c819}` (states 0..3).

State 0 builds the two squared bounds (`0x0056c500..0x0056c575`):

```text
0056c500 MOV EDX,[ESI+0xc]       ; element context
0056c503 MOV EAX,[EDX+0x4]       ; .bai action record
0056c506 MOV ECX,[EAX+0x4]       ; base = field_18
0056c509 MOV EAX,[EAX+0x84]      ; mode = field_1c
0056c51f mode 1: r = base * [target+0x268]
0056c52b mode 2: r = base * [target+0x26c]
0056c537 mode 3: r = base * ([target+0xf4] * 0.01)
0056c545 mode 4: r = base * ((float)[target+0x60] + 1.0)
default       : r = base
0056c55a FMUL r, r
0056c567 FMUL [0x00642fcc] (0.8)
0056c56d FSTP [node+0]           ; 0.8 r^2
0056c56f FMUL [0x00642fc8] (1.2)
0056c575 FSTP [node+4]           ; 1.2 r^2
```

The target actor is `[node+0x14]+0x28` (a `saDinosaur` in this executor).
State 1 (`0x0056c718`) measures
`d² = (own - target.mover[+0x2d4/+0x2d8])²` and tests
`0.8 r² ≤ d² ≤ 1.2 r²` (`0x0056c75b`/`0x0056c764`). Inside the band it sets
state 3 and calls `FUN_00569660` (`0x0056c774`); outside it re-plans
(state 0) when the target mover is idle and re-issues the marker otherwise.
The marker kind at `0x0056c619..0x0056c631` is
`0x34 + ([target+0x2b8] != 2)`, and the queue call is
`0x0056c65f CALL 0x00573f40(ECX=target+0x2c8, from, out, 0, 1, kind, 1, 1)`.

Fields:
- `target+0xf4` = phenotype `+0xa4` = `part_length[3]` (body segment length,
  cm). Cpg04 anky `226`.
- `target+0x60` = phenotype `+0x10` = `SizeCategory*Radius`
  (`TranslatorConstants.ini`): `0/2/4/7` for size class 0..3.
- `target+0x268`/`+0x26c` are the mode-1/2 scales; no Eat action uses them.

Constants: `0x00642fcc = 0x3f4ccccd` (0.8f),
`0x00642fc8 = 0x3f99999a` (1.2f), `0x0062588c = 0x3f800000` (1.0f),
`0x00626a7c = 0x3c23d70a` (0.01f).

## 2. `.bai` values (all 64 files checked)

| action | field_18 | field_1c | radius recipe |
| --- | ---: | ---: | --- |
| Eat, EatBait | 3.1 | 3 | `3.1 × 0.01 × body` |
| EatForSmallies, EatBaitForSmallies | 2.5 | 4 | `2.5 × (size_radius + 1)` |

Cpg04 anky (`size_class 1`, `size_radius 2`, body 226):
`r = 3.1 × 0.01 × 226 = 7.006`, band `39.27 ≤ d² ≤ 58.90`.

## 3. Bite amount `dino+0x288`

`FUN_00569a00` (LunchBox/StaticBait branch, element profile contains kind 8):

```c
amount = (float)*(int*)(dino + 0x288);
value  = (int)FUN_00576e50(source, amount);   // amount * nutrition
FUN_005727b0(0x17, value, 100, 0, ...);       // food gauge += value
```

`saDinosaur::reset_attribute_caches` (`0x5710c0`) copies
`+0x9c..+0xac` to `+0x288..+0x298`. The ctor copies the 0x1b8 phenotype
block to `dino+0x50`, so `+0x9c` = phenotype `+0x4c` = `attack_head`.
(After `GetCourage` completes, the same slot is overwritten from `+0x270`;
the port's park node set excludes that action, so a fresh park reads
`attack_head`.)

Cpg04 anky: `attack_head = 30` → the LunchBox meal drains 30 units and adds
`30 × 100 = 3000` to the gauge (clamped at max 4255).

## 4. Actor-target executor `FUN_0056f7a0` (dino→dino actions)

The same ±20 % band, but the distance is a 4×4 size-class grid plus both
actors' `+0x60`:

```text
d = DAT_00675708[4*target.sizeClass + self.sizeClass]
    + self+0x60 + target+0x60;
arrive = 0.8 d² <= dist² <= 1.2 d²;
```

`DAT_00675708` is set by `0x005874a0` from `GeneralParams.bgd` (`cfg+0x1a8`,
a 4×4 row-major float grid): `3,3,3,3 / 3,2,2,2 / 3,2,2,2 / 3,2,2,2`
(3.0 iff target or self class is 0). `0x00675700 = 0.1f` is unused by the
gate. No current port action uses this path; keep the recipe for attacks.

## 4b. Stand-cell search (`FUN_0055b5b0` / `FUN_0055cb50` / `0x0055b780`)

- `FUN_0055b5b0` takes the feeder-side cell `(ownX, ownY)`, the radius `r`
  in world units, computes `R = ftol(r + 0.5)` cells, and asks
  `FUN_0055cb50` for a filled spiral disc of cells around the feeder cell.
- Per cell `0x0055b780` runs the `0x0055af20` occupancy predicate
  (`cls = tile >> 12`; class 0 free; class 1 only if
  `(tile & 0x7ff) == actor+0x28`; every other class blocks) plus a
  `K = actor+0x13` clearance scan.
- Cost (proven): `(actorCellX - x)^2 + (actorCellY - y)^2`, the squared grid
  distance to the **dino's cell** (`+0x8/+0xc`), not the mover floats.
- `FUN_0055b5b0` picks the strict minimum (`<`, first wins) and writes the
  cell indices (no `*8+4`) to `out`. If no cell is appended it returns 0;
  the executor asserts (`0x0056c670`), clears the action and posts the stop
  record (`0x0056c70e`).
- The port's `find_stand_cell` enumerates the same disc (round-half-down
  `R`), filters with the port occupancy predicate and the proven arrival
  band (`0.8 r² .. 1.2 r²`; the retail visitor itself has no band filter, but
  the square disc has corners beyond the Euclidean outer bound), and picks
  the minimum squared distance to the dino's cell. The cell is cached per
  element (`ImportedDino::eat_stand_x/y`) and reset on action change; the
  mover walks to the cell coordinate.
- Cpg04: feeder (454,314), dino (463,327), `R = 7`, chosen `(457,321)`;
  `d² = 58` is inside `[39.3, 58.9]`.

## 4c. Element lifetime and meal timing (`0x0056c0c0` / `0x0056c3b0`)

- `FUN_0056c0c0` initialises `action+0x1c` from the `.bai` `field_2e`
  (30.0 for Eat) and `FUN_0056d880` decrements it; that is the **action cap**,
  not the element duration. On expiry it runs the end ramp
  `FUN_0056ddb0(action+0x30, action+0x34)` and unregisters the action.
- The object executor plays the element when the dino is in the arrival band
  (or already on the stand cell) and `exec+4` (the previous animation
  finished latch) is 1: it calls `FUN_00569660` and clears `exec+4`.
- State 3 waits for the target mover `actor+0x4b4 == 0` (idle), then calls
  `FUN_0056c3b0`: `FUN_00569a00(action, element, actor)` applies the element
  (meal/needs), `action+0x14 = 1`, `FUN_0056c260` unregisters it and
  `exec+4 = 1`.
- The Eat element's own `duration` is **0.0**; the meal fires on
  arrival + mover idle, not after 30 s.
- Port: `tick_dino` applies the meal when `d² <= 1.2 r²` and
  `mover.state() == 0`, sets `ImportedDino::eat_applied`, then calls
  `DinoActionLoop::finish_current()` so the next action is chosen. The
  `field_2e` cap is still the action-loop duration for the case where the
  dino never reaches the feeder. The FOOD need only updates on the next 10 s
  pass, so the dino can re-select Eat while still at the bowl; that is
  consistent with the gauge/need split.

## 5. Port mapping

- `src/world/dino_actions.*`: `eat_arrival_radius(field_18, mode, body,
  size_radius)` implements the two proven modes; `DinoAttributes::size_radius`
  is filled from `SizeCategoryNRadius` when the phenotype is built.
- `ParkSession::tick_dino`: `r` from the selected action, walk to the cached
  stand cell while `d² > 1.2 r²`, stop/meal when inside, bite `attack_head`,
  drain through `feeder_drain` and `apply_food`.
- Evidence: `test_cpp_lunchbox_eat` logs
  `dino eat gate: anky r=7.01 ... stand=457,321` and
  `dino eat: anky <- LunchBox nutrition=3000 quantity=9970 gauge=4255`;
  `--selftest` checks `4.65 / 12.5 / base`.

## Open

1. The exact spiral start/direction and the `K`-square clearance geometry
   inside `0x0055b780`; the port uses the proven centre + 4-neighbour
   footprint. Ties are resolved by the port's ring order.
2. The `vtable+0x14` own-position source for the `0x0056c490` class (feeder
   cell vs last stand cell); the port uses the feeder cell.
3. The `exec+4` latch writer outside `FUN_0056c3b0` (the ctor sets 1) and
   whether an animation callback clears `actor+0x32c` to end the cap early;
   the port uses `finish_current` at the meal.
4. The actor `+0x60` name beyond `SizeCategory*Radius`.

## Evidence

- Ghidra: `0x0056c490` (states, bounds, band, marker),
  `FUN_0056f7a0`/`0x56f817..0x56f8fa`, `FUN_00569a00`,
  `FUN_00576e50`/`FUN_005727b0`, `0x5710c0`, `0x005874a0`,
  `0x00542f80` (GeneralParams), `0x0056fb70`.
- `Data/Design/attractions/actions/{Eat,EatBait,EatForSmallies,EatBaitForSmallies}.bai`.
- `Data/Design/generalparams/GeneralParams.bgd` (4×4 grid tail).
- `notes/dino-action-table-move-recon.md` §2.2/§2.3,
  `notes/dino-normal-action-target-gesture-recon.md`,
  `notes/lunchbox-eat-recon.md`, `notes/dino-food-gauge-recon.md`.
