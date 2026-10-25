# Dinosaur action indices and needs (DEC-2 follow-up)

Date: 2026-10-25
Scope: static decompilation; no writes to `game/` or `original/`.

Status: **action index table + action-effect switch solved; the two need slots
are still only partially named.** See section 7 for the SIM-3 correction and
the final need mapping.

## 1. Action list order

`FUN_0053e060` loads `actionlist.ini`, reads the count (64) and one action name
per line, then loads `DATA\design\attractions\actions\<name>.bai` for each and
stores the action records at stride `0xac`. The vector index is therefore the
action ID used by the simulation.

All 64 names in `ActionList.ini`:

| ID | Action | ID | Action |
| ---: | --- | ---: | --- |
| 0 | Exploration | 32 | JumpHole2 |
| 1 | RunAway | 33 | Smellbad |
| 2 | Sleep | 34 | Jump1m_Size1 |
| 3 | Victory | 35 | Jump1m_Size2 |
| 4 | Eat | 36 | Jump1m_Size3 |
| 5 | Fall | 37 | Jump2m_Size1 |
| 6 | Healed | 38 | MineExplosion |
| 7 | Danse | 39 | Teleport |
| 8 | Restore | 40 | ForceField |
| 9 | Smell | 41 | ReachWaypoint |
| 10 | Speed | 42 | Piege |
| 11 | GetInvincibility | 43 | GetSleep |
| 12 | GetEndurance | 44 | ForceFieldSmall |
| 13 | GetCourage | 45 | JumpForBiggies |
| 14 | GetIntelligence | 46 | Jump2m_Size2 |
| 15 | GetPerception | 47 | EatBait |
| 16 | GetBonus | 48 | LinkObstAttackSize1 |
| 17 | GetInvis | 49 | Jump2m_Size3 |
| 18 | GetJump | 50 | Jump3m_Size1 |
| 19 | GetQuad | 51 | Jump3m_Size2 |
| 20 | GetSlide | 52 | Jump3m_Size3 |
| 21 | GetScotch | 53 | Jump4m_Size1 |
| 22 | GetArmor | 54 | Jump4m_Size2 |
| 23 | Explode | 55 | Jump4m_Size3 |
| 24 | KickTheirAss | 56 | ObstAttackSize2 |
| 25 | HeadCharge | 57 | ObstAttackSize3 |
| 26 | ObstAttackSize1 | 58 | ObstAttackSize4 |
| 27 | DinoAttackForHerbivore | 59 | LinkObstAttackSize2 |
| 28 | DinoAttackForCarnivore | 60 | LinkObstAttackSize3 |
| 29 | Devour | 61 | LinkObstAttackSize4 |
| 30 | JumpFire | 62 | EatForSmallies |
| 31 | JumpHole | 63 | EatBaitForSmallies |

All 64 `.bai` files exist under `Data/design/attractions/actions/`.

## 2. `saDinosaur::ApplyActionEffect` (`0x005727b0`)

Signature observed: `(this, action_id, amount, percent, duration, name)`.

It switches directly on the action ID. The cases prove the IDs are the
`actionlist.ini` order:

| Action ID | Action | Effect |
| ---: | --- | --- |
| 1 | RunAway | `+0x92 += amount`, clamp 0..99; `0x005714b0(1)` |
| 9 | Smell | `+0x99 = clamp(amount + stat[0x21]*percent/100)` |
| 10 | Speed | `+0xa0 = clamp(amount + stat[0x22]*percent/100)` |
| 11 | GetInvincibility | `+0xa1 = clamp(amount + stat[0x23]*percent/100)` |
| 13 | GetCourage | `+0xa2..+0xa6` (5 stats), sum `+0xa7`, `+0xa8` |
| 16 | GetBonus | `+0x9c = clamp(amount + stat[0x2e]*percent/100)` |
| 20 | GetSlide | `+0xa9 = clamp(amount + stat[0x31]*percent/100)` |
| 26 | ObstAttackSize1 | `+0x9d`, `+0x9e = value²`, `+0x9f` |
| 27 | DinoAttackForHerbivore | `+0x9a`, `+0x9b` |
| 28 | DinoAttackForCarnivore | flag `+0xaa = 1` |
| 25 | HeadCharge | `+0x96 = amount + percent*+0x96/100`, clamp 0..`+0x76` |
| 23 | Explode | `+0x97 = amount + percent*+0x97/100`, clamp 0..`+0x77` |
| 24 | KickTheirAss | scale `int16 +0x16`; `vtable+0xc(-amount)` |

The same function also sets the current action id at **`+0xb0`**
(byte `0x2c0`), the duration at `+0x98`, and the `+0x4d` one-frame flag.

## 3. Action completion (`0x00571190`)

`0x571190` is called when an action's countdown expires (and from other actor
types). It switches on the current action id at `+0xb0`:

| Action ID | Action | Completion effect |
| ---: | --- | --- |
| 9 | Smell | copy `+0x84` to `+0x264` |
| 10 | Speed | copy `+0x88` to `+0x280` |
| 11 | GetInvincibility | copy `+0x8c` to `+0x284` |
| 13 | GetCourage | copy `+0xb0`/`+0x94` and 5 dwords into `+0x288` |
| 16 | GetBonus | copy `+0xb8` to `+0x270` |
| 20 | GetSlide | copy `+0xc4` to `+0x2a4` |
| 26 | ObstAttackSize1 | copy `+200`/`+0xcc`/`+0xd0` to `+0x274..+0x27c` |
| 27 | DinoAttackForHerbivore | copy `+0x7c`/`+0x80` to `+0x268`/`+0x26c` |
| 28 | DinoAttackForCarnivore | clear flag `+0x2a8` |

It then sets the action id to **`0x1f` (31)** and raises `+0x515` and `+0x4d`.
Note: 31 is `JumpHole` in the action list, so `0x1f` is acting as the
"action finished/idle" sentinel rather than a real action. This should be
re-checked at runtime.

## 4. Per-frame need decay (`0x00571540`)

The dinosaur update decays two gauges:

```text
+0x96  need A, decays every frame by +0x126 * table[+0x354 + type*4]
+0x97  need B, decays every 10 s by 10 * +0x1b
+0x98  action countdown (seconds)
+0x76  max A
+0x77  max B
+0xb0  current action id (byte 0x2c0)
```

`+0x130` selects the current activity/need type and therefore the decay
multiplier:

```text
1, 2          -> stat[0x9a] * 0.2      (RunAway, Sleep)
9, 10, 11     -> stat[0xa2]            (Smell, Speed, GetInvincibility)
12            -> stat[0xa3]            (GetEndurance)
13            -> stat[0xa4]            (GetCourage)
14            -> stat[0xa5]            (GetIntelligence)
15            -> stat[0xa6]            (GetPerception)
16, 0x25      -> stat[0x99]            (GetBonus, HeadCharge)
25            -> stat[0x2f]            (HeadCharge special case)
```

The two gauges are restored by HeadCharge (`+0x96`) and Explode (`+0x97`).
They are two independent need/status slots; the semantic names (hunger,
fatigue, health, stamina) are still open. The max fields and the UI need
labels are the next place to look.

## 5. UI need axes

The dinosaur list panel (`bvwDinosaurListPanel.cpp`, `0x005df900`) builds four
need indicators per row from these widget names:

```text
DINO_INDICATEURS_BESOINS_VIOLENCE_SLOTn
DINO_INDICATEURS_BESOINS_REPOS_SLOTn
DINO_INDICATEURS_BESOINS_FOOD_SLOTn
DINO_INDICATEURS_BESOINS_SECU_SLOTn
```

`ParkEng.loc` gives the corresponding labels/warnings:

```text
&DinoNeed       = Needs
&DinoStarving   = Starving
&DinoExhausted  = Exhausted
&DinoBerserk    = Irritated
&DinoPanicked   = Panicked
```

So the four need axes are **violence, rest/sleep, food, security**. The two
gauges decoded in `saDinosaur::Update` (`+0x96`, `+0x97`) are two of those
four; **food** and **rest** are the most likely pair because they decay over
time, while violence/security are probably derived from context. This is not
yet proven.

## 6. Open

- Semantic names for `+0x96` and `+0x97` (and the max fields `+0x76`/`+0x77`).
- Whether `0x1f` is a true idle sentinel or a real JumpHole action.
- The `+0x130` activity values 3..8, 17..24, etc.
- The effect fields `+0x264..+0x2a8` and how they map to the UI dinosaur stats.
- The 17-byte AI entries in the save (possibly the same action/need state).

## 7. SIM-3 correction: the real need offsets and UI mapping

Date: 2026-10-25

The offsets in sections 3–5 above are **dword indices**, not byte offsets.
Ghidra decompiled against `int *param_1`, so `param_1[0x96]` is byte
**`+0x258`**, `param_1[0x76]` is `+0x1d8`, and `param_1[0xb0]` is `+0x2c0`.
The corrected layout is:

### Raw gauges

| Byte | Meaning | Max | Evidence |
| ---: | --- | ---: | --- |
| `+0x258` | rest/sleep gauge | `+0x1d8` | `0x571905`/`0x571911`; init in `0x570fd0` |
| `+0x25c` | food gauge | `+0x1dc` | `0x57137a`/`0x5713e9`; init in `0x570fd0` |
| `+0x260` | action countdown | — | `0x5719fa` |
| `+0x2c0` | current action | — | `0x572826`, `0x57283a` |
| `+0x4c0` | activity type | — | `0x5718e2` |

`0x570fd0` initialises rest to `+0x1d8` (full), food to `0x1dc * 0.5`,
and the four need values to 99.

### Four UI needs

| Byte | UI axis | State index | State widgets |
| ---: | --- | ---: | --- |
| `+0x244` | **FOOD** | `+0x2ac` | `+0xd0` GOOD, `+0xd4` MID, `+0xd8` BAD |
| `+0x248` | **VIOLENCE** | `+0x2b0` | `+0xe8` GOOD, `+0xec` MID, `+0xf0` BAD |
| `+0x24c` | **SECU** | `+0x2b4` | `+0xf4` GOOD, `+0xf8` MID, `+0xfc` BAD |
| `+0x250` | **REPOS** | `+0x2b8` | `+0xdc` GOOD, `+0xe0` MID, `+0xe4` BAD |

Evidence:

- `0x5138b0` reads `+0x2ac`, `+0x2b8`, `+0x2b0`, `+0x2b4` and enables the
  FOOD, REPOS, VIOLENCE, SECU state widgets in that order.
- `0x511d80` stores the FOOD/REPOS/VIOLENCE/SECU state widgets at the offsets
  above.
- `0x572e10` returns the largest state index (0..3, 4 if none) in the order
  `+0x2ac`, `+0x2b8`, `+0x2b4`, `+0x2b0`, which matches FOOD/REPOS/SECU/
  VIOLENCE in the UI helper.
- `0x5714b0(category)` maps a need value to a state index using the threshold
  array at `+0x3c + category*4`; `0x570590` initialises every array to
  `[0x50, 0x3c, 0x1e]` = `[80, 60, 30]`.
- `0x571350` recomputes all four states.

State mapping (value higher is better):

```text
<= 30       -> 2 BAD
30 .. 60    -> 1 MID
60 .. 80    -> 0 GOOD
> 80        -> 3 (rendered as GOOD)
```

### Feed-forward

- `RunAway` adds to `+0x248` (VIOLENCE) and calls `0x5714b0(1)` (`0x5727fc`).
- `0x571410` decays `+0x248` (VIOLENCE).
- `0x571370` decays `+0x25c` (food) and writes the normalised value to
  `+0x244` (FOOD).
- `0x571540` decays `+0x258` (rest) and writes the normalised value to
  `+0x250` (REPOS).
- `HeadCharge` restores `+0x258`; `Explode` restores `+0x25c`.

### Idle sentinel

`0x571540` skips completion when `+0x2c0 == 0x1f`; `0x572826` checks the same
before calling `0x571190`; `0x571190` sets `+0x2c0 = 0x1f`. **Confirmed:**
`0x1f` is the idle/finished sentinel.

### Eat / sleep

`Eat` (4) and `Sleep` (2) are real actions. The FOOD and REPOS needs above are
their feed. The exact dinosaur-AI chooser that turns a BAD state into action
4/2 is still open; the `.bai` action records and the dinosaur behavior tree are
the next step.

## Evidence

- `FUN_0053e060` — actionlist.ini loader; vector stride `0xac`.
- `FUN_005727b0` — `ApplyActionEffect` switch.
- `FUN_00571190` — action completion switch.
- `FUN_00571540` — per-frame need decay and `+0x130` activity switch.
- `FUN_0053a400` / `FUN_0051d1d0` — action directory
  `DATA\design\attractions\actions\`.
- Files: `Data/design/attractions/actions/ActionList.ini` and 64 `.bai`
  files (read-only).
