# Retail park camera - the "tilt when the camera moves" (motion feel)

Status: **negative result on roll.** The retail park camera has no roll/tilt
state and never rotates the view up vector. The lean that a playtest reports is
a *pitch* effect produced by the single `progress` parameter plus the yaw
low-pass. Details, addresses and the port contract below.

Binary: `DinoIsland.exe-ebea17` (PE32, MSVC 6). Addresses are file/load RVAs
(image base 0x00400000).

## 1. Which object is active, and what updates it

The camSwitcher `S` is built by `FUN_005e57c0` (camManager.cpp):

```text
005e582b  alloc 0x164              ; the only camera class created in this build
005e5848  call FUN_005e5d10        ; ctor: sets vtable 0x006481a4, config +0x64
005e585c  [S+0x30] = camera
005e5815  [S+0x28] = 0             ; the two "other camera" slots stay null
005e581c  [S+0x2c] = 0
005e5867  alloc 0x28               ; pose proxy
005e5874  call FUN_005e5b70        ; proxy ctor: [+0x20]=0, [+0x24]=0
005e587d  [S+0x14] = proxy
005e5880  ECX = proxy ; PUSH [S+0x30]
005e5886  call FUN_005e5cd0        ; proxy[0x20]=proxy[0x24]; proxy[0x24]=camera
```

So `camSwitcher+0x20`/`+0x24` from the older note is **not** a vtable: `+0x24`
is the pose proxy (0x28 bytes: eye at +8, target at +0x14, camera slots at
+0x20/+0x24). The active camera is the 0x164-byte camSwitcher.cpp object at
`[proxy+0x24]`; `[proxy+0x20]` ("previous") is always 0 in the park
(`FUN_005e5bc0` asserts if both are set, `0x005e5beb` -> assert at
`0x005e5c35` "camSwitcher.cpp:99").

Active camera vtable `0x006481a4` (read 32 bytes at `0x006481a4`:
`20 5f 5e 00 | b0 5f 5e 00 | f0 5f 5e 00 | 35 fa 8e 3c ...`):

| slot | address | role |
| --- | --- | --- |
| +0x0 | 0x005e5f20 | read/deserialize (3 args, `RET 0xc`) |
| +0x4 | 0x005e5fb0 | write/serialize (2 args, `RET 0x8`) |
| +0x8 | **0x005e5ff0** | **Update(dt)** (`RET 0x4`) |

`FUN_005e5bc0` does `CALL [eax+8]` with one `dt` argument at `0x005e5bdb`
(proxy+0x20) and `0x005e5be8` (proxy+0x24), then copies the result:

```text
005e5beb  if [proxy+0x20] != 0 and [proxy+0x24] != 0 -> assert
005e5c69  eax = [proxy+0x24]            ; active camera
005e5c6f  ecx = eax+0x20 -> proxy+0x08  ; eye   (3 floats)
005e5c88  eax = eax+0x2c -> proxy+0x14  ; target(3 floats)
```

`FUN_005e5960` (`camSwitcher::Update`) then: `[S+0xc]=0` -> dispatch ->
`FUN_005e6930([S+0x34])` (clears the 8 action bytes) -> `FUN_005e5ce0`
(returns `[camera+0x39]`, "eye moved") -> `SetPosition([S], proxy+8)` at
`0x005e5995` and `SetTarget([S], proxy+0x14)` at `0x005e59a3`. `[S]` is the
`mgCamera` embedded in the park 3D render view at `renderView+0x70`
(`FUN_00529c50`, `0x00527889-0x0052789d`).

The tilt is in **none** of the copies: `FUN_005e6930` clears two dwords,
`FUN_005e5ce0` returns one byte, and `FUN_00447580`/`FUN_004475b0` write only
the eye / target of the mgCamera record. The camera object itself writes only
two vec3s (`0x005e678f` -> +0x20 eye, `0x005e677a` -> +0x2c target). It has no
up vector, no quaternion and no roll field.

## 2. Camera state (0x164-byte object)

| offset | type | meaning |
| --- | --- | --- |
| +0x20 | vec3 | **eye output** (consumed by FUN_005e5bc0) |
| +0x2c | vec3 | **target output** |
| +0x50 | float | camera altitude above terrain (set by FUN_005e5a00 from `[[park+0x12c]+0xf4]+0x38`, see 0x0052459e) |
| +0x54/+0x60 | vec3/vec3 | eye current / eye goal |
| +0x84/+0x90 | vec3/vec3 | target current / target goal |
| +0x98 | float | target goal Z (terrain height) |
| +0x6c/+0x74/+0x7c | pair x3 | eye smoothing: PositionHardness, PositionHardness, (1000,1000) |
| +0x9c/+0xa4/+0xac | pair x3 | target smoothing: TargetHardness, TargetHardness, (1000,1000) |
| +0xb4 | ptr | 8 input bytes (same block as camSwitcher+0x34) |
| +0xb8..+0xc4 | 4 floats | eye height clamp |
| +0x108 | ptr | heightfield (set by FUN_005e59d0) |
| +0x114 | float | rotation speed = min(RotationSpeedMin,Max) = 1.570796 (`0x005e68ac`) |
| +0x118/+0x12c | float | **yaw current / yaw goal** (z component; x/y unused) |
| +0x130/+0x138/+0x140 | pair x3 | yaw smoothing, all (10,10) (`0x005e5d5a..0x005e5db1`) |
| +0x148 | pair | elevation = lerp(config+0x7c, config+0x80, p) = 3..500 (`0x005e6812`) |
| +0x150 | pair | horizontal distance = lerp(config+0x84, config+0x88, p) = 10..300 (`0x005e6803`) |
| +0x158 | float | **progress p in [0,1]**, init 0.1 (`0x005e5d5d`), clamp at `0x005e661f..0x005e666e` against `0x006254a0`=0.0 and `0x0062588c`=1.0 |
| +0x15c | pair | progress speed = config+0x64/+0x68 = 0.05..0.2 (`0x005e6884`) |
| +0x39 / +0x3a | byte | eye-moved / moved |

Config block at `+0x64` = camera.ini `[Config1]`; defaults in `FUN_005e5560`,
reader `FUN_005e5690`, copy into the smoothing objects in `FUN_005e67f0`.

## 3. The motion model (0x005e5ff0)

```c
// every frame; dt = argument 1, p = this->progress
this->yaw_goal += dir4_5 * dt * 1.570796f;          // 0x5e64fc / 0x5e654e
this->progress += dir6_7 * dt * lerp(0.05,0.2,p);   // 0x5e65a2 / 0x5e65e8
clamp(this->progress, 0, 1);                        // 0x5e661f . . 0x5e666e

// pan (0..3): move the target GOAL in the ground plane
step = lerp(1, 20, p);                              // config+0x74/+0x78
target_goal.xy += dir * step;  // 0x5e632f..0x5e64db; step = lerp(1,20,p), no dt factor visible in this function
//   dir: flags0/1 = -(target-eye)_xy normalized * (FCHS for flag 1)
//        flags2/3 = that vector rotated +-90 deg (0x5e7050 / 0x5e7130)
target_goal.z += terrain(target_cur.xy) - target_cur.z;  // 0x5e60ae..0x5e610e (glues the target to the terrain; exact expression)

// eye GOAL: horizontal radius = distance(p), height = elevation(p)
W = Rz(yaw_current) * (1,0,0) * lerp(10,300,p);     // 0x5e610e..0x5e61ab
eye_goal.xy = target_goal.xy + W.xy;
eye_goal.z  = target_goal.z + lerp(3,500,p);        // 0x5e625f..0x5e62c5
eye_goal.z  = max(eye_goal.z, terrain(eye_goal.xy) + this->camera_altitude);

// first-order smoothing (FUN_005e6960)
//   cur += (goal-cur) * clamp(dt * lerp(min,max,p), 0, 1)   per axis
smooth(&yaw_current,   dt, p);                      // pair (10,10)  -> 0x5e6689
smooth(&target_current,dt, p);                      // hardness 6..2 -> 0x5e675e
smooth(&eye_current,   dt, p);                      // hardness 6..2 -> 0x5e676d
// X/Y use the hardness pair; Z uses (1000,1000) i.e. instant.

out.eye    = eye_current;
out.target = target_current;
```

`FUN_005e6960` in full (`0x005e6960`):

```c
void smooth(vec3* v, float amount, float t) {
    for (i = 0; i < 3; ++i) {
        float s = v->pair[i].a + (v->pair[i].b - v->pair[i].a) * clamp01(t);
        float k = clamp01(amount * s);
        v->cur[i] += (v->goal[i] - v->cur[i]) * k;
    }
}
```

### Pitch, not roll

There is no pitch state either: the look angle is a consequence of the two
progress-driven values.

```text
horizontal radius r(p) = 10 + 290*p
height above target h(p) =  3 + 497*p
pitch(p) = atan2(h(p), r(p)) = [16.7 deg @ p=0, 53.5 deg @ p=0.1, 59.0 deg @ p=1]
eye = target + (cos(yaw), sin(yaw), 0) * r(p);  eye.z = target.z + h(p)
```

Cross-check against retail data: the Free Game save camera tail is eye
(551,512,84.7) / target (512,512,32) on isl01, i.e. horizontal radius 39,
height above target 52.7, yaw 0 - exactly r(0.1)=39 and h(0.1)=52.7. That
confirms both the horizontal-radius interpretation and the height offset
(and rules out a slant-distance-with-independent-pitch model).

`progress` is advanced by the *same* action indices as CameraRotateUp/Down:
`NAVIGATION_ZOOM` and `NAVIGATION_DEZOOM` register `LAB_00525dc0` (index 6) and
`LAB_00525de0` (index 7) in `FUN_005257a0`, i.e. exactly `CameraRotateUp/Down`
(`0x00525e60/0x00525e70`). There is no separate zoom axis and no separate pitch
axis in retail: zoom == tilt == tilt-and-dolly of the orbit.

### What the playtest feels

1. **Zoom/rotate-up-down** changes `p` -> both `r` and `h` change -> the camera
   visibly tilts (16.7..59 deg) while also moving in/out. This is the designed
   "tilt" and it is a pure pitch around the target, never a roll.
2. **Yaw rotation** is low-passed at 10/s. While holding Shift+Left/Right the
   eye goal runs around the target at 90 deg/s and the current point trails it:
   steady-state phase lag `omega/lambda = 1.5708/10 = 0.157 rad (9 deg)`, and
   the orbit radius shrinks by `1/sqrt(1+(omega/lambda)^2) = 0.988` (~1%),
   because the eye's Z is already at the goal (Z pair is (1000,1000)). At p=0.1
   that is a pitch change of only ~0.4 deg - measurable, but far too small to be
   the reported "tilt". What the player actually feels is the 9 deg *yaw* lag:
   the camera keeps turning briefly after the key is released. Still no roll.
3. **Panning** is rigid: eye and target goals are offset by the constant `W` and
   both are smoothed with identical factors, so the view direction does not
   change at all while panning on flat ground.

### Up vector is constant

`mgCamera::SetUp` is `FUN_004475e0` (writes record `+0x18..0x20`). All four
call sites:

| site | value | what |
| --- | --- | --- |
| 0x005e4daf | (0,0,1) | other camera init |
| 0x005d8001 | (0,0,1) | debug/minimap camera |
| 0x0052785a | **(0,0,1)** | **park mgCamera** (renderView+0x70) |
| 0x006060b6 | (0,0,-1) | mirrored (reflection) camera |

The park camera never sets it again. `mgCamera`'s data record is
`{eye @+0, target @+0xc, up @+0x18, projection @+0xa4..+0xb0}`; the view basis
is built from those (`FUN_0043d380` -> `FUN_00466e40`, `FUN_004476d0` returns
up orthogonalized against the view direction, `FUN_00447850` the right vector).
With up == world +Z and eye/target always in the same vertical plane, the roll
around the view axis is identically 0 for every frame.

Correction to `notes/camera-rotation-recon.md`: the `+0x1d..+0x20` byte flags
consumed by `pvwParkObserver::Update` (`FUN_00529d00`) have **no writer** in
this build (only the ctor `FUN_00529c50` and the per-frame clear at
`0x0052a59b/0x52a59e` touch them; `FUN_0052d8e0` writes `+0x1d` of an unrelated
construction-kit object). That whole orbit/zoom block in the observer is inert;
the live input path is the 8 action bytes at `camSwitcher+0x34` -> camera
`+0xb4`, consumed only by `0x005e5ff0`.

## 4. Port contract (`src/viewer/main.cpp`)

Hook the park-camera block at `main.cpp:11418-11510` (the
`// Retail park camera (camera.ini / KeyMapper.ini)` section) and the Free Game
start camera at `main.cpp:8054-8072`.

Do **not** add a roll; retail's up is +Z. To match the feel:

1. Keep one authoritative `progress p` in [0,1] (init 0.1). Derive
   `r = lerp(10, 300, p)`, `h = lerp(3, 500, p)`,
   `pitch = atan2(h, r)` (16.69 deg at p=0, 53.49 at 0.1, 59.04 at 1).
   Replace the current independent `pitch`/`distance` state; `distance` in
   retail is the *horizontal* radius, not the slant distance.
2. Zoom in/out **and** Shift+Up/Down both change `p`:
   `p += sign * lerp(0.05, 0.2, p) * dt`, clamp [0,1]. (This is why a wheel
   notch and Shift+Up must not diverge.)
3. Yaw: keep a goal and a current value; rotate keys add
   `pi/2 * dt` (rad) to the goal; then
   `yaw_cur += (yaw_goal - yaw_cur) * min(1, 10.0f * dt)`. The 10/s low-pass is
   the rotation "feel" (9 deg lag at 90 deg/s).
4. Pan: `step = lerp(1, 20, p) * 4 * dt` world units (the port's unit conversion; retail 0x005e5ff0 itself adds `dir*lerp(1,20,p)` per update with no dt factor - the 1..20 look like cells per camera tick, see uncertainty);
   apply to the target goal, then `eye = target + (cos yaw, sin yaw, 0)*r` with
   `eye.z = target.z + h`, and smooth both poses:
   `k = min(1, lerp(6, 2, p) * dt)` for X/Y, Z instant (`k = 1`).
5. Camera altitude clearance (`camera+0x50`) is supplied by the park
   (`0x0052459e`); the port's `terrain.height_at(eye) + 1.6` free-camera clamp is
   a separate path.

Retail projection (for reference, `0x0052a3f1`):
`FUN_00447610(fov = 1.3962634 rad (80 deg), aspect = 1.3333334,
near = max(1, 0.1*(height above ground)), far = 1000)`.

### Log for the test

`DINO_DEBUG_CAMERA=1` already prints target/yaw/pitch/dist every 30 frames
(`main.cpp:11513`). Extend it with `p`, `r`, `h`, `yaw_goal`, and the view up
vector. Expected: `up == (0,0,1)` on every frame; `pitch ==
atan2(3+497p, 10+290p)`; `pitch` constant while panning; `pitch` rising while
zooming out (p -> 1) and while holding rotate, decaying after release.

## 5. Runtime watch (only if the static claim is challenged)

The claim "no roll" can be falsified with one breakpoint; nothing is unresolved
statically.

```text
; x32dbg
bp 0x004475e0                      ; mgCamera::SetUp
log "SetUp {d:[esp+4]} {d:[esp+8]} {d:[esp+0xc]}"
```

Expected: it fires only during startup/load (the park value is `0 0 1`), never
while panning or rotating. If it fires with a non-zero Z component during
movement, this note is wrong.

To dump the feel variables instead:

```text
bp 0x005e5ff0
log "yaw={d:[ecx+0x120]} yawgoal={d:[ecx+0x12c]} p={d:[ecx+0x158]} eye={d:[ecx+0x20]},{d:[ecx+0x24]},{d:[ecx+0x28]} tgt={d:[ecx+0x2c]},{d:[ecx+0x30]},{d:[ecx+0x34]}"
```

`ecx+0x120` = yaw current (rad), `ecx+0x12c` = yaw goal, `ecx+0x158` = p.

## 6. Uncertainty

- `0x005e5ff0` is not a Ghidra function (only referenced from the vtable), so
  its listing was decoded by hand; the eye/target stores, the yaw/progress
  updates and the smoothing calls above are read directly from the disassembly,
  but the exact `0x5e61b1..0x5e625a` clamp chain (eye Z clamp against `+0xb8`,
  `+0xbc`, `+0xc4`, `+0x98` and the heightfield) is not fully linearized. It does
  not affect the no-roll conclusion: no up vector is read or written there.
- `camera+0x50` (altitude) is set from the park at `0x0052459e`
  (`[[park+0x12c]+0xf4]+0x38`); its value was not traced to a file field.
- Pan units: 0x005e5ff0 adds `dir*lerp(1,20,p)` per update without an explicit dt, while the port note measured `MoveSpeed*4` units/s. Either the action dispatch rate or a scale outside this function converts the value; this does not affect the tilt/pitch result (the pan is rigid).
- The reflection camera uses up (0,0,-1) (`0x006060b6`); that is a separate
  camera and not the park view.

## Serialization (2026-10-28)

The active camera vtable `+0`/`+4` (`0x5e5f20` read, `0x5e5fb0` write)
serialize four floats: `progress` at `+0x158` and the yaw-goal vector at
`+0x124..+0x12c` (only z is used). The read is guarded by a stream
version/size `>= 0xe` and copies yaw current `+0x118` from the goal after
loading, so a loaded camera starts at its saved yaw without smoothing. The
derived eye/target are not serialized by the camera object; the `.sav` tail's
eye/target belongs to the camera-switcher record.

## Pan is per-frame (2026-10-28)

`0x5e6ae0(config_pair, p)` is a plain `lerp(1,20,p)` and `0x5e6a90` adds
`unit_dir * step` to the target goal without a `dt` multiply. The pan block
therefore has no time factor: retail speed depends on the update/frame rate.
The port's `* 4` world-units-per-second factor is a feel approximation, not a
recovered constant. The yaw block does use `dt`: `FLD [esp+0xb4]; FMUL
[ESI+0x114]` at `0x5e64fc`.
