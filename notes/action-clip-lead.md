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
The object that actually supplies the two `u16` values is therefore still
unidentified.

## Next trace (runtime)

1. Breakpoint at `0x56c0c0` (or at the two stores `0x56c1a7`/`0x56c1b8`);
   on an Eat/Sleep action, dump `param_1`, `[param_1]` (vtable) and the two
   returned `u16`s. This identifies the class and the clip pair.
2. Alternatively watch `[[actor+0x28]+0x314/+0x316]` while forcing a known
   action (e.g., `Eat` from the P3 need state) and correlate with the clip
   the animator plays.
3. Then map all 64 action ids from the two returned indices and store the
   table in the port instead of the inferred names.

## Evidence

- `tools/formats/anim_trans.py` — 51-clip order.
- Ghidra: `0x65e4d0` table (raw dump), `0x5d8e60` animation manager,
  `0x56c0c0` clip-pair writer, `0x521cc0`/`0x521e18` dinosaur update call,
  `0x642abc`/`0x64302c`/`0x643040` vtables.
- `docs/formats/anim-trans.md`, `notes/dino-action-chooser-recon.md`.
