# Park camera (CAM-1)

Status: **static hypothesis; port rejected by playtest 2026-10-28.** The
object/no-roll fields below may be correct, but the action mapping, initial
`progress` and port implementation do not reproduce the retail controls. The
default viewer camera is back on the previous orbit; `--camera-retail` is
A/B only. Re-verification: `notes/camera-controls-binary-recon.md`. Do not use
the `camera-retail-*.png` files as retail evidence (they are port captures).
The retail park camera is one 0x164-byte object (vtable `0x6481a4`, update
`0x5e5ff0`) driven by a single `progress` parameter. It has no roll state and
its up vector is always world +Z. Raw static evidence:
`notes/camera-motion-tilt-recon.md`, `notes/camera-rotation-recon.md`.

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
| `+0x50` | float | camera altitude above terrain (`FUN_005e5a00`) |
| `+0x54` / `+0x60` | vec3 | eye current / goal |
| `+0x84` / `+0x90` | vec3 | target current / goal |
| `+0x98` | float | target goal Z (terrain height) |
| `+0x6c` / `+0x74` / `+0x7c` | pair | eye smoothing: PositionHardness, PositionHardness, (1000,1000) |
| `+0x9c` / `+0xa4` / `+0xac` | pair | target smoothing: TargetHardness, TargetHardness, (1000,1000) |
| `+0xb4` | ptr | eight input action bytes |
| `+0xb8..+0xc4` | float | eye height clamp |
| `+0x108` | ptr | heightfield (`FUN_005e59d0`) |
| `+0x114` | float | rotation speed = 1.570796 rad/s (`0x5e68ac`) |
| `+0x118` / `+0x12c` | float | yaw current / goal (z component) |
| `+0x130` / `+0x138` / `+0x140` | pair | yaw smoothing, all (10,10) |
| `+0x148` | pair | elevation = lerp(3, 500, p) |
| `+0x150` | pair | horizontal distance = lerp(10, 300, p) |
| `+0x158` | float | **progress p** in [0,1], init 0.1 |
| `+0x15c` | pair | progress speed = lerp(0.05, 0.2, p) |
| `+0x39` / `+0x3a` | byte | eye-moved / moved |

Config block `+0x64` is `camera.ini [Config1]`; defaults in `FUN_005e5560`,
reader `FUN_005e5690`, smoothing copy in `FUN_005e67f0`.

## Motion (`0x5e5ff0`)

```c
// dt = argument, p = this->progress
yaw_goal += dir45 * dt * 1.570796f;
progress += dir67 * dt * lerp(0.05f, 0.2f, p);
clamp(progress, 0, 1);

// pan (dir 0..3): move the target goal in the ground plane
target_goal.xy += dir * lerp(1, 20, p);        // no dt factor in the recovered block
target_goal.z += terrain(target_cur.xy) - target_cur.z;

// eye goal: one progress drives radius and height
eye_goal.xy = target_goal.xy +
              (cos(yaw), sin(yaw)) * lerp(10, 300, p);
eye_goal.z  = target_goal.z + lerp(3, 500, p);
eye_goal.z  = max(eye_goal.z, terrain(eye_goal.xy) + altitude);

// first-order smoothing (FUN_005e6960)
cur += (goal - cur) * clamp(dt * lerp(min, max, p), 0, 1);
smooth(yaw_current,    dt, (10,10));
smooth(target_current, dt, TargetHardness 6..2);
smooth(eye_current,    dt, PositionHardness 6..2);
// X/Y use the hardness pair; Z uses (1000,1000), i.e. instant

out.eye = eye_current; out.target = target_current;
```

Zoom and tilt are the **same axis**: `NAVIGATION_ZOOM`/`DEZOOM` register the
same handlers (indices 6/7) as `CameraRotateUp`/`Down`. `pitch` is derived,
not stored: `atan2(h(p), r(p))` = 16.7 deg at p=0, 53.5 at p=0.1, 59.0 at
p=1. The "tilt while moving" is the 10/s yaw low-pass (9 deg steady-state
lag), not roll. The Free Game save tail confirms the model: eye
`(551,512,84.7)` / target `(512,512,32)` on isl01 is exactly
`r(0.1)=39`, `h(0.1)=52.7`, yaw 0.

## Controls (`Data/Config/KeyMapper.ini`)

| Action | Keys |
| --- | --- |
| `CameraKeyUp/Down/Left/Right` | arrows: pan the target |
| `CameraRotateLeft/Right` | `Shift+Left`/`Delete`, `Shift+Right`/`PageDown` |
| `CameraRotateUp/Down` | `Shift+Up`/`Home`, `Shift+Down`/`End`; also zoom in/out |
| `NAVIGATION_TURNLEFT/TURNRIGHT` | same rotation state |
| `NAVIGATION_ZOOM/DEZOOM` | same progress axis (indices 6/7) |

`FUN_005257a0` registers the callbacks; `FUN_00527b80` gates them on the
controller state and writes one of the eight bytes through
`FUN_005e59b0`.

## Port

- `src/viewer/camera_config.{h,cpp}` reads `camera.ini [Config1]`.
- `src/viewer/main.cpp` keeps one authoritative `progress` (init 0.1) for the
  interactive park camera, derives radius/height, applies the 10/s yaw
  low-pass, and never adds roll.
- Zoom and `Shift+Up/Down` both change `progress`; arrows pan; the
  `NAVIGATION_PANEL` buttons use the same actions.
- Explicit `--camera-target` / `--distance` / `--yaw` / `--pitch` views keep
  the legacy spherical orbit for deterministic screenshots and tests.
- Tests: `tests/test_viewer_cpp.py::test_cpp_camera_retail_progress_default`,
  `test_cpp_free_camera_start`, `tests/test_config.py::test_camera`.

Retail pan is **per frame**, not per second: `0x5e6ae0(config_pair, p)` is a
plain `lerp(1, 20, p)` and `0x5e6a90` adds the unit direction times that step
to the target goal with no `dt` factor. The port multiplies it by 4 world
units/s as a feel approximation; the exact retail frame rate is not
calibrated.

## Serialization

The active camera's `vtable+0` / `+4` (`0x5e5f20` read, `0x5e5fb0` write)
serialize exactly four floats:

```text
float progress        (+0x158)
float yaw_goal[3]     (+0x124..+0x12c; only the z component is used)
```

The read is guarded by a stream version/size `>= 0xe`; after reading, yaw
current `+0x118` is copied from yaw goal `+0x124` (no smoothing on load).
The derived eye/target are not part of this block. The save tail's eye/target
is a separate camera-switcher record; the port's `--sav-park` applies it to
the orbit camera.

## Open

- Retail frame rate / exact effective pan speed (the code is per-frame).
- `TargetHardness`/`PositionHardness` clamp and transition semantics beyond
  the first-order form above.

## Evidence

- Ghidra: `0x5e57c0` camSwitcher builder, `0x5e5d10` ctor, `0x5e5b70`
  proxy, `0x5e5cd0`, `0x5e5960` update, `0x5e5bc0`, `0x5e5ff0` camera
  update, `0x5e6960` smoothing, `0x5e5995`/`0x5e59a3` / `0x5e59d0`,
  `0x5e5560` defaults, `0x5e5690` INI reader, `0x5e67f0`, `0x5e68ac`,
  `0x5e57a0` action registration, `0x527b80`, `0x5e59b0`, `0x529c50`,
  `0x529d00`, `0x52785a`, `0x4475e0`.
- `notes/camera-motion-tilt-recon.md` (full offsets, runtime cross-check),
  `notes/camera-rotation-recon.md` (keybindings, dispatch), and the
  `notes/camera-retail-*.png` reference screenshots.
