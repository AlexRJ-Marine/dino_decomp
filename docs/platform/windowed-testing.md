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

## Normal windowed play

`scripts/play-windowed-game.ps1` creates `run-window/` on first launch (Data
junction, its own `save/`) and starts the game with a bordered window:

```powershell
.\scripts\play-windowed-game.ps1
```

The retail application creates a `WS_POPUP` window (`style 0xB4080000`) with no
title bar. `scripts/Set-WindowedStyle.ps1` changes it to a normal overlapped
window (`0x16CA0000`: caption, system menu, minimize/close, visible) and
corrects the client area back to 800x600. The game remains windowed; Alt-Enter
does not switch it to fullscreen.

Normal play sets `DINO_MOUSE_CLICK_CAPTURE=1`. The proxy suppresses mouse
axes/buttons while the gate is inactive. The first left click inside the game
window activates input; `WindowFromPoint` is used, so the game does **not**
need to be foreground and the click does not raise it. The activating click is
passed through so it can also operate the UI. A left click outside the game
window releases the gate. The cursor is not clipped or warped.
`-NoClickCapture` disables the gate.

The normal-play launcher restores focus to whatever window was active before
launch and sends the game to the bottom of the z-order.

## Virtual DirectInput mouse

Set `DINO_VIRTUAL_MOUSE=1` before launching the game. The `DINPUT8.dll` proxy
then:

1. patches `IDirectInput8::CreateDevice` (vtable slot 3);
2. records every `GUID_SysMouse` device;
3. patches that device's `GetDeviceState` (slot 9) and `GetDeviceData`
   (slot 10);
4. reads `dino_input.txt` on each poll.

The harness records the previous foreground window, sends the game to the
back, and restores the previous window. Screenshots use `PrintWindow`, not a
screen grab. A minimized game is restored with `SW_SHOWNOACTIVATE`.

**Input limitation (updated 2026-10-27):** retail calls
`SetCooperativeLevel(..., 6)` = `DISCL_FOREGROUND | DISCL_NONEXCLUSIVE`
(mouse; keyboard uses `0x16`). The proxy forces the mouse device to
`DISCL_BACKGROUND | DISCL_NONEXCLUSIVE` (and strips `DISCL_NOWINKEY`), and
with `DINO_FORCE_INPUT_ACTIVE=1` it also keeps the retail input update
(`FUN_00524610`) running at ~60 Hz while the window is deactivated. Result:
`run-test/dino_hook.log` now shows `state_calls` growing at ~64/s and one
`state button=down/up` pair per scripted click, with no crash.

**However, the menu still does not act on those clicks in the background.**
The engine's deactivation path also skips the main-loop message/UI update;
the x32dbg loader/Entrance breakpoints stay cold after 9 delivered clicks
(even after the 116.5 s `generic.mpg` menu movie). Do not cite the background
*click flow* as working yet; only input delivery is verified. The Ghidra
evidence is `0x4454f1` (`SetCooperativeLevel`), `0x44591f`
(`GetDeviceState`), `0x44592e` (retry `Acquire`), `FUN_00524610`
(view/input update), and WndProc `0x423609` (`WM_SETFOCUS` sets the active
byte). The proxy drives `FUN_00524610` only when `app+0x18` bit `0x00010000`
is clear (the deactivated state from the active/stuck memory diff); see
`notes/background-input-recon.md`.

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
complete per-screen button map is generated from the overlay INIs and kept in
[`ui-coordinates.md`](ui-coordinates.md) (human) and
[`ui-coordinates.json`](ui-coordinates.json) (machine). Query a control
instead of guessing:

```powershell
.venv\Scripts\python.exe tools\formats\ui_map.py find OPEN_LABORATORY
.venv\Scripts\python.exe tools\formats\ui_map.py find --screen park-hud
```

The original menu cross-check is in `notes/ui-recon.md`; `tests/test_ui_map.py`
pins the menu, HUD, save and campaign points used by the harness.

## x64dbg version notes (2026.05.27)

`scripts/setup-tools.ps1` installs x64dbg 2026.05.27. Two behaviours differ
from the build the harness was written against:

- It stops at TLS callbacks by default (`dinput8.dll` has two), so the
  harness's fixed number of `run` commands left the game paused before its
  window opened ("no main window handle"). `setup-tools.ps1` sets
  `TlsCallbacks=0` in `headless\headless.ini`.
- `log` drops the spaces from its text (`log PARKREADY fence={p:ecx}` prints
  `PARKREADYfence=...`). Match marker words without trailing spaces; parsers
  of older logs must accept both forms.

Windows PowerShell 5.1 also prefixes the first stdin line sent to the
debugger with a UTF-8 BOM; `start-campaign.ps1` sends a blank line first.
`-FileTrace` on `start-campaign.ps1` logs every `CreateFileA` path (`CF_...`)
during the park load.

## Verification

The click sequence was exercised with the default `play-freegame.ps1`:

- Free Mode -> IGUANAO -> easy -> Start
- park loaded (`entrance_lod1.skel/skin/anim/mesh` in the trace)
- lab, options and save panels opened
- the physical cursor was not moved

**Background input and the park load are verified.** With
`DINO_FORCE_INPUT_ACTIVE=1` the proxy keeps DirectInput polling, delivers
every scripted click while the window is behind other windows
(`state_calls` ~64/s, down/up per click), and patches the app-loop scheduler
gate (`0x4024db`). For the level-start Reset it also forces 16-bit back
buffers to X8R8G8B8 and activates the game window on the Reset thread.
Without the last two, dgVoodoo returns `D3DERR_DEVICELOST` and mcLib opens a
modal "Unable to restore previous video mode" dialog that looks exactly like
a frozen load. The free-game park now loads and renders from the background
harness (`notes/park-retail-windowed.png`). `DINO_LOG_ERRORS=1` logs every
`MessageBoxA`; `DINO_AUTO_DISMISS_ERRORS=1` dismisses it (`IDIGNORE`). See
`notes/background-input-recon.md` section 4b.

Evidence:

- `notes/windowed-main-menu.png` - retail menu in a window
- `notes/windowed-freegame-virtual.png` - Free Game page after a virtual click
- `notes/windowed-park.png` - park reached through the virtual-input path
- `notes/park-retail-windowed.png` - free-game park after the windowed Reset fix
- `notes/x32dbg-windowed-freegame.out.log` - loader trace from the run
- `notes/harness/campaign-sim.png` - Cpg04 running after the normal clicking campaign path

### Bordered-window frame offset (all UI managers)

The retail `moWidgetManager::OnMouseMove` (`0x5bb4d0`) is installed in three
vtables: the ViewManager (`0x642894`, slot `0x6428b0`), the park interface
manager (`0x642b1c`) and the campaign manager (`0x647098`). The WndProc
converts the client coordinate to window space, adding the bordered-window
frame offset (~3x29 px in the test runtime). The proxy's
`hook_view_mouse_move` subtracts that offset, but it was only patched into
the ViewManager slot: menu clicks were correct while park/briefing HUD clicks
landed ~26 px low (a briefing click could hit the objectives button and
re-pause the sim). `install_view_probes` now patches all three slots. Verified
by the normal (clicking) `start-campaign.ps1 -CampaignRow 3` path: it advances
the briefing and reaches a running Cpg04 park (`notes/harness/campaign-sim.png`).

Limitations:

- only the left mouse button is scripted;
- keyboard injection is not implemented yet;
- `dino_input.txt` is polled from the game's working directory;
- the Reset fix briefly activates the game window on the Reset thread (the
  harness restores the previous foreground afterwards);
- the dgVoodoo2 watermark remains visible in windowed screenshots.
