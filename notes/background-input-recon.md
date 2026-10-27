# Background virtual input / activation trace

Status: **input polling solved; the retail UI still does not act on clicks while
backgrounded.** DirectInput now keeps being polled at ~64 Hz in the
background and every scripted click is delivered, but the engine's
deactivation path also skips the main-loop message/UI processing, so the menu
does not advance.

## Working shim (2026-10-27)

The proxy (`src/hook/dinput8_proxy.cpp`, env `DINO_FORCE_INPUT_ACTIVE=1` only)
now does three things:

1. `IDirectInputDevice8::SetCooperativeLevel` (mouse vtable slot 13) is forced
   to `DISCL_BACKGROUND | DISCL_NONEXCLUSIVE`, clearing `DISCL_FOREGROUND` and
   `DISCL_NOWINKEY`. Log:
   `virtual mouse: SetCooperativeLevel flags=0x6 -> 0xa hr=0x0` and the
   former `0x16 -> 0x1a hr=0x80004001` (E_NOTIMPL) is gone.
2. A 5 ms thread forces the `miInputDx8` poll flags (`[0x675104]+8` mouse,
   `+0x30` keyboard) and `app+0x54` (active), and drives the retail view
   update `FUN_00524610` (input poll + key dispatch) while the engine is
   deactivated. `app+0x18` bit `0x00010000` is 0 in the deactivated state
   (active/stuck memory diff; the bit is recomputed by the engine every
   frame), which is used as the inactivity detector.
3. `dino_input.txt` is re-read at most every 50 ms (instead of on every poll)
   so the x32dbg `CreateFileA` breakpoint storm is bounded. The 100 ms click
   hold still lands in a poll.

Verified in a direct (no debugger) run, fully backgrounded:

```text
virtual mouse: force sample slot_changes=2100 state_calls=2454 ... view_updates=1963
virtual mouse: state button=down / state button=up   (one pair per scripted click)
```

and under x32dbg (`build/force-active-6-x32.log`): 9 clicks -> 9 down/9 up,
`state_calls=4120`, `view_updates=2655`, `stuck=0`, no crash.

## Remaining blocker: the menu never advances

The x32dbg harness's loader/Entrance/save breakpoints stay completely cold
after the clicks (0 hits in `build/force-active-6-x32.log` and
`build/force-active-7b-x32.log`, even after a 235 s wait). Save files are
unchanged. So the clicks reach DirectInput but not the menu action path.

Two related observations:

- With the shim the game believes it is active and starts the 116.5 s
  `data/video/generic.mpg` menu background movie (quartz/DirectShow graph);
  earlier inactive runs skipped it. Waiting out the movie (235 s) does not
  help.
- The memory diff shows the engine's deactivation path also sets the frame
  timing to 100 ms / 10 ms (`app+0x44`/`+0x48`) and leaves the view update
  counter/deadline untouched; the main loop still runs (~64 Hz) but does not
  dispatch the view/menu update.

Next options, in order:

1. Find the deactivation branch in the app loop (the code that clears
   `app+0x18` bit `0x10000` and skips the view update) and either patch the
   branch or call the **view manager message dispatcher** (`0x5960c0`,
   registered from `FUN_00404990`) on the game thread while backgrounded.
   The input update alone is not enough.
2. One foreground run (`-PhysicalInput`) to unblock the P3 provider probe
   (`start-campaign.ps1 -ProviderTrace`); this needs the user's explicit OK.
   The foreground path is documented as working (state calls 42+).

## Earlier evidence (still valid)

- `run-test/dino_hook.log` before the shim showed the mouse device captured
  and `state button=down/up` delivered in foreground runs.
- The retail WndProc (`0x4235b0`) handles `WM_SETFOCUS` (sets `app+0x54`) and
  `WM_NCACTIVATE` (calls `app+0x10`/`app+0x14`), but not `WM_ACTIVATE` or
  `WM_ACTIVATEAPP`; the engine pause is above that.
- `FUN_005234e0` registers the PauseGame action; `FUN_005257a0` registers the
  park camera/pause bindings. The debug string
  `L'action PauseGame n'a pas de CallBack associé` comes from the key mapper
  (`FUN_00419330`), not from the deactivation pause itself.

## Evidence paths

- Hook: `src/hook/dinput8_proxy.cpp` (`DINO_FORCE_INPUT_ACTIVE`), run-test DLL
  refreshed with `scripts/setup-test-runtime.ps1`.
- Direct runs: `build/force-active-2..5` logs; x32dbg no-trace runs:
  `build/force-active-6-x32.log`, `build/force-active-7b-x32.log`; harness
  artifacts under the gitignored `notes/harness/force-active-*`.
- Memory diff: `build/dino_hook-run3..run4-*.log` (app+0x18/0x1A,
  app+0x44/0x48, view+0x60).
- x32dbg/campaign scripts: `scripts/x32dbg/play-freegame.ps1`,
  `scripts/x32dbg/start-campaign.ps1` (`-ProviderTrace` at `0x56e455`).
