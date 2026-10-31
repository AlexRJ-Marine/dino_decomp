# Dinosaur action IDs and need gauges

Status: **decompiled**; runtime confirmation of the idle sentinel and need
names remains.

The simulation refers to dinosaur actions by an integer index into the 64-entry
`actionlist.ini` table. The order is fixed by the loader `0x0053e060`, which
reads `actionlist.ini`, then loads `Data/design/attractions/actions/<name>.bai`
for each entry into a vector with stride `0xac`.

## Action IDs

| ID | Name | ID | Name | ID | Name | ID | Name |
| ---: | --- | ---: | --- | ---: | --- | ---: | --- |
| 0 | Exploration | 16 | GetBonus | 32 | JumpHole2 | 48 | LinkObstAttackSize1 |
| 1 | RunAway | 17 | GetInvis | 33 | Smellbad | 49 | Jump2m_Size3 |
| 2 | Sleep | 18 | GetJump | 34 | Jump1m_Size1 | 50 | Jump3m_Size1 |
| 3 | Victory | 19 | GetQuad | 35 | Jump1m_Size2 | 51 | Jump3m_Size2 |
| 4 | Eat | 20 | GetSlide | 36 | Jump1m_Size3 | 52 | Jump3m_Size3 |
| 5 | Fall | 21 | GetScotch | 37 | Jump2m_Size1 | 53 | Jump4m_Size1 |
| 6 | Healed | 22 | GetArmor | 38 | MineExplosion | 54 | Jump4m_Size2 |
| 7 | Danse | 23 | Explode | 39 | Teleport | 55 | Jump4m_Size3 |
| 8 | Restore | 24 | KickTheirAss | 40 | ForceField | 56 | ObstAttackSize2 |
| 9 | Smell | 25 | HeadCharge | 41 | ReachWaypoint | 57 | ObstAttackSize3 |
| 10 | Speed | 26 | ObstAttackSize1 | 42 | Piege | 58 | ObstAttackSize4 |
| 11 | GetInvincibility | 27 | DinoAttackForHerbivore | 43 | GetSleep | 59 | LinkObstAttackSize2 |
| 12 | GetEndurance | 28 | DinoAttackForCarnivore | 44 | ForceFieldSmall | 60 | LinkObstAttackSize3 |
| 13 | GetCourage | 29 | Devour | 45 | JumpForBiggies | 61 | LinkObstAttackSize4 |
| 14 | GetIntelligence | 30 | JumpFire | 46 | Jump2m_Size2 | 62 | EatForSmallies |
| 15 | GetPerception | 31 | JumpHole | 47 | EatBait | 63 | EatBaitForSmallies |

All 64 `.bai` files exist.

## ApplyActionEffect (`0x005727b0`)

Signature: `(this, action_id, amount, percent, duration, name)`.

| Action | Effect |
| --- | --- |
| RunAway (1) | `+0x92 += amount`, clamp 0..99 |
| Smell (9) | `+0x99` |
| Speed (10) | `+0xa0` |
| GetInvincibility (11) | `+0xa1` |
| GetCourage (13) | `+0xa2..+0xa6`, sum `+0xa7`, `+0xa8` |
| GetBonus (16) | `+0x9c` |
| GetSlide (20) | `+0xa9` |
| ObstAttackSize1 (26) | `+0x9d`, `+0x9e = value²`, `+0x9f` |
| DinoAttackForHerbivore (27) | `+0x9a`, `+0x9b` |
| DinoAttackForCarnivore (28) | flag `+0xaa` |
| HeadCharge (25) | `+0x96` need gauge, clamp 0..`+0x76` |
| Explode (23) | `+0x97` need gauge, clamp 0..`+0x77` |
| KickTheirAss (24) | scale `int16 +0x16`, damage through `vtable+0xc` |

The function also stores the active action id at **`+0xb0`** (byte `0x2c0`),
the duration at `+0x98`, and raises the one-frame `+0x4d` flag.

## Completion (`0x00571190`)

When the action countdown expires, `0x571190` switches on `+0xb0`:

- 9 Smell → `+0x264`
- 10 Speed → `+0x280`
- 11 GetInvincibility → `+0x284`
- 13 GetCourage → `+0x288` + 5 dwords
- 16 GetBonus → `+0x270`
- 20 GetSlide → `+0x2a4`
- 26 ObstAttackSize1 → `+0x274..+0x27c`
- 27 DinoAttackForHerbivore → `+0x268`/`+0x26c`
- 28 DinoAttackForCarnivore → clear `+0x2a8`

It then sets the action id to `0x1f` (31) and raises `+0x515`/`+0x4d`.
`saDinosaur::Update` treats `+0x2c0 == 0x1f` as "no action" and skips the
completion call, so **`0x1f` is the idle/finished sentinel**, not a real
`JumpHole` action.

## Need gauges and the four UI needs

**Offset correction.** Earlier notes used `param_1[0x96]` / `param_1[0x97]`
from a decompiler where `param_1` is an `int *`. Those are **dword indices**:
`param_1[0x96]` is byte **`+0x258`**, not `+0x96`. The same mistake applied to
`+0x76`/`+0x77` (`+0x1d8`/`+0x1dc`) and `+0xb0` (`+0x2c0`).

### Raw gauges

`saDinosaur::Update` (`0x00571540`) and the periodic need pass (`0x00571370`)
maintain two raw gauges:

| Byte offset | Meaning | Max | Decay |
| --- | --- | ---: | --- |
| `+0x258` | rest / sleep gauge | `+0x1d8` | every frame, `+0x498 * table[+0x354 + activity*4]` |
| `+0x25c` | food gauge | `+0x1dc` | periodic pass, `+0x6c` scaled |
| `+0x260` | action countdown | — | `saDinosaur::Update` |
| `+0x2c0` | current action id | — | `ApplyActionEffect` |
| `+0x4c0` | activity/need type | — | selects the decay multiplier |

`+0x4c0` selects the activity multiplier:

| `+0x4c0` | Actions | Multiplier |
| ---: | --- | --- |
| 1, 2 | RunAway, Sleep | `stat[0x9a] * 0.2` |
| 9, 10, 11 | Smell, Speed, GetInvincibility | `stat[0xa2]` |
| 12 | GetEndurance | `stat[0xa3]` |
| 13 | GetCourage | `stat[0xa4]` |
| 14 | GetIntelligence | `stat[0xa5]` |
| 15 | GetPerception | `stat[0xa6]` |
| 16, 25 | GetBonus, HeadCharge | `stat[0x99]` |
| 25 | HeadCharge | `stat[0x2f]` (special case) |

`0x00570fd0` initialises them: rest to `+0x1d8` (full), food to
`0x1dc * 0.5`, and the four need values to 99.

### Four UI needs

The four UI axes live in a separate 0..99 block:

| Byte offset | UI axis | State index | UI state widgets |
| --- | --- | ---: | --- |
| `+0x244` | **FOOD** | `+0x2ac` | `+0xd0` GOOD, `+0xd4` MID, `+0xd8` BAD |
| `+0x248` | **VIOLENCE** | `+0x2b0` | `+0xe8` GOOD, `+0xec` MID, `+0xf0` BAD |
| `+0x24c` | **SECU** (fear/security) | `+0x2b4` | `+0xf4` GOOD, `+0xf8` MID, `+0xfc` BAD |
| `+0x250` | **REPOS** (rest) | `+0x2b8` | `+0xdc` GOOD, `+0xe0` MID, `+0xe4` BAD |

`0x005714b0(category)` turns a need value into a state index using the
threshold array at `+0x3c + category*4` (initialised by `0x00570590` to
`[0x50, 0x3c, 0x1e]` = `[80, 60, 30]`):

```text
value <= 30           -> state 2 (BAD)
30 < value <= 60      -> state 1 (MID)
60 < value <= 80      -> state 0 (GOOD)
value > 80            -> state 3 (also rendered GOOD)
```

`0x00571350` recomputes all four states. `0x00572e10` returns the index of the
most urgent state (0..3, or 4 when none), used by the list/info UI.

### Feed-forward from actions

- `RunAway` adds to `+0x248` (VIOLENCE) and calls `0x005714b0(1)`.
- `0x00571410` decays `+0x248` (VIOLENCE) over time.
- `0x00571370` decays `+0x25c` (food) and stores the normalised feed value in
  `+0x244` (FOOD).
- `saDinosaur::Update` decays `+0x258` (rest) and stores the normalised value
  in `+0x250` (REPOS).
- `HeadCharge` restores `+0x258`; `Explode` restores `+0x25c`.

### UI widgets

`bvwDinosaurListPanel.cpp` (`0x005df900`) creates four need indicators per
dinosaur row:

```text
DINO_INDICATEURS_BESOINS_VIOLENCE_SLOTn
DINO_INDICATEURS_BESOINS_REPOS_SLOTn
DINO_INDICATEURS_BESOINS_FOOD_SLOTn
DINO_INDICATEURS_BESOINS_SECU_SLOTn
```

The info panel `0x00511d80` creates the per-axis BAD/MID/GOOD state widgets.
The needs-tab update `0x005138b0` reads `+0x2ac`, `+0x2b8`, `+0x2b0`, `+0x2b4`
in that order and enables the matching FOOD/REPOS/VIOLENCE/SECU state widget.
`ParkEng.loc` supplies the labels:

```text
&DinoNeed       = Needs
&DinoStarving   = Starving
&DinoExhausted  = Exhausted
&DinoBerserk    = Irritated
&DinoPanicked   = Panicked
```

### Eat / sleep

`Eat` (action 4) and `Sleep` (action 2) are real `actionlist.ini` actions.
Their need feed is the FOOD/REPOS pair above. `FUN_00570fd0` sets the rest
gauge full and the food gauge to 50 % of its maximum, but writes 99 into all
four UI needs; the first 10 s tick (`FUN_00571370(10.0)`) rewrites FOOD from
the gauge, so the chooser effectively starts at `(50, 99, 99, 99)`. The raw
gauge formulas are in `notes/dino-action-chooser-recon.md`.

**The chooser is solved** (`FUN_0056e410` / `FUN_0056e5d0` /
`FUN_0056e670`): every available action node is scored from its `.bai`
`need_deltas` using a per-need weight table indexed by the need's
GOOD/MID/BAD state:

```text
FOOD     [2, 15, 300, 1]
VIOLENCE [1, 10,  60, 1]
SECU     [2, 15,  65, 1]
REPOS    [5, 20,  90, 1]
```

`Eat` (+30 FOOD, +10 VIOLENCE) therefore scores ~9009 when FOOD is BAD and 70
when it is GOOD; `Sleep` (−50 SECU, +100 REPOS) scores up to 7060 when REPOS
is BAD and is negative when it is full. The highest-scoring available node
(score ≥ 1) starts, so a starving dinosaur eats and a tired one sleeps without
any need→action table. Deltas are capped at the current need: +100 on a need
of 20 adds 79, +30 on 90 adds 9. Preconditions add the size gate (`Eat`:
size ≥ 200 cm, `EatForSmallies`: ≤ 200). Full formula and evidence:
`notes/dino-action-chooser-recon.md`.

### Idle sentinel

`saDinosaur::Update` skips completion when `+0x2c0 == 0x1f`, and
`ApplyActionEffect` calls `0x00571190` first if the current action is not
`0x1f`. `0x00571190` sets `+0x2c0 = 0x1f` after every completed action.
Therefore **`0x1f` is the idle/finished sentinel**, not a real `JumpHole`
action. This is confirmed by the two independent checks.

## Port status

- `src/world/dino_actions.{h,cpp}`: `DinoNeeds` (four values + states +
  attribute accessor), the retail weight tables, `need_term`, `score_action`,
  `preconditions_ok`, `choose_action`, and `DinoActionLoop` (need decay,
  action countdown, `.bai` deltas applied on completion). The FOOD gauge and
  the VIOLENCE 10-point 10 s pass are exact; the SECU/REPOS decay rates in
  `DinoActionLoopConfig` remain port tuning. The chooser and completion rule
  are the retail algorithm.
- `--dino-actions` prints the chooser result for starving / tired / both /
  full / small scenarios and a 40 s action timeline.
- The viewer runs one `DinoActionLoop` per imported dinosaur
  (`--dino-action-loop`, `--dino-needs F,V,S,R`, `--dino-action-speed N`).
  The default needs are `50,99,99,99`: `FUN_00570fd0` puts the food gauge at
  50 % and the first 10 s tick rewrites UI FOOD to 50.
- The viewer drives every imported dinosaur with `DinoActionLoop`
  (`--dino-action-loop`, `--dino-needs F,V,S,R`, `--dino-action-speed N`).
  The selected action switches the animation clip through the inferred
  name mapping (`Eat`→`Chew`, `Sleep`→`Sleep`, `Exploration`→`Walk`,
  `Smell`→`Sniff`, `Victory`→`Winner`, `RunAway`→`Run`, idle→`Neutral`).
  The loop prints `dino action: <species> <name> score=<n> needs=...` on
  each action change. Without a LunchBox, Cpg04 with `--dino-needs
  20,90,50,90` picks Exploration (Eat is unavailable); `70,90,90,20` gives
  20 Sleep 7060; `99,99,99,99` stays idle. With `--place-lunchbox` the
  herbivore anky gets Eat and walks to the bowl.
- Tests: `tests/test_dino_actions.py` (Python oracle/loop tests),
  `tests/test_app_cpp.py::test_cpp_dino_action_chooser`,
  `test_cpp_dino_action_loop` and `test_cpp_lunchbox_eat`.
- Open for the next increment: calibrate the remaining need gauges and wire
  the SECU danger grid into the chooser. The feeding interaction is the
  LunchBox item actor (`docs/platform/feeding.md`); `.bsi` sources are
  visitor/employee-only.

### SECU danger grid

`FUN_00589CD0` rebuilds the 8-unit danger grid once per second
(`DAT_00675a20` accumulator, threshold 1.0). It clears the grid
(`FUN_005774F0`), stamps every dinosaur's `+0x2a0` into the cells within
its `+0x27c` radius (`FUN_00571B50`), then recomputes every dinosaur's SECU:

```text
secu = 99 + trunc((health - max_health) * 100 / max_health)
          - trunc((grid[x>>3][y>>3] - threat) * 0.1)      // clamp >= 0
```

`health` is `int16 +0x16`, `max_health` is `int16 +0x1d4`; `0.1` is the
constant at `0x675704`. `FUN_00571440`. The SECU field is therefore
recomputed, not decayed, and `.bai` SECU deltas are transient. Validated
on Cpg04: **862/862 SECU values over 45 rebuilds** match the grid formula,
including damaged dinosaurs (`notes/x32dbg-secu6.out.log`,
`tools/recon/secu_grid_check.py`).

The chooser's threat term is ported. `DinoActionLoop` accepts an optional
`ActionThreatProvider`; `chooser_threat_score` implements the retail
`node+0x3c` policy (`GetTarget` at vtable +0x14, `-1` = no target):

```text
threat = -cell(target)
if (|tx - actor.x| <= actor+0x27c && |ty - actor.y| <= actor+0x27c)
    threat += cell(actor) - actor+0x2a0
score += need_term(SECU, threat / 10)
```

`FUN_00572DD0` is the actor-threat-radius box test (`|tx-x| <= r`), not a
world bounds test. The vtable ABI and the three concrete classes (actor,
box centre, no-target) are in `notes/dino-target-vtable-recon.md`. The factory
is `FUN_00569640`, called only by `FUN_0056a9a0`: one target object is attached
per action list and the class is selected by the producer (show element ->
box, asItem -> no-target, reaction -> actor). `FUN_0056e190` skips an action
whose target owner (`vtable +0x0c`) equals the chooser owner. The viewer maps
Eat/EatForSmallies to the nearest stocked food source (an inferred park
target); the `behavior+0xf4`/`+0xb8`/`+0xc8` producers are still open.
Selftest: `dino target box/actor/none` and `dino choose threatbox`.

The completion writers are ported: `DinoActionLoop::finish_action` copies
`DinoNeeds::combined_attack` (phenotype `+0x44`) into `threat` (`+0x2a0`) on
GetCourage and `obstacle_radius` (phenotype `+0x80`) into `threat_radius`
(`+0x27c`) on ObstAttackSize1. Imported/bought dinosaurs fill both sources
from `DinoAttributes`; `DINOPSAV` persists the caches and sources. Details:
`notes/dino-action-chooser-recon.md` ("Danger-grid population").

## Runtime execution: element events, never the need deltas

`FUN_00569a00` executes the selected `.bai` **element**:

- it walks the element's `events` vector and applies each through
  `FUN_005727b0` (`event.kind`, value, percent, duration, texture);
- it copies five dwords from the element (`+0x14..+0x28`) to `actor+0x504`;
- when `element+0x54 == 2` it calls `0x57af70`;
- it raises the one-frame `+0x4d` flag.

The consequence is easy to get wrong: the `need_deltas` tail (`(need, delta)`)
is a **scoring** input only. `FUN_0056e410` sums it into the candidate score;
nothing iterates it when an action executes or completes. The completion
`FUN_00571190` only runs the per-action writers (courage threat cache,
obstacle radius, DinosaurAttack flags) and returns the actor to `0x1f`.

Port (2026-10-30): `DinoActionLoop::decide` applies the element events at
action start through `apply_action_events`; `finish_action` no longer applies
the deltas. It walks every element of the action, while retail executes the
one element selected by the element scorer; single-element actions
(Eat/Exploration/Sleep) are exact, multi-element actions (Danse, HeadCharge,
ObstAttackSize1) can over-apply until the element selector is ported. Ported
kinds:

| event kind | effect |
| ---: | --- |
| 1 | VIOLENCE (`actor+0x248`) delta, clamped to 99 |
| 25 (`0x19`) | REPOS from the rest gauge (`actor+0x258`), percent value |

Other kinds write actor/phenotype stats the port does not model yet: `0x14`
courage (`+0x2a4`), `0x0a` perception, `0x17/0x18` food/health, `9..0x1c`
active action/duration. This is why Eat in retail raises **VIOLENCE +15** and
does not restore FOOD; employee feeding is the food-source refill path.

### Action lifecycle (partly open)

`FUN_00571540` decrements the action countdown at `saDinosaur+0x260` by dt and
calls `FUN_00571190` when it goes negative. `FUN_005727b0` sets that countdown
only for the special action ids 9..0x1c (Smell, Speed, ...); normal actions
(`0 Explore`, `4 Eat`, `2 Sleep`) do not get it from the element. Their
lifecycle is the element/animation state machine (`FUN_00522300` is the Eat
state machine; it calls `saFoodSource::SetLevel 0x580e60` when the eating
animation ends, not when the action id clears). The port currently uses the
`.bai` `field_2e` (30 s) as the action duration and gates the meal on a
3-unit proximity; **both are port scaffolding until the runtime probe
(`notes/dino-mover-runtime-probe.md`) pins the real element duration and the
SetLevel distance.**

## The mover (`saDinosaur+0x2c8`)

Park actions are straight-line steering, not a pathfinder. `saDinosaur::Update`
(`0x571540`) ticks a 0x220-byte mover (`0x5715cb`, ECX = `dino+0x2c8`) after
the behaviour update; the mover is a queue of 0x18-byte records and an
integrator.

| record offset | meaning |
| ---: | --- |
| `+0x00/+0x04` | target x/y (map units) |
| `+0x08` | mode-2 threshold/duration |
| `+0x0c/+0x0d` | flags |
| `+0x10` | move kind (1 walk, 2 run, 3/4/5 turn, 0x34/0x35 jump) |
| `+0x14` | arrival mode: 0 free, 1 exact position, 2 timed |

State (`mover+0x1ec`): 0 idle, 1 moving, 2 wait, 3 knocked down. `FUN_00574450`
integrates `pos += dt * speed * facing`; kind 1 uses `saDinosaur+0x268`
(walk), kind 2 `+0x26c` (run). Those are the phenotype speeds in **map units
per second**: measured Cpg04 values are 1.431/5.736 (6 m allosaur),
2.502/10.007 (7.4 m), 2.909/11.635 (1.7 m), so no centimetre scaling is
involved. Mode 2 accumulates `+0x24` and finishes when it reaches the record
threshold; mode 1 compares the position exactly. `FUN_005759c0` pre-drops
walk/run records within 0.01 units.

Turns are **45 degrees per record** (`FUN_005758f0` side 3 = +45, side 4 =
-45), with a single 180-degree record (side 5). The duration is **per family**
from `X[0x1bc + kind*4]`, i.e. `Actions.bad` **f[411..461]** (the old recon
read f[111] and saw a false 1.0 s). The 2026-10-30 Cpg04 probe measured the
table exactly: LightBiped 1.2/1.2/1.5 s (kinds 3/4/5) on 15/20 dinos,
HeavyBiped 2.0 s, LowQuadruped 2.0/2.0/1.5. The port now loads the five
family rows (`src/formats/actions_bad.*`) and sets `DinoMover`'s turn
durations from `DinoAttributes::family`; Part 10 family 2/3 read file blocks
3/2 (verified against the 20 Cpg04 `DINORESET f1e0` size classes and the
measured 15×1.2 / 4×2.0 groups). The exact `FUN_00575410` plan:

| dot | plan |
| ---: | --- |
| `>= 0.99` | `newFacing = desired`, no record |
| `>= 0.9238795` (<= 22.5 deg) | `newFacing = desired`, no record |
| `>= 0.38268343` (22.5..67.5 deg) | one record, unless run kind 2 and size class < 2 |
| `>= 0.0` (67.5..90 deg) | two records |
| `< 0.0` (> 90 deg) | one 180-degree record |

`cross = desired.y*facing.x - facing.y*desired.x`. For a normal turn the
new facing is `rotate(desired, side, steps)` with side 3 for `cross < 0` and
side 4 otherwise, then the inserted records use the **opposite** side
(kind 4 for `cross < 0`, else kind 3). That pre-rotation makes the completed
records land **exactly on the desired vector**, not on a 45-degree grid.
The `steps = 3` branch is dead code: both globals at `0x675710`/`0x675714`.
The port implements this plan in `DinoMover::begin_segment` (pre-rotate,
then one record per 1 s step).

Confinement is `FUN_0055af70(actor, x, y, flag, checkBounds)`: a step outside
the path object's paddock rect (`path+0x1c0`) or into an occupied footprint
cell (`path+0x1ac`, u16 grid) is refused. There is no per-dino paddock id and
no A*; the pathfinder (`FUN_0055ac30`/`0055ba50`) is only used for the
jump run-up kinds 0x34/0x35.

Queue semantics (`FUN_00573dc0` / `FUN_00573f40`): append and start when
state 0; a command that arrives while moving cancels the current record's
arrival (`mode = 0`) and keeps at most one next record. The element emitter
`FUN_00569660` takes the move kind from the element's profile pointer
(`element+0x04`, not the `+8` bone vector), always queues mode 2 with
`duration = FUN_00575de0(kind)` and targets the nearest reachable cell.

Port (2026-10-30): `src/world/dino_motion.{h,cpp}` implements the record
queue, 45-degree turn records (fixed 1 s in the port; retail is 1.2/1.5/2.0 s
per species), walk/run speeds from the phenotype, exact arrival and the
paddock-rect clamp. `ParkSession::tick_dino` sets the bounds from the current
paddock and copies the live position/angle back into `ImportedDino`;
`place_on_terrain` then draws the dino at the moved pose. The `.bsi` FoodSource
is the visitor/employee source, not a dinosaur feeder; the port's former
Eat-to-source/3-unit meal path was removed on 2026-10-31. The real dino path
is the LunchBox/StaticBait item actor (`FUN_00576e50`). The move starts and
stops are logged as `dino move: <species> start|stop|blocked`.

**Runtime correction (2026-10-30, `notes/dino-mover-runtime-probe.md`):** the
Cpg04 retail park does **not** take this path. The park behaviour layer calls
`0x573f40` directly and emits 1–3 record lists per element; `FUN_00569660` and
the action completion `0x571190` fired **0** times, no kind-8 Eat record
appeared, and `saFoodSource::SetLevel 0x580e60` never fired (the six
`0x522300` controllers are visitor-side with a null source). The port's
former one-record Eat and 3-unit consumption were **unsupported scaffolding**
and have been removed; the LunchBox/StaticBait actor + `FUN_00576e50` is the
replacement. The 0x34/0x35 walk/run markers are re-issued every frame in
retail, not once per action.

The actor-target executor's Eat walk is now ported (`0x56f94a`): while the
element is outside the target gate the port keeps one marker active toward
the feeder; arrival clears it (`0x56f98a` stop). The marker kind is
`0x34 + (REPOS state != 2)` from `dino+0x2b8` (state 2 = BAD/tired), so a
tired dino walks (0x34) and a rested one runs (0x35). Cpg04 evidence:
`--dino-needs 20,90,50,90` logs `kind=0x35`; `20,90,50,20` logs `kind=0x34`
(`test_cpp_lunchbox_eat`).

Port approximations (see `TASKS.md` P3-MOTION-1): one record per action
instead of the behaviour's record list; the paddock inset is
`clamp(length_m*0.15, 1, 3)` rather than the retail footprint grid; the
element→move profile (`element+0x04` weighted table) is not parsed yet; and
the `+0x1c/+0x20` initial facing writer is still open in
`notes/dino-mover-tick-recon.md`. The turn plan and the exact end facing are
retail-exact; the turn duration now comes from the family table (the Cpg04
allos log `turn=1.20 uturn=1.50`).

**Enclosure confinement (2026-10-31):** the port adopts the paddock index
once at spawn. Retail fixes the path rect at creation (`FUN_0055ac30` stores
it at `path+0x1c0`) and never re-resolves it from the live position, so a
boundary cell cannot re-home a dino into a neighbouring rect. A 600 s Cpg04
soak (20 dinosaurs, live action loop) logged 0 escapes and 0 blocked steps
(`notes/dino-enclosure-soak.md`, `test_cpp_dino_confinement_soak`).

`--fixed-dt SECONDS` makes a headless run deterministic; `--dino-move-speed N`
scales the mover for tests only. Selftest: `dino mover walk 2 m/s`, `turn
step 45`, `turn 90`, `180-degree record`, `paddock clamp`. Integration:
`test_cpp_lunchbox_eat` (Cpg04 anky runs/walks to the LunchBox by REPOS
state and drains it through `FUN_00576e50`).

## Move profiles (`special_action`)

`FUN_0056eec0` dispatches on the action profile's `+0x60`, which the runtime
probe identified as the `.bai` **`special_action` (1..6)**, not `effect_class`.
The jump table at `0x56f1e0` indexes `special_action - 1`:

| `special_action` | action examples | case | records emitted (state 0 unless noted) |
| ---: | --- | --- | --- |
| 0 (normal) | Eat, EatBait, Smell, gestures | — | **never enters `FUN_0056eec0`**; the `0x56f9xx` executor emits the per-frame `0x34/0x35` marker, then the stop/abort and the element gesture (`0x12..0x17`) |
| 1 | Exploration (0) | `0x56ef12` | `0x27` 2.0 s self; `0x27` `X[0x258]+0.5` action point; `0x34` marker to the action point; `0x20` `X[0x23c]`; `0x20` 5..20 s random self |
| 2 | RunAway (1) | `0x56ef8e` | `0x19` 2.0 s self, or a `0x35` marker to the steering cell |
| 3 | Sleep (2) | `0x56f087` | state 3: `0x1c` 1.5 s self; state 0: `0x1d` 1.5 s self + `0x1e` sentinel (`0x7f7fc99e`) self |
| 4 | DinoAttackForHerbivore/Carnivore (27/28) | `0x56f1c7` | none from this emitter |
| 5 | ObstAttackSize1-4, LinkObstAttack* | `0x56f1c7` | none here; `FUN_0056fb70` emits the `0x0b..0x0f` gestures and the `0x12..0x18` responses |
| 6 | Victory (3) | `0x56f152` | context gesture with duration `[0x006430a8]` (`0x7f7fc99e`) |

`FUN_0056fb70` (sp5) picks a random group from the dino's `+0x288` counts,
then a gesture kind (`0x0c..0x0f`, or 9/10 by the target's size class), and a
second gesture from the target's movement state (`FUN_00575be0` 0..3 →
`0x12/0x14/0x13/0x17` and the paired `0x13/0x16/0x18/0x17`). The probe saw the
full record lists and action mapping; see
`notes/dino-move-record-actions.md`.

The port's current one-record Eat move is therefore the **normal-action**
approximation; of the special-action lists only Sleep is implemented. The next
slice is the normal `0x56f9xx` executor (per-frame marker + stop/gesture) and
the remaining special lists.

**Port (2026-10-31):** the Sleep special list is implemented
(`ParkSession::tick_dino`): state 0 queues `0x1d` + the `0x1e` sentinel at the
dino position, the wake queues `0x1c`; gesture kinds have zero movement speed
and a mode-2 self record waits its full duration. Exploration is implemented
too: `paddock_patrol_point` reproduces
`min + rand() % (max-min-7) + 4`, then the five records (`0x27` self,
`0x27` point, `0x34` marker, `0x20` point, `0x20` self 5..20 s). Verified by
`test_cpp_dino_sleep_records` and `test_cpp_dino_exploration_records`.

The food gauge is also exact: `max_food = weight`, `food_gauge` starts at
`max_food*0.5`, the 10 s pass subtracts `10*food_consumption` and sets
`FOOD = ftol(gauge*99/max)`, and `apply_food(value, percent)` implements
`FUN_005727b0` case 0x17 (`gauge = value + percent*gauge/100`, clamped). The
same 10 s pass calls `FUN_00571410(10.0f)`, which subtracts **exactly 10
VIOLENCE points per 10 s** (`value = max(0, value - 10)`), replacing the tuned
0.2/s rate. Caller confirmed: `FUN_005869e0` resets its `+0x10` 10 s
accumulator at `0x586aa0` and calls `FUN_00571370(10.0f)` +
`FUN_00571410(10.0f)` + `FUN_00571350()` per dinosaur. The `.bsi` dino Eat path is removed; the LunchBox actor is ported: the
anky walks to `bowl.mesh`, `feeder_drain` implements `FUN_00576e50`, and
`apply_food(amount*nutrition)` fills the gauge (`test_cpp_lunchbox_eat`).

### Normal-action executor (`0x56f7a0`)

Normal actions (`special_action == 0`, including Eat) enter a separate
executor whose start is `0x56f7a0` (`SUB ESP,0x3c`; Ghidra did not create the
function boundary). Its jump table `0x56fb5c` switches on the action-node
state (`node+0x10`, 0..3):

- state 0 (`0x56f817`): compute the target/distance, call
  `FUN_0055b5b0` (nearest reachable cell); if reachable and the target is
  outside the node's distance gate (`node+0x00/+0x04`), emit the marker
  `kind = 0x34 + (dino[0x2b8] != 2)` through `FUN_00573f40` at `0x56f94a`
  (mode 1, duration 0), set state 1; otherwise call `FUN_0056f2f0` and set
  the element's done flag. If unreachable, emit the stop record
  `{self,self,0,mode2,0}`.
- state 1 (`0x56f994`): wait for arrival, then a stop record and state 2.
- state 2 (`0x56fa3b`): emit the element gesture from `node+0x40` with
  `FUN_00575de0(kind)` at `0x56fa98`; the probe saw kinds `0x12..0x17`.
- state 3: epilogue.

The marker is re-issued every frame; `FUN_00573f40` cancels the current
record's arrival (`mode = 0`) and keeps at most one pending. The source of
`node+0x40` (the per-action gesture kind) is still open.

## Evidence

- `notes/dinosaur-actions-recon.md` — decompiled switches and field evidence.
- `FUN_0053e060` — actionlist loader.
- `FUN_005727b0` — `ApplyActionEffect`.
- `FUN_00571190` — completion.
- `FUN_00571540` — per-frame rest decay and need normalisation.
- `FUN_00571370` — periodic food decay.
- `FUN_00571410` — violence decay.
- `FUN_005714b0` — need value to state index.
- `FUN_00572e10` — most urgent need index.
- `FUN_00570590` — need threshold init (`[80, 60, 30]`).
- `FUN_00589CD0` / `FUN_005774F0` / `FUN_00571B50` / `FUN_00571C00` —
  per-update danger-grid clear and per-dinosaur threat stamp.
- `FUN_00572DD0` - chooser target-inside-actor-radius box test.
- `FUN_00570fd0` — need init/reset.
- `FUN_00511d80`, `FUN_005138b0`, `FUN_005df900` — UI need widgets.
- `ActionList.ini` + 64 `.bai` files.
