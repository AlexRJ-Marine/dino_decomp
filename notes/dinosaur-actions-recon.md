# Dinosaur action indices and needs (DEC-2 follow-up)

Date: 2026-10-25
Scope: static decompilation; no writes to `game/` or `original/`.

Status: **action index table + action-effect switch solved; the two need slots
are still only partially named.**

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

## Evidence

- `FUN_0053e060` — actionlist.ini loader; vector stride `0xac`.
- `FUN_005727b0` — `ApplyActionEffect` switch.
- `FUN_00571190` — action completion switch.
- `FUN_00571540` — per-frame need decay and `+0x130` activity switch.
- `FUN_0053a400` / `FUN_0051d1d0` — action directory
  `DATA\design\attractions\actions\`.
- Files: `Data/design/attractions/actions/ActionList.ini` and 64 `.bai`
  files (read-only).
