# Windowed testing and virtual input

Status: **default harness path.** The retail game runs in an 800x600 window
under dgVoodoo2, and the x32dbg harness clicks through the menu without moving
the physical mouse. `-PhysicalInput` restores the legacy fullscreen path.

## Why this exists

The legacy harness used:

```powershell
SetCursorPos(...)
mouse_event(0x0002, ...)
```

Those calls move the real Windows cursor, so an automated campaign run takes
over the mouse. The debugger is not the problem: `headless.exe` is already
non-interactive. The input injection is.

Dino Island reads mouse buttons through DirectInput8, not `WM_LBUTTONDOWN`.
The main window procedure at `0x4235b0` handles `WM_MOUSEMOVE` (`0x200`),
`WM_NCACTIVATE`, `WM_SYSCOMMAND` and a few others, but not `WM_LBUTTONDOWN`
(`0x201`) or `WM_LBUTTONUP` (`0x202`). Simply switching to `PostMessage`
clicks therefore does not work.

## Windowed runtime

`scripts/setup-test-runtime.ps1` creates `run-test/`:

- `run-test/Data` is a directory junction to `run/Data` (no asset copy).
- The retail executable, patch, hook DLL and dgVoodoo2 wrappers are copied.
- `run-test/save` is seeded from `run/save` on first creation.
- `dgVoodoo.conf` is changed to:

```ini
FullScreenMode                 = false
AppControlledScreenMode        = false
CaptureMouse                   = false
FreeMouse                      = true
CenterAppWindow                = true
EnableInactiveAppState         = true
```

That gives a normal 800x600 client window and stops dgVoodoo2 from capturing
or confining the cursor. `run-test/` is gitignored.

```powershell
.\scripts\setup-test-runtime.ps1
.\scripts\play-windowed.ps1 -Seconds 8 -Shot notes\windowed-main-menu.png
```

## Virtual DirectInput mouse

Set `DINO_VIRTUAL_MOUSE=1` before launching the game. The `DINPUT8.dll` proxy
then:

1. patches `IDirectInput8::CreateDevice` (vtable slot 3);
2. records every `GUID_SysMouse` device;
3. patches that device's `GetDeviceState` (slot 9) and `GetDeviceData`
   (slot 10);
4. reads `dino_input.txt` on each poll.

The harness also posts `WM_MOUSEMOVE` to update the game's own cursor/hover.
It writes the target and button state to `dino_input.txt`:

```text
395 234 1     # target client x,y, left button down
395 234 0     # same target, left button up
```

The proxy reads `miInputDx8` at `0x675104`, gets the current cursor from the
three-slot ring (`+0x294`, x at `slot*0x10+0xc`, y at `(2-slot)*0x10`), and
feeds the relative `lX/lY` deltas that `FUN_004458f0` adds to the game cursor.
It then writes `0x80` into `DIMOUSESTATE.rgbButtons[0]` while the button is
held. No `SetCursorPos`, `mouse_event` or `SendInput` is called.

The proxy does not install any of this unless `DINO_VIRTUAL_MOUSE=1`, so normal
`run/` play is unchanged.

## Harness usage

The windowed + virtual path is the default; no extra switches are needed.

Free Game through the retail menu:

```powershell
.\scripts\x32dbg\play-freegame.ps1 -Seconds 30
```

Campaign trace:

```powershell
.\scripts\x32dbg\start-campaign.ps1 -UnlockCampaigns
```

The legacy fullscreen + physical-click path is still available:

```powershell
.\scripts\x32dbg\play-freegame.ps1 -PhysicalInput -Seconds 30
.\scripts\x32dbg\start-campaign.ps1 -PhysicalInput -UnlockCampaigns
```

`-VirtualMouse` is kept as a compatibility alias; it is the default. The
standalone helpers default to `run-test` as well. The same defaults apply to
`trace-bad.ps1`, `trace-calibration.ps1`, `run-headless.ps1`,
`probe-game.ps1` and `play-freegame-direct.ps1`; pass `-PhysicalInput` to any
of them for the legacy fullscreen path.

Harness screenshots are written to `notes/harness/` (gitignored) by default;
pass `-ShotDir` to change it. Existing tracked `notes/*.png` evidence is not
overwritten by a run.

Single click / screenshot:

```powershell
.\scripts\x32dbg\Send-VirtualClick.ps1 -X 395 -Y 234 -GameDir run-test `
  -Shot notes\windowed-freegame.png

.\scripts\x32dbg\Capture-Window.ps1 -Path notes\windowed-park.png
```

Coordinates are client coordinates in the retail 800x600 design space. The
existing menu coordinates from `notes/ui-recon.md` apply unchanged.

## Verification

The full path was exercised with the default `play-freegame.ps1` (no
`-GameDir` or `-VirtualMouse`):

- Free Mode -> IGUANO -> easy -> Start
- park loaded (`entrance_lod1.skel/skin/anim/mesh` in the trace)
- lab, options and save panels opened
- the physical cursor was not moved

Evidence:

- `notes/windowed-main-menu.png` - retail menu in a window
- `notes/windowed-freegame-virtual.png` - Free Game page after a virtual click
- `notes/windowed-park.png` - park reached through the virtual-input path
- `notes/x32dbg-windowed-freegame.out.log` - loader trace from the run

Limitations:

- only the left mouse button is scripted;
- keyboard injection is not implemented yet;
- `dino_input.txt` is polled from the game's working directory;
- the game must be foreground for DirectInput to poll, so the helper briefly
  brings the window forward (but never moves the pointer);
- the dgVoodoo2 watermark remains visible in windowed screenshots.
