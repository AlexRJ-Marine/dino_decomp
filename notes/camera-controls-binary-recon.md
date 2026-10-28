# Retail park camera controls - binary recon

Binary: `DinoIsland.exe-ebea17` (PE32, MSVC 6, image base 0x00400000, MD5 5c835242...).
Every claim below is from the disassembly/decompiler in Ghidra 12.2; UI/data claims
from `Data/Config/*.ini` read-only.

This note **corrects** `notes/camera-motion-tilt-recon.md` and
`notes/camera-rotation-recon.md`. Corrections are listed in section 9.

---

## 1. Object graph and the live update chain

```
park frame  FUN_00524630(dt)                       (0x524f50 -> FUN_00527b00)
  FUN_00527b00(pvwParkView*, dt)                   park view per-frame
    if ([view+0x08] != 8) FUN_0052a650([view+0x08]) ; held NAV button -> action byte
    FUN_00529d00(observer, dt)                     pvwParkObserver::Update
      FUN_005e5960(camSwitcher, dt)                camSwitcher::Update
        FUN_005e5bc0(proxy, dt)                    proxy dispatch
          [proxy+0x20]->vtable[+8](dt)             (unused slot, always 0 in park)
          [proxy+0x24]->vtable[+8](dt)  == 0x005e5ff0   ACTIVE CAMERA UPDATE
        FUN_005e6930([S+0x34])                     clear the 8 action bytes
        FUN_005e5ce0(proxy)                        -> camera+0x39 (dirty)
        mgCamera::SetPosition([S], proxy+0x08)     FUN_00447580
        mgCamera::SetTarget  ([S], proxy+0x14)     FUN_004475b0
      ...observer extras (dead, section 7)...
```

Evidence:

* `0x00527b00`: `CMP dword [param_1+8],8` / `CALL 0x0052a650` / `CALL 0x00529d00`.
  Only caller: `FUN_00524630@0x524f50` (the park update; the second arg is the frame dt).
* `0x00529d1b  e8 40 bc 0b 00   CALL 0x005e5960` is the **only** xref to
  `camSwitcher::Update`.
* vtable `0x006481a4` = `20 5f 5e 00 | b0 5f 5e 00 | f0 5f 5e 00 | 35 fa 8e 3c`
  -> `[+0]=0x005e5f20` (read), `[+4]=0x005e5fb0` (write), `[+8]=0x005e5ff0` (Update),
  `[+0xc]=0x3c8efa35` (0.017453292 deg->rad constant, unused by Update).
  `0x005e5ff0` has exactly one xref: `DATA` from `0x006481ac` (the vtable slot).

Only 0x164-byte camera object exists in this build; it is created once by
`FUN_005e57c0` (camSwitcher ctor, `0x005e5848 CALL 0x005e5d10`) with
`alloc 0x164`; class-string xrefs for `camSwitcher.cpp` are only the ctor/dtor/camera ctor.

`camSwitcher+0x34` = pointer to 8 action bytes (alloc 8, filled with 0x94 by `0x005e6920`);
the camera object keeps that pointer at `+0xb4`, so `action[i] == *(byte*)([S+0x34]+i)`.

---

## 2. Config: `data\config\camera.ini`, section `Config1`

Ctor `0x005e5d10`: `CALL 0x005e5560` (defaults on the stack) ->
`PUSH 0x65e350 ("Config1")`, `PUSH 0x65e358 ("data\config\camera.ini")`, `0x005e5660`
(reader; calls `0x005e5690`) -> `0x005e5ee7 CALL 0x005e67f0` (apply to camera).

`FUN_005e67f0` (verified by disassembly, `0x005e67f0`..`0x005e68be`):

| config offset | camera field | value in shipped ini |
| --- | --- | --- |
| +0x64 pair | +0x15c progress speed | 0.05 / 0.2 |
| +0x6c,+0x70 | **+0x114 = min()** rotation speed | 1.570796 (scalar) |
| +0x74 pair | **+0x18** pan step (MoveSpeed) | 1 / 20 |
| +0x7c pair | +0x148 elevation pair | 3 / 500 |
| +0x84 pair | +0x150 distance pair | 10 / 300 |
| +0x8c pair | +0x9c, +0xa4 target hardness | 6 / 2 |
| +0x94 pair | +0x6c, +0x74 eye hardness | 6 / 2 |
| (immediate) | +0xac, +0x7c = 1000 / 1000 | Z smoothing = instant |
| (ctor) | +0x130/+0x138/+0x140 = 10 / 10 | yaw low-pass, not in ini |

`+0x114` is a *scalar*, loaded once: `FLD [EDI+0x6c]; FLD [EDI+0x70]; FCOMPP;` then
min -> `FSTP [ESI+0x114]` (`0x005e688f`..`0x005e68ac`).

Base ctor `0x005e6b40` initial values (before config):

* eye goal `(128,128,200)` then `+= (0,0,50)` -> `(128,128,250)`; cur := goal
  (`+0x60/+0x54`).
* target goal `(512,512,50)`; cur := goal (`+0x90/+0x84`).
* `+0xb8..+0xc4` rect = `(-250, 1274, -250, 1274)` via `0x005e72b0` (Xmin,Xmax,Ymin,Ymax).
* `+0x18/+0x1c` pan pair = (1,1) then overwritten by config.
* `+0x3c` = screen-position int* (see 8), `+0x4c` = freeze byte*, `+0x50` = altitude
  float, `+0xb4` = action bytes.

Derived ctor `0x005e5d10`: vtable `0x6481a4`, `+0x158 = 0x3dcccccd` = **0.1f**,
yaw smoother `+0x118..+0x12c` zeroed by `0x005e6940`, yaw pairs = 10.0 (six stores of
`0x40a00000`).

---

## 3. The action table (what each `action[i]` does)

`FUN_005e59b0` (`0x005e59b0`) is the only writer:
```
005e59b0 8b4134        MOV EAX,[ECX+0x34]
005e59b3 c6410c01      MOV byte [ECX+0x0c],1
005e59b7 8b4c2404      MOV ECX,[ESP+4]
005e59bb c6040801      MOV byte [EAX+ECX],1      ; action[idx] = 1
```
Flags are consumed and then cleared each frame (`0x005e5960` -> `0x005e6930`, two dwords).

| idx | consumer (`0x005e5ff0`) | effect | sign / dt / units / clamp |
| --- | --- | --- | --- |
| 0 | `0x005e6407`..`0x005e6468` | `target_goal.xy += dir0 * lerp(1,20,p)` | dir0 = normalize(target_cur.xy - eye_cur.xy) ("forward"); **no dt**; world units; goal box clamp |
| 1 | `0x005e6471`..`0x005e64d6` | same as 0 but **negated** (`FCHS` at `0x005e64a1`) | -dir0; no dt |
| 2 | `0x005e632f`..`0x005e6392` | `target_goal.xy += dir0 rotated +90 deg` (`0x005e7050`, cos/sin of +90) = (-dy,dx) = **left** | no dt |
| 3 | `0x005e639b`..`0x005e63fe` | `dir0` rotated -90 deg (`0x005e7130`) = (dy,-dx) = **right** | no dt |
| 4 | `0x005e64df`..`0x005e652d` | `yaw_goal.z += dt * [this+0x114]` (1.570796 rad/s) | +; dt **is** applied (`0x5e64fc FLD [ESP+0xb4]; FMUL [ESI+0x114]`); radians |
| 5 | `0x005e6531`..`0x005e6581` | `yaw_goal.z -= dt * [this+0x114]` (`FCHS` at `0x5e6570`) | - |
| 6 | `0x005e65cb`..`0x005e660b` | `progress -= dt * lerp(progressPair,p)` (`FSUBR` at `0x5e6605`) | **zoom IN / lower** |
| 7 | `0x005e6585`..`0x005e65c5` | `progress += dt * lerp(progressPair,p)` | **zoom OUT / higher** |

Extra conditions for 0..3: each applies if `action[i]` **or** (mouse-edge test AND
`!uiMode`); the edge test is `x<2` (idx 2), `x>w-2` (idx 3), `y<2` (idx 0), `y>h-2` (idx 1)
from two ints at `[camera+0x3c]` and `[0x00674528]+0x31ce4/0x31ce8` (screen size).
See section 8.

Also: if any of 0..7 fired (`+0x3a`), the follow provider is dropped:
`0x005e6618 MOV dword [ESI+4],0`.

`+0x3a` is set by every pan/rotate/zoom branch (`MOV byte [ESI+0x3a],1`).

### Derived camera math (same function, order of operations)

1. follow (`+0x4`) or pending `SetTargetXY` (`+0x40`,`+0x44`) can overwrite the target goal.
2. target Z snap: `goal.z += max(terrain(target_cur.xy), +0x50 altitude) - target_cur.z`
   (`0x005e60ae`..`0x005e6109`, terrain = `0x005e6ea0`, `ushort*1/256`).
3. eye goal:
   ```
   base  = (clamp(goal.x, +0xb8, +0xbc), clamp(goal.y, +0xc0, +0xc4), goal.z)
   eye   = base + (cos(yaw_cur), sin(yaw_cur), 0) * lerp(10,300,p)
   eye.z = max(goal.z, terrain(eye.xy), +0x50 altitude) + lerp(3,500,p)
   ```
   (yaw block `0x005e610e`..`0x005e61ab`; clamps `0x005e61b1`/`0x005e6206` with
   `0x005e68d0`; eye.z max chain `0x005e6271`..`0x005e62c5`; `FADD [ESI+0x50]` at `0x005e62af`.)
4. pan (idx 0..3), yaw (4/5), progress (6/7), then `clamp(progress,0,1)`
   (`0x006254a0` = 0.0f, `0x0062588c` = 1.0f).
5. smoothing `0x005e6960`: `k = clamp(dt * lerp(pair,p),0,1); cur += (goal-cur)*k`
   per axis. yaw pair (10,10); eye/target X,Y pairs lerp(6,2,p); Z pair (1000,1000) => k=1.
6. output: `[this+0x20..0x28] = eye_cur`, `[this+0x2c..0x34] = target_cur`
   (`0x005e6772`..`0x005e679a`).

Note the pan/look coupling is rigid: eye goal = target goal + constant W, and eye/target
use the same k, so the view direction does not change while panning.

Pitch is not a state: `pitch = atan2(3+497p, 10+290p)`. No roll: the park mgCamera's up
vector is set once to (0,0,1) at `0x0052785a` and never rewritten (no up write in
`0x005e5ff0`).

---

## 4. Key / UI -> index (verified, not from the old note)

`FUN_005257a0` registers the handlers; the lambda bodies are at `0x00525d00..0x00525e7b`
and contain the literal index (`PUSH 0..7`):

| action | lambda | call | index |
| --- | --- | --- | --- |
| CameraKeyUp | 0x525e00 | `0x00527b80` | 0 |
| CameraKeyDown | 0x525e10 | 0x527b80 | 1 |
| CameraKeyLeft | 0x525e20 | 0x527b80 | 2 |
| CameraKeyRight | 0x525e30 | 0x527b80 | 3 |
| CameraRotateLeft | 0x525e40 | 0x527b80 | 4 |
| CameraRotateRight | 0x525e50 | 0x527b80 | 5 |
| CameraRotateUp | 0x525e60 | 0x527b80 | 6 |
| CameraRotateDown | 0x525e70 | 0x527b80 | 7 |
| NAVIGATION_GOTOP | 0x525d00 | `0x00527b60` | 0 |
| NAVIGATION_GOBOTTOM | 0x525d20 | 0x527b60 | 1 |
| NAVIGATION_GOLEFT | 0x525d40 | 0x527b60 | 2 |
| NAVIGATION_GORIGHT | 0x525d60 | 0x527b60 | 3 |
| NAVIGATION_TURNLEFT | 0x525d80 | 0x527b60 | 4 |
| NAVIGATION_TURNRIGHT | 0x525da0 | 0x527b60 | 5 |
| NAVIGATION_ZOOM | 0x525dc0 | 0x527b60 | 6 |
| NAVIGATION_DEZOOM | 0x525de0 | 0x527b60 | 7 |

* Keyboard: `0x00527b80` gates on (a) `[0x6744e0]+0x5c == 0`, (b) `[view+0x2c]+0x18 == 0`,
  (c) `[view+0x08] == 8` ("no nav button held"), then `0x0052a650` -> `0x005e59b0(idx)`.
* UI: `0x00527b60` stores the index in `[view+0x08]`; key-up handler `0x00527b70` writes 8.
  `0x00527b00` converts the held index to an action byte **every frame**, so a held UI
  button is continuous, while a keyboard event is one latch (one pan step per event).

Measured key effects (default yaw 0, eye at +X of target, view direction -X):

| key | idx | net effect |
| --- | --- | --- |
| Up | 0 | target moves along (target-eye) = the view direction (view slides forward) |
| Down | 1 | opposite |
| Left | 2 | target moves to screen left (-Y at yaw 0) |
| Right | 3 | screen right |
| Shift+Left (or Del) | 4 | yaw += 90 deg/s (orbit left; view turns left) |
| Shift+Right (PgDn) | 5 | yaw -= 90 deg/s |
| Shift+Up (Home) | 6 | progress **decreases** -> zoom in, camera lower (pitch 53.5 -> 16.7 deg) |
| Shift+Down (End) | 7 | progress increases -> zoom out, camera higher |

`Data/Config/KeyMapper.ini` confirms the bindings (Up/Down/Left/Right = `Fl?cheHaut` etc.,
Shift variants + Delete/PageDown/Home/End). No mouse or wheel entries exist in the file
(0 matches for MOUSE/WHEEL/Souris/Molette).

---

## 5. Start state (park view creation)

`FUN_00527650` (park view ctor, `0x00527650`):

* allocates the render camera object, `0x52785a` sets `mgCamera::SetUp(0,0,1)` on
  `[view+0x10]+0x70`; initial pose `SetPosition(200,200,140)` `0x527832`-region and
  `SetTarget(200,600,0)` `0x5277f7`-region. These are **overwritten on the first update**.
* `0x0052786d MOV dword [view+8],8` (no nav button held).
* `0x0052789d CALL 0x00529c50` creates the observer at a fresh 0x60 allocation with
  arg = `[view+0x10]+0x70` (mgCamera record) and `FUN_005b9070()`.
* observer float block at `+0x28`: `40, 1.5708, -0.7854, 10000, 2, 10, 10, 1000, 1000, 3`
  (rep movsd from `0x00527905`) - its own rotation speed and its own clamp box
  `+0x3c..+0x48 = (10,10,1000,1000)`, height band `+0x34/+0x38 = 10000/2`.
* `0x00527911 MOV [renderTarget+0x20], camera`.

The camera object is created by the observer ctor `0x00529c50` ->
`FUN_005e57c0(mgCamera, 0.0f)` -> `FUN_005e5d10`, so:

| state | value at park start | who sets it |
| --- | --- | --- |
| progress p | **0.1** | `0x005e5d5d MOV [EDI+0x158],0x3dcccccd` |
| yaw cur / goal | **0 / 0** | `0x005e5d4a CALL 0x005e6940` (zeroes 6 dwords) |
| target cur / goal | **(512,512,50)** | `0x005e6c2c` (rep of 3 dwords), `0x005e6c44` |
| eye cur / goal | **(128,128,250)** | `0x005e6c08` (200) then `+50` at `0x005e6c60` |
| altitude `+0x50` | 0.0 at ctor; later the park calls `FUN_0052a680([[[park+0x12c]+0xf4]+0x38])` from `FUN_00524570@0x5245aa` | |
| heightfield `+0x108` | 0 until `FUN_0052a660` from `0x524589` | |
| map box `+0xb8..+0xc4` | (250..1274) x (250..1274) | base ctor, never overwritten |

No code sets the target before the first frame: the only `SetTargetXY` path is
`0x00527bd0` <- `FUN_00524630@0x5250d8` (gameplay "look here"), not the view ctor.

First update result (flat ground, terrain(target)=32, altitude <= 32):

```
p = 0.1           r = 10+290*0.1 = 39          h = 3+497*0.1 = 52.7
target = (512,512,32)
eye    = (512+39, 512, 52.7+32) = (551,512,84.7)
```

Those are exactly the numbers in the Free Game `.sav` camera tail
(eye 551,512,84.7 / target 512,512,32) - independent confirmation of the model and of
"the park does not override the ctor target".

Transient: the eye starts at (128,128,250) and is low-passed to the goal with
`k = clamp(dt*lerp(6,2,0.1)) = dt*5.6` (Z instant), i.e. roughly a 0.4-0.6 s fly-in at
30-60 Hz. The target XY already equals its goal, so only the eye moves.

---

## 6. `0x005e5ff0` head/tail details worth knowing

```
005e5ff9 8b464c        MOV EAX,[ESI+0x4c]      ; pointer to camSwitcher+0xd
005e5ffc 803800        CMP byte [EAX],0
005e5fff 0f85dd070000  JNZ 0x005e67e2          ; frozen -> early out, +0x39=1
```
`camSwitcher+0xd` is only ever written by the camSwitcher ctor (`0x005e57dc`, = 0), so the
camera always updates in this build.

Epilogue `0x005e67b4`..`0x005e67e9`: `+0x39 = 0 if (byte+0xe4 && byte+0x104) else 1`,
where `+0xe4`/`+0x104` are the "unchanged" bytes of two 0.1-threshold movement detectors
(`0x005e7210`/`0x005e7240`). `+0x39` is only a redraw hint: `0x005e5ce0` returns it,
`0x005e5960` ORs it into `S+0xc`, the observer copies `S+0xc` into `observer+0x21`, and the
park view turns `+0x21` into a global "repaint" byte. No motion consequence.

---

## 7. `pvwParkObserver::Update` is live, but its own input block is dead

`0x00529d00` is live: it is called every park frame (section 1) and its first action is
`CALL 0x005e5960` - the camSwitcher update that actually drives the camera. It is not
inert. What *is* inert is its extra movement block:

* It reads four bytes at `observer+0x1d, +0x1e, +0x1f, +0x20` (`0x00529db5`, `0x00529e45`,
  `0x00529ed5`, `0x00529fd7`, and again at `0x0052a1ca..0x0052a1e3`) and, if set, rotates
  the (eye-target) XY delta by `+/-dt*[observer+0x2c]` (`1.5708`) and adds it to a **local**
  target vector. That local is only used to recompute the eye's ground height and the near
  plane; the observer never calls `SetPosition`/`SetTarget` on the mgCamera
  (no call to `0x00447580`/`0x004475b0` inside `0x00529d00` - only Get* + `SetProjection`).
* **Store census** over the whole park-view module `0x00527650..0x0052a700` (linear sweep,
  every instruction): the only writes to `+0x1d..+0x20` are
  `0x00529c7b MOV [ESI+0x1d],EDX` (ctor, EDX=0) and
  `0x0052a59e MOV [ESI+0x1d],EDX` (per-frame clear, EDX=0). No `=1` store anywhere in the
  module. The previous note's "no writer" claim is **confirmed** (scope: this module, which
  is the only code that holds the observer pointer; the pointer is not stored in a global).

So: the pan/rotate deltas of the observer never fire; the observer is a pass-through that
sets the projection (`0x0052a3f8 FUN_00447610(fov 1.3962634, aspect 1.3333334,
near = max(1, 0.1*(eye.z - ground)), far = 1000)`) and a view/frustum struct, then clears
`+0x19/+0x1d`.

---

## 8. Mouse

* `KeyMapper.ini` has no mouse/wheel bindings.
* The only mouse-dependent code inside the camera is the **edge-scroll** branch of
  `0x005e5ff0` (section 3). It reads two ints through `[camera+0x3c]`, which the camSwitcher
  ctor sets to `&camSwitcher+0x4` (`0x005e583c LEA ECX,[ESI+4]` -> derived ctor arg2 ->
  base ctor `+0x3c` at `0x005e6b6e`), and compares them with `[0x00674528]+0x31ce4/0x31ce8`
  (screen width/height).
* The gate: `0x005e6317 MOV CL,[EAX+0x36]` / `0x005e631e MOV CL,[EAX+0x2a]` where
  `EAX = [0x006750f0]`; `uiMode = (byte[+0x36] != 0 || byte[+0x2a] != 0)`; the edge pan
  fires only when `uiMode == 0`.
* **No writer of `camSwitcher+4/+8` exists** in the camSwitcher translation unit (verified:
  the ctor/dtor/API list, and the class-string xrefs give the full TU), and the park-view
  module never stores through `[view+0xc]+0x5c`. So the edge ints are never populated in
  this build and the branch is effectively dead (with uninitialised values it would also be
  nonsense). Treat retail edge-scroll as **not active**.
* Consequence for the port: retail has **no mouse-drag and no wheel zoom** for the park
  camera; the port's mouse-drag convenience is port-only (it is not "equivalent to" retail).

Runtime check if this is ever challenged: `bp 0x005e6335` (reads `action[2]`) and dump
`[ecx+0x3c]` (two ints), `[[0x006750f0]+0x2a]`, `[[0x006750f0]+0x36]`.

---

## 9. Corrections to the existing notes

`notes/camera-motion-tilt-recon.md`:

1. **`+0xb8..+0xc4` are not an "eye height clamp".** The base ctor initialises them to
   `(-250, 1274, -250, 1274)` (`0x005e6ba0`.., `0x005e6bc0 CALL 0x005e72b0`) and they are
   used as the target **X/Y bounds** (`clamp(goal.x, +0xb8, +0xbc)` at `0x005e6206`,
   `clamp(goal.y, +0xc0, +0xc4)` at `0x005e61b1`). There is no eye-height clamp.
2. **The eye Z is not `target.z + h`.** It is
   `h + max(target_goal.z, terrain(eye_xy), +0x50 altitude)` (`0x005e6271`..`0x005e62c5`).
   The extra `FADD [ESI+0x50]` at `0x005e62af` is inside that max, not an offset on the eye.
3. **Yaw field offsets**: cur is `+0x118..0x120`, goal is `+0x124..0x12c`. The table entry
   `+0x118/+0x12c yaw current / yaw goal` is off by one dword on the goal; `+0x12c` is
   `goal.z`, which is the only component used.
4. The `+0x1d..+0x20` "no writer" claim is **correct** (now proven by store census, section 7),
   and the claim that those flags are "pan" is wrong - they are orbit (rotate) deltas, and
   they never reach the camera anyway.
5. Pan mapping: idx 2 is `+90 deg` (`(-dy,dx)`, left) via `0x005e7050`; idx 3 is `-90 deg`
   (`(dy,-dx)`, right) via `0x005e7130`; idx 1 is idx 0 negated, not "that vector rotated".
   The note's prose is right, its pseudo-code order (flags0/1 then 2/3) is fine.
6. `FUN_005e6960` is `k_i = clamp(dt*lerp(pair_i,p),0,1)`; confirmed, and the "pair" is
   `(min,max)` with `p` as the blend factor.
7. Pan has **no dt** (confirmed) and is **one step per accepted action**, i.e. per key event
   (keyboard) or per frame (held UI button). It is therefore frame-rate dependent; the
   port's `*4 units/s` is not a recovered constant.

`notes/camera-rotation-recon.md`:

1. **`NAVIGATION_ZOOM`/`DEZOOM` are not separate dolly controls** - they are indices 6/7,
   exactly `CameraRotateUp`/`CameraRotateDown`. There is one dolly/tilt axis (`progress`).
2. `FUN_00529d00` is **live** (called from `0x00527b00` every frame) but its `+0x1d..+0x20`
   block is dead, and it does **not** "move the target and apply the terrain-height clamp"
   to the camera - its target delta is local and discarded.
3. Shift+Up (idx 6) **decreases** progress (zoom in / lower), Shift+Down increases it.
4. The rotation speed is a scalar `+0x114 = min(RotationSpeedMin,Max) = 1.570796`, not a pair.

---

## 10. Port contract

State (per park camera):

```
float yaw_cur, yaw_goal;      // radians, angle of the eye around the target from +X
float progress;               // [0,1], init 0.1
vec2  target_cur, target_goal;
float target_cur_z, target_goal_z;
vec2  eye_cur; float eye_cur_z;
float altitude;               // +0x50, park value; may be 0
bool  action[8];              // latched, cleared after each camera update
```

Per frame, with `dt`, `p = progress`:

```
step   = 1 + 19*p;                       // MoveSpeed lerp(1,20,p)
rot    = 1.570796f;                      // +0x114
pspeed = 0.05f + 0.15f*p;                // lerp(0.05,0.2,p)
el     = 3 + 497*p;
dist   = 10 + 290*p;
hard   = 6 - 4*p;                        // lerp(6,2,p)

// 1. target Z snap (uses CURRENT target xy)
ground = terrain(target_cur);
target_goal_z += max(ground, altitude) - target_cur_z;

// 2. eye goal from the (clamped) goal
gx = clamp(target_goal.x, -250, 1274);
gy = clamp(target_goal.y, -250, 1274);
target_goal = (gx, gy);                       // clamp writes back
eye_goal = (gx + cos(yaw_cur)*dist, gy + sin(yaw_cur)*dist);
eye_goal_z = max(target_goal_z, terrain(eye_goal), altitude) + el;

// 3. input
fwd = normalize(target_cur - eye_cur);        // 2D; (0,0) if degenerate
if (a[0]) target_goal += fwd*step;
if (a[1]) target_goal -= fwd*step;
if (a[2]) target_goal += vec2(-fwd.y,  fwd.x)*step;
if (a[3]) target_goal += vec2( fwd.y, -fwd.x)*step;
if (a[4]) yaw_goal += dt*rot;
if (a[5]) yaw_goal -= dt*rot;
if (a[6]) progress -= dt*pspeed;
if (a[7]) progress += dt*pspeed;
progress = clamp(progress, 0, 1);
if (any a[0..7]) follow = none;
clear a[0..7];

// 4. smoothing (same k for eye and target X/Y => rigid pan)
kyaw = clamp(dt*10.0f, 0, 1);
kxy  = clamp(dt*hard, 0, 1);
yaw_cur    += (yaw_goal - yaw_cur)*kyaw;
target_cur += (target_goal - target_cur)*vec2(kxy,kxy);
target_cur_z = target_goal_z;                 // Z pair is (1000,1000) => instant
eye_cur    += (eye_goal.xy - eye_cur)*kxy;
eye_cur_z   = eye_goal_z;                     // instant

out_eye = (eye_cur, eye_cur_z); out_target = (target_cur, target_cur_z);
```

Test assertions (deterministic, terrain(height)=32 on the flat test park):

1. Start: `progress == 0.1`, `yaw == 0`, `target == (512,512,32)`,
   `eye == (551,512,84.7)` after convergence; `r == 39`, `h == 52.7`.
2. `up` vector is `(0,0,1)` on every frame and `roll == 0`.
3. One plain-Left action (single latch, yaw 0): `target_goal.y -= 2.9` exactly
   (`step = lerp(1,20,0.1) = 2.9`, `fwd = (-1,0)`, left = `(-fwd.y,fwd.x) = (0,-1)`),
   no dt factor: two actions in one frame are one step (the latch), two frames = 5.8.
4. `pitch` is constant while panning on flat ground (`atan2(h,dist)` unchanged).
5. Shift+Left 1.0 s at 60 Hz: `yaw_goal += 1.5708` rad; `yaw_cur` lags by ~9 deg
   (`omega/lambda = 1.5708/10`).
6. Shift+Up 1.0 s from p=0.1: `progress -= integral(0.05+0.15p)dt` (~0.0565 rad/s at start)
   and `dist` decreases; Shift+Down increases both.
7. Holding NAVIGATION_ZOOM applies the same `progress` delta **per frame**, not per event.
8. No wheel/right-drag changes any of these states.

If a constant is unclear, watch these (do not guess):
`camera+0x114` (rotation speed), `+0x18/+0x1c` (pan pair), `+0x150` (distance pair),
`+0x148` (elevation pair), `+0x15c` (progress speed pair), `+0x158` (progress),
`+0x124` (yaw goal), `+0xb8..+0xc4` (map box), `+0x50` (altitude):

```
bp 0x005e5ff0
log "p={d:[ecx+0x158]} yaw={d:[ecx+0x120]} yawg={d:[ecx+0x124]} tgt={d:[ecx+0x90]},{d:[ecx+0x94]},{d:[ecx+0x98]} eye={d:[ecx+0x20]},{d:[ecx+0x24]},{d:[ecx+0x28]}"
```

## 11. Uncertainty

* `0x005e5ff0` is not a Ghidra function (only reachable through vtable slot
  `0x006481ac`); the stack offsets above come from a hand-verified stack simulation of the
  whole function (52 pushes, 36 calls, epilogue balance checked). Two facts are certain
  regardless of that bookkeeping: the flags' consumers (`0x005e632f`..`0x005e660b`) and the
  constants loaded (`+0x18/+0x1c/+0x150/+0x148/+0x15c/+0x114`).
* `0x005e5560` (the ini-less defaults) was not enumerated constant by constant; the shipped
  `data\config\camera.ini` exists and its `[Config1]` values are exactly what `0x005e67f0`
  copies, so the defaults are not load-bearing for retail.
* The first-frame fly-in from (128,128,250) is what the ctor + smoothing implies; it was not
  observed at runtime. If the port must match the very first frames, record
  `0x005e6772`/`0x005e677a` for 30 frames at park start.
* Edge-scroll: the code exists but has no input producer and no reachable writer for
  `camSwitcher+4/+8`; the exact value of `[0x006750f0]+0x2a/+0x36` in the park was not
  traced (see the breakpoint in section 8).


## 12. Runtime verification (2026-10-28) — supersedes parts of §7 and §10

> **Update (same day):** the "no mouse" item 5 and the §8/§11 edge-scroll
> conclusion are wrong. `notes/camera-mouse-tilt-recon.md` proves the live
> edge scroll (`0x527b00` mouse callback → `0x52a690` → `camSwitcher+4/+8`),
> the 2 px bands and the pre-pan eye goal that produces the tilt. The rest of
> §12 (5/s yaw, +0x120/+0x12c, per-update pan, fly-in) stands.

A live probe was added to the DINPUT8 proxy (`DINO_CAMERA_PROBE=1`): it patches
vtable `0x6481a4` slot 2 (`0x5e5ff0`) and samples the real object each update
into `run-test/dino_camera.csv`. Driver: `scripts/x32dbg/camera-probe.ps1`
(free-game park, virtual keyboard via `dino_keys.txt`). Results:

| Claim | Runtime result |
| --- | --- |
| ctor target/eye | target `(512,512,32)` after the first Z glue; eye goal `(551,512,84.7)`; p=0.099997, yaw 0 |
| first-frame fly-in | eye X `128 -> 177.7449` at dt=0.021: `k = 0.1176 = dt*5.6` (Target/PositionHardness 6..2 at p=0.1) |
| pan step | exactly `-2.9` per update at p=0.1 (`lerp(1,20,p)`), dt varied 0.001..0.015 with no effect |
| map clamp | target goal stops at record `-250` (isl01 box `-250..1274`) |
| yaw | `+dt*1.570796`; measured `k/dt = 5.0` from consecutive samples |
| yaw lag | 0.293 rad (16.8 deg) at 90 deg/s, i.e. `a*(1-k)/k` with k=dt*5 |
| zoom | flag 6: `dp/dt = -0.065 = -lerp(0.05,0.2,p)` at p=0.1 |
| eye Z | `h + max(target_goal.z, terrain(eye_xy), 16)`; target goal/current Z equal every frame |

Corrections to this note:

1. **Yaw field offsets.** Yaw current is the z component at **`+0x120`** and
   yaw goal the z at **`+0x12c`** (vec3s at +0x118..+0x124 and +0x124..+0x130).
   `+0x124` is the x component of the goal vec3 and stayed 0.000000 through
   every rotation, so the §7 "goal is +0x124..+0x12c" wording and the §10 watch
   `yawg={d:[ecx+0x124]}` are wrong; use `[ecx+0x120]` / `[ecx+0x12c]`.
2. **Yaw smoothing is 5.0/s, not 10/s.** The ctor at `0x5e5d67` stores
   `0x40a00000` (= 5.0) six times for the three yaw pairs. The measured filter
   coefficient is `k/dt = 5.0` on both rotation segments. Consequently the
   steady yaw lag at 90 deg/s is ~16.8 deg, not ~9 deg; §10 item 5 and the
   "10/s" wording elsewhere in this note are superseded.
3. **Pan is per Update**, not per input event: every Update while the flag is
   set adds `lerp(1,20,p)` (2.9 at p=0.1). The harness runs the game unpaced in
   the background (75..1000 updates/s), so the CSV gives per-update steps, not
   per-second speed. The port runs the camera on a fixed 60 Hz tick (--camera-hz,
   10..240), so an uncapped render loop keeps the 60 Hz pan speed.
4. **The fly-in is real** (was §11 uncertainty): the first frame starts from
   eye `(128,128,250)` and the 6..2 filter brings it in.
5. **No mouse**: no mouse movement or wheel changed any field in the probe;
   this confirms KeyMapper.ini has no mouse camera entries.

Port contract implemented in `src/viewer/main.cpp` (`RetailParkCamera`):
start p=0.1/yaw 0/target (0,0,terrain)/eye (128-half,128-half,250);
per-update pan; dt-scaled yaw (5/s low-pass) and progress; XY filters
6..2 with instant Z; eye = target_goal + yaw offset * distance(p), height
`h + max(target_goal.z, terrain(eye), 16)`; no roll. `--camera-orbit` keeps
the legacy orbit; `--camera-selftest` asserts the four measured equations.
