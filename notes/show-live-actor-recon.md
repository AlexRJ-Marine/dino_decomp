# Live show/attraction actor chain (2026-10-31)

Read-only Ghidra pass on the retail `DinoIsland.exe`. Scope: SHOW-4's remaining
live `.bfi` arena/judge actors, cast and score. The port already has the show
catalog, `ShowRuntime` (10 hourly slots), the construction panel and the ten
attraction rows.

## 1. `FUN_00520880` is the BehaviorManager command queue

Called from `FUN_0051fab0` at `0x51fd81` when `[mgr+0x5c] != [mgr+0x60]`.
Input: 0x10-byte records at `mgr+0x5c..+0x60`; the queue is cleared after the
pass. Record type `rec[0]`:

| type | action |
| ---: | --- |
| 1 | group follow: each team member (`rec[2]+0xf4`) emits mover kind **7**, duration `[member+0x1f8]+0x1d8` |
| 2 | approach partner: if within the group-centre squared radius (`[rect+0x28]+0x38`), emit kind **0x12**, duration `[member+0x1f8]+0x204` |
| 3 | pick a random point from `[actor+0x28]+0x64/+0x68` (5 tries), validate `FUN_0055af70(actor,y,x,actor+0x2e4,1)`, notify `actor->vtable+0x20`, stop the mover, `FUN_0058f670(actor,&point)` |
| 4 | finish: `FUN_00520c30(rec[2]); FUN_0056ab30(rec[3]); FUN_0056c380()` |

This is a generic command queue, **not** the show timer. 0x07/0x12/0x1b/0x24
are mover/action kinds, not queue types.

## 2. Show arena creation (`FUN_0053cfb0`)

```c
design = DB+0x6c + showRecord[0x14]*0xb4;                 // 0xb4 show design
arena  = FUN_005868f0(showRecord+0x18, showRecord+0x20,
                      design+0xc, showRecord+0x44);        // -> FUN_00588ac0
ai     = FUN_0051fe20(arena, DB+0x158, DB+0x174, showRecord+0x48); // 0x274 AI
scene  = FUN_005f4b60(design+0x24, arena);                 // gpkParkScene
audio  = FUN_004fea70(DB+0x120, arena, ...);               // sdpAudioPark
FUN_005e7630(arena, ai, scene, audio, ...);
for (r = showRecord+0x2c; r < showRecord+0x30; r += 0x18)
    FUN_0053acd0(showRecord, r, 0);                        // judge/props/cast
```

`FUN_005868f0` -> `FUN_00588ac0` -> `FUN_00579440` = **`stShow` ctor**
(0x154 bytes, vtable `0x643220`, assert `stShow.cpp`). It creates five
0x40-entry arrays, `FUN_0057c910(4,...)`, and `stShowStatistics` at `+0x3b`
(ctor `0x57f5e0`, 0x84 bytes). `FUN_005e7630` stores AI at `arena+0x5c`,
scene at `+0x64`, audio at `+0x60`, extra at `+0x68`.

## 3. `.bfi` actors (`FUN_0053acd0`)

Per 0x18 record:

```c
design = DB+0x7c + rec[2]*0x1d8;
if (design+0x34 && strcmp(design+0x34, "Judge") == 0) { ... judge branch ... }
obj = FUN_00589020(rec[3], design+0xc, rec[4], rec[5], iVar1+0xc, *rec);
if (obj->type() == 0x16 || 0x17)
    FUN_0056a730(featureDef, obj, design+0x68);   // action list from .bfi
FUN_005f4e10(..., design+0x84, obj, ...);         // scene/model
FUN_004fef10(..., obj, design+0x18c);             // attributes
if (flag) FUN_005e9250(rec);
```

- Judge detection is the **name string at `design+0x34 == "Judge"`**.
- Action list = `design+0x68`, scene = `+0x84`, attributes = `+0x18c`.
- Judge kind is `featureDef+0x40`; `FUN_0057c260` dispatches it:
  Race `0x584040`/`0x64350c`, Obstacle `0x583fa0`/`0x6434e8`, Fight
  `0x583ef0`/`0x6434c4`, Destruction `0x583e90`/`0x6434a0`, Team
  `0x583e40`/`0x64347c`, Capture `0x583dd0`/`0x643458`.
- Judge actor stored at `stShow+0xcc`.
- `.bfi` `attribute_sets`/`score_steps` (`+0xE8/+0x104`) have **no runtime
  consumer** in this build.

## 4. Cast/team

`FUN_00583230(showObj, dino)` searches `showObj+0x50..+0x54` (0x10-byte
records) for the dino pointer; returns 1/0. The show object is `dino+0xcc`.
A show dino is created by `FUN_0053a980(genotype)` -> `FUN_00586690`
(0x52c, `saDinosaur` ctor `0x5701f0`), then scene/anim `FUN_005f4450`/
`FUN_005d9200`.

**OPEN:** the producer that appends the dino into `show+0x50/+0x54`. The
natural candidates `FUN_005e4cf0`/`FUN_00535710` were not proven.

## 5. Score and Victory

- `.bfi` `score_steps` are not consumed.
- Runtime score: `FUN_00576550(dino5, profile5)` = dot of five floats,
  clamped `[-100,+100]`; `FUN_00581770` takes the max over participants, adds
  `dino+0x4f8/+0x4f9` bonuses, clamps `[-130,+130]`, smooths at
  `0.07/frame`.
- Dino five floats come from genotype -> `FUN_00618020`; `FUN_00586690` fills
  them at dino creation.
- Victory kind 26/35 is the class-6 animation. The normal park class-6 gate
  is dead (returns 50; scorer needs 1), so a live show must **force** the
  Victory element/action, not rely on node availability. The exact
  show-end -> Victory producer is OPEN.

## 6. Minimal live-show recipe

```text
1. stShow = FUN_005868f0(world, showRec+0x18, design+0xc, showRec+0x44)
   -> FUN_00588ac0 -> FUN_00579440 (vtable 0x643220)
2. AI = FUN_0051fe20(arenaEntity, DB+0x158, DB+0x174, showRec+0x48)
3. scene = FUN_005f4b60(design+0x24, arena); audio = FUN_004fea70(...);
   FUN_005e7630(arena, AI, scene, audio, ...)
4. for each 0x18 rec in showRec+0x2c: FUN_0053acd0(showRec, rec, 0)
   judge .bfi -> featureDef+0x40 kind -> FUN_0057c260 -> judge at stShow+0xcc
5. add dino: FUN_0053a980(genotype) -> saDino, append to stShow+0x50/+0x54
   (producer OPEN)
6. run stShowStatistics; score with FUN_00581770; at show end force the
   Victory element (26/35) on the winner.
```

Files: `Data/Design/Scenario/show/*.shw`, `Data/Design/attractions/features/*.bfi`,
`WallList.ini`/`FeatureList.ini`.

## Open

1. Cast append producer (`stShow+0x50/+0x54`).
2. Exact show timer/end trigger.
3. Show-end Victory emitter.
4. `.bfi` `score_steps`/`attribute_sets` are unused in this build (negative
   result).
