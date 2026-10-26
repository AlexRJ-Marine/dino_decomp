# Dinosaur action chooser — decompiled (P3)

Follow-up to `notes/dinosaur-actions-recon.md` and
`notes/dinosaur-behavior-graph.md`. This closes the "BAD FOOD/REPOS → Eat/Sleep"
gap: the chooser is a **score over the four need axes**, not a hard-coded
need→action table.

## Need block

`saDinosaur` keeps the four AI/UI needs as a contiguous block at `+0x214`:

```text
actor+0x244  int  FOOD      0..99      block+0x30
actor+0x248  int  VIOLENCE  0..99      block+0x34
actor+0x24c  int  SECU      0..99      block+0x38
actor+0x250  int  REPOS     0..99      block+0x3c
actor+0x2ac  int  FOOD state      0..3  block+0x98
actor+0x2b0  int  VIOLENCE state  0..3  block+0x9c
actor+0x2b4  int  SECU state      0..3  block+0xa0
actor+0x2b8  int  REPOS state     0..3  block+0xa4
```

So `FUN_0056e5d0(need, delta, actor+0x214)` is exactly the need scoring
function: `block+0x30` and `block+0x98` are the value/state arrays.

### State mapping

`FUN_005714b0(category)` scans the thresholds at `actor+0x3c + category*4`
(three `int` per category, `[80, 60, 30]`, `DAT_00643114 = [3,3,3,3]`):

```text
value >  80  -> state 3   (full)
60..80       -> state 0   (GOOD)
30..60       -> state 1   (MID)
value <= 30  -> state 2   (BAD)
```

`FUN_00571350` recomputes all four after a need changes.

## Score term

`FUN_0056e5d0` (`__cdecl`, 3 args):

```c
int need_term(int need, int delta, int *block) {
    int value = block[0x30/4 + need];
    if (value + delta > 99) delta = 99 - value;   // upper clamp only
    int weight = table[need][ block[0x98/4 + need] ];
    if (delta < 1 && weight < 1) return -(weight * delta);
    return weight * delta;
}
```

Weight tables (`DAT_00643054`, `+0x10` apart), index = state 0..3:

| Need | state 0 (GOOD) | state 1 (MID) | state 2 (BAD) | state 3 (full) |
| --- | ---: | ---: | ---: | ---: |
| FOOD | 2 | 15 | **300** | 1 |
| VIOLENCE | 1 | 10 | **60** | 1 |
| SECU | 2 | 15 | **65** | 1 |
| REPOS | 5 | 20 | **90** | 1 |

A bad need is worth 30–150× more than a good one. That is the whole
"starving dinosaur wants to eat" behaviour.

## Node score

`FUN_0056e410` runs once per `apDinosaurBehavior` sub-behavior each tick.
For every available node (`node+0x08 == 1`):

```text
action = node+0x38 -> +0x04            # .bai runtime record
score  = 0
for (delta, need) in action.need_deltas:      # action+0x74 vector, 8-byte
    score += need_term(need, delta, actor+0x214)
score += need_term(3, node+0x10 / 100, actor+0x214)     # rest cost
if (node+0x38 != -1) {
    # threat/fear term; world-grid lookup, NEG'd, then /10
    score += need_term(2, threat_score / 10, actor+0x214)
}
if (node+0x14 != 100 && score > 0):
    score = node+0x14 * score / 100          # node percentage
node+0x0c += score                            # added to base score
```

`node+0x0c` is zeroed before the pass and the vtable `+0x10` hook
(`FUN_0056e820`) first adds `.bai` `field_32`:

```c
node->score += action->field_32;      // Victory 50000, Smell 5000,
                                      // jumps/linked 900, ReachWaypoint 800
```

(`FUN_0056cb30` / `FUN_0056cc20` are the two node vtables that call it; they
then multiply by `params[7]` / indexed `params[i]` percentages.)

The threat term is now decoded (disassembly `0x0056E4D1..0x0056E550`). The
coordinates are the target returned by the node action's vtable `+0x14`
(`GetTarget`); `-1` means no target and the term is skipped (self actions
such as Eat/Sleep). With `world = actor+0x20`:

```c
if (x != -1) {
    // 8-unit cells; [world+0x10] is the row-pointer array, rows are 32 bytes
    cell = *(int *)(*(int *)(*(int *)(world + 0x10) + (x >> 3) * 4)
                    + (y >> 3) * 0x20);
    threat = -cell;
    // inside `[world+8]/[world+0xc] +/- [world+0x27c]`
    if (FUN_00572DD0(world, x, y) == 1) {
        threat += danger(actor_cell) - actor+0x2a0;
    }
    score += need_term(2, threat / 10, actor+0x214);
}
```

`FUN_00572DD0` is a world-bounds test (`|x - world+8| <= world+0x27c`,
same for y), not a target-distance test. The grid is a per-cell **danger
value**, so the term rewards moves to a cell whose danger is lower than the
actor's current cell by more than `actor+0x2a0` (after the /10 scale). With
no danger population in the port the term is 0, which is why omitting it is
currently exact; it becomes relevant once visitors/guards feed the map.


## Availability gate

`FUN_0056d980` (called before scoring, `0x56d980` from `FUN_0056d880`):

- `node+0x08` is the "kind" byte; kind 1 nodes are scored.
- Node target is `node+0x3c` (vtable object); `node+0x38` is the action data.
- Distance² is stored in `node+0x04`; it is scaled by `actor+0x1f4 / 100`
  and compared with the target radius² (target vtable `+0x18`).
  Out of range or no target → node unavailable.
- Class-5 (obstacle/linked) nodes are filtered by `FUN_0056db70`; class 6
  (victory) has an extra check.
- Then `FUN_0056e0e0` (preconditions) is applied (below).

So a node is only scored when its target is in range **and** its
preconditions hold.

## Preconditions (`FUN_0056e0e0`, `saDinosaur::CanDoAction`)

Runtime triple `(threshold, attribute, comparison)`:

```text
comparison 0: fail if threshold < value      => requires value <= threshold
comparison 1: fail if value < threshold      => requires value >= threshold
```

Attribute → actor field (`FUN_00572620`, ECX = actor):

| id | field | id | field | id | field |
| ---: | --- | ---: | --- | ---: | --- |
| 0 | +0x244 FOOD | 9 | +0x264 | 18 | +0xbc aggression |
| 1 | +0x248 VIOLENCE | 10 | +0x280 | 19 | +0xc0 stamina |
| 2 | +0x24c SECU | 11 | +0x284 | 20 | +0x2a4 |
| 3 | +0x250 REPOS | 12 | +0x98 size | 22 | +0x200 |
| 4 | +0x50 size cm | 13 | +0x29c | 24 | i16 +0x16 |
| 5 | +0x64 | 14 | +0x28c | 29 | +0x1e0 agility |
| 6 | +0x68 | 15 | +0xb4 | 30 | +0x528 != 0 |
| 7/8 | float→int | 16 | +0x270 | | |

`consume_flag` (`action+0x2d`): if set, the action additionally requires a
non-zero entry at `actor+0x214 + index*4`, where `index` comes from the
target's vtable `+0x28`. This is the inventory/item gate (bait, etc.).

## Selection (`FUN_0056e670`)

After scoring, the node with the highest `+0x0c` is selected:

- no node, or best score `< 1` → stop the current action, clear it, emit the
  stop event;
- no current action → `FUN_0056db70` (can-do) then start it;
- a different node with a higher score → stop the current action, start the
  new one.

`field_2e` (30.0 s in all 64 files) is the action duration countdown
(`FUN_0056c0c0` / `FUN_0056d880`); `node+0x10` also feeds the REPOS term.

## Eat / Sleep, worked example

`Eat.bai`: precondition `size >= 200` (`attr 4`, comparison 1), deltas
`FOOD +30`, `VIOLENCE +10`, `consume_flag = 1` (the feeder target supplies
the item).
`Sleep.bai`: no preconditions, deltas `SECU -50`, `REPOS +100`.
`EatForSmallies.bai`: `size <= 200`, `FOOD +30`, `VIOLENCE +10`.

Deltas are capped at the current value: `+100` on a need of 20 becomes `+79`,
`+30` on 90 becomes `+9`, and on a need of 99 it becomes 0.

| Needs (food/violence/secu/repos) | Eat | Sleep | Winner |
| --- | ---: | ---: | --- |
| 99/99/99/99 (all full) | 0 | −50 | none / idle |
| 20/90/50/90 (starving) | 300·30 + 1·9 = 9009 | 15·(−50) + 1·9 = −741 | Eat |
| 70/90/90/20 (tired) | 2·30 + 1·9 = 69 | 1·(−50) + 90·79 = 7060 | Sleep |
| 20/90/50/20 (both bad) | 9000 + 9 = 9009 | −750 + 7110 = 6360 | Eat |

So a starving dinosaur picks Eat, a tired one picks Sleep, when both are bad
food wins, and a fully satisfied dinosaur scores 0 on all three and stays
idle. No explicit need→action mapping is needed.

## Animation clips

The 51 clip names in `anim_trans.dic` are mirrored by a 51-entry pointer
table in the exe (`0x0065e4d0`): Neutral, Walk, Run, SwingL, SwingR, UTurn,
Backward, WalkDizzy, BowlAttack, HeadAttackLow/High/Middle, HeadChargeOK,
HandAttack, FeetAttack, TailAttack, Trample, HeadChargeFail, TakeaBlowFront,
TakeABlowL/R, Collapse, SideStepL/R, Backstep, Scream, Winner, Chew, Wakeup,
LieDown, Sleep, WaitScratch/Sweep/Fight/Happy/Sad, Fart, LJumpOK,
CrashDownWall, Sniff, TakeAWall, FallInTrapp, Stamina, QuadPower,
Invincibility, Armor, GetOut, SommerSault, SuperAttack, Dance, Slip.

`Eat.bai` carries no clip name; the action→clip link is still open. Likely
candidates by name: Eat→Chew, Sleep→Sleep, Smell→Sniff, Danse→Dance,
Exploration→Walk, RunAway→Run, Victory→Winner. Do not present this as
retail-verified until the table is found.

`FUN_00569660` is the per-action **element** selector (not the node chooser):
element score = `Σ attribute_value * effect_weight + rand() % field_14`, then
the first element whose `time >= score` is used. With one element per action
(Eat/Sleep) it is trivial.

## Need decay (retail timing)

`spSimuManager::Update` (`0x005869e0`) drives the dinosaur needs:

```text
per frame:   FUN_00571320()            # per-frame bookkeeping
             FUN_00571540(dt)          # rest gauge decay + action countdown
every 5 s:   FUN_005725e0()            # 5 s pass
every 10 s:  FUN_00571370(10.0)        # FOOD gauge -> UI FOOD
             FUN_00571410(10.0)        # VIOLENCE decay
             FUN_005714b0 x4           # recompute all four states
```

`FUN_00571370(rate)`:

```text
decay = rate * actor+0x6c
food_gauge (+0x25c) -= decay
actor+0x244 = (int)(food_gauge * 100.0 / actor+0x1dc)
```

`FUN_00571410(rate)`:

```text
actor+0x248 -= (int)(rate * actor+0x78);  clamp 0
```

`FUN_00570fd0` initializes the gauges/needs:

```text
rest gauge (+0x258) = max (+0x1d8)
food gauge (+0x25c) = max (+0x1dc) * 0.5
UI needs (+0x244..+0x250) = 99
```

The food gauge is at half, so the **first 10 s tick rewrites UI FOOD to
~50**. The steady-state starting value for the chooser is therefore
`(50, 99, 99, 99)`, not 99/99/99/99. Open: the values of `actor+0x6c`
(decay scale, likely phenotype `+0x1c` = volume × FoodConsumptionRate × 0.01),
`actor+0x1dc` (food maximum) and `actor+0x78` (violence decay scale, likely
phenotype-linked).

## Port loop

`DinoActionLoop` (`src/world/dino_actions.*`, mirrored in
`tools/formats/dino_actions.py`) ticks need decay, the action countdown and
the decision timer. It applies an action's `.bai` need deltas when the action
completes, then re-scores. With `action_scale=0.1` and
`(food,violence,secu,repos) = (20,90,50,20)` the timeline is:

```text
0.1  Eat          food=20 violence=90 secu=50 repos=20
3.1  Sleep        food=49 violence=99 secu=50 repos=19
6.1  Eat          food=47 violence=98 secu=0  repos=99
9.1  Exploration  food=76 violence=99 secu=0  repos=98
```

The decay rates (0.5 food/rest, 0.2 violence per second) are port tuning: the
raw-gauge constants are not runtime-calibrated yet.

## Evidence

- `FUN_0056e5d0` disassembly `0x0056e5d0..0x0056e64e`; tables
  `0x00643054/64/74/84`; state thresholds `0x00643114`.
- `FUN_0056e410` disassembly `0x0056e410..0x0056e5a0`.
- `FUN_005714b0` / `FUN_00571350` state mapping.
- `FUN_0056e0e0` preconditions; `FUN_00572620` attribute accessor.
- `FUN_0056d980` availability; `FUN_0056e670` selection; `FUN_0056e820`
  base score.
- `FUN_005727b0` / `FUN_00571190` action effect/completion.
- Retail `.bai` dump: 46 preconditions, 43 weights, 134 need deltas, 204
  elements, 171 events (see `docs/formats/bai.md`).

## Danger-grid population (2026-10-26)

The 8-unit danger grid read by the chooser is at world `+0x10` (row array;
row stride `0x20`); the world descriptor also has `+0x08/+0x0c` = centre and
`+0x27c` = world radius (`FUN_00572DD0` bounds test). It is rebuilt every
park update in `FUN_00589CD0`:

1. `FUN_005774F0(world)` clears every cell: the words at `+0`, `+4`,
   `+0x18`, `+0x1c` and the vector at `+0xc..+0x10` are zeroed.
2. For every dinosaur in `park+0x68`, `FUN_00571B50(dino)` walks the cells
   inside `[dino+0x08 - r, dino+0x08 + r] x [dino+0x0c - r,
   dino+0x0c + r]` with `r = dino+0x27c` (8-unit cells, `>> 3`).
3. `FUN_00571C00` adds `dino+0x2a0` to each visited cell. Its `this` is a
   stack object `{vtable 0x643150, dino+0x214}`; `0x571b6c` stores
   `dino+0x214` at temp `+4` and `0x571c03` reads `[temp+4 + 0x8c]` =
   `dino+0x2a0`.

`+0x27c` and `+0x2a0` are the phenotype-cache copies made by
`FUN_00571190`: `+0x27c <- +0xd0` (threat/perception radius) and
`+0x2a0 <- +0x94` (aggression/threat value). The chooser then computes
`threat = -cell(target) + cell(actor) - actor+0x2a0`, which cancels the
actor's own stamped value and leaves the other dinosaurs' threat at its cell
minus the threat at the target cell.

The port has no danger grid yet; its `choose_action` still passes
`threat_score = 0`. Implementing it needs per-action target positions (the
node target vtable `+0x14`) in addition to the grid.
