# Retail park camera: mouse control and the "tilt"

Binary: `DinoIsland.exe-ebea17` (PE32, MSVC 6, image base 0x00400000).
Everything below is disassembly/xrefs/store-census + the retail `.ini`/`.lnd` data.
This note **corrects** `notes/camera-controls-binary-recon.md` §8 ("edge scroll is
unwired") and §11.

Status: **mouse path solved (edge scroll + minimap click); the tilt is pitch, not
roll.** Two claims in the previous note were wrong; they are listed in §6.

---

## 1. Summary

* The park camera is moved by the mouse in two ways:
  1. **Edge scroll**: the cursor's *client pixel* position is written into the
     camera (`camSwitcher+4/+8`) on every mouse move; `0x005e5ff0` tests the four
     2-pixel edge bands and ORs the matching pan action (indices 0..3) into the
     normal action flags. It is per update, no dt, same step as the keyboard.
  2. **Minimap click**: a click on the `MAP_RENDERVIEW` widget sets a pending
     "put the target here" request; the park update calls the camera's
     `SetTargetXY`, which snaps the target goal (and its ground height).
* The edge scroll is suppressed while **Shift** is held. `[0x006750f0]+0x2a/+0x36`
  is *not* a UI mode: it is the DirectInput keyboard state array at scan codes
  `0x2A` (LSHIFT) and `0x36` (RSHIFT).
* There is **no roll and no up-vector change anywhere** in the park view. The
  visible "tilt" is **pitch**, and it has three sources (all quantified in §5):
  the terrain under the eye, the one-update eye/target lag created by the
  order of operations, and the yaw/orbit lag. The largest is the terrain term:
  measured up to **+6.8 deg** at the isl01 entrance, **~0 deg** on the flat
  free-game start plateau.

---

## 2. The mouse chain (addresses + bytes)

### 2.1 Where the position is written (the missing writer)

`FUN_0052a690(observer, int2* p)`:
```
0052a690 8b442404   MOV EAX,[ESP+4]
0052a694 8b495c     MOV ECX,[ECX+0x5c]      ; camSwitcher
0052a697 8b10       MOV EDX,[EAX]           ; x
0052a699 895104     MOV [ECX+4],EDX         ; camSwitcher+4  (camera reads it via +0x3c)
0052a69c 8b4004     MOV EAX,[EAX+4]         ; y
0052a69f 894108     MOV [ECX+8],EAX         ; camSwitcher+8
```
Its only xrefs are `0x527bc0` (park API wrapper) and **`0x5279b0`**.

`0x00527970` is the park view's `RENDERVIEW` mouse-move callback (installed by
`FUN_00527650` at `[element+0x60]+0x18`, see 0x5278xx):
```
00527970 83ec08        SUB ESP,8
00527973 8b44240c      MOV EAX,[ESP+0xc]        ; widget
00527978 8b742414      MOV ESI,[ESP+0x14]       ; int2* point
0052797d 8b785c        MOV EDI,[EAX+0x5c]       ; -> park view P
00527980 8b4e04        MOV ECX,[ESI+4]          ; y
00527986 8b4f20        MOV ECX,[EDI+0x20]       ; render target
0052798a e821de0c00    CALL 0x005f57b0          ; [[rt+0x20]+0x20]+0x18/1c = x,y (mcLib cursor)
0052798f 8b4724        MOV EAX,[EDI+0x24]
00527998 ff5210        CALL [EDX+0x10]          ; forward the point to the view input
0052799b 8b4e04        MOV ECX,[ESI+4]
005279a0 8d542408      LEA EDX,[ESP+8]
005279a8 8b4f0c        MOV ECX,[EDI+0xc]        ; <<< pvwParkObserver
005279b0 e8db2c0000    CALL 0x0052a690          ; observer->SetScreenPos(&pt)
```
`0x00527bc0` is the same setter exposed by the park view (`[ECX+0xc]` then
`0x0052a690`), i.e. a second entry point, not a second writer.

**Store census (whole park module `0x00524000..0x0052c000`, 8000 instructions):**
the only stores of the form `MOV [reg+4]/[reg+8]` reached from a `[reg+0x5c]`
load are the two in `0x0052a690`. No direct store through the camera's `+0x3c`
pointer exists in this module, and the camSwitcher TU (`camSwitcher.cpp`, xrefs
of the class string `0x0066ff30`) only constructs/uses the object, never fills
`+4/+8`.

So the previous "no writer" conclusion was wrong: the writer is
`FUN_0052a690`, reached only from the RENDERVIEW mouse-move callback.

### 2.2 Which object the port was hooking

The vtable the proxy patches, `0x00642894`, is **not** the park observer. It is
the **ViewManager** (`FUN_00517390` ctor, allocated 0xC0 by `FUN_00596580` from
`VIEW\Src\ViewManager.cpp`, published in `DAT_00675a88`):

| slot | address | role (from the window-message dispatcher) |
| ---: | --- | --- |
| 6 | `0x005bb540` | `WM_MOUSEWHEEL` |
| 7 | `0x005bb4d0` | `WM_MOUSEMOVE` (0x200) -> `[vtbl+0x1c]` at `0x596199` |
| 10 | `0x005bb640` | `WM_LBUTTONDOWN` (0x201) -> `[vtbl+0x28]` at `0x59620a` |
| 11 | `0x005bb600` | `WM_LBUTTONUP` (0x202) -> `[vtbl+0x2c]` at `0x596269` |
| 12/13 | `0x005bb6c0/0x005bb6e0` | `WM_CHAR`/`WM_KEYDOWN` |

`ViewManager::OnMouseMove` (`0x005bb4d0`) only keeps its own cursor state and
forwards to the input manager:
```
005bb4d0 8b4118      MOV EAX,[ECX+0x18]        ; last x
005bb4d7 83f8ff      CMP EAX,-1                ; first move?
005bb504 8bfa        MOV EDI,EDX               ; x
005bb508 2bf8        SUB EDI,EAX               ; dx
005bb50e 897920      MOV [ECX+0x20],EDI        ; +0x20 = dx
005bb521 e89a98e5ff  CALL 0x00414dc0          ; input manager
005bb529 e8120a0000  CALL 0x005bbf40          ; inputMgr->vtbl[0x40](&pos)
```
It never touches the camera.

The park observer (`0x00529c50`) has no vtable at all in the mouse path: its
`+0x5c` field is the camSwitcher, and `0x00529d00` only *reads* the camSwitcher.

The park view object `P` (`FUN_00527650`, vtable `0x00642b98`) is reached from
the widget through `[widget+0x5c]`; `P+0xc` = observer, `P+0x20` = render target,
`P+0x24` = view input sink. The 5 callbacks installed on the RENDERVIEW element
are `0x00527960` (frame), **`0x00527970` (mouse move)**, `0x005279c0`,
`0x005279f0` (buttons), `0x00527a20` (command id 0xa).

### 2.3 The edge-scroll block in `0x005e5ff0`

```
005e62d4 8b463c        MOV EAX,[ESI+0x3c]       ; &camSwitcher+4  (ctor arg2)
005e62dd 8b08          MOV ECX,[EAX]            ; x (client px)
005e62df 8baae41c0300  MOV EBP,[EDX+0x31ce4]     ; EDX = [0x00674528]; screen width
005e62e5 8b4004        MOV EAX,[EAX+4]          ; y
005e62e8 83f902        CMP ECX,2
005e62eb 0f9c44241a    SETL [ESP+0x1a]          ; left   (x < 2)
005e62f0 83ed02        SUB EBP,2
005e62f3 3bcd          CMP ECX,EBP
005e62f5 8b8ae81c0300  MOV ECX,[EDX+0x31ce8]     ; screen height
005e62fb 0f9f44241b    SETG [ESP+0x1b]          ; right  (x > w-2)
005e6300 83f802        CMP EAX,2
005e6303 0f9c442419    SETL [ESP+0x19]          ; top    (y < 2)
005e6308 83e902        SUB ECX,2
005e630b 3bc1          CMP EAX,ECX
005e630d a1f0506700    MOV EAX,[0x006750f0]
005e6312 0f9f442418    SETG [ESP+0x18]          ; bottom (y > h-2)
005e6317 8a4836        MOV CL,[EAX+0x36]        ; DIK_RSHIFT
005e631a 84c9          TEST CL,CL
005e631c 750c          JNZ 0x005e632a
005e631e 8a482a        MOV CL,[EAX+0x2a]        ; DIK_LSHIFT
005e6321 c644241300    MOV byte [ESP+0x13],0
005e6326 84c9          TEST CL,CL
005e6328 7405          JZ 0x005e632f
005e632a c644241301    MOV byte [ESP+0x13],1    ; shiftDown
```
then each of the four pan blocks is
`if (action[i] != 0 || (edge_i != 0 && !shiftDown)) { step = lerp(1,20,p); target_goal.xy += dir_i * step; }`

| block | tested flag | edge term | direction helper |
| --- | --- | --- | --- |
| `0x005e632f` | `action[2]` | `x < 2` | `0x005e7050`, `fwd` rotated +90 deg (left) |
| `0x005e639b` | `action[3]` | `x > w-2` | `0x005e7130`, `fwd` rotated -90 deg (right) |
| `0x005e6407` | `action[0]` | `y < 2` | `0x005e6fc0`, `fwd` (view direction) |
| `0x005e6471` | `action[1]` | `y > h-2` | `0x005e6fc0` + `FCHS` (backwards) |

where `fwd = normalize(target_cur.xy - eye_cur.xy)` (`0x005e6fc0`, current values,
not goals).

### 2.4 What `[0x006750f0]` really is (correction)

`FUN_004456c0(inputMgr)` recomputes it every frame:
```c
DAT_006750f0 = base + slot*0x100 + 0x31;   // current  256-byte key array
DAT_006750fc = base + 0x131 - slot*0x100;   // previous 256-byte key array
slot = 1 - slot;
```
so `[0x6750f0 + i]` is the **DirectInput keyboard scan-code state for code `i`**
of the current frame. Cross-check: `FUN_00445af0` reads `+0x57`, `+1` (DIK_ESCAPE)
and `+0x1c` (DIK_RETURN) as keys, and `FUN_00418b00(0)` returns 0 when
`+0x2a || +0x36` (the shift modifier test). So:

* `+0x2a` = `DIK_LSHIFT` (42), `+0x36` = `DIK_RSHIFT` (54).
* The camera's gate is "**neither Shift is down**".

This is the opposite of the previous note's "uiMode" reading: it is not a UI
modal flag, it is the rotate modifier. Design intent: while Shift is held the
arrows rotate; the mouse edge must not pan at the same time.

### 2.5 Rate, deadzone, coordinates, activation

* Coordinates: **client pixels** taken from `WM_MOUSEMOVE`'s `lParam`
  (`0x59614a AND EAX,0xffff` = x, `0x596151 SHR ECX,0x10` = y), i.e. not a ray,
  not relative deltas, not DirectInput.
* Deadzone/edge band: **2 px**, hard (no speed ramp, no depth scaling).
* Speed: exactly the keyboard pan step, `lerp(1,20,p)` world units **per camera
  update**, no `dt`. At p=0.1 that is 2.9 units/update; at a 60 Hz tick 174
  units/s, at p=1 20 units/update (1200 units/s at 60 Hz).
* It is continuous: the position persists between moves, so parking the cursor
  inside the 2-px band keeps panning every update until the next mouse move
  moves it out.
* Interaction with keys: the edge test is OR-ed into the same `action[0..3]`
  flags, and it also sets `+0x3a` ("moved"), which clears the follow target
  (`0x005e6618 MOV dword [ESI+4],0`). It never rotates and never writes the
  target directly.
* Activation: there is **no activation/focus check in the camera**. The position
  only updates when the window receives `WM_MOUSEMOVE` (the park's widget
  callback), so an unfocused/covered window keeps the last position - and
  therefore keeps scrolling if it was left in an edge band. The port must not
  "auto-release" the edge on focus loss if it wants retail parity; it should
  release on the next mouse move.
* The four observer flags `+0x1d..+0x20` remain **dead** (store census over
  `0x00524000..0x0052c000`: only the ctor and the per-frame clear write them,
  both zero). The mouse does not use them.

### 2.6 Second path: minimap click -> target jump

```
00529a40 ...                          ; MAP_RENDERVIEW click callback
00529a48 8b705c      MOV ESI,[EAX+0x5c]      ; minimap panel
00529a4b 8b442418    MOV EAX,[ESP+0x18]     ; point
00529a64 e8e71d0d00  CALL 0x005fb850        ; screen -> map cell
00529a6c c6460401    MOV byte [ESI+4],1     ; pending target request
00529a70 e81b1e0d00  CALL 0x005fb890        ; -> world position
00529a77 894e08      MOV [ESI+8],ECX
00529a7d 89560c      MOV [ESI+0xc],EDX
```
and the park update consumes it:
```
005250b8 8a4804      MOV CL,[EAX+4]         ; [park+0x16c]+4
005250bd 7428        JZ 0x5250e7
005250c6 8b500c      MOV EDX,[EAX+0xc]
005250d8 e8f32a0000  CALL 0x00527bd0        ; park view -> observer
005250e3 c6410400    MOV byte [ECX+4],0     ; clear
```
`0x00527bd0 -> 0x0052a6b0 -> 0x005e5a30 -> 0x005e5cb0`:
`camera+4 = 0` (drop follow), `camera+0x40 = 1`, `camera+0x44/+0x48 = x,y`.
The next `0x005e5ff0` then does `0x5e609b: target_goal = (x, y, terrain(x,y))`
(`0x005e6ea0`), i.e. **a snap**, not a gliding move. That is a click-to-place
target, not a drag.

---

## 3. Roll audit (negative)

The park `mgCamera` record is `{eye@+0, target@+0xc, up@+0x18, proj@+0xa4}` and
the view matrix is built by `FUN_0043d380 -> FUN_00466e40 -> FUN_005a89b9`:

```c
// 0x005a89b9(mat, eye, target, up)
d    = normalize(eye - target);      // backward
side = normalize(cross(d, up));
up'  = cross(d, side);
mat  = { side, up', d, -dot(...) };
```
This is a plain look-at; **roll is 0 for any eye/target while up is world +Z**,
regardless of the eye/target transient.

`FUN_004475e0` (`SetUp`) has exactly four xrefs and all pass a constant:
`0x005e4daf (0,0,1)`, `0x005d8001 (0,0,1)`, `0x006060b6 (0,0,-1)` (reflection),
and **`0x0052785a (0,0,1)`** - the park record - once, at view construction.
No per-frame up write exists in the park module.

The one place a non-world "up" is computed is the tail of `0x00529d00`
(`0x0052a4c8..0x0052a56b`): `FUN_00447990` (view dir) negated, then
`FUN_004476d0` (orthogonalized up), then `FUN_005b8fe0(pose, 0)`. That pose is
**the DirectSound listener**, not the render camera: `0x005b8fe0` ->
`0x005c3260` calls `msListenerHardDx8::Set3DParams` (string `0x0066dfac`,
`0x005c3130` is the matching `Get3DParams`). It is audio only.

So: no roll, no quaternion, no bank. Any perceived "tilt" is pitch.

---

## 4. Pitch: the complete state

```
pitch = atan2(eye.z - target.z, |eye.xy - target.xy|)
eye.z - target.z = h(p) + max(terrain(eye_goal.xy), target_goal.z, altitude) - target_cur.z
                 = h(p) + max(0, terrain(eye_goal.xy) - T, altitude - T)
T     = max(terrain(target_cur.xy), altitude)          (target Z is instant)
h(p)  = 3 + 497p,   r(p) = 10 + 290p,   altitude = water level = 16.0 on isl01
```
(`0x005e60ae..0x005e6109` target glue, `0x005e625a..0x005e62c5` eye Z max,
`0x005e6271` `FADD ST0,ST1` is the `+h`, `0x005e62af` is the altitude term.)

Pitch is therefore **not** a function of p alone: the `max()` is asymmetric and
depends on the ground under both the eye and the target.

---

## 5. The three dynamic pitch terms (measured)

### 5.1 Terrain under the eye (`terrain(eye_goal)`)

Sensitivity `d(pitch)/d(terrain) = r / (r^2 + (h+d)^2)`:
`0.405 deg/unit` at p=0.1, `0.102 deg/unit` at p=0.5.

Measured directly on the retail `isl01` heightmap (`heightmap_01.raw`,
512x512 u8, 2 world units/cell, height scale 1.0), eye ring radius 39:

| park location (record - 512) | terrain at centre | ring min..max | max pitch shift at p=0.1 |
| --- | ---: | --- | ---: |
| free-game start (0,0) | 32.0 | 32.0 .. 32.8 | **+0.3 deg** |
| entrance (8,-136) | 76.0 | 44.9 .. 91.7 | **+6.8 deg** |
| far corner (300,300) | 0.0 | 0.0 .. 0.0 | 0 (water) |

So on the start plateau the terrain term is invisible, but anywhere with relief
it swings the pitch by several degrees - and only when the camera moves, because
the sampled point is `eye_goal` (`target_goal + W`), not a fixed point.

### 5.2 One-update eye/target lag (order of operations)

`0x005e5ff0` computes the **eye goal first** (`0x005e610e..0x005e62c5`) and pans
the **target goal afterwards** (`0x005e632f..0x005e64db`). The smoothing at the
end then moves `target_cur` toward the *new* goal while `eye_cur` moves toward
the *old* goal + W, so

```
D = eye_cur - target_cur   ->   D_inf = W - step_vector
```

* held **forward** pan: `|D| = r + step` -> at p=0.1 `39 -> 41.9`,
  pitch `53.5 -> 51.5 deg` (**-2.0 deg**), and back when the key/cursor stops.
* held **lateral** pan: `|D| = sqrt(r^2+step^2)` and the view direction rotates
  by `atan(step/r) = atan(2.9/39) = 4.25 deg` (**a yaw lean**, not a roll).

The same formula means the eye's Z is sampled ~`step/k` = 31 world units
*ahead* of the actual eye while moving (the look-ahead of the terrain sample),
which is what makes the pitch change *before* the camera reaches a slope.

### 5.3 Yaw/orbit lag

Yaw is a 5/s low-pass (`k = dt*5`, pair (`0x40a00000`)=5.0 at `0x5e5d67`), and
the eye goal is built from `yaw_cur` *before* the yaw smoothing. Holding rotate
at 90 deg/s:

```
yaw lag  = omega/lambda = 1.5708/5 = 0.314 rad (18.0 deg), measured 0.293 rad
|D|      = r*cos(lag_extra) : 39 -> 37.7 (3.4 %)
pitch    : 53.5 -> 55.9 deg  (+2.4 deg)
```
(Also measured: `rxy` shrinks to 37.7 and the pitch settles at 55.9.)

### 5.4 What it is not

* not roll (section 3),
* not a `progress` change while panning (p is untouched by the mouse; only
  flags 6/7 move it),
* not the observer block (dead), not `mgCamera::SetUp` (never re-called).

---

## 6. Corrections to `notes/camera-controls-binary-recon.md`

1. **§8 / §11 "the edge-scroll branch is unwired / camSwitcher+4/+8 has no
   writer" is wrong.** The writer is `FUN_0052a690`, called from `0x5279b0`
   inside the park's `RENDERVIEW` mouse-move callback. The edge scroll is live.
2. **§8 `[0x6750f0]+0x2a/+0x36` is not a UI mode.** It is the DirectInput
   keyboard array at `DIK_LSHIFT`/`DIK_RSHIFT`; the gate is "no Shift held",
   so the edge scroll is suppressed exactly while the rotate modifier is down.
3. **The vtable `0x00642894` is the ViewManager**, not the park observer; its
   slots 7/10/11 are `WM_MOUSEMOVE`/`WM_LBUTTONDOWN`/`WM_LBUTTONUP` and they
   never reach the camera. Hooking only those will not move the park camera.
4. **§10 port contract misses the mouse** and has the eye goal in the wrong
   place: the port computes `eye_goal` *after* the pan (`main.cpp:3573` after
   3544-3559); retail computes it *before*, which is what produces the lean.
5. **§10 "no roll" is right** - and the tilt is pitch; §5 quantifies it.
6. §7's "observer is a pass-through" is right; its `+0x1d..+0x20` block is
   still dead (re-verified over the extended range `0x524000..0x52c000`).

---

## 7. Port contract

### 7.1 Mouse -> camera (edge scroll)

State: the camera needs a 2-int cursor in **client pixels** and the render-view
size. Both are new to the port; nothing else changes.

```
// once per mouse move (or on the port's window message / each frame from the
// platform cursor):
cam.cursor = { client_x, client_y };          // integer pixels, no scaling

// inside the retail camera update, BEFORE the pan and AFTER nothing else:
const int w = render_view_width, h = render_view_height;   // [0x674528]+0x31ce4/8
const bool shift = key_down(DIK_LSHIFT) || key_down(DIK_RSHIFT);
auto edge = [&](int idx){
    if (shift) return false;
    switch (idx) {
      case 2: return cursor.x < 2;          // left
      case 3: return cursor.x > w - 2;      // right
      case 0: return cursor.y < 2;          // forward (top)
      case 1: return cursor.y > h - 2;      // backward (bottom)
    }
    return false;
};
flag[i] = flag[i] || edge(i);   // for i in 0..3; keyboard flags unchanged
```
Then the existing pan runs once per camera tick with `step = lerp(1,20,p)`, no dt.
Deadzone = 2 px, no ramp, no click-drag, no wheel.

* If the port uses its fixed 60 Hz camera tick, the edge scroll speed is
  `60 * 2.9 = 174 u/s` at p=0.1 - the same as retail at 60 fps.
* The cursor must be the **client** position (0..w-1), not the OS screen
  position and not a normalised value.
* Do **not** auto-clear the cursor on focus loss; retail keeps the last value.
* `altitude` = the park water level (16.0 on isl01) - already in the port.

### 7.2 Minimap click

`click -> world (x,y)` (the port already ray-picks the minimap) must do what
`0x005e5cb0` does: `follow = none; target_goal = (x, y, terrain(x,y));` as a
snap (no smoothing on the goal; the current target still smooths with k).

### 7.3 Tilt (pitch) - what the port must reproduce

1. Order of operations **inside the update**:
   ```
   target_goal.z += max(terrain(target_cur.xy), alt) - target_cur.z;
   eye_goal   = target_goal + (cos(yaw_cur), sin(yaw_cur)) * r(p);   // PRE-input
   eye_goal.z = h(p) + max(target_goal.z, terrain(eye_goal.xy), alt);
   <input: flags 0..7 + edge>
   <smooth yaw_cur, target_cur, eye_cur>
   ```
   The port currently pans before the eye goal; swapping the two lines restores
   the 1-step lean (`rxy -> r + step` when panning forward, 4.25 deg yaw when
   panning sideways, -2 deg pitch at p=0.1).
2. `eye_goal.z` must keep the `max(...)` with `terrain(eye_goal.xy)` (the port
   has it) - that is the multi-degree terrain tilt.
3. No up-vector change, no roll, ever.
4. Assertions (deterministic, flat test terrain = 32, alt = 16):
   * no input, settled: `pitch == atan2(52.7, 39) = 53.49 deg`, `r == 39`.
   * hold idx 0 (forward) one tick: `eye_goal` is computed from the
     pre-pan target, so after the smoothing `|eye-target|_xy == 39 + 2.9`
     (not 39); pitch drops ~2.0 deg; it returns to 53.49 deg within ~0.3 s of
     releasing.
   * hold idx 2 (left) one tick: the eye->target direction is rotated by
     `atan(2.9/39) = 4.25 deg` from the pre-pan direction.
   * cursor at (1,1) with no Shift: `target_goal` moves by `2.9*(fwd+left)`
     per tick; at the centre: no motion; at (1,1) with Shift: no motion.
   * with terrain rising 15 units under the eye: pitch +6.6 deg at p=0.1 (53.49 -> 60.05).

---

## 8. Runtime cross-check list (DINPUT8 camera probe)

The probe already samples `+0x20/+0x2c/+0x54/+0x60/+0x84/+0x90/+0x120/+0x12c/+0x158/+0xb4`.
Add (all from the camera object, `ecx` at the `0x5e5ff0` entry):

| field | why |
| --- | --- |
| `[ecx+0x3c]` then `[[ecx+0x3c]]`, `[[ecx+0x3c]+4]` | the cursor x,y actually seen by the edge test |
| `[0x006750f0]+0x2a`, `[0x006750f0]+0x36` | Shift gate (must be 0 for the edge scroll) |
| `[[0x00674528]+0x31ce4]`, `[[0x00674528]+0x31ce8]` | screen w/h used by the edge test |
| derived `rxy = |eye.xy-target.xy|` | shows the 1-step lag (`r+step` while panning) |
| derived `pitch = atan2(eye.z-target.z, rxy)` | the "tilt" series |
| derived `yaw = atan2(target-eye)` vs `+0x120` | shows the yaw lean |

Breakpoints (log only, no code change):
```
bp 0x005279b0   ; log x=*[esp+4], y=*[esp+8] before 0x52a690  -> mouse writes
bp 0x005e6335 0x005e63a1 0x005e640d 0x005e6477   ; which edge flags fire
bp 0x004475e0   ; SetUp must never fire while moving (roll stays 0)
```
Probe procedure: park the virtual cursor at (1,1), (w-1,1), (1,h-1), (w-1,h-1)
and at (w/2,h/2), 60 updates each, with and without Shift; then a minimap click.
Expected: `target_goal` delta per update = 2.9 in the four directions (p=0.1),
0 at the centre and with Shift; `rxy` grows to ~41.9 while panning forward;
`pitch` follows `h + max(0, terrain(eye)-T)`.

---

## 9. Uncertainty

* `[[0x00674528]+0x31ce4/+0x31ce8]` were not traced to their writer; the code
  compares client pixels against them, so they are almost certainly the render
  view width/height (800x600 retail). Log them (section 8) before hard-coding.
* The widget dispatch that calls `0x527970` was not stepped through; it is the
  only writer of the camera cursor, so if the port's virtual cursor does not
  reach it, no camera motion happens. Hooking `0x52a690` (or `0x5279b0`) is a
  direct confirmation.
* The `max()` in the eye Z is asymmetric (it can only raise the eye); the port
  must keep `max(0, ...)` semantics rather than a symmetric offset.
* Pitch numbers in `5.x` come from a reproduction of the exact equations
  (dt=1/60, p=0.1) plus the retail heightmap; they were not read from a retail
  run. The qualitative claim (pitch, never roll) is proved by section 3.


## 8. Runtime confirmation (2026-10-28)

The DINPUT8 camera probe (`scripts/x32dbg/camera-probe.ps1`) parked the
virtual cursor at the exact 2-px bands on the bordered 800x600 test window:

| cursor | camera result | flags |
| --- | --- | --- |
| (799,300) right | target y `512 -> 974.9` (+462.9) | 0x00 |
| (400,599) bottom | target x `575.3 -> 1274` (+698.7, clamp) | 0x00 |
| (799,599) bottom-right | y `974.9 -> 1274` (+299.1) | 0x00 |
| (1,300) left | no camera change | 0x00 |
| (400,1) top | no camera change | 0x00 |

The left/top failures are a **bordered-window harness artifact**, not retail:
the game's WndProc converts the client `WM_MOUSEMOVE` to window coordinates
(adding the ~3 px / ~29 px frame) before the park RENDERVIEW callback sees it,
so x=1 becomes ~4 and y=1 becomes ~30, outside the `x<2`/`y<2` bands. Right and
bottom (x=799, y=599) still cross `w-2`/`h-2`. In fullscreen or a borderless
window all four bands are reachable. The port uses SDL client-pixel mouse
coordinates, so its edge scroll works on all four edges (`--edge-scroll N`,
default 2, for windowed comfort).

This also explains the playtest note "the harness mouse only works for right
and down": it was the frame offset, not the retail path.

The playtest's "no tilt" is covered by §5: the port now builds the eye goal
before the pan, and `--camera-selftest` asserts `lean_goal dx=41.9` at p=0.1.
