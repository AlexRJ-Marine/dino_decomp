# Action → animation clip: negative results and next trace

Status: **open.** This records what does *not* explain the mapping, so the next
agent does not repeat it. The port currently uses an inferred name mapping
(`Eat`→`Chew`, `Sleep`→`Sleep`, …); it is not retail-verified.

## Clip ID space is the `anim_trans.dic` order (51 clips)

`anmAnimationManager` (`0x5d8e60`) loads `Design/Genotype/Anim_trans.dic`
and builds the family animation libraries from it. The 51 clip names are in
this fixed order (verified with `tools/formats/anim_trans.py`):

```text
0 Neutral   1 Walk    2 Run     3 SwingL   4 SwingR   5 UTurn
6 Backward  7 WalkDizzy 8 BowlAttack 9 HeadAttackLow 10 HeadAttackHigh
11 HeadAttackMiddle 12 HeadChargeOK 13 HandAttack 14 FeetAttack
15 TailAttack 16 Trample 17 HeadChargeFail 18 TakeABlowFront
19 TakeABlowL 20 TakeABlowR 21 Collapse 22 SideStepL 23 SideStepR
24 Backstep 25 Scream 26 Winner 27 Chew 28 Wakeup 29 LieDown 30 Sleep
31 WaitScratch 32 WaitSweep 33 WaitFight 34 WaitHappy 35 WaitSad 36 Fart
37 LJumpOK 38 CrashDownWall 39 Sniff 40 TakeAWall 41 FallInTrapp
42 Stamina 43 QuadPower 44 Invincibility 45 Armor 46 GetOut
47 SommerSault 48 SuperAttack 49 Dance 50 Slip
```

## The 57-entry name table at `0x65e4d0` is dead data

`0x65e4d0..0x65e5b0` is a 57-entry `char*` table: the 51 clips above followed
by `MISC`, `EXPLO`, `RUNAW`, `SLEEP`, `D_ATT`, `O_ATT` (`0x65e59c..0x65e5b4`).

- No instruction or data references the table base (`0x65e4d0`): a raw scan
  for the dword finds zero hits, and Ghidra has no xrefs.
- The individual strings are referenced only for unrelated labels: `MISC` is
  a config key at `0x51d668`, `Armor`/`Stamina` are attribute labels in the
  lab/purchase panels. The uppercase six are never played as `.anim` files
  (each family folder has exactly the 51 lower-case clips).
- Conclusion: the table is debug/editor metadata in this build, not the
  action→clip map.

## `.bai` element `bones` are skeleton bones, not clip ids

Checked against `allo.skel`: Eat `8` = `Bip01_Neck`, Dance `49` =
`Bip01_L_Foot`, GetArmor `35`/`45` = `Bip01_R_Clavicle`/`Dum_doshaut`.
These are attachment/dummy bones for the effect, not animation indices.

## `+0x314`/`+0x316` are the only clip-id fields

`FUN_0056c0c0` (`asItemBehavior::RegisterForAction`) stores two `u16` ids at
`[[actor+0x28]+0x314]` and `+0x316` from `vtable+0x20` / `vtable+0x24` of its
first argument (`0x56c1a7`, `0x56c1b8`). Those are the only writers; the
dinosaur action loop does not otherwise carry a clip id.

`apDinosaurBehavior` (`0x521b30`) calls the record update `FUN_0056d880`
(which drives `FUN_0056e670` → `FUN_0056c0c0`), but its own vtable
(`0x642abc`) has no code at slot `+0x20`/`+0x24`, and the two sub-behaviors
(`0x64302c`/`0x643040`) return comparison/score values there, not clip ids.
The object that actually supplies the two `u16` values was identified at
runtime as the show/item behavior classes with vtables `0x6430AC` and
`0x6430E4`; their getters return 0/0 and a state id/1, so this path is not the
dinosaur action→clip map (see below).

## Runtime result: `0x56c0c0` is not the dinosaur action→clip map

A live Cpg04 run logged `0x56c0c0` with two real vtables, `0x6430AC` and
`0x6430E4`. Their `+0x20`/`+0x24` slots are:

| vtable | `+0x20` | `+0x24` |
| --- | --- | --- |
| `0x6430AC` | `0x56F290` → returns `0` | `0x56F290` → returns `0` |
| `0x6430E4` | `0x56FE70` → `AX = [[[this+0x38]+0x28]+0x28]` | `0x56FE80` → returns `1` |

So `[actor+0x28]+0x314/+0x316` are **item/show animation state ids**, not an
action-id→clip table. This lead is closed. Details:
`notes/campaign-unlock-runtime.md`.

## Conclusion: there is no action→clip table

The `aacDinosaurAnimator` vtable is `0x6480B0` (ctor `0x5D4880`). Its update
method `FUN_005D82F0` reads `saDinosaur+0x2BC` and, when the value is 0..2,
blends the matching animation slot (`FUN_005D8870(1, animator + value*2 + 5)`
then `FUN_005D8960`). A separate branch reads `saDinosaur+0x514`,
`+0x518` and `+0x520` and applies an **event/envmap texture string**
(`FUN_00607200` on the actor's string).

`+0x2BC` is a locomotion/activity state 0..4, written by the saDinosaur
constructor (`0x57031F`, `0x570604`), reset to 0 (`0x570FA5`) and updated by
the 5 s/10 s AI passes (`0x57250D..0x5725AC`, including value 4). It is **not**
the action id `+0x2C0`; the action id is never passed to the animator.

The `+0x518` string is the **`.bai` element's texture path** (the bonus
envmap TGAs: `blindage.tga`, `endurance.tga`, `jump.tga`, `quaddamage.tga`,
`speed.tga`). `ApplyActionEffect` copies it into `saDinosaur+0x518` and sets
`+0x514 = 1`; the animator then applies it through `FUN_00607200` /
`FUN_005D8870` / `FUN_005D8960` as a material/envmap effect, not as a clip.
So the action visuals are:

- a locomotion/activity slot from `+0x2BC`,
- a bonus envmap texture from the element string,
- bone/event effects from the `.bai` element and `.seq` events.

There is no whole-body action clip to look up.

So the earlier premise ("find the 64-entry action→clip table") is wrong. The
port should:

1. drive the locomotion slot from movement/activity (Neutral/Walk/Run),
2. apply the `.bai` element envmap texture for bonus actions, and
3. play the action's element bones/events as overlays, not swap a whole-body
   clip per action.

The current inferred name mapping (`Eat`→`Chew`, etc.) is a port convenience,
not retail behavior.

## Remaining evidence

- The element `bones`/event effects are applied by the animator/sequence
  system; the exact bone-overlay path is not traced.
- Locomotion-state thresholds (when `+0x2BC` becomes 0/1/2/4) come from the
  periodic AI passes, not the action loop.

## Evidence

- `tools/formats/anim_trans.py` — 51-clip order.
- Ghidra: `0x5D4880` animator ctor (vtable `0x6480B0`), `0x5D82F0`
  locomotion/overlay update, `0x5D8870`/`0x5D8960` blend/play, `0x57031F`
  `+0x2BC` init, `0x57250D..0x5725AC` periodic state writes.
- Runtime: `0x56c0c0` vtables `0x6430AC`/`0x6430E4` return show/item state ids
  0/0 and state/1 (`notes/campaign-unlock-runtime.md`).
- `docs/formats/anim-trans.md`, `notes/dino-action-chooser-recon.md`.
