---
name: Dino retail run
description: Run the retail game headlessly or windowed for evidence, without stealing focus or overwriting tracked screenshots. Use for x32dbg traces, campaign checks, save probes, and runtime confirmation.
---

# Dino retail run

## Normal play (interactive)

```powershell
.\scripts\play-windowed-game.ps1
```

Uses `run-window/` (Data junction, own `save/`), a normal bordered 800x600
window, starts behind the current window, and only captures the mouse after a
left click inside the game. Click outside to release.

## Headless / x32dbg (default)

```powershell
.\scripts\x32dbg\play-freegame.ps1 -Seconds 30
.\scripts\x32dbg\start-campaign.ps1 -UnlockCampaigns
.\scripts\play-freegame-direct.ps1 -Seconds 25 -Shot notes\harness\park-hook.png
```

- Uses `run-test/`, virtual input (`DINPUT8.dll` + `run-test/dino_input.txt`),
  sends the game to the back, restores the previous foreground window, and
  uses `PrintWindow` for screenshots. It must not call `SetCursorPos`,
  `mouse_event` or `SetForegroundWindow`.
- **Input caveat:** retail uses DirectInput `DISCL_FOREGROUND`. A fully
  backgrounded game logs `virtual mouse: state calls=0` and ignores virtual
  clicks. Screenshots still work; keep the game active for click tests until
  the proxy shims activation/cooperative level.
- Rebuild the proxy with `scripts\build-hook.ps1`, then refresh `run-test/`
  with `scripts\setup-test-runtime.ps1`.
- Screenshots default to `notes/harness/` (gitignored). Do not overwrite
  tracked `notes/*.png`; copy a final shot deliberately when it is evidence.

## Choosing click coordinates

`docs/platform/ui-coordinates.md` maps every interactive control on every
retail screen (800x600 client space). `docs/platform/ui-coordinates.json` is
the machine-readable form.

```powershell
.venv\Scripts\python.exe tools\formats\ui_map.py find OPEN_LABORATORY
.venv\Scripts\python.exe tools\formats\ui_map.py find --screen park-hud
.venv\Scripts\python.exe tools\formats\ui_map.py find --screen park-options --active
```

Use the named `Click` point; do not hand-guess or reuse a coordinate from a
different panel. `On open=no` means the control is behind a tab/switch that
must be selected first (for example the Save tab, a lab gene tab, or a
construction category).

## Campaign and save state

- `run-test/save/dino.dat` is the campaign unlock dword: `0F 00 00 00`
  unlocks campaigns 1-4 for Cpg04 tests; restore `01 00 00 00` after.
- Live retail runs overwrite `run-test/save/auto/auto.sav`. Restore the test
  reference from `run-test/save/auto/slot001.sav` (7,116 bytes) before save
  tests.
- A campaign briefing is modal: dismiss it before expecting simulation hits.

## When something is locked

- Stop only the exact PID (`Get-Process DinoIsland | Select Id,Path`), not all
  game/viewer processes; another lane may be testing.
- If the window never appears, check `run-test/dgVoodoo.conf` for
  `FullScreenMode=false` and `run-test/DINPUT8.dll` for the virtual-mouse build.
- The legacy fullscreen path is `-PhysicalInput` / `run/`; use it only when a
  test genuinely cannot run virtually, and document why.
