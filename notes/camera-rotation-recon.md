# Retail camera rotation

Status: **bindings, dispatch and speed solved; the active-camera integration
is the remaining CAM-1 unknown.**

## Bindings (`Data/Config/KeyMapper.ini`)

| Action | Keys |
| --- | --- |
| `CameraRotateLeft` | `Shift+Left`, `Delete` |
| `CameraRotateRight` | `Shift+Right`, `PageDown` |
| `CameraRotateUp` | `Shift+Up`, `Home` |
| `CameraRotateDown` | `Shift+Down`, `End` |
| `CameraKeyUp/Down/Left/Right` | `Up` / `Down` / `Left` / `Right` (target pan) |

The `NAVIGATION_TURNLEFT` / `NAVIGATION_TURNRIGHT` UI buttons use the same
rotation state; `NAVIGATION_ZOOM` / `DEZOOM` are the separate dolly controls.

## Dispatch

`FUN_005257a0` registers the action callbacks:

```text
CameraKeyUp      0x525e00 -> FUN_00527b80(0)
CameraKeyDown    0x525e10 -> FUN_00527b80(1)
CameraKeyLeft    0x525e20 -> FUN_00527b80(2)
CameraKeyRight   0x525e30 -> FUN_00527b80(3)
CameraRotateLeft 0x525e40 -> FUN_00527b80(4)
CameraRotateRight 0x525e50 -> FUN_00527b80(5)
CameraRotateUp   0x525e60 -> FUN_00527b80(6)
CameraRotateDown 0x525e70 -> FUN_00527b80(7)
PauseGame        0x525e80 -> camera/navigation controller
```

`FUN_00527b80` runs the handler only when the camera/navigation controller is
in its normal state (`this+8 == 8` and not paused), then calls
`FUN_0052a650(index)` -> `FUN_005e59b0(camera, index)`:

```text
camera+0x0c = 1
camera[0x34 + index] = 1
```

`FUN_00527b60`/`FUN_00527b70` write `camera+8` (the "no key" sentinel is
`8`). `FUN_005e5960` (`camSwitcher::Update`) clears `+0x0c`, calls
`FUN_005e5bc0(dt)` (which delegates to the active camera object's
`vtable+8` at `camSwitcher+0x20`/`+0x24`), then `FUN_005e6930()` and
`FUN_005e5ce0()`, and copies the resulting eye/target state.

## Speeds and limits (`camera.ini` `[Config1]`)

- `RotationSpeedMin/Max = 1.570796` rad/s (`pi/2`, i.e. 90 deg/s)
- `MoveSpeedMin/Max = 1 / 20` (target pan)
- `ProgressSpeedMin/Max = 0.05 / 0.2` (dolly/zoom)
- `ElevationMin/Max = 3 / 500`
- `DistanceMin/Max = 10 / 300`
- `TargetHardnessMin/Max = 6 / 2`, `PositionHardnessMin/Max = 6 / 2`

The defaults are initialised in `FUN_005e5560`; `FUN_005e5690` reads the INI
into the same fields.

## What is still open

`FUN_00529d00` (`pvwParkObserver::Update`) consumes only the four **pan**
flags at `+0x1d..+0x20`, moves the target and applies the terrain-height
clamp. The four **rotation** flags (`+0x34+4..7`) are consumed by the active
camera object's update, which is the missing piece:

- the object's vtable/update at `camSwitcher+0x20`/`+0x24` (rotation axis,
  sign, and yaw/elevation coupling);
- `TargetHardness`/`PositionHardness` smoothing and its time constant;
- the `TURNLEFT`/`TURNRIGHT` UI release path (`LAB_00525ce0`).

## Port status

The current port's mouse-drag rotation is a placeholder, not the retail
binding. `CAM-1` should use `Shift+arrows` and the UI `TURNLEFT`/`TURNRIGHT`
buttons with `pi/2` rad/s, keep `C` free-fly as a debug mode, and verify the
rotation against the retail windowed run. Evidence addresses:
`0x005257a0`, `0x00527b80`, `0x005e59b0`, `0x005e5960`, `0x005e5bc0`,
`0x00529d00`, `0x005e5560`.
