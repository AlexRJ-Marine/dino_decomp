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

`FUN_00583230(judge, dino)` searches `judge+0x50..+0x54` (0x10-byte records)
for the dino pointer. **The writer is `FUN_00583060(judge, groupId)`**
(`0x00583060`):

```c
src = *(int**)(judge+0x44);              // = stShow+0xf0 participant vector
for each p in src[+4..+8]:               // all show participants (dinos)
    if (p+0x1d0 == groupId) append p -> judge+0x50;  // cast
    else                    append p -> judge+0x60;  // spectators
```

`judge+0x44 = stShow+0xf0` is set by `FUN_0057c260`. `FUN_00583060` is
called from the judge start/update subclasses (`FUN_00583fe0`, `0x583f78`).
A second staging append `FUN_00582840(obj, dino)` pushes the dino pointer into
a `+0x54` vector plus a 0 flag byte at `+0x44`; called from performance start
`FUN_0057ba50` over `stShow+0xf4` participants and the judge groups
`stShow+0xc0/+0x128/+0x148`.

The participant list is `stShow+0xf0` (begin `+0xf4`, end `+0xf8`); team id is
`dino+0x1d0`.

## 5. Show timer / phases

`stShow` vtable `0x643220` (ctor `0x579440`):

| slot | address | role |
| ---: | --- | --- |
| `+0x10` | `0x57a4a0` | participant/selection helper |
| `+0x14` | `0x581710` | serialization/read |
| `+0x18` | `0x57ba30` -> `FUN_00581770` | per-frame score |
| `+0x1c` | `0x579830` | update/end helper |

Scheduler (`FUN_0058c2f0`/`FUN_0058c760`/`FUN_0058c390`):

- `FUN_0058c2f0` advances show time and sets `+0x28` when the hour slot
  changes; `+0x29` is the after-10th-slot clamp.
- `FUN_0058c760` at each slot: `show = list[sched[6]]`; requires
  `show+0xd3 != 0` (scheduled), `show+0xe0 == 2`, `show+0xe4 == 0` (idle),
  and `FUN_0058c390(show, parkTime)` in the window -> **`FUN_0057b110(show)`**
  (`+0x14=1, +0xd4=1, +0xe4=1`).
- **`FUN_0057ba50(show)`** is the performance reset/start: `+0x14=1,
  +0xd5=0, +0xe4=2`; clears the `+0x108/+0x138` vectors, calls
  `FUN_0057c700(+0x7c)`, then rebuilds judge cast lists via `FUN_00582840`
  over `stShow+0xf4`.
- End: when the window/state check passes, `FUN_0058c8f0(show)` then
  **`FUN_0057b130(show, result)`** (`+0xd5=1, +0x94=result`).
- `FUN_0057b150(show)` is the per-performance start: `+0x90=0, +0x88=0,
  +0xd6=1, +0xd2=0, +0x80=+0x7c`, calls `FUN_00571190` per participant,
  `FUN_0057ba50()`, `FUN_00576b80(0)` and `FUN_0057fe40(+0x94)`.
- `stShow+0xe4`: 0 idle, 1 starting/running, 2 performing.

**Performance length:** the scheduler slot window (`FUN_0058c390`) is the
start/end boundary; `stShow+0x94` is the performance budget (not a `.shw`
length). Runtime probe (`notes/show-budget-runtime-probe.md`):
`+0x94 = GeneralParams+0x110 / time_scale = 3000.0/40.0 = 75.0` real seconds
= **50 park minutes**. State 4 adds raw `dt` to `+0x90` and ends at
`+0x94 <= +0x90`; the judge can end earlier. Cpg04's show is closed
(`+0xd3 = 0`) and invalid (`+0xe0 = 0`), so no performance was observed — the
75 s is the field value, not a measured run. The port's `ShowRuntime` duration
is now 50 park minutes at the default time_scale 40. The exact `iterations`
spacing within the slot remains open.

## 7. Arena wall geometry (`FUN_00584090` / `FUN_00584200`)

The `stShow` arena is an `stPerimeter` at `stShow+0x1c`:
`FUN_00579440` -> `FUN_00580f00` (`stAttraction`) -> `FUN_00584090`
(`stPerimeter`, vtable `0x643550`). It stores the four rectangle corners at
`+0x1c/+0x20/+0x24/+0x28` and calls `FUN_00582210` once per edge; each edge
allocates an `stWall` (`FUN_00584200`) and walks it cell-by-cell.

- **Segment count** (`0x584249..0x584295`):
  `count = int((edgeLen + 1.0) / wallRecord[0])`, with
  `edgeLen = sqrt(dx²+dy²)`; `FUN_005ffa50` allocates that many pieces.
- `wallRecord = design+0xc`, `design = DB+0x6c + show.field_14*0xb4`;
  `wallRecord[0]` is the **runtime wall pitch**, not the `.bwi` version (4)
  or height (8). Its exact builder/value is OPEN (Cpg04 likely 4).
- `FUN_00584700` fills the 0x28 `stWall`: mesh/resource pointer
  (`wallRecord+8`), a cell vector (8-byte {x,y}), `field44` at `+0x14`, the
  pitch at `+0x20`, wall record at `+0x24`. It does **not** write a mesh
  transform.
- `FUN_005849a0` marks the world grid cells (occupancy class
  `DAT_006757b4<<12`), not geometry.
- The per-piece **position/yaw/scale/Z transform is applied by the generic
  display/scene builder**; that helper was not isolated. The `.bwi` mesh
  `fence_show_lod1.mesh` has local +Y as its long axis (bbox ~4.4 units,
  height 8.5), so local +Y maps to the edge direction (yaw 0/90/180/270);
  whether `BWI.scales[0]`/height are applied and where Z comes from are OPEN.
- `showRecord+0x44` is not the wall id; it is a scheduler/registration handle
  (`stAttraction+0x1c`). `field_14` selects the WallList/BWI record.

Port state: `src/formats/bwi` parses all 24 walls (`Show.bwi` ->
`fence_show_lod1.mesh`, height 8, scale 1.0, `--bwi-info`). The wall DDA
(`count = int((edge+1)/pitch)`, per-cell pieces) is mapped, but rendering is
deferred until the display transform is resolved; no yaw/scale/Z is guessed.

## 8. Score and Victory

- `.bfi` `score_steps`/`attribute_sets` are **unused** in this build.
- `FUN_00581770(show, dt, participants)`:
  - each participant `p`, each visitor profile `w` in `[show+0x74]+0x34`:
    `FUN_00576550([p+0x4fc], w)` = five-float dot, clamp `[-100,+100]`;
  - category = max over participants; add `dot(p+0x500,w)` if `p+0x4f8`,
    `dot(p+0x504,w)` if `p+0x4f9`; clamp `[-130,+130]`;
  - smooth each `current[show+0x58+i]` toward the category at
    `[DAT_00675770+0xc]` per second.
- The five floats `[p+0x4fc]` are **static per dino**, set at creation by
  `FUN_00586690` from `FUN_00618020`: beauty, danger, spectacle, conventional
  appeal, tamability. Profiles are visitor preference vectors (`+0x18..+0x2c`).
- `FUN_0057f5e0` = `stShowStatistics` ctor (0x84 bytes at `stShow+0x3b`):
  five 0xDB-float arrays (`+0x28..+0x3c`), participant vectors
  `+0x1c/+0x20/+0x24`, show pointer `+0x50`.
- Victory kind 26/35 is the class-6 animation. **No park class-6
  availability path was found**; the earlier "gate returns 50" claim is not
  supported (the target-vtable note records the `+0x1c` result as unused).
  **OPEN:** the exact show-end -> Victory
  emitter. Port recipe: at `FUN_0057b130` end, pick the winner from the judge
  cast and force the element (26/35, self, mode 2, sentinel, instant 1,
  allowTurn 0).

## 9. Scheduler slot boundary (2026-10-31, exact)

`FUN_0058c1c0(time_scale, 9)` is the time manager: `+0x00 = 9` (base
hour), `+0x10 = time_scale` = **40.0** at runtime (Cpg04 probe; `cfg+0x1a0`),
`+0x14 = 1/time_scale`, `+0x18 = time_scale/60`, `+0x1c = 60/time_scale`,
`+0x20 = time_scale/3600`, `+0x24 = 3600/time_scale`. `FUN_0058c2f0` adds the
raw frame `dt` to `+0x08` and copies the old value to `+0x0c`; the clock is
elapsed **seconds**.

`FUN_0058c390(sched, offset)` is a boundary-crossing test, not a continuous
window:

```c
prev = sched[0x0c];
now  = sched[0x08];
i    = (int)(prev * sched[0x20]) + 1;
end  = i * sched[0x24] - offset * sched[0x14];
cross = (end >= prev) && (end <= now);
```

The `offset` argument is **not the clock**: `scheduleList+0x1c = cfg+0x110`
(the config pair), and the pushed dword is `[cfg+0x114] = 1800.0`. With
`time_scale = 40`:

```text
i = (int)(prev / 90) + 1
end = i * 90 - 1800/40 = 90*i - 45 s
```

So slot boundaries are at 45 s, 135 s, 225 s, ... (09:30, 10:30, ... game
time); the slot length is 3600/40 = 90 real seconds = **1 game hour**. The
11th hour ends the day.

`FUN_0057b130(show, result)` sets the performance budget `show+0x94` to
`scheduleList[0] = *(float*)(cfg+0x110) * (1/time_scale) = 3000.0/40.0 =
**75.0 real seconds** (= 50 park minutes at 40x). `dt` is raw real seconds,
bit-identical to the park-clock dt; `show+0x90 += dt` and the shared end
check (`0x57b81f`, reached from states 4 and 5) ends at `+0x94 <= +0x90`
(the `0x57b8da` `+0x90 += dt` site is **state 5**, not state 4). Cpg04's show
is closed (`+0xd3 = 0`) and invalid (`+0xe0 = 0`), so no performance ran —
the 75 s is the field value, not a measured run. The port's `ShowRuntime`
duration is now 50 park minutes at the default `time_scale` 40.

**Port approximation (not a guess):** `ShowRuntime::start_hour` still starts
slot `h` when the hour index changes, while the retail raw boundary is
`slot_len*i - offset` (`90*i - 45` s at 40x); the `i` → `show+0x100` mapping
was not observed because Cpg04's show is gated. Do not change the port start
mapping until a park with an open/valid show is probed.

## 10. Live-show recipe (latest)

```text
1. stShow = FUN_005868f0(world, showRec+0x18, design+0xc, showRec+0x44)
   -> FUN_00588ac0 -> FUN_00579440 (vtable 0x643220)
2. AI = FUN_0051fe20(arenaEntity, DB+0x158, DB+0x174, showRec+0x48)
3. scene = FUN_005f4b60(design+0x24, arena); audio = FUN_004fea70(...);
   FUN_005e7630(arena, AI, scene, audio, ...)
4. for each 0x18 rec in showRec+0x2c: FUN_0053acd0(showRec, rec, 0)
   judge .bfi -> featureDef+0x40 kind -> FUN_0057c260 -> judge at stShow+0xcc
5. add dinos: FUN_0053a980(genotype) -> saDino; the participant list is
   stShow+0xf0 (+0xf4/+0xf8), team id dino+0x1d0.
6. start: FUN_0057b110(show) -> FUN_0057ba50(show) rebuilds the judge cast
   via FUN_00582840/FUN_00583060; per frame FUN_00581770 scores.
7. end: FUN_0058c8f0(show) -> FUN_0057b130(show,result); force the Victory
   element (26/35) on the winner (retail emitter OPEN).
```

Files: `Data/Design/Scenario/show/*.shw`, `Data/Design/attractions/features/*.bfi`,
`WallList.ini`/`FeatureList.ini`.

## Open

1. Exact show-end Victory emitter (retail direct emitter not found).
2. Scheduler slot and budget: **resolved** (`FUN_0058c1c0(40, 9)`,
   `FUN_0058c390`: `i=int(prev/90)+1`, `end=90*i-45` s; `show+0x94 =
   3000/40 = 75` real s = 50 park minutes; section 9). Cpg04's show never
   performs because `+0xd3=0`/`+0xe0=0`, so the performance length is the
   field value, not a measured run; a park with an open/valid show would
   confirm the 75 s end.
3. `.bfi` `score_steps`/`attribute_sets` are unused in this build (negative
   result).
4. The cast writer and the scheduler start/end are now mapped
   (`FUN_00583060`, `FUN_0058c760`/`FUN_0057b110`/`FUN_0057ba50`/
   `FUN_0057b130`).
