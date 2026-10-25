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
Because 31 is `JumpHole` in the table, `0x1f` is most likely the
"action finished / idle" sentinel, not a real action; confirm at runtime.

## Need gauges

`saDinosaur::Update` (`0x00571540`) maintains two gauges:

```text
+0x96  need A, decays every frame by +0x126 * table[+0x354 + type*4]
+0x97  need B, decays every 10 s by 10 * +0x1b
+0x76  max A
+0x77  max B
+0x98  action countdown
+0xb0  current action id
```

`+0x130` selects the current activity/need type and its decay multiplier:

| +0x130 | Actions | Multiplier |
| ---: | --- | --- |
| 1, 2 | RunAway, Sleep | `stat[0x9a] * 0.2` |
| 9, 10, 11 | Smell, Speed, GetInvincibility | `stat[0xa2]` |
| 12 | GetEndurance | `stat[0xa3]` |
| 13 | GetCourage | `stat[0xa4]` |
| 14 | GetIntelligence | `stat[0xa5]` |
| 15 | GetPerception | `stat[0xa6]` |
| 16, 25 | GetBonus, HeadCharge | `stat[0x99]` |
| 25 | HeadCharge | `stat[0x2f]` (special case) |

The gauges are restored by HeadCharge (`+0x96`) and Explode (`+0x97`). The UI
shows four axes (violence, rest/sleep, food, security); which two correspond to
`+0x96`/`+0x97` is still open, with food/rest the most likely pair.

## UI need axes

`bvwDinosaurListPanel.cpp` (`0x005df900`) creates four need indicators per
dinosaur row:

```text
DINO_INDICATEURS_BESOINS_VIOLENCE_SLOTn
DINO_INDICATEURS_BESOINS_REPOS_SLOTn
DINO_INDICATEURS_BESOINS_FOOD_SLOTn
DINO_INDICATEURS_BESOINS_SECU_SLOTn
```

`ParkEng.loc` supplies the matching states:

```text
&DinoNeed       = Needs
&DinoStarving   = Starving
&DinoExhausted  = Exhausted
&DinoBerserk    = Irritated
&DinoPanicked   = Panicked
```

The four axes are **violence, rest/sleep, food, security**. The two decaying
gauges `+0x96`/`+0x97` are two of those four; food and rest are the most likely
pair, but this still needs a runtime or UI-field cross-check.

## Evidence

- `notes/dinosaur-actions-recon.md` — decompiled switches and field evidence.
- `FUN_0053e060` — actionlist loader.
- `FUN_005727b0` — `ApplyActionEffect`.
- `FUN_00571190` — completion.
- `FUN_00571540` — per-frame decay.
- `ActionList.ini` + 64 `.bai` files.
