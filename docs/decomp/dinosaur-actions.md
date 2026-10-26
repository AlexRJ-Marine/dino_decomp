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
Their need feed is the FOOD/REPOS pair above; the FOOD gauge starts at 50 % of
its maximum and the REPOS gauge starts full.

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
  `preconditions_ok` and `choose_action`.
- `--dino-actions` prints the chooser result for starving / tired / both /
  full / small scenarios using the self-action node set.
- Tests: `tests/test_dino_actions.py` (Python oracle) and
  `tests/test_viewer_cpp.py::test_cpp_dino_action_chooser`.
- Open for the next increment: need decay into the needs, action duration and
  need-delta application, and the action→animation clip link.

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
- `FUN_00570fd0` — need init/reset.
- `FUN_00511d80`, `FUN_005138b0`, `FUN_005df900` — UI need widgets.
- `ActionList.ini` + 64 `.bai` files.
