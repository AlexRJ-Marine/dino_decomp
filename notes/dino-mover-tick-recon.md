# Dino mover tick/turn mechanics -- exact code paths

Date: 2026-10-30. Read-only Ghidra recon (/DinoIsland.exe-ebea17).
Scope: saDinosaur+0x2c8 mover, questions 1-8 of the parent task. Supersedes
the corresponding parts of notes/dino-ai-gap-audit.md sections 1.3/2 where they
disagree (see "Corrections" at the end).

All offsets are **mover-relative** unless marked abs. abs = mover + 0x2c8.
mover+0x200 holds the saDinosaur pointer; mover+0x1fc the needs block
(abs 0x4c4) whose +0x54/+0x58 are the walk/run speeds = abs 0x268/0x26c.
P = phenotype = abs saDinosaur+0x50; X = [P+0x1a8] = the per-family action
block (0x81c stride, from Data/Design/Actions.bad).

## 0. Field map (verified against FUN_00573370 ctor + writers)

| mover off | abs | type | meaning |
| --- | --- | --- | --- |
| 0x00 | 0x2c8 | ptr | vtable 0x00643170 |
| 0x04/0x08 | 0x2cc/0x2d0 | f32 | position copy refreshed at segment start |
| 0x0c/0x10 | 0x2d4/0x2d8 | f32 | **live position** x,y |
| 0x14/0x18 | 0x2dc/0x2e0 | i32 | last accepted cell |
| 0x1c/0x20 | 0x2e4/0x2e8 | f32 | **facing unit vector** |
| 0x24 | 0x2ec | f32 | mode-2 elapsed (only accumulates in mode 2) |
| 0x28 | 0x2f0 | f32 | segment elapsed |
| 0x2c | 0x2f4 | f32 | segment progress 0..1 |
| 0x30 | 0x2f8 | f32 | segment duration |
| 0x34 | 0x2fc | f32 | 1 / duration |
| 0x38 | 0x300 | f32 | wait-state blend accumulator (+= dt*2.0) |
| 0x3c | 0x304 | f32 | wait-state elapsed |
| 0x40 | 0x308 | f32 | wait-state target duration |
| 0x44 | 0x30c | f32 | wait-state accumulator rate, always 2.0 |
| 0x48 | 0x310 | u32 | sentinel 0x7f7fc99e |
| 0x50 | 0x318 | u8 | "moved this frame" |
| 0x54/0x58 | 0x31c/0x320 | f32 | out: facing before the tick |
| 0x5c/0x60 | 0x324/0x328 | i32 | out: dino cell before the tick |
| 0x64 | 0x32c | u8 | **blocked** (give up) |
| 0x65 | 0x32d | u8 | set when the queued kind == 8 |
| 0x66 | 0x32e | u8 | restart flag (kinds 0x1b/0x24/0x1e) |
| 0x67 | 0x32f | u8 | pending pose to latch |
| 0x68..0xc7 | | i32[8][2] | 8 cached waypoint cells (jump run-up) |
| 0x1c8 | 0x490 | i32 | waypoint count |
| 0x1cc | 0x494 | u8 | path valid |
| 0x1d0 | 0x498 | f32 | progress at the last accepted move |
| 0x1d4 | 0x49c | u8 | paused |
| 0x1dc/0x1e0/0x1e4 | 0x4a4/0x4a8/0x4ac | ptr | move queue begin/end/cap (0x18-byte records) |
| 0x1e8 | 0x4b0 | ptr | phenotype (saDinosaur+0x50) |
| 0x1ec | 0x4b4 | i32 | **state**: 0 idle, 1 moving, 2 waiting, 3 knocked down |
| 0x1f0 | 0x4b8 | i32 | kind being planned (next) |
| 0x1f4 | 0x4bc | i32 | kind currently executing (0 = none) |
| 0x1f8 | 0x4c0 | i32 | first kind that actually moved (activity type) |
| 0x1fc | 0x4c4 | ptr | needs block (abs 0x214) |
| 0x200 | 0x4c8 | ptr | self saDinosaur |
| 0x204 | 0x4cc | ptr | apfDinoPath* (nullable) |
| 0x208 | 0x4d0 | u8 | resend/pending flag |
| 0x20c/0x210 | 0x4d4/0x4d8 | i32 | pending cell |
| 0x214/0x218 | 0x4dc/0x4e0 | f32 | pending facing (post-turn) |
| 0x21c | 0x4e4 | u8 | latched-pose-changed flag |
| 0x220/0x224 | 0x4e8/0x4ec | i32 | latched cell (consumed by FUN_005721e0) |
| 0x228/0x22c | 0x4f0/0x4f4 | f32 | latched facing |

Move record (0x18): +0x00 f32 x, +0x04 f32 y, +0x08 f32 dur/threshold,
+0x0c u8 instant, +0x0d u8 allowTurn, +0x10 i32 kind, +0x14 i32 mode.

## 1. FUN_00574350 -- mover tick (the state machine)

Called from saDinosaur::Update FUN_00571540 at 0x5715cb as
FUN_00574350(dt, &outPrevFacing, &outPrevPos, &outPrevPos2); the byte return
is "moved" and drives the grid re-stamp.

```c
dino = *(int*)(mover+0x200);
mover->0x208 = 0; mover->0x21c = 0; mover->0x50 = 0;
mover->0x5c = dino->x;  mover->0x60 = dino->y;      // i32 cell (+8/+0xc)
mover->0x54 = mover->0x1c; mover->0x58 = mover->0x20;
mover->0x1f8 = 0;
if (mover->0x1d4 == 0) switch (mover->0x1ec) {
  case 1: FUN_00574450(dt); break;                            // moving
  case 2: mover->0x3c += dt;
          mover->0x38 += dt * mover->0x44;                    // 0x44 = 2.0
          if (mover->0x40 <= mover->0x3c) FUN_00574ad0(&mover->0x0c, 0);
          break;                                              // wait done
  case 3: if (dino->health /*i16 at +0x16*/ > 0) {
            mover->0x66 = 0; FUN_005759c0(1); FUN_00574ad0(&mover->0x0c, 1); }
          break;                                              // stand up
}                                                             // case 0: nothing
if (mover->0x50 == 1) {                                       // publish + re-sync
  *out5 = {0x54,0x58}; *out3 = {0x5c,0x60}; mover->0x50 = 0;
  mover->0x5c = dino->x; mover->0x60 = dino->y;
  mover->0x54 = mover->0x1c; mover->0x58 = mover->0x20;
}
return moved;
```

- **State field = +0x1ec** (abs 0x4b4). The only writer of 3 is FUN_00574450
  (kind 0x15, health < 1, near 0x57472c).
- Timers: +0x28 segment elapsed, +0x24 mode-2 elapsed, +0x3c wait elapsed vs.
  target +0x40 (0.5 s), +0x38 wait blend (dt*2.0).
- dt usage per state: 1 -> all integration (+0x28 += dt, +0x24 += dt when
  mode 2, displacement speed*dt); 2 -> +0x3c += dt and +0x38 += 2*dt only;
  0/3 -> no dt.
- +0x1d4 (paused) suppresses everything but the publish step.

## 2. FUN_00574450 -- integrator (state 1)

```c
segT = mover->0x28 + dt;
px = dino->x; py = dino->y;                 // int cell, as float
mover->0x28 = segT;
if (rec->mode == 2) mover->0x24 += dt;      // mode-2 elapsed
go = true;
if (segT < mover->0x30)          mover->0x2c = segT * mover->0x34;
else if (rec->0x0c /*instant*/) { mover->0x28 = 0; mover->0x2c = 0;
                                  mover->0x1d0 = 1.0f;
                                  mover->0x04 = mover->0x0c; mover->0x08 = mover->0x10;
                                  mover->0x1f8 = mover->0x1f4; }
else { go = false; mover->0x2c = 1.0f; }    // segment over, stop integrating
if (go && rec->mode != 0) {                 // mode 0 never integrates
  out = FUN_00574930(rec->kind, dt, ...);   // pos + facing*dist (+ perp for kind>2)
  // kinds {1,2,0x34,0x35}: clamp out to rec->xy when the target lies between
  // pos and out (no overshoot)
  cx = (int)out.x; cy = (int)out.y;         // truncation (__ftol)
  if (cx == px && cy == py) { mover->0x0c = out.x; mover->0x10 = out.y; }
  else if (mover->0x204 == 0 || (kind != 1 && kind != 2) ||
           FUN_0055af70(path, dino, cx, cy, &unused, 1)) {
       mover->0x0c = out.x; mover->0x10 = out.y;
       dino->x = cx; dino->y = cy; mover->0x50 = 1;         // 0x5746c3
  } else {                                                   // blocked on foot
       mover->0x1cc = 0;                                     // path invalid
       if (kind == 1) FUN_00573f40(&pos, &mover->0x14 /*lastCell*/, 0, 1, 0x34, 1, 1);
       else if (kind == 2) FUN_00573f40(&pos, &mover->0x14, 0, 1, 0x35, 1, 1);
       else mover->0x64 = 1;
  }
}
if (FUN_00575ac0()) {                                       // arrival
  if (mover->0x1f8 == 0) { mover->0x1f8 = mover->0x1f4;     // 0x5746f2
                           mover->0x1d0 = mover->0x2c; }    // 0x5746f8
  if (rec->kind in {3,4,5} && rec->mode != 0) {
      mover->0x50 = 1;                                      // 0x574717
      FUN_005758f0(&mover->0x1c, rec->kind, 1);             // 0x574727  REAL TURN
  }
  if (rec->kind == 0x15) { if (health < 1) state = 3;
      else { push {pos, dur = X[0x22c], mode 2, kind 0x1c};
             if (state == 0) FUN_00574ad0(&pos, 1); } }
  if (state != 3) { mover->0x66 = 0; FUN_005759c0(1); FUN_00574ad0(&pos, 1); }
}
```

**+0x50 ("moved") is set at exactly three places**: 0x5746c3 (a position step
accepted), 0x574717 (a turn record completed and the facing rotated), and
inside FUN_00575410 (0x57436x, a turn was planned).

**Mode 0/1/2 effect**: 0 = never integrates and FUN_00575ac0 reports arrival
immediately, so the record lives one frame and is discarded -- FUN_00573f40
uses this to *cancel* the executing record (queue[0].mode = 0). 1 = integrate
until the exact target point (duration is only a cap; detail below). 2 =
integrate for +0x30 seconds, then arrival via mover->0x24 >= rec->0x08.

Cell test note: the integrator truncates (__ftol of the float), while
FUN_00574ad0 rounds half-up ((int)(pos + 0.5f), const 0x62544c).

## 3. FUN_00574ad0 -- start/advance a segment

FUN_00574ad0(&mover->0x0c, afterArrival); afterArrival == 1 adds the 0.5 s
pause. Position pointer is arg1 so the callee can enqueue "move to self".

Order (verified in disassembly 0x574ad0..0x5752fb):

1. if (mover->0x67) { 0x67 = 0; 0x21c = 1; 0x220..0x22c = {0x20c,0x210,0x214,0x218}; }
   -- latched pose for the display/grid consumer (FUN_005721e0 at 0x56b5xx).
2. idx = (mover->0x1f4 != 0) ? 1 : 0; mover->0x1ec = 1;
   FUN_005759c0(idx) -- drop already-reached walk records from idx.
3. if (count <= idx): if 0x1f4 == 0 -> state 0, return. Else
   0x1f0 = 0; 0x40 = afterArrival ? 0.5 : 0; -> 0.5 s wait (state 2, 0x38=0,
   0x3c=0, 0x44=2.0), or pop the front record, 0x1f4 = 0, state 0.
4. rec = queue[idx].
5. **rec->kind == 0x34 || 0x35** (path marker):
   cell = ((int)rec->x,(int)rec->y); if (!0x1cc or cell != (0x14,0x18)) then
   FUN_0055b360(dino) (reset that dino path queue); 0x14/0x18 = cell;
   FUN_0055b3b0(&wp, dino, cell.x, cell.y, &found, 0x1cc);
   0x1cc = 1; 0x1c8 = path->0x1a0; copy the 8-byte waypoints from
   path+0x40 into mover+0x68.
   found == 0 -> 0x64 = 1 (blocked); else
   rec->x/rec->y = (float)wp; rec->kind = (kind != 0x34) + 1; -> **0x34->1
   (walk), 0x35->2 (run)**, i.e. these kinds never execute as themselves.
6. If not blocked:
   a. if (rec->0x0d /*allowTurn*/ && kind not in {3,4,5} && (rec->xy != pos)):
      desired = normalize(rec->xy - pos); if changed ->
      FUN_00575410(&desired, idx, &newFacing) (see section 4).
   b. **mover->0x1f0 = rec->kind** (0x574da9) -- the only writer of 0x1f0.
   c. if newFacing != facing: x = (int)(pos.x+0.5), y = (int)(pos.y+0.5);
      FUN_0055af70(path, dino, x, y, &newFacing, 1): 1 -> 0x1c/0x20 = newFacing,
      else 0x64 = 1. (FUN_0055af70 5th arg is **never written** -- the facing
      survives from FUN_00575410; verified in the callee prologue, param_5 is
      dead.)
   d. if still not blocked:
      - kind in {3,4,5}: pred = rotate(facing, kind, 1); if path != 0:
        FUN_0055af70(path, dino->x, dino->y, &pred, 1) -> fail 0x64 = 1;
        success -> 0x67 = 1; 0x20c/0x210 = dino cell; 0x214/0x218 = pred; 0x208 = 1.
      - kind not in {1,2,0x34,0x35} && rec->mode != 0: compute
        dur = durationFor(kind) and call FUN_00574930(kind, dur, ...) to
        **predict the end cell with dt = dur**; if the predicted cell differs
        from the dino cell, run the same FUN_0055af70 footprint test on it and
        on failure 0x64 = 1.
7. Bookkeeping (LAB_00574fcd):
   - 0x64 -> state = 0; if 0x1f4 == 0 clear the queue; else enqueue a
     mode-0 kind-0 record at pos and take the 0.5 s wait path.
   - kind = 0x1f0; kinds 0x1b/0x24/0x1e set 0x66 = 1.
   - 0x40 = 0; if (afterArrival || queue length changed) then
     0x40 = 0.5 **unless** (prev,new both in {1,2,0x34,0x35}) or
     (prev == 0 && new in {1,2,0x34,0x35}) or (both in {3,4,5}) -> 0.
   - 0x40 == 0: pop the front record if 0x1f4 != 0; 0x1f4 = kind; kind == 0
     -> state 0; else **0x30 = durationFor(kind) at 0x5752c7**,
     0x04 = pos.x, 0x08 = pos.y (0x5752d2/d9), **0x24 = 0x28 = 0x2c = 0**
     (0x5752de/e1/e4), **0x34 = 1.0/dur** (0x5752e9, 1.0 at 0x62588c),
     0x48 = 0x7f7fc99e (0x5752f1).
   - else -> state 2, 0x38 = 0x3c = 0, 0x44 = 2.0.

So +0x2c is written only in FUN_00574450 (progress) and reset here;
+0x30/+0x34 only in this tail block.

## 4. FUN_00575410 + FUN_005758f0 -- the turn (45 deg per step, not 90)

FUN_005758f0(f32* dir, int side, int n) at 0x5758f0:

| side | code | rotation |
| --- | --- | --- |
| 3 | x=(x-y)*0.70710677; y=(y+x)*0.70710677 | **+45 deg** per step |
| 4 | x=(y+x)*0.70710677; y=(y-x)*0.70710677 | **-45 deg** per step |
| 5 | x=-x; y=-y | 180 deg per step |

0.70710677 = cos 45, so the audit "90 deg per step" is **wrong**; the
*0.7071 matrix is a 45 deg rotation (0x64315c..0x643168 hold 0.99, 0.9238795,
0.38268343, 0.70710677).

FUN_00575410(desired, idx, &newFacing) at 0x575410. Both comparison constants
0x675710 and 0x675714 are **0.0f**, so the "steps = 3" branch is dead code:

```c
if (facing == desired) return;                 // exact compare, no move flag
mover->0x50 = 1;                               // 0x57436x "moved"
dot   = facing.x*desired.x + facing.y*desired.y;
if (dot >= 0.99f)           { newFacing = desired; return; }   // <= 8.1 deg: snap
cross = facing.x*desired.y - facing.y*desired.x;
steps = 0; side = 0;
if      (dot >= 0.9238795f)   steps = 0;                  // <= 22.5 deg: snap
else if (dot >= 0.38268343f)                             // 22.5 .. 67.5 deg
     steps = (queue[idx].kind == 2 && P[0x190] <= 1) ? 0 : 1;
else if (dot >= 0.0f)         steps = 2;                  // 67.5 .. 90 deg
else                        { steps = 0; side = 5; }      // > 90 deg: U-turn
newFacing = desired;
if (steps > 0)      newFacing = rotate(desired, (cross < 0) ? 3 : 4, steps);
else if (side == 5) newFacing = rotate(desired, 5, 1);    // = -desired
// then, if steps > 0: insert `steps` records at index idx
//   kind = (cross < 0) ? 4 : 3,  record = {pos.x, pos.y,
//   dur = X[0x1bc + kind*4], flag 0, allowTurn 0, kind, mode 2}
// if steps == 0 && side == 5: insert ONE record {pos, X[0x1d0], 0, 0, 5, mode 2}
```

Answers:

- **Each turn step is 45 degrees** (kind 3 = +45, kind 4 = -45); a 180 turn is
  **one** record of kind 5 (not 4 x 45).
- `steps` (number of turn records) is only **0, 1 or 2**, plus the single-record
  180 U-turn. The audit "1/2/3/5" is wrong: 3 is unreachable and 5 is the
  U-turn *side*, not a step count.
- Sides: 3 = +45/step, 4 = -45/step, 5 = 180/step.
- The facing is **pre-rotated backwards** by steps*45 (opposite side) at
  planning time, then advanced by +45 per completed turn record at 0x574727,
  so the net result lands exactly on the desired vector.
- Inserted kinds: 3 / 4 / 5, mode 2, target = the current position (no
  translation), flags 0/0, **duration = X[0x1bc + kind*4] = 1.00 s for kinds
  3, 4 and 5 in all five Actions.bad families** (X[0x1d0] for kind 5 is the
  same table slot). So **45 deg/step, 1.0 s/step, 180 deg in 1.0 s**.
- Turn records are inserted **before** the pending record (index idx), so they
  execute first.

## 5. FUN_005759c0 / FUN_00575ac0 -- drop and arrival

FUN_005759c0(idx): while idx < count, look at queue[idx]; **only** kinds
1, 2, 0x34, 0x35 are droppable, and only when
(pos.x-rec.x)^2 + (pos.y-rec.y)^2 <= 0.0001 (0.01 unit radius); anything else
stops the loop. Turn records and mode-2 records are never dropped here.

FUN_00575ac0():

```c
switch (rec->mode) {
  case 0: return 1;                              // arrived immediately
  case 1: if (rec->x != mover->0x0c || rec->y != mover->0x10) return 0;  // EXACT f32
          if (kind not in {1,2}) return 0;       // non walk/run: no repair
          if ((int)rec->x,(int)rec->y == mover->0x14,mover->0x18) {
              mover->0x14 = 0; mover->0x18 = 0; return 1; }
          FUN_00573f40(&pos, {lastCell}, 0, 1, (kind != 1) + 0x34, 1, 1);
          return 1;                              // re-path through the path object
  case 2: return (mover->0x24 >= rec->0x08);
}
```

So: **mode 1 arrival is an exact float comparison of both coordinates, not a
radius.** The 0.01 radius is only FUN_005759c0 pre-drop. When a walk/run record
cannot reach the exact point (the snapped 8-direction facing stops short), the
mover re-issues a 0x34/0x35 record aimed at the last accepted cell
+0x14/+0x18, which FUN_00574ad0 resolves with FUN_0055b3b0 (the greedy
8-direction pathfinder) and converts back to kind 1/2 at the next waypoint.
Only kinds 1 and 2 re-path; every other kind just returns 0 (stays until its
timer ends). Arrival of 3/4/5 happens through the mode-2 timer.

## 6. FUN_00573f40 / FUN_00573dc0 -- queue

- FUN_00573dc0(mover, target, dur, mode, kind, u8 a, u8 b): **append only** at
  the end; if kind == 0x35 && needs->0x58 == 0 (run speed 0) downgrade to 0x34;
  if state == 0 -> FUN_00574ad0(&pos, 1) (**idle mover starts immediately**),
  else it only sits in the queue.
- FUN_00573f40 = same plus:
  - if (state == 1) { k = queue[0].kind; if (k not in {3,4,5}) queue[0].mode = 0; }
    -- **a new command cancels the arrival test of the executing record** (mode
    0 = no integration + instant arrival).
  - if the queue is already deeper than n (n = 2 when state == 2 && 0x1f4 &&
    0x1f0, else 1) it inserts a copy of the new record before the end
    (FUN_00576150, the MSVC6 vector-insert helper) -- the queue keeps at most
    one "next" record plus turns.
  - if ((0x1f4 == 0x1e || 0x1f4 == 0x15) && kind != 0x1c && !0x64) it appends an
    extra {target, dur = X[0x22c], mode 2, kind 0x1c} record.
  - mover->0x65 = (kind == 8).
- Append order is strictly FIFO; only turn records and the blocked "move to
  self" marker are ever inserted mid-queue (always at the pending index).

## 7. FUN_00569660 -- element -> move emitter

Signature (from the only 3 call sites, 0x56c5f4/0x56c788/0x56c7ff):
FUN_00569660(element /*ECX*/, dino, float* targetPoint, id).

```c
element->0x01 = 0;                       // "no move emitted" flag
prof = *(void**)(element + 0x04);        // the action move profile
if (prof->0x60 == 5) {                   // "idle / random" profile
    n = dino->0x29c ? rand() % dino->0x29c + 1 : 0;
    walk dino->0x288[] (counts, stride 4) until n <= 0, accumulating idx;
    kind = idx + 0xb;
    FUN_00573f40(&mover->0x0c, targetPoint, FUN_00575de0(kind), /*mode*/2,
                 kind, 0, 1);
    return rand() % dino->0x288[idx*4];
}
// weighted choice over prof->0x54..0x58 (0xc-byte records {+4 weight, +8 attr})
total = sum over records of (record->4 * FUN_00572620(record->8));
r = rand() % total (plus rand() % **(prof+4));
scan table prof->0xa0 stride 0x58 for the first entry with entry->0 >= r;
kinds = dword vector [entry+8 .. entry+0xc);
if (kinds.size == 0) { element->0x01 = 1; return; }
k = kinds[0]; FUN_00573f40(&mover->0x0c, targetPoint, FUN_00575de0(k), 2, k, 0, 1);
for (i = 1; i < kinds.size; i++)
    FUN_00573dc0(&mover->0x0c, targetPoint, FUN_00575de0(kinds[i]), 2, kinds[i], 0, 0);
```

- The move kind comes from the **profile table pointed to by element+0x04**
  (indexed by the action list and the dino attributes), **not** from the
  element +8 int vector -- that vector is the bone list, as
  action-clip-lead.md says. Audit Open #4 resolved.
- Target point = the caller float*; the caller (0x56c5xx) first gets the
  nearest reachable cell from FUN_0055b5b0(dino, cellX, cellY, ...) and passes
  that cell converted to floats (fallback: the raw target cell).
- Mode is always **2**, the threshold/duration always **FUN_00575de0(kind)**
  (0x575de0):
  kind 1: (P[0xa4]*X[0x14]*0.01) / dino[0x268],
  kind 2: (P[0xa4]*X[0x1c]*0.01) / dino[0x26c],
  else: X[0x1bc + kind*4].
- Separate path next to the emitter call (0x56c615): one 0x34/0x35 record
  {pos, target, dur 0, mode 1, kind = 0x34 + (dino->0x2b8 != 0), 1, 1} when the
  dino has to be walked to a cell (resolved by the pathfinder at section 3 step 5).

## 8. Initial facing / angle convention

- FUN_00573370 (mover ctor) zeroes mover+0x04..+0x2c, i.e. **pos (0,0) and
  facing (0,0)**; 0x48 = 0x7f7fc99e, 0x40/0x44/0x38/0x3c = 0, state 0,
  0x1cc = 1, 0x1e8 = phenotype, 0x1fc = needs, 0x1dc/0x1e0/0x1e4 = the empty
  queue (10-record capacity).
- saDinosaur ctor FUN_005701f0 and spSimuManager::CreateDinosaur FUN_00586690
  do **not** touch +0x2e4/+0x2e8.
- No literal write to saDinosaur+0x2e4/+0x2e8/+0x2d4/+0x2d8 exists anywhere in
  the decompiled image (only reads: 0x55b3b0, 0x58f670, 0x58dfc0, 0x58f210,
  0x58f430, 0x5d4990). The only writers are the mover itself: 0x574e08/0x574e10
  (facing from FUN_00575410) and 0x574727 (turn record arrival).
- Convention: facing is a unit vector used directly as a multiplier
  (FUN_00574930: out.x = facing.x*dist + pos.x), so **facing (1,0) = +x**.
  FUN_005758f0 side 3 is (x-y, y+x)*cos45, a mathematically positive 45 deg
  rotation in the (x,y) plane (CCW if y is up, CW on screen if y is down). The
  apfDinoPath angle table (FUN_0055ac30, +0x10..+0x2c = 0, +-0.785398,
  +-1.570796, +-2.35619, 3.14159) agrees that facing is a plain angle vector.
- After every moved frame FUN_00571540 pushes &saDinosaur+0x2e4 into
  vtable[+4] (FUN_00571c20) -- but that callee only re-stamps the footprint and
  ignores the pointer, so nothing downstream writes the facing back.

## Port recipe

```cpp
void DinoMover::tick(double dt) {              // FUN_00574350
    moved = false;
    resend = false; poseChanged = false; lastMovedKind = 0;
    prevCell = {dino.x, dino.y};  prevFacing = facing;  // out params
    if (!paused) switch (state) {
      case Moving:   moved = integrate(dt); break;
      case Waiting:  waitT += dt; waitBlend += dt * 2.0f;
                     if (waitDur <= waitT) stepQueue(/*afterArrival=*/false);
                     break;
      case Down:     if (dino.health > 0) { flag66 = false;
                         dropReached(1); stepQueue(true); } break;
      case Idle:     break;
    }
    movedThisFrame = moved;                    // +0x50, cleared at the end
}

bool DinoMover::integrate(float dt) {          // FUN_00574450
    bool moved = false;
    segT += dt;
    if (rec.mode == 2) modeT += dt;
    bool go = true;
    if (segT < segDur) progress = segT * invDur;
    else if (rec.instant) { segT = 0; progress = 0; progressAtMove = 1.0f;
                            pos4 = pos; lastMovedKind = activeKind; }
    else { go = false; progress = 1.0f; }
    if (go && rec.mode != 0) {
        Vec2 out = pos + facing * speedFor(rec.kind) * dt;   // kinds > 2 add a
                                                            // perpendicular term
        if (rec.kind in {1,2,0x34,0x35}) out = clamp_to_segment(pos, out, rec.xy);
        Cell c = {(int)out.x, (int)out.y};                  // truncation
        if (c == dino.cell) pos = out;
        else if (!path || rec.kind not in {1,2} || cellFree(dino, c)) {
            pos = out; dino.cell = c; moved = true;
        } else {
            pathValid = false;
            if      (rec.kind == 1) queue({pos, lastCell, 0, 1, 0x34, 1, 1});
            else if (rec.kind == 2) queue({pos, lastCell, 0, 1, 0x35, 1, 1});
            else blocked = true;
        }
    }
    if (arrived()) {                                       // FUN_00575ac0
        if (lastMovedKind == 0) { lastMovedKind = activeKind;
                                  progressAtMove = progress; }
        if (rec.kind in {3,4,5} && rec.mode != 0) {
            moved = true;
            facing = rotate(facing, rec.kind, 1);          // 45 / 180 deg
        }
        if (rec.kind == 0x15) { /* death: state = Down, or queue kind 0x1c */ }
        if (state != Down) { flag66 = false; dropReached(1); stepQueue(true); }
    }
    return moved;
}

void DinoMover::stepQueue(bool afterArrival) {             // FUN_00574ad0
    int idx = activeKind ? 1 : 0;
    state = Moving;
    dropReached(idx);                                      // FUN_005759c0
    if (queue.size() <= idx) {
        if (!activeKind) { state = Idle; return; }
        pendingKind = 0;
        waitDur = afterArrival ? 0.5f : 0.0f;
        if (waitDur) { state = Waiting; waitT = waitBlend = 0; waitRate = 2.0f; return; }
        queue.pop_front(); activeKind = 0; pos4 = pos; state = Idle; return;
    }
    Record& rec = queue[idx];
    if (rec.kind == 0x34 || rec.kind == 0x35) {            // path marker
        Cell cell = {(int)rec.x, (int)rec.y};
        if (!pathValid || cell != lastCell) path.resetQueue(dino);
        lastCell = cell;
        Cell wp; bool found;
        path.nextWaypoint(&wp, &found, dino, cell, pathValid);
        pathValid = true; waypointCount = path->count; copy 8 waypoints;
        if (!found) blocked = true;
        else { rec.x = (float)wp.x; rec.y = (float)wp.y;
               rec.kind = (rec.kind != 0x34) + 1; }         // 0x34 -> 1, 0x35 -> 2
    }
    Vec2 newFacing = facing;
    if (!blocked && rec.allowTurn && rec.kind not in {3,4,5} && (rec.xy != pos)) {
        Vec2 desired = normalize(rec.xy - pos);
        if (desired != facing) turnPlan(desired, idx, newFacing);   // see 4.
    }
    pendingKind = rec.kind;                       // +0x1f0
    if (newFacing != facing) {
        Cell c = {(int)(pos.x + 0.5f), (int)(pos.y + 0.5f)};
        if (cellFree(dino, c)) facing = newFacing; else blocked = true;
    }
    if (!blocked) {
        if (rec.kind in {3,4,5}) {
            Vec2 pred = rotate(facing, rec.kind, 1);
            if (cellFree(dino, dino.cell)) latchPose(dino.cell, pred); // 0x67 etc.
            else blocked = true;
        } else if (rec.kind not in {1,2,0x34,0x35} && rec.mode != 0) {
            Vec2 end = predictEnd(rec.kind, durationFor(rec.kind));
            if ((Cell)end != dino.cell && !cellFree(dino, (Cell)end))
                blocked = true;
        }
    }
    if (blocked) { state = Idle;
        if (!activeKind) { queue.clear(); return; }
        queue.push_back({pos, pos, 0, /*mode*/2, /*kind*/0, 0, 0});
        pendingKind = 0; waitDur = afterArrival ? 0.5f : 0.0f;
        if (!waitDur) { queue.pop_front(); activeKind = 0; pos4 = pos; return; }
        state = Waiting; waitT = waitBlend = 0; waitRate = 2.0f; return; }
    if (pendingKind == 0x1b || pendingKind == 0x24 || pendingKind == 0x1e)
        flag66 = true;
    waitDur = 0;
    if (afterArrival || queueSizeChanged) {
        waitDur = 0.5f;
        // loco(k): k in {1,2,0x34,0x35}; turn(k): k in {3,4,5}
        if ((loco(prevKind) && loco(pendingKind)) ||
            (prevKind == 0 && loco(pendingKind))  ||
            (turn(prevKind) && turn(pendingKind)))  waitDur = 0;
    }
    if (waitDur == 0) {
        if (activeKind) queue.pop_front();
        activeKind = pendingKind;
        if (!activeKind) { state = Idle; pos4 = pos; return; }
        segDur = durationFor(activeKind); invDur = 1.0f / segDur;
        pos4 = pos; modeT = segT = progress = 0; sentinel48 = 0x7f7fc99e;
    } else { state = Waiting; waitT = waitBlend = 0; waitRate = 2.0f; }
}

void DinoMover::turnPlan(Vec2 d, int idx, Vec2& out) {     // FUN_00575410
    float dot   = facing.x*d.x + facing.y*d.y;
    float cross = facing.x*d.y - facing.y*d.x;
    int steps = 0, side = 0;                       // side 0 = none
    if (dot >= 0.99f)           steps = 0;
    else if (dot >= 0.9238795f) steps = 0;
    else if (dot >= 0.38268343f)
        steps = (queue[idx].kind == 2 && P[0x190] <= 1) ? 0 : 1;
    else if (dot >= 0.0f)       steps = 2;
    else                      { steps = 0; side = 5; }
    out = d;
    if (steps > 0)      out = rotate(d, cross < 0 ? 3 : 4, steps);
    else if (side == 5) out = rotate(d, 5, 1);     // -d
    if (steps > 0) {
        int k = (cross < 0) ? 4 : 3;
        for (int i = 0; i < steps; i++)
            insertBefore(idx, {pos, X[0x1bc + k*4], 0, 0, k, /*mode*/2});
    } else if (side == 5) {
        insertBefore(idx, {pos, X[0x1d0], 0, 0, 5, /*mode*/2});
    }
}
```

**kind -> degrees/step, seconds/step, speed source**

| kind | role | deg/step | s/step | speed / duration source |
| --- | --- | --- | --- | --- |
| 1 | walk | - | P[0xa4]*X[0x14]*0.01 / dino[0x268] | dino[0x268] u/s, stride P[0xa4]*X[0x14]*0.01 |
| 2 | run | - | P[0xa4]*X[0x1c]*0.01 / dino[0x26c] | dino[0x26c] u/s |
| 3 | turn + | **+45** | **1.00** | X[0x1bc+12] |
| 4 | turn - | **-45** | **1.00** | X[0x1bc+16] |
| 5 | U-turn | **180** | **1.00** | X[0x1d0] (== X[0x1bc+20]) |
| other | gesture/effect | - | X[0x1bc+kind*4] | P[0xa4]*X[0x288+kind*4]*X[0x0c+kind*8]*0.01 u/s, plus lateral P[0xa4]*X[0x288+kind*4]*X[0x10+kind*8]*0.01 along rotate(facing,3,2) (+90) |
| 0x34 | "walk to cell" marker | - | 10.0 s cap | rewritten to kind 1 at the next waypoint |
| 0x35 | "run to cell" marker | - | 0.0 s | rewritten to kind 2 (or 0x34 when run speed == 0) |

Actions.bad block 0 (all five families agree on the turn durations):
dur[0]=0.5, dur[1]=dur[2]=0, dur[3]=dur[4]=dur[5]=1.0; walk stride factor
X[0x14] = 0.7301 and run X[0x1c] = 2.0267 for family 0 (1.1822/6.1981,
1.9507/4.3745, 2.3025/8.2312, 0.8184/1.8407 for families 1-4).

## Corrections to notes/dino-ai-gap-audit.md

1. Section 2.2 "FUN_005758f0 rotates by 90 deg per step" -- **wrong, 45 deg**
   (*0.70710677), side 5 = 180 deg.
2. Section 2.2 "steps = 2 or 3 (or 5 when blocked...)" -- **only 0/1/2** plus a
   single 180 record; the 3 branch is dead (0x675714 == 0x675710 == 0.0f) and
   there is no "blocked" condition in the step choice.
3. Section 2.1 record layout is correct, but +0x0d (allowTurn) gates the whole
   desired-vector/turn computation in FUN_00574ad0.
4. Section 2.1/2.4: FUN_0055af70 4th arg (the facing) is **never written** by
   the callee in this build; the facing set at 0x574e08 is purely
   FUN_00575410 out value.
5. Section 2.2/1.3: the facing rotation really happens at **record arrival**
   (0x574727), not in FUN_00574ad0 turn branch (that branch only predicts the
   post-turn facing for the footprint test and the latched pose).
6. Section 1.3 "+0x66/+0x67 blocked / restart-path flags": +0x64 is the blocked
   flag; +0x66 is set for kinds 0x1b/0x24/0x1e; +0x67 requests the latched pose
   copy.

## Open

1. **The facing is pre-rotated backwards by steps*45 and snapped forward by the
   turn records.** The net result is correct, but the intermediate pose points
   away from the target (a 180 turn snaps to -desired for 1 s). No smoothing is
   visible in the mover; the display may filter it. Cheapest probe: watch
   saDinosaur+0x2e4 and the model yaw across one 90 deg turn (bp 0x574727, log
   [mover+0x1c]/[0x20] before and after).
2. **Initial facing / initial mover position.** The ctor zeroes both ((0,0)) and
   no code writes saDinosaur+0x2d4/+0x2e4 except the mover itself, so the first
   planning step starts from facing (0,0) (which yields steps = 2 with
   cross = 0). Either a placement/.sav loader copies the mover block with
   pointer arithmetic Ghidra folded, or retail really starts at (0,0) and the
   first plan self-corrects over two turn records. Cheapest probe: bp 0x574350
   on the first tick of one Cpg04 dino and log mover+0x0c..+0x20 (expect the
   dino cell + a unit facing if a loader set it).
3. **kind 0x35 segment duration is 0** (X[0x1bc+0x35*4] = 0), which would make
   +0x30 = 0, +0x34 = inf and the integrator never run. In practice a 0x35
   record is always rewritten to kind 2 by the path branch (section 3 step 5) or
   downgraded to 0x34 when the run speed is 0. Probe: bp 0x5752c7 and log the
   kind/+0x30 when a run-to-cell record starts; if a real 0x35 survives, the
   port must substitute P[0xa4]*X[0x1c]*0.01 / dino[0x26c].
4. **Unit of walk/run speed** (audit Open #1) is untouched here; the stride
   duration formula is consistent with "one record = one stride of
   P[0xa4]*0.73*0.01 units", but the world-unit scale still needs the
   0x574d99/0x574f2d probe.

## Evidence read this pass

Decompiled: FUN_00574350, FUN_00574450, FUN_00574ad0, FUN_00575410,
FUN_005758f0, FUN_005759c0, FUN_00575ac0, FUN_00574930, FUN_00575de0,
FUN_00573f40, FUN_00573dc0, FUN_00575f50, FUN_00576100, FUN_00576150,
FUN_00569660, FUN_0056c3b0, FUN_00573370, FUN_005701f0, FUN_00570590,
FUN_00571540, FUN_00586690, FUN_0055af70, FUN_0055b3b0, FUN_0055b5b0,
FUN_005721e0, FUN_0056b550, FUN_00571c20.
Disassembly: 0x575410..0x57559b (dot/cross/threshold/side), 0x574c80..0x574e30
(desired vector, facing store, footprint test), 0x574e20..0x574fcd (turn
branch, latch), 0x5746f0..0x574727 (arrival rotation), 0x575230..0x5752fb
(0x30/0x34/0x24/0x28/0x2c/0x48 writes), 0x55af70..0x55b25e (dead facing arg).
Constants: 0x64315c = 0.99, 0x643160 = 0.9238795, 0x643164 = 0.38268343,
0x643168 = 0.70710677, 0x6254a0 = 0.0, 0x62544c = 0.5, 0x62588c = 1.0,
0x626a7c = 0.01, 0x675710 = 0x675714 = 0.0.
Data: Data/Design/Actions.bad (5 x 462 f32 after a u32 header of 5) -- offsets
0x0c+kind*8 forward/lateral, 0x1bc+kind*4 duration, 0x288+kind*4 scale,
0x14/0x1c walk/run stride factors.
