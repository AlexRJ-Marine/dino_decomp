# P3 — dinosaur action loop contract

Status: **port-ready for needs, action execution and completion; the
action→animation clip lookup is the one open lead.** Builds on
`docs/formats/bai.md` and `docs/decomp/dinosaur-actions.md`.

## Need reset (`0x570fd0`)

Called when a dinosaur is created or its care state is reset:

```text
+0x258 (rest gauge) = +0x1d8 (max)
+0x25c (food gauge) = +0x1dc * 0.5
+0x244..+0x250 (FOOD, VIOLENCE, SECU, REPOS) = 99
+0x254 = 100
FUN_005710c0()
+0x514..+0x517 = 0
+0x524 = 0
if (+0x200 == 0):
    +0x214..+0x224 (5 phenotype stats) = 100
    +0x228..+0x240 (7 ints) = 0
else:
    +0x214..+0x224 = 0
    +0x228..+0x240 = 100
```

## Need decay (`saDinosaur::Update 0x571540`)

- rest gauge `+0x258` decays every frame by
  `+0x498 * table[+0x354 + activity*4]`, clamped to `0..+0x1d8`.
- food gauge `+0x25c` decays in the periodic pass `0x571370`/`0x571410`.
- The four UI needs `+0x244..+0x250` are separate 0..99 axes; threshold
  states are `[80, 60, 30]` (`0x5714b0`).
- Action id `+0x2c0` is the current action; `0x1f` is idle.

## Action selection (`.bai`, already decoded)

`FUN_0056e0e0` gates an action on the `(threshold, attribute, comparison)`
precondition triples. `FUN_00569660` scores each element:

```text
score = sum(attribute * weight) + rand() % field_14
pick the first element whose time >= score
```

`FUN_0056e410` walks the usable action nodes, applies the effect weights and
the actor's percentage, and stores the score at node `+0xc`. The best node's
action is started. This is the BAD FOOD/REPOS → Eat/Sleep path; no separate
node-type table is involved.

## Element execution (`FUN_00569a00`)

For the selected `.bai` element:

- Walks the element's effect-id list; effect id `8` sets the "exploded" flag
  and applies effect class `0x17` (Explode) through `0x5727b0`.
- Applies the element's events (effect id + percent-of-current rule from
  `docs/formats/bai.md`).
- Copies five dwords from the element (`+0x14..+0x28`) to `actor+0x504`.
- If effect class `+0x54` is `2`, calls `0x57af70(actor)`.
- Raises the one-frame `+0x4d` flag.

## Completion (`FUN_00571190`)

On countdown expiry `0x571190` switches on the action id and copies the
temporary reward fields into permanent stats:

| Action | Destination |
| --- | --- |
| 9 Smell | `+0x264` |
| 10 Speed | `+0x280` |
| 11 GetInvincibility | `+0x284` |
| 13 GetCourage | `+0x288` + 5 dwords |
| 16 GetBonus | `+0x270` |
| 20 GetSlide | `+0x2a4` |
| 26 ObstAttackSize1 | `+0x274..+0x27c` |
| 27 DinoAttackForHerbivore | `+0x268`/`+0x26c` |
| 28 DinoAttackForCarnivore | clear `+0x2a8` |
| 0x1c | `+0x2a8 = 0` |

Then it sets `+0x2c0 = 0x1f`, `+0x260 = 0`, `+0x515 = 1`, `+0x4d = 1`.

## Animation clips

The 51 clips per family are named by behaviour, not by action id
(`Neutral`, `Walk`, `Run`, `Sleep`, `Chew`, `Sniff`, `Scream`, `Winner`,
`WaitHappy`/`WaitSad`/`WaitFight`, attack and hit clips). The action→clip
binding is not in the `.bai` file: it is registered per action by the actor's
animation/behavior layer. `FUN_0056c0c0` creates a 0x18-byte action record
(`+0x14` action id, `+0xc` duration, `+0x10` data, `+0x8` flag), stores the
two animation ids returned by vtable `+0x20`/`+0x24` at actor
`+0x28+0x314/+0x316`, and sets `+0x1c = descriptor[+0x38]`.

**Next lead:** locate the concrete action→clip table behind vtable
`+0x20`/`+0x24` (the `aac*`/`anm*` animation classes). The clip names and
`.anim` files are already mapped in `docs/formats/anim-trans.md`.

## Rest-gauge table evidence (2026-10-31)

The per-activity scale table is `Actions.bad` block float `105 + kind` (the 51
scale floats); the caller indexes it as `[block + 0x354 + kind*4]`. All five
families agree:

```text
scale[1]    = 0.5    walk (record kind 1)
scale[2]    = 2.0    run  (record kind 2)
scale[0x1d] = 0.0    sleep transition
scale[0x1e] = -10.0  sleep sentinel -> gauge -= rate * (-10) = +10/frame
scale[0x1c] = 0.0    wake gesture
```

`FUN_00571540` at `0x5718e2`: `kind = [dino+0x4c0]` (mover activity type);
`gauge(+0x258) = clamp(gauge - [dino+0x498] * scale[kind], 0, +0x1d8)`;
`REPOS(+0x250) = ftol(gauge * 99 / +0x1d8)`. `[dino+0x498]` is the mover's
`0x1d0` progress-at-last-accepted-move (usually 1.0). `FUN_00574350` resets
`+0x4c0` every mover tick and sets it on record arrival (`FUN_00575ac0`).

Open: how the `0x1e` sentinel (mode 2, duration FLT_MAX) becomes the activity
type so sleep actually restores rest; needs a retail probe of
`+0x4c0/+0x498/+0x258` while a dino sleeps. Max rest is `def+0x188` =
`stamina + x` (`0x617dfb`, x untraced; store at `0x617a0a`).

## Port order

1. `SaDinosaur` state: gauges, four needs, action id, current element.
2. On spawn: `need_reset` above.
3. Per frame: decay gauges; every 5 s run the `.bai` precondition/score and
   start the best action; every 10 s run the slow need pass.
4. While acting: advance `.bai` element time, apply events/effects, play the
   mapped clip.
5. On completion: apply the table above and return to idle `0x1f`.
