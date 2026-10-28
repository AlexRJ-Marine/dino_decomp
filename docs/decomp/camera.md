# Park camera (CAM-1)

Status: **solved from the binary and verified against a live retail run; the
port uses it as the interactive default.** The retail park camera is one
0x164-byte object (vtable `0x6481a4`, update `0x5e5ff0`) driven by a single
`progress` parameter. It has no roll state and its up vector is always world
+Z. Static evidence: `notes/camera-controls-binary-recon.md`,
`notes/camera-motion-tilt-recon.md`, `notes/camera-rotation-recon.md`.
Runtime evidence: `run-test/dino_camera.csv` from
`scripts/x32dbg/camera-probe.ps1` (the DINPUT8 proxy samples the live object).

## Object and update chain

`FUN_005e57c0` builds the camSwitcher:

```text
alloc 0x164              -> active camera, ctor FUN_005e5d10 (vtable 0x6481a4)
alloc 0x28               -> pose proxy, ctor FUN_005e5b70
[proxy+0x24] = camera    -> FUN_005e5cd0; [proxy+0x20] stays null in the park
```

`FUN_005e5960` (camSwitcher::Update) clears `+0xc`, dispatches the eight
action bytes, then `FUN_005e5bc0` calls the active camera's `vtable+8`
(`0x5e5ff0`) and copies eye `+0x20` / target `+0x2c` into the proxy. The
park `mgCamera` (`renderView+0x70`) receives them through `0x5e5995`
(SetPosition) and `0x5e59a3` (SetTarget). Its up vector is set once to
`(0,0,1)` at `0x52785a` (`FUN_004475e0`) and never changed; the reflection
camera is the only `(0,0,-1)` user.

The older `pvwParkObserver::Update` (`0x529d00`) pan/orbit block is inert:
its byte flags `+0x1d..+0x20` have no writer in this build. The live input is
the eight action bytes at `camSwitcher+0x34` -> camera `+0xb4`.

## Fields (0x164-byte object)

| Offset | Type | Meaning |
| ---: | --- | --- |
| `+0x20` | vec3 | **eye output** |
| `+0x2c` | vec3 | **target output** |
| `+0x50` | float | camera altitude above terrain (`FUN_005e5a00`), 16 in the park |
| `+0x54` / `+0x60` | vec3 | eye current / goal |
| `+0x84` / `+0x90` | vec3 | target current / goal |
| `+0x98` | float | target goal Z (terrain height) |
| `+0x6c` / `+0x74` / `+0x7c` | pair | eye smoothing: PositionHardness, PositionHardness, (1000,1000) |
| `+0x9c` / `+0xa4` / `+0xac` | pair | target smoothing: TargetHardness, TargetHardness, (1000,1000) |
| `+0xb4` | ptr | eight input action bytes |
| `+0xb8..+0xc4` | float | target X/Y map box (isl01: -250..1274 record) |
| `+0x108` | ptr | heightfield (`FUN_005e59d0`) |
| `+0x114` | float | rotation speed = 1.570796 rad/s (`0x5e68ac`) |
| `+0x120` / `+0x12c` | float | **yaw current / goal** (z of the two vec3s; x/y unused) |
| `+0x130` / `+0x138` / `+0x140` | pair | yaw smoothing, all **(5.0, 5.0)** (`0x5e5d67..`) |
| `+0x148` | pair | elevation = lerp(3, 500, p) |
| `+0x150` | pair | horizontal distance = lerp(10, 300, p) |
| `+0x158` | float | **progress p** in [0,1], init 0.1 |
| `+0x15c` | pair | progress speed = lerp(0.05, 0.2, p) |
| `+0x39` / `+0x3a` | byte | eye-moved / moved |

Config block `+0x64` is `camera.ini [Config1]`; defaults in `FUN_005e5560`,
reader `FUN_005e5690`, smoothing copy in `FUN_005e67f0`.

## Motion (`0x5e5ff0`)

Per update, in this order (`dt` = the update argument, p = progress):

```c
// 1. target Z glue (0x5e609b): before the eye goal
target_goal.z += max(terrain(target_cur.xy), altitude) - target_cur.z;

// 2. eye goal: built from the PRE-input target, current yaw and progress
eye_goal.xy = target_goal.xy +
              (cos(yaw_cur), sin(yaw_cur)) * lerp(10, 300, p);
eye_goal.z  = lerp(3, 500, p) +
              max(target_goal.z, terrain(eye_goal.xy), altitude);

// 3. input. 4/5 yaw and 6/7 progress are dt-scaled; 0..3 pan is per update
//    (no dt) and the mouse edge test is OR-ed into flags 0..3.
yaw_goal += dir45 * dt * rot_speed(p);        // rot_speed = 1.570796
progress += dir67 * dt * lerp(0.05f, 0.2f, p);
clamp(progress, 0, 1);
fwd = normalize((target_cur - eye_cur).xy);
target_goal.xy += rotate(fwd, flag) * lerp(1, 20, p);
clamp(target_goal.xy, map_box);

// 4. first-order smoothing (FUN_005e6960)
cur += (goal - cur) * clamp(dt * lerp(min, max, p), 0, 1);
yaw_cur    += (yaw_goal - yaw_cur) * clamp(dt * 5.0, 0, 1);
target_cur += ... TargetHardness 6..2 (X/Y), Z instant;
eye_cur    += ... PositionHardness 6..2 (X/Y), Z instant;

out.eye = eye_cur; out.target = target_cur;
```

Because the eye goal is built **before** the pan, a held pan leaves the eye
one step behind: `eye - target = W - step`. At p=0.1 a forward pan stretches
`r` 39 -> 41.9 (pitch 53.5 -> 51.5 deg) and a lateral pan swings the view by
`atan(2.9/39) = 4.25 deg`. That one-update lag is the visible lean/tilt while
moving; there is still no roll (the up vector is world +Z). The terrain under
the moving eye adds up to +6.8 deg on isl01, and the 5/s yaw lag adds +2.4 deg
while rotating. See `notes/camera-mouse-tilt-recon.md` §5.

Runtime checks (isl01 Free Game, `run-test/dino_camera.csv`):

- ctor: eye `(128,128,250)`, target `(512,512,50)` record; first update
  target Z `= terrain(512,512) = 32`, eye goal `(551,512,84.7)`.
- first update dt 0.021: eye X `128 -> 177.7449`; `k = 0.1176 = dt * 5.6`
  (`TargetHardness = PositionHardness = 6..2`, p=0.1).
- pan at p=0.1: target goal `-2.9` per update, independent of dt
  (0.001..0.015 in the same run). Map clamp at record `-250`.
- yaw: goal `+dt*1.570796`; measured `k/dt = 5.0` from consecutive samples;
  steady lag at 90 deg/s is 0.293 rad (16.8 deg).
- zoom in at p=0.1: `dp/dt = -0.065 = -lerp(0.05,0.2,p)`.
- eye Z: `h + max(target, terrain(eye), 16)`; target/goal Z are equal every
  sampled frame (`tgt_goal_z == tgt_cur_z`).

**Frame rate:** the binary applies the pan step per Update, so retail pan
speed depends on the frame rate. The background harness runs unpaced
(75-1000 updates/s measured), so the CSV is used for per-update steps, not
per-second speed. The port runs the retail camera on a fixed 60 Hz tick
(--camera-hz, 10..240) so an uncapped render loop does not change the pan
speed; the camera self-test uses dt = 1/60.

## Controls (`Data/Config/KeyMapper.ini`)

| Action | Keys |
| --- | --- |
| `CameraKeyUp/Down/Left/Right` | arrows: pan the target (indices 0..3) |
| `CameraRotateLeft/Right` | `Shift+Left`/`Delete`, `Shift+Right`/`PageDown` (4/5) |
| `CameraRotateUp/Down` | `Shift+Up`/`Home`, `Shift+Down`/`End` (6/7, progress) |
| `NAVIGATION_TURNLEFT/TURNRIGHT` | same rotation state |
| `NAVIGATION_ZOOM/DEZOOM` | same progress axis (6/7) |

`KeyMapper.ini` has no mouse or wheel bindings, but the park camera still has
a **mouse edge scroll**: the client-pixel cursor is written by the park
`RENDERVIEW` mouse-move callback (`0x5279b0` -> `0x52a690`) into
`camSwitcher+4/+8`, and `0x5e5ff0` ORs `x<2`, `x>w-2`, `y<2`, `y>h-2` into
pan flags 2/3/0/1. It is suppressed while Shift is held, has a hard 2 px
band (no ramp), and persists when the window stops receiving moves. A
`MAP_RENDERVIEW` click is a separate path that snaps the target to the
clicked world point (`0x529a40` -> `0x5e5cb0`); the port's minimap click is
not wired yet. No drag and no wheel: the wheel goes to
`ViewManager::OnMouseWheel` (`0x5bb540`), which stores the signed delta at
`+0x28`, clears its dx/dy and forwards `&+0x18` to the UI input manager
(`vtable+0x40`); the eight camera action callbacks
(`0x525e00`..`0x525e70`) are keyboard-only. `FUN_005257a0` registers those
callbacks; `FUN_00527b80` gates them on the controller state and writes one
of the eight bytes through `FUN_005e59b0`.

## Port

- `src/viewer/camera_config.{h,cpp}` reads `camera.ini [Config1]`.
- `src/viewer/main.cpp` has `RetailParkCamera` with the exact state:
  target/eye current+goal, yaw current+goal, progress, altitude. It is the
  **default** interactive park camera; `--camera-orbit` restores the legacy
  spherical orbit, and explicit `--camera-target` / `--distance` /
  `--focus-*` views keep the deterministic orbit.
- Start state: `p=0.1`, yaw 0, target `(0,0,terrain)` in viewer coordinates,
  eye `(128-half, 128-half, 250)` (the retail ctor fly-in). The saved camera
  tail (record coordinates) is converted by `-half` for eye and target.
- A port-only `.psav` save stores the orbit form (target/yaw/pitch/distance);
  loading it rebuilds the retail camera's eye/target/goals from that form so
  the session keeps its view. `--camera-retail` forces retail mode even for
  `--start-dino`/explicit views.
- Pan is per update (`lerp(1,20,p)`, no dt), yaw is `dt*pi/2` with the 5/s
  low-pass, progress is `dt*lerp(0.05,0.2,p)`; no roll. The eye goal is
  computed **before** the pan (retail order) so the one-step lean is
  reproduced; `--camera-selftest` asserts `lean_goal dx=41.9`.
- Mouse edge scroll: the last client-pixel cursor position (SDL mouse motion,
  persisted on focus loss) sets pan flags 2/3/0/1 within a 2 px band
  (`--edge-scroll N` widens it for windowed play), disabled while Shift is
  held. No drag and no wheel.
- Tests: `tests/test_viewer_cpp.py::test_cpp_camera_retail_progress_default`,
  `test_cpp_camera_retail_selftest`, `test_cpp_camera_orbit_option_keeps_legacy_view`.

## Serialization

The active camera's `vtable+0` / `+4` (`0x5e5f20` read, `0x5e5fb0` write)
serialize exactly four floats:

```text
float progress        (+0x158)
float yaw_goal[3]     (+0x124..+0x12c; only z, +0x12c, is used)
```

The read is guarded by a stream version/size `>= 0xe`; after reading, yaw
current (+0x120) is copied from yaw goal (+0x12c) with no smoothing on load.
The derived eye/target are not part of this block. The save tail's eye/target
is a separate camera-switcher record; `--sav-park` applies it to the retail
camera (converting record to centred world coordinates).

## Open

- Retail foreground frame rate (pan is per frame). The port uses the display
  rate; a different frame rate changes pan speed by the same factor, as in
  the original.

## Evidence

- Ghidra: `0x5e57c0` camSwitcher builder, `0x5e5d10` ctor, `0x5e5b70`
  proxy, `0x5e5cd0`, `0x5e5960` update, `0x5e5bc0`, `0x5e5ff0` camera
  update, `0x5e6960` smoothing, `0x5e5995`/`0x5e59a3` / `0x5e59d0`,
  `0x5e5560` defaults, `0x5e5690` INI reader, `0x5e67f0`, `0x5e68ac`,
  `0x5e57a0` action registration, `0x527b80`, `0x5e59b0`, `0x529c50`,
  `0x529d00`, `0x52785a`, `0x4475e0`.
- `notes/camera-controls-binary-recon.md` (action table, corrected offsets
  and 5/s yaw pair), `notes/camera-motion-tilt-recon.md` (older static
  analysis; its 10/s claim is superseded), `notes/camera-rotation-recon.md`.
- `scripts/x32dbg/camera-probe.ps1` + `run-test/dino_camera.csv`
  (`DINO_CAMERA_PROBE=1`, vtable `0x6481a4` slot 2) and
  `--camera-selftest`.
