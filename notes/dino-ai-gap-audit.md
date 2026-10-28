# Dinosaur AI gap audit — actor update, navigation, targets, effects

Date: 2026-10-28. Read-only recon (Ghidra MCP `/DinoIsland.exe-ebea17`).
Scope: what is still missing so park dinosaurs behave like retail, with
addresses. Port side: `src/world/dino_actions.*`, `src/world/danger_grid.*`,
`src/viewer/main.cpp` (`add_imported_dino`, the per-frame dino loop).

**Headline:** the chooser/needs/target-ABI side is done. The missing half is a
whole subsystem the port has no counterpart for: `saDinosaur`'s **mover**
(`saDinosaur+0x2c8`), its **0x18-byte move-command queue**, and the
**element → move-command emitter** (`FUN_00569660`). Movement is *not* derived
from the `.bai` need deltas; it is a queue of straight-line "move records"
issued when an action element is selected.

---

## 0. What the existing notes already cover (do not redo)

| Area | Note | Section |
| --- | --- | --- |
| Need block `+0x214`, state thresholds, weight tables, `need_term` | `dino-action-chooser-recon.md` | "Need block" … "Score term" |
| Node score, danger/threat term, availability gate, preconditions, selection | same | "Node score" … "Selection" |
| Need decay/derivation per tick (`0x571370/0x571410/0x571440/0x5714b0/0x571540`) | same + `dinosaur-actions-recon.md` §7 | |
| Danger grid build/stamp/SECU (`0x589CD0`, `0x571B50`, `0x571440`) | `dino-action-chooser-recon.md` §"Danger-grid population" | |
| `node+0x3c` target ABI, 3 concrete vtables, `GetTarget` gas, `action+0x08` factory | `dino-target-vtable-recon.md` §§1-5, §8 | |
| Live class census (no-target / actor / box) | `target-vtable-runtime.md` | |
| Action ids 0-63, `ApplyActionEffect` switch, completion switch | `dinosaur-actions-recon.md` §§1-3 | |
| `.bai` header/event/precondition map, event→stat table | `docs/formats/bai.md` §"Event effects" | |
| No action→clip table; `+0x2BC` locomotion slot; animator `0x5D82F0` | `action-clip-lead.md` | all |
| Behavior graph vtables, node ctor | `dinosaur-behavior-graph.md`, `dino-target-vtable-recon.md` §1 | |
| Cache writers `+0x27C/+0x2A0` | `dino-cache-writers-recon.md`, chooser note §"Cache writers" | |

Everything below is **new** (verified this pass) unless marked *open*.

---

## 1. Actor update — the chain and the fields

### 1.1 Tick order (per park frame)

`FUN_0051e6c0` = park frame update (called from the app loop `0x403e10` /
`0x402bc0` / `0x402f10`):

```c
FUN_0055dc80(); FUN_0055d980(); FUN_0055d710(dt, flag);  // spatial/path passes
FUN_0053c560(dt, flag); FUN_00586ad0(dt, flag);          // park + simu manager
for (v : [mgr+0x50..0x54]) FUN_0051f950(v+0x4c, v+0x54);  // visitor reactions (+0xB8 list)
for (d : [mgr+0x40..0x44]) FUN_0051f810(d+0x5c, d+0x64);  // dino reactions   (+0xF4 list)
FUN_0051f290(...); FUN_005e5920(...); FUN_005f6950(); FUN_005d9310();  // UI/anim
```

The **dinosaur behaviour tick** is `FUN_0051fab0` (AI manager update), called
from `0x51d179`:

```c
for (dino : [mgr+0x24..0x28]) {                 // 0x51fb2c
    beh = dino->[+0x270];
    if (beh->[0x14] == 1 && beh->[0xb8] != beh->[0xbc])   // list non-empty
        for (o : perceived) if (find(beh->[0xb8..0xbc], o->id))
            FUN_00521d80(beh, dino);             // reaction 1
}
for (dino : [mgr+0x14..0x18]) {                 // 0x51fd20
    beh = dino->[0x270];
    if (beh->[0x14] != 1) continue;
    if (beh->[0xd6] == 1) for (o : perceived)
        if (find(beh->[0xf4..0xf8], o->id)) FUN_00521d10(beh, dino);  // reaction 2
    if (beh->[0xd8]) { FUN_0056bcb0(); FUN_0056ab00(); }
    if (beh->[0xd9] || beh->[0xda]) FUN_0051f8d0(dino);
    dino->vtable[+0xc](dt);                      // actor update
}
if ([mgr+0x5c] != [mgr+0x60]) FUN_00520880();    // show/attraction command queue
for (dino : [mgr+0x14..0x18]) FUN_00521df0(dt);  // behaviour update
```

`FUN_00521df0` (`apDinosaurBehavior::Update`) is the gate that decides
*where* an action is chosen from:

```c
if (this->[0x14] == 1) FUN_0056d880(dt);                       // park mode
else if (this->[0x14] == 2 &&
         [[this+0xc]+0x10] != 0 && [[[this+0xc]+0x10]+0xe4] in {4,5})
    FUN_0056d880(dt);                                          // show mode
```

`FUN_0056d880` (per-tick chooser driver) decays the element countdown, and
only when `beh+0x14 == 1` (needs decision), `dino+0x32e == 0`, and
`dino+0x16 > 0` runs the availability gate + scorer + selector (twice).

So the **action current node is advanced** by `FUN_0056d880` →
`FUN_0056e670` (select) → `FUN_0056c0c0` (start, per `actionlist` id), and the
action *executes* through `FUN_00569660` + `FUN_00569a00` (see §4).

### 1.2 `saDinosaur` layout (new, proven)

Ctor `FUN_005701f0` (size `0x52c`, assert `saDinosaur.cpp:0xfe`), called from
`spSimuManager::CreateDinosaur` `FUN_00586690`:

```c
FUN_00617250(genotype, P);                 // 0x1b8-byte phenotype, stack copy
FUN_00618020(genotype, P, needs5);         // 5 visitor needs (20 bytes)
saDinosaur = new 0x52c;
FUN_005701f0(P, genotype, actionList, needs5);
    memcpy(this+0x50, P, 0x1b8);                       // phenotype copy
    FUN_00573370(this+0x2c8, this+0x50, this+0x214);    // MOVER ctor
    this+0x2c4 = new 0xd8 (FUN_00584b80: copy of 0x1b8? no: 18*3 dwords)
    this+0x4fc = new 0x30 (FUN_00576720): per-action speed vector
    this+0x508 = action list (64 records, stride 0xac)
    FUN_00570fd0();                                    // need reset
    this+0x2bc = 0;                                    // locomotion slot
    this+0x14 = (int16)(this+0x68 / 10);
    this+0x16 = (int16)this+0x1d4;                     // health = max
    this+0x4c8 = this;                                 // mover+0x200 = self
```

| absolute | value |
| --- | --- |
| `+0x08` / `+0x0c` | `int x`, `int y` — **world map units** (same space as the danger grid) |
| `+0x13` | `u8` footprint radius (cells) used by the stamp test |
| `+0x16` | `int16` health; `+0x1d4` max health |
| `+0x50` | phenotype copy, `0x1b8` bytes (`FUN_00617250`) |
| `+0x214` | needs block (`+0x30` values = `+0x244`…; `+0x98` states = `+0x2ac`…) |
| `+0x268` / `+0x26c` | walk / run speed (`f32`, attributes 7/8) — see §2.3 |
| `+0x2bc` | locomotion slot 0..4 read by the animator |
| `+0x2c0` | current action id, `0x1f` = idle |
| `+0x2c4` | object 0xd8 (18×3 dwords, `FUN_00584b80`) |
| `+0x2c8` | **mover** (0x220 bytes; vtable `0x00643170`) |
| `+0x4c8` | self pointer (mover+0x200) |
| `+0x4fc` | per-action speed table object (`FUN_00576720`, `+0x18` = f32*) |
| `+0x508` | action list (64 × 0xac) |
| `+0x514..+0x517` | one-frame flags (bonus overlay / loco / damage) |

### 1.3 The mover (`saDinosaur+0x2c8`)

Ctor `FUN_00573370(mover, phenotype@+0x50, needs@+0x214)`; vtable `0x00643170`
written at `mover+0x0`; `mover+0x1d8 = 0x0064313c` (a sub-object).

| mover offset | absolute (`+0x2c8`) | meaning |
| --- | --- | --- |
| `+0x0c`/`+0x10` | `+0x2d4`/`+0x2d8` | `f32 x`, `f32 y` (live position, the value the animator/mover use) |
| `+0x14`/`+0x18` | `+0x2dc`/`+0x2e0` | `int` cell of the last accepted step |
| `+0x1c`/`+0x20` | `+0x2e4`/`+0x2e8` | facing unit vector (`cos`,`sin`) |
| `+0x24` | `+0x2ec` | elapsed for mode-2 (timed) records |
| `+0x28` | `+0x2f0` | elapsed of the current segment |
| `+0x2c` | `+0x2f4` | segment progress 0..1 |
| `+0x30` | `+0x2f8` | segment duration (= duration of the move kind) |
| `+0x34` | `+0x2fc` | `1 / duration` |
| `+0x38`/`+0x3c`/`+0x40`/`+0x44` | `+0x300..0x30c` | wait/stall timer (state 2) |
| `+0x48` | `+0x310` | `0x7f7fc99e` sentinel |
| `+0x50` | `+0x318` | "moved this frame" flag (feeds `FUN_0055cec0`) |
| `+0x54`/`+0x58`, `+0x5c`/`+0x60` | `+0x31c..0x328` | previous facing / previous position (out params of `FUN_00574350`) |
| `+0x66`, `+0x67` | `+0x32e`, `+0x32f` | blocked / restart-path flags |
| `+0x68..0xc7` | | 8 cached path waypoints (jump only) |
| `+0x1c8` | `+0x490` | cached waypoint count |
| `+0x1cc` | `+0x494` | path valid |
| `+0x1d4` | `+0x49c` | paused flag (`0x56c3b0` clears it) |
| `+0x1dc/+0x1e0/+0x1e4` | `+0x4a4/+0x4a8/+0x4ac` | **move-command queue** (vector of 0x18-byte records) |
| `+0x1e8` | `+0x4b0` | phenotype pointer (`saDinosaur+0x50`) |
| `+0x1ec` | `+0x4b4` | state: 0 idle, 1 moving, 2 waiting/turning, 3 knocked down |
| `+0x1f0` | `+0x4b8` | current move kind (the record's `+0x10`) |
| `+0x1f4` | `+0x4bc` | active move (0 = none) |
| `+0x1f8` | `+0x4c0` | last move kind that actually moved (+0x4c0 is the "activity type" of the decay switch) |
| `+0x1fc` | `+0x4c4` | needs block pointer (`saDinosaur+0x214`) |
| `+0x200` | `+0x4c8` | self (`saDinosaur`) |
| `+0x204` | `+0x4cc` | `apfDinoPath*` (nullable — `FUN_00574450` guards on it) |
| `+0x208` | `+0x4d0` | resend flag |
| `+0x20c`/`+0x210` | `+0x4d4`/`+0x4d8` | last path cell |
| `+0x214`/`+0x218` | `+0x4dc`/`+0x4e0` | pending path target |

Per-frame entry: `saDinosaur::Update` `FUN_00571540` calls
`FUN_00574350(mover, dt, &prevFacing, &prevPos, &prevPos)` **before** the need
decay:

```asm
005715b1 LEA EDX,[ESP+0x48]      ; out4
005715c9 PUSH ECX ; out3
005715cb MOV ECX,EDI             ; EDI = ESI+0x2c8  -> mover
005715cd CALL 0x00574350
```

`FUN_00574350` is the state machine:

```c
mover->[0x50]=0; mover->[0x1f8]=0;
mover->[0x5c]=dino->x; mover->[0x60]=dino->y;
mover->[0x54]=mover->[0x1c]; mover->[0x58]=mover->[0x20];
if (!mover->[0x1d4]) switch (mover->[0x1ec]) {
  case 1: FUN_00574450(dt); break;                       // moving
  case 2: mover->[0x3c]+=dt;
          mover->[0x38]+=dt*mover->[0x44];
          if (mover->[0x40] <= mover->[0x3c]) FUN_00574ad0(pos, 0);  // wait done
          break;
  case 3: if (dino->health > 0) { mover->[0x66]=0; FUN_005759c0(1); FUN_00574ad0(pos,1); }
}
```

`FUN_00574450` (state 1) is the **position integrator**:

```c
mover->[0x28] += dt;
if (mover->[0x28] < mover->[0x30]) mover->[0x2c] = mover->[0x28] * mover->[0x34];
else if (rec->[0xc] == 1) { /* instant: snap, flag moved */ }
else mover->[0x2c] = 1.0f;

start = mover->pos;                       // out x/y of FUN_00574930
FUN_00574930(mover, kind = rec->[0x10], dt, 0, &out, &out+4, &dist);
if ((int)out == (int)dino->x && (int)out+4 == (int)dino->y)
    mover->pos = out;                     // same cell: move freely
else if (mover->[0x204] == 0 || ((kind != 1 && kind != 2) ||
         FUN_0055af70(dino, (int)out, (int)out+4, 0, 1))) {
    mover->pos = out; dino->x = (int)out; dino->y = (int)out+4; mover->[0x50] = 1;
} else {                                  // BLOCKED while walking/running
    mover->[0x1cc] = 0;
    if (kind == 1) FUN_00573f40(pos, pos, 0, 1, 0x34, 1, 1);  // jump 3m
    else if (kind == 2) FUN_00573f40(pos, pos, 0, 1, 0x35, 1, 1); // jump 4m
    else mover->[0x64] = 1;               // give up
}
```

So: **no z, no terrain query, no slope test in the sim** — only `int x/y`.
Terrain clamping is display-side (`gacDinosaur`/`rsActor`), see Open #3.

### 1.4 Animation clip selection (confirmed, no action→clip table)

`aacDinosaurAnimator` update `FUN_005D82F0` reads `saDinosaur+0x2BC`:

```c
if (actor->[0x4d]) {                                   // one-frame flag
  if (actor->[0x514] == 0) {
     if (actor->[0x515] || actor->[0x516]) {
        slot = actor->[0x2BC];
        if (slot < 3) FUN_005d8870(1, animator + slot*2 + 5);   // Neutral/Walk/Run
        FUN_005d8960(0);
     }
  } else if (actor->[0x518] && actor->[0x520]) { ... envmap texture ... }
}
```

`+0x2BC` is *not* the action id: 0..2 = Neutral/Walk/Run, 4 also exists (the
5 s heal pass `FUN_005725e0` skips healing when `+0x2BC == 4`). Writers:
ctor (`0x57031F`), `FUN_00570590` (`700 = 0x2BC`), and the periodic AI passes
(`0x57250D..0x5725AC`). See `action-clip-lead.md`.

---

## 2. Navigation — straight-line steering, not A*

### 2.1 The move-command record (0x18 bytes)

`FUN_00573f40` (queue + start if idle) and `FUN_00573dc0` (queue only) append:

| off | type | meaning |
| --- | --- | --- |
| `+0x00` | f32 | target x (world units) |
| `+0x04` | f32 | target y |
| `+0x08` | f32 | mode-2 threshold / duration (usually `FUN_00575de0(kind)`) |
| `+0x0c` | u8 | flag (`param_7`) |
| `+0x0d` | u8 | flag (`param_8`) |
| `+0x10` | int | **move kind** (`record[4]` → `mover+0x1f0` → `saDinosaur+0x4c0`) |
| `+0x14` | int | arrival mode: 0 = free, 1 = position, 2 = timed |

Arrival test `FUN_00575ac0`:

```c
if (rec->[5] == 1) { if (rec->x != mover->x || rec->y != mover->y) return 0;
                     /* walk/run: re-path to the last path cell */ }
if (rec->[5] == 2) { return mover->[0x24] >= rec->[0x08]; }   // timed
return 1;                                                     // mode 0
```

`FUN_005759c0(1)` drops already-reached records: for kinds 1/2/0x34/0x35 it
compares the squared distance to the record with `0.0001` (**0.01 unit
radius**); other kinds are only dropped by the mode test.

### 2.2 Steering and turning

`FUN_00574ad0` (start/advance a segment) — per frame:

```c
kind = queue[active ? 1 : 0]->[0x10];
if (kind not in {3,4,5} && (rec->x != pos->x || rec->y != pos->y)) {
    dx = rec->x - pos->x; dy = rec->y - pos->y;
    len = sqrt(dx*dx+dy*dy);
    desired = (dx/len, dy/len);                     // 0x574d17..0x574d5a
    if (desired != mover->facing)
        FUN_00575410(&desired, active, &newFacing);  // smooth/quantised turn
}
mover->[0x1f0] = rec->[0x10];
if (facing changed) {
    x = (int)(mover->[0x0c] + 0.5f); y = (int)(mover->[0x10] + 0.5f);   // 0x62544c = 0.5
    if (FUN_0055af70(dino, x, y, &facing, 1) == 1) mover->facing = facing;
    else mover->[0x64] = 1;                          // blocked
}
/* then set speed/duration (below) */
```

`FUN_00575410` is a **quantised turn**, not a rad/s rate:

```c
c = dot(desired, facing);
if (c >= 0.99f)  return desired;            // < 8 deg: snap
if (c >= 0.9238795f) steps = 1;             // 22.5 deg bucket
else if (c >= 0.38268343f) steps = 2 or 3 (or 5 when blocked & queue kind==2 & weight<2);
else steps = 5;                             // U-turn
side = (cross < 0) ? 4 : 3;                 // 3 = +90/step, 4 = -90/step, 5 = 180
FUN_005758f0(&dir, side, steps);            // snap the facing to the 8-direction set
/* and insert `steps` turn records (kind = 1..5) via FUN_00575f50 / FUN_00576100 */
```

`FUN_005758f0(dir, 3|4, n)` rotates by 90° per step (`*0.70710677` matrix),
`5` rotates 180°. The path object's ctor `FUN_0055ac30` pre-computes the eight
direction angles at `+0x10..+0x2c`: `0, ±0.785398, ±1.570796, ±2.35619, 3.14159`.

So: **retail turns in 45° snaps** (up to 5 at once), and each turn is a queued
record with its own duration. There is no continuous turn rate to port.

### 2.3 Speed (`FUN_00574930` / `FUN_00575de0`)

```c
// FUN_00574930(mover, kind, dt, 0, &outX, &outY, &outDist)
case 1: dist = dt * *(f32*)(mover->[0x1fc] + 0x54);        // = saDinosaur+0x268
case 2: dist = dt * *(f32*)(mover->[0x1fc] + 0x58);        // = saDinosaur+0x26c
default: X = *(int*)(mover->[0x1e8] + 0x1a8);              // species block
         dist = *(int*)(mover->[0x1e8]+0xa4) * X[0x288+kind*4]
                * X[0x0c+kind*8] * dt * 0.01;
         lateral = *(int*)(mover->[0x1e8]+0xa4) * X[0x288+kind*4]
                   * X[0x10+kind*8] * dt * 0.01;           // rotated 90 deg
outX = facing.x*dist (+perp.x*lateral) + mover->x;
outY = facing.y*dist (+perp.y*lateral) + mover->y;
```

`FUN_00575de0(mover, kind)` (the *duration* stored in the record and used for
the progress normalisation):

```c
kind 1: (P[0xa4] * X[0x14] * 0.01) / saDinosaur[0x268]
kind 2: (P[0xa4] * X[0x1c] * 0.01) / saDinosaur[0x26c]
else   : X[0x1bc + kind*4]
```

where `P = saDinosaur+0x50` (phenotype) and `X = [P+0x1a8]` = the **per-species
action table**: `0x81c` bytes per species (20 species), built by the
translator (`FUN_00615e80` → `FUN_0061a5b0`, stride `0x81c`) from
`DATA\Design\Actions.bad` (5 family blocks × 462 f32; loaded by
`FUN_0051d1d0` line `+0x38`).

Proven read offsets inside `X`: `+0x14`, `+0x1c` (walk/run stride factors);
`+0x0c+kind*8` (forward), `+0x10+kind*8` (lateral); `+0x1bc+kind*4` (duration);
`+0x288+kind*4` (scale); `+0x1d0` and `+0x22c` (jump durations);
`+0x134` (size scale). The rest of the 519-dword block is still unmapped
(Open #2).

Cross-check against the file: `Actions.bad` block floats `[3..0x1b8]` are the
2-float-per-kind table (`block0`: kind1 = 0.73, kind2 = 2.027 — walk/run, zero
lateral), and `[0x1bc..]` the duration array (`0.5, 0, 0, 1.0, 1.0, …`), which
matches the code's index math for kind 0..0x35.

### 2.4 Arrival / confinement test `FUN_0055af70`

`FUN_0055af70(actor, x, y, outFlag, checkBounds)`:

```c
if (checkBounds) {
    rect = path->[0x1c0];                       // paddock/enclosure rect
    if (x < rect->[0x1c] || rect->[0x24] < x || y < rect->[0x20] || rect->[0x28] < y)
        return 0;                               // OUTSIDE THE PADDOCK -> reject
}
/* stamp/scan the actor's footprint: radius r = actor->[0x13] cells,
   rows y-r..y+r, columns x-r..x+r, callback = visitor {vt=0x642f78,
   u16 actor[0x28], u16 actor[0x314], u16 actor[0x316], int* gridRow},
   grid = path->[0x1ac]:  [grid+0]=width  [grid+4]=height
                          [grid+0x10]=row pointer array, cells are u16        */
```

**This is the paddock confinement**: a position outside
`[path+0x1c0]`'s rect is refused, and the u16 occupancy grid refuses cells
occupied by walls/buildings. There is no per-dinosaur "paddock id" in the
mover.

### 2.5 Is there a pathfinder?

Yes, but it is used **only for jump run-ups**:

- `FUN_0055ac30` = `apfDinoPath` ctor: grid `+0x1ac`, per-dino id vector
  `+0x1b4/+0x1b8` (vtable `0x642f6c` at `+0x1b0`), 8 angles `+0x10..+0x2c`,
  paddock rect `+0x1c0`, per-dino queues from `+0x34` (stride 0x10).
  Created by `FUN_00569e00` at `0x569e9f` with `grid = [owner+0x1c]`.
- `FUN_0055b3b0` = pop the next waypoint for a dino id; when the queue is
  empty it tests the target cell (footprint) and then calls `FUN_0055ba50`.
- `FUN_0055ba50` = **greedy 8-direction walk** toward the goal, recursion
  depth limited by `path->[0x0]`; on the way it tests each cell with the
  footprint visitor (`FUN_0055cec0`) and records the accepted cells.
- `FUN_0055b360` = reset the queue; `FUN_0055b2a0` = find a dino id (assert
  `apfDinoPath.cpp:0x177 "unknown dino"`).
- The depth limit is set in `FUN_0055b3b0` from **attribute 11**
  (`FUN_00572620(0xb)`): `<20 -> 15`, `<40 -> 20`, `<60 -> 25`, else `40` steps.

In `FUN_00574ad0` the path object is touched **only** when
`record[4] == 0x34 || 0x35` (the two jump kinds) — it caches 8 waypoints into
`mover+0x68` and uses them as a run-up. Everything else is the straight-line
steering of §2.2.

**Answer: park action execution is "turn and move straight" to a point, with a
per-cell footprint test; there is no A* / navmesh / waypoint graph in the
normal path.**

---

## 3. Target factory (`node+0x3c` → `GetTarget`)

Already decoded in `dino-target-vtable-recon.md` §§2-3, 8. Summary of the three
concrete classes and their producers (unchanged):

| vtable | ctor | `+0x14` GetTarget | `+0x18` radius | producer |
| --- | --- | --- | --- | --- |
| `0x00642FFC` | `FUN_0056BFD0` | centre of the box `[this+0x28]` | `[[this+0x0c]]` | `FUN_0056A730` ← `FUN_0053ACD0` (show element types 0x16/0x17) |
| `0x006430AC` | `FUN_0056EDC0` | `-1,-1` (self) | `[[this+0x0c]]` | `FUN_00569E00` ← `FUN_0056BC40` / `FUN_0056BF50` |
| `0x006430E4` | `FUN_0056F620` | `[[this+0x38]+0x28]` actor x/y | `actor+0x278` | `FUN_0056AC80` ← `FUN_00521D10`/`FUN_00521D80` |

New this pass:

- `FUN_00569E00` is the **item/park-object behaviour ctor** (vtable
  `0x00642FA0`). It calls `FUN_0055AC30(path, [owner+0x1c] grid, owner)` and
  `FUN_0056A9A0(this+0x18, record)` to attach the **no-target** policy to the
  object's own action list. This is the class the park food source / bait uses.
- `FUN_0056A730` → `FUN_0056A9A0` is the **box** producer; it is reached only
  from `FUN_0053ACD0` (the show/attraction element factory, `types 0x16/0x17`),
  and `FUN_0053ACD0` is called all over the park loader `FUN_0055DF50` and the
  construction kit (`0x52b30a`). That is why the live trace saw the box class
  gated for `Eat` 446 times: the food source is an *item actor* created by the
  same element factory.
- One target per action list, one-fill lag: unchanged (see §8 of the target
  note).
- The **selection logic for which food source** is *not* in the actor: the
  target object is created by whoever builds the item actor, and
  `FUN_0056E190` skips actions whose target owner == the chooser owner. So the
  "nearest stocked food source" rule the port implements is an approximation;
  the real rule is "whichever item actor published this action list".

*Open*: the exact park-side call that turns a placed `.bsi` instance into an
action list + box target (candidate `FUN_0055DF50` → `FUN_0053ACD0` type 0x16
or 0x17). Experiment: breakpoint `0x53AED2` (`FUN_0056A730` call) and log the
`type` and the item pointer for a park with one Cow + one Flower.

---

## 4. Action execution effects

### 4.1 What actually gets applied

`FUN_00569A00` executes the selected `.bai` element:

- walks the element's effect-id list, applies each through
  `FUN_005727B0` (event kinds, see `docs/formats/bai.md` §"Event effects");
- copies five dwords from the element (`+0x14..+0x28`) to `actor+0x504`;
- if `element->[0x54] == 2` calls `0x57AF70`;
- raises the one-frame flag `+0x4d`.

`FUN_005727B0(dino, kind, value, percent, duration, texture)` — full switch
(verified): `1 → +0x248` (VIOLENCE), `9..0x1c` set the current action id
`+0x2c0` and duration `+0x260`, `0x19/25 → +0x258` rest gauge,
`0x17/23 → +0x25c` food gauge, `0x18/24 → int16 +0x16` health, plus the
stat writers in the doc table.

### 4.2 The `.bai` need deltas are **not** applied

Whole-image check: the only writers of the four need values are the derived
paths `FUN_00571370` (FOOD from the food gauge), `FUN_00571410` (VIOLENCE
decay), `FUN_00571440` (SECU from the danger grid), `FUN_00571540` (REPOS from
the rest gauge), plus the init `FUN_00570FD0`. Nothing iterates
`action.need_deltas` at completion.

Consequence — **the port's `DinoActionLoop::finish_action` need-delta
application is not retail**; retail's only action-driven need change is the
element *event* (e.g. `Eat.bai` = event kind 1, VIOLENCE +15).

| action | `.bai` event(s) | retail need effect |
| --- | --- | --- |
| Eat / EatForSmallies | (1, +15, 100%) | VIOLENCE +15 |
| Exploration | (1, +5, 100%) | VIOLENCE +5 |
| Sleep | none | none (REPOS only rises through the rest gauge) |
| Danse | (25, 0, 99/95/90%) | rest gauge → 99/95/90 % |
| ObstAttackSize1 | (1,-2),(1,+5),(25,0,99%) | VIOLENCE ±, rest gauge |
| GetCourage | (20, 10..50, 100%) | `+0x2a4` courage |
| HeadCharge | (10,…),(20,…),(25,…),(1,+50) | perception/courage/rest/violence |

### 4.3 Negative result: no eating → food restore

`saDinosaur+0x25c` (food gauge) has exactly three writers in the whole image:
`FUN_00570FD0` (init = max×0.5), `FUN_00571370` (decay, and starvation damage
via `vtable+0xc`), `FUN_005727B0` case 23 (event kind 23 — **no retail `.bai`
uses it**, `docs/formats/bai.md`). `FUN_005727B0`'s only caller is
`FUN_00569A00`, so no other route exists.

The food source side is discrete: `saFoodSource::SetLevel` `FUN_00580E60` →
`FUN_00576D50(level 0/1/2)` sets quantity = `profile[+4]` × `1.0 / 0.66666 /
0.33333` and asserts `"Invalid food quantity"` (`saFoodSource.cpp:0xb8`). The
eat state machine `FUN_00522300` calls `FUN_00580E60` once when the eating
animation ends, so **one level is consumed per meal**, but the dino's gauge is
not refilled there either.

Implication: **employee feeding is the real refill path** (`SIM-3` open item,
`notes/employee-ai-recon.md`), or long sessions end with all dinos starving.
The port must not "fix" this by restoring FOOD on Eat.

### 4.4 Sleeping / violence / social / fleeing

- Sleep: no event, no need delta → the only effect is the animation slot and
  the `+0x2c0=0x1f` idle reset. REPOS recovers only because the rest gauge
  (`+0x258`) decays (that is the *model* of resting).
- Violence (fight): `KickTheirAss` (effect class 1) applies the element events
  and calls `vtable+0xc(-amount)` on the target (health), `HeadCharge` clamps
  `+0x96` to `+0x76`, `DinoAttackForCarnivore` sets flag `+0x2a8`.
- Social/herding: only the show/attraction controller `FUN_00520880` (kinds 7
  = follow/group, 0x12 = approach a partner within `[beast+0x38]`), not the
  park chooser.
- Fleeing: `RunAway.bai` has `+0x248` (VIOLENCE) +50 in the *deltas* only (not
  applied); the actual effect is `FUN_005727B0` case 1. Movement is a normal
  move record.

---

## 5. Perception lists (`beh+0xf4` / `beh+0xb8` / `beh+0xc8`)

### 5.1 Structure

`FUN_0051f810` and `FUN_0051f950` are twins:

```c
// FUN_0051f810(perceived, beh) -> reaction 2 (visitor)
list = beh->[0xf4..0xf8];                       // 16-byte records
for (obj : perceived->[+4..+8]) {               // perceived vector
    id = obj->[+8];
    for (rec = list.begin; rec != list.end; rec += 4 /*dwords*/)
        if (rec[0]==id || rec[1]==id || rec[2]==id || rec[3]==id) found;
    if (found) FUN_00521d10(beh, obj);
}
// FUN_0051f950 identical with beh->[0xb8..0xbc] -> FUN_00521d80 (dino)
```

So each list is a vector of **4-dword records** and the reaction only fires
when the perceived object's id is inside one of them. `FUN_0056ac80`'s
`FUN_005f6fb0` find (`0x56afde`) uses the same list.

### 5.2 Live entry points and when retail calls them

| entry | callers |
| --- | --- |
| `FUN_00521d80` (reaction 1, dino) | `0x51f9ec` (`FUN_0051f950`, needs a `+0xb8` hit), `0x51fb9c` (`FUN_0051fab0`, needs `+0xb8` non-empty + a find hit) |
| `FUN_00521d10` (reaction 2, visitor) | `0x51f8ac` (`FUN_0051f810`, needs a `+0xf4` hit), `0x51fcb4` (`FUN_0051fab0`, needs `+0xd6 == 1` and a `+0xf4` hit) |

`FUN_0051e6c0` calls `FUN_0051f950` for each visitor (`visitor+0x4c`,
`visitor+0x54`) and `FUN_0051f810` for each dino (`dino+0x5c`, `dino+0x64`).
`FUN_0055df50` also calls `FUN_0051f950` three times (`0x60109/0x6011c/0x60133`).

### 5.3 Correction to the P3 "no writer" claim

`notes/NOW.md` / P3-PERCEPTION says the `+0xf4` / `+0xb8` / `+0xc8` lists have
"no writer in this build". **That cannot hold for `+0xb8`**: both callers of
`FUN_00521d80` are guarded by `list.begin != list.end`, and
`notes/target-vtable-runtime.md` recorded `REACT1_ENTER (0x521d80)` firing
**20 times** in the Cpg04 park. A non-empty `+0xb8` vector therefore exists at
runtime, so some producer fills it (candidate: the campaign/save load of the
behaviour record — the `.sav` `saDinosaur` tail carries `0x16*N` records and
the `list1/list2` blocks, `notes/campaign-list0-tail-recon.md`).

Status: **open**, with a cheap decisive test — breakpoint `0x51f9ec` and dump
`beh->[0xb8]`, `[0xbc]`, the first record's 4 dwords, and the perceived id; do
the same at `0x56afde`. That names the producer by call stack.

`+0xc8` is not referenced by the reaction path at all in the functions read
this pass; keep the P3 note's "inert" verdict for it until a writer appears.

Live direct entry points (`start_visitor` / `start_dino`) are
`FUN_00521d10` / `FUN_00521d80`; the show/attraction controller reaches the
same start machinery through `FUN_0056a9a0` + `FUN_0056e300`.

---

## 6. Paddock interaction

1. **Confinement** — `FUN_0055af70` rejects a step outside
   `[path+0x1c0]`'s rect and inside occupied grid cells (§2.4). The rect comes
   from the path-object ctor argument (`FUN_0055ac30(arg1 = rect owner)`),
   i.e. from the enclosure at `FUN_00569e00(…, owner, owner+0x1c, …)`.
   No explicit "paddock id" on the dino.
2. **Food choice in a paddock** — not a search: the food source is an item
   actor (`FUN_00569e00`, no-target policy) or a show element (box policy,
   `FUN_0053acd0` types 0x16/0x17) that publishes its action list. The chooser
   only scores the `Eat` node it is given (§3). Availability additionally needs
   `consume_flag` → `actor[0x214 + item_index*4] != 0` where `item_index` is
   the target's vtable `+0x28` (`11` for actor targets, a name lookup for box).
3. **Enclosure sell non-empty gate** — the sell panel update
   `FUN_00532CC0` (caller of the loc strings at `0x532EA7` / `0x5330E2`):

   ```c
   [ESP+0x27] = 1;                                    // "sellable" flag
   for (sel : [panel+0x40]->begin..end) {
       bld = FUN_0053C2D0(buildingList, sel);         // building/feature record
       [panel+0x50] = bld;
       vec = *([bld+0x10] + 0x14) + 0x3c;             // the park's dino list
       for (entry : vec) {
           beh = *(entry + 0x34);
           if (find(beh->[0xf4..0xf8], key, flag) == end)
               [ESP+0x27] = 0;                        // a dino is not in the list
       }
   }
   if ([sel->[0x34] + 0x64] & mask) { REQUEST_SELL… } else { WARNING_SELLING_UNAVAILABLE_FEATURE }
   ```

   `0x663DC4 = "TEST_SELL_FEATURE"`, `0x663DCC = "&WARNING_SELLING_UNAVAILABLE_DINO"`,
   `0x663DF8 = "&REQUEST_SELL_DINO"`, `0x663D94 = "&WARNING_SELLING_UNAVAILABLE_FEATURE"`.
   The gate is therefore **"every dino in the park must be in the selected
   feature's occupant id list"** — i.e. it is a *membership* test, not a
   per-paddock count. (Confidence: medium — the loop bounds/`find` args need a
   dedicated pass; the port's current "non-empty" gate is a different rule.)

---

## 7. Port gap list (ordered)

| # | Missing piece | Retail | Where to add |
| --- | --- | --- | --- |
| 1 | **Mover + move-command queue** | `saDinosaur+0x2c8`, `FUN_00574350` / `FUN_00574450` / `FUN_00574ad0`, record `0x18` | new `src/world/dino_motion.{h,cpp}` |
| 2 | **Displacement per move kind** | `FUN_00574930`, `FUN_00575DE0`, species table `[P+0x1a8]` | `dino_motion.cpp` (`kind` → speed/duration), data from `Actions.bad` |
| 3 | **Turn quantisation** | `FUN_00575410`, `FUN_005758F0` | `dino_motion.cpp` |
| 4 | **Paddock clamp + footprint test** | `FUN_0055AF70` (rect `path+0x1c0`, u16 grid `path+0x1ac`, radius `actor+0x13`) | `dino_motion.cpp` + a new occupancy-grid adapter (`src/world/occupancy.*`; `src/world/build.*` already has classes 1/2/7/0xF) |
| 5 | **Element → move emitter** | `FUN_00569660` (scores elements, emits `FUN_00573F40`/`00573DC0` with `duration = FUN_00575de0(kind)`, mode 2) | `dino_actions.cpp`: `emit_move_script()` called on action start |
| 6 | **`.bai` element effect application** | `FUN_00569A00` + `FUN_005727B0` (event kinds) | `dino_actions.cpp`: replace `finish_action` delta application with an event table (`BaiEffectElement.events`) |
| 7 | **Action completion switch** | `FUN_00571190` (already partially ported) | `dino_actions.cpp` `finish_action` |
| 8 | **Per-action speed table object** | `saDinosaur+0x4fc`, `FUN_00576720` + `FUN_00576550` (dot of 5 profile floats with the record's `+0x18..0x28`, clamp ±100) | `dino_data` / `dino_actions` |
| 9 | **Food source levels** | `FUN_00580E60` / `FUN_00576D50` (3 discrete levels, 100/66/33 %) | `src/world/food.*`: replace `remaining` f32 with `level 0..2` + `max_quantity` |
| 10 | **Perception list producer** | `beh+0xb8` (and `+0xf4`) | open — trace first (§5.3) |
| 11 | **Terrain clamp** | display side (`gacDinosaur`), not sim | keep the port's `place_on_terrain` |
| 12 | **Needs are gauge-driven** | `FUN_00571370/410/440/540` | `dino_actions.cpp`: drop the "decay rates" tuning and drive FOOD/REPOS/SECU from gauges |

Deliberately **not** on the list: a pathfinder (retail has none for park
actions), an action→clip table (`action-clip-lead.md` proves there is none).

### Minimal next vertical slice (recommended)

**"Walk to the target, turn, eat, consume one food level."**

Scope (one lane, ~300 lines + tests):

1. `src/world/dino_motion.h`: `struct MoveCommand {float x,y,threshold; uint8_t
   flag,flag2; int kind; int mode;};` and `class DinoMover` with the fields of
   §1.3 (`pos` f32 x/y, facing, state, queue, active, kind, path rect) and
   `void start(kind, x, y, mode); void queue(...); void tick(double dt);`
   `bool tick(double dt)` returns "moved".
2. `tick` mirrors `FUN_00574350`: state 2 timer, `FUN_00574450` integration,
   `FUN_00575410` 45°-snap turn, `FUN_0055af70` cell test.
3. Speeds: `walk = length_f`, `run = height_f` (saDinosaur `+0x268/+0x26c`,
   attribute 7/8), `other kinds` from the `Actions.bad` per-kind table. Until
   the unit question is measured (Open #1), gate the constants behind
   `--dino-move-speed` and **measure first** (breakpoint `0x574d99`, log
   `[[mover+0x1fc]]+0x54` and the per-frame position delta).
4. Wire `add_imported_dino`: on action start, `emit_move_script` issues one
   record toward the action target (Eat → the selected food source centre,
   `mode 2`, `threshold = duration(kind)`), then let the loop run.
5. On `Eat` completion: `food_source.level -= 1` (discrete levels), apply the
   element events (VIOLENCE +15), do **not** touch FOOD.
6. Test/evidence: `--dino-actions` selftest extended with
   `dino move: kind=1 dist=…` and `dino eat: level 2 -> 1`; a retail
   side-by-side capture of an IGUANO walking to a Cow in the campaign park
   (Cpg04) plus the x32dbg log of `0x574ad0`/`0x574450` for 5 s.

Estimated risk: low for 1-4 (all formulas read directly), medium for 5 (the
`.bai` element id space) and 6 (the event table is proven but the *timing*
within the action needs the `+0x4d`/`+0x515` flag sequence verified).

---

## 8. Open items (with the cheapest experiment for each)

1. **Unit of walk/run speed.** `saDinosaur+0x268` (attribute 7) is used as
   world-units/s. If it is the phenotype length in cm, a 3 m dino would cover
   300 map units/s. Probe: breakpoint `0x574d99`, log
   `[[mover+0x1fc]]+0x54` and `[mover+0x0c]/[0x10]` deltas per frame.
2. **`Actions.bad` field map.** 462 f32 per family; the code proves
   `[0x0c+kind*8]`, `[0x10+kind*8]`, `[0x1bc+kind*4]`, `[0x288+kind*4]`,
   `[0x14]`, `[0x1c]`, `[0x1d0]`, `[0x22c]`, `[0x134]`. The rest (incl. the
   519-462 = 57 extra dwords of the runtime block) is unmapped.
3. **Terrain / z.** No terrain call in `FUN_00574350`/`0x574450`
   (callee lists checked). Find the `gacDinosaur` height code; the port's
   `place_on_terrain` is a stand-in.
4. **Element int vector: move ids or bones?** `FUN_00569660` feeds the
   element's `+8` vector to `FUN_00573F40`/`00573DC0` as the move kind, while
   `action-clip-lead.md` reads the same vector as skeleton bones (Eat = 8 =
   `Bip01_Neck`). Either that note or the "bones" label in
   `tools/formats/bai.py` is wrong. Probe: breakpoint `0x573dc0`, log
   `param_6` (kind) in a park session and compare to the `.bai` vectors.
5. **`beh+0xb8` / `+0xf4` writer** (§5.3).
6. **Sell gate exact rule** (§6.3).
7. **`+0x2BC` locomotion slot values 3 and 4** — who writes them, and whether
   the animator ignores 3/4 (it only plays `< 3`).

## Evidence

- Decompiled: `FUN_0051e6c0`, `FUN_0051fab0`, `FUN_0051f810`, `FUN_0051f950`,
  `FUN_00521b30`, `FUN_00521cc0`, `FUN_00521df0`, `FUN_00520880`,
  `FUN_00522050`, `FUN_00522080`, `FUN_00522300`, `FUN_0055ac30`,
  `FUN_0055af70`, `FUN_0055b2a0`, `FUN_0055b360`, `FUN_0055b3b0`,
  `FUN_0055ba50`, `FUN_0055c120`, `FUN_0055c210`, `FUN_0055cec0`,
  `FUN_00569640`, `FUN_00569660`, `FUN_00569e00`, `FUN_0056a9a0`,
  `FUN_0056c3b0`, `FUN_0056d880`, `FUN_0056e670`, `FUN_0056fb70`,
  `FUN_005701f0`, `FUN_00570fd0`, `FUN_005710c0`, `FUN_00571190`,
  `FUN_005712e0`, `FUN_00571320/370/410/440`, `FUN_00571540`, `FUN_005724d0`,
  `FUN_005725e0`, `FUN_005727b0`, `FUN_00573370`, `FUN_00573dc0`,
  `FUN_00573f40`, `FUN_005741c0`, `FUN_00574350`, `FUN_00574450`,
  `FUN_00574930`, `FUN_00575410`, `FUN_005758f0`, `FUN_005759c0`,
  `FUN_00575ac0`, `FUN_00575be0`, `FUN_00575cc0`, `FUN_00575de0`,
  `FUN_00576d50`, `FUN_00586690`, `FUN_00617250`, `FUN_005d82f0`,
  `FUN_00580e60`, `FUN_00583d30`, `FUN_0061a5b0`.
- Callsite disassembly: `0x5715cb` (mover tick), `0x570223` (mover ctor args),
  `0x574d17..0x574d99` (steering + turn), `0x57527d..0x5752c7` (speed
  formula), `0x5733b5..0x57343a` (ctor arg → field), `0x532d84..0x5330e2`
  (sell gate).
- Data: `Data/Design/Actions.bad` dump (5 × 462 f32, `[3..0x1b8]` =
  2 f32/kind, `[0x1bc..]` = duration/kind), `Data/design/attractions/actions/*.bai`
  event dump via `tools/formats/bai.py`, `src/world/dino_actions.*`,
  `src/world/danger_grid.*`, `src/world/food.*`, `src/viewer/main.cpp`
  (`add_imported_dino`, dino frame loop).
