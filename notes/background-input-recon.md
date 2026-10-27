# Background virtual input / activation trace

Status: **menu interaction solved in the background; the park load still does
not complete.** The scheduler tick was gated on the D3D8 device state, the
mouse cursor readback used the wrong ring slot, and the WndProc shifted client
coordinates by the window frame. All three are fixed: Free Game -> IGUANO ->
Easy -> Start now runs through the menu (click sounds, `menu_outcome.tga`,
`loading_800x600.tga`, `ParkSoundDB.ini`). Level start resets the device to
exclusive fullscreen; forcing `Windowed=1` removes the Present hang, but the
loading loop still never reaches the world files.

## 2026-10-27: menu works, world load blocks in Present

### 1. Scheduler gate `0x004024db` (fixed)

`FUN_00402460` (APPLI per-frame update) tests the per-frame D3D8
`TestCooperativeLevel` result before running the task scheduler:

```text
004024cc CALL 0x00412230            ; app+0x1a = TestCooperativeLevel hr >= 0
004024d1 MOV EAX,[0x00674528]       ; app
004024d6 MOV CL,[EAX+0x1a]
004024d9 TEST CL,CL
004024db JZ  0x004024ef             ; 74 12  <- skipped while backgrounded
004024e7 MOV ECX,[EBP+0x4c]
004024ea CALL 0x00507530            ; scheduler: view update, UI, ...
```

The view update `FUN_00524610` is the first scheduler task (stub `0x00596550`,
vtable `0x642894+0x10`), so while the gate is closed the WndProc queues events
and nothing consumes them. The shim patches `0x004024db` to `EB 0A` (jump
straight to the tick) under `DINO_FORCE_INPUT_ACTIVE`. `app+0x1a` is also the
0x00010000 bit of the dword at `app+0x18`, which is why earlier memory diffs
read it as a flag.

Evidence: `gate=1 retail_view=<presents>` in `dino_hook.log`;
`build/scheduler-gate-3.out.log`, `build/scheduler-gate-5.out.log`.

### 2. UI event path

The WndProc (`0x5960c0`) forwards window messages to the view object
(vtable `0x642894`): `+0x1c` mouse move (`FUN_005bb4d0`), `+0x28` mouse down
(`0x5bb640`), `+0x2c` mouse up (`0x5bb600`), `+0x30` char (`0x5bb6c0`),
`+0x34` key (`0x5bb6e0`). The shim wraps these slots and counts them:

```text
virtual mouse: ui mouse_down wparam=0 element=00000000 ui=(395,234) calls=1
```

With the gate patch every scripted click arrives (9/9 in the x32dbg run,
4/4 in the direct probe) and the free-game flow completes the menu, plays
`clic#2/clic#3.wav`, loads `menu_outcome.tga`, `loading_800x600.tga` and
`ParkSoundDB.ini`.

### 3. Cursor ring readback (fixed)

`FUN_004458f0` writes the new absolute position into `state[slot]` using
`state[1-slot]` as the base; there are two 0x10 slots (`x` at `+0x0c`, `y` at
`+0x10`, index at `+0x294`). At `GetDeviceState` time the slot still has its
pre-poll value, so the *other* slot holds the last delivered position. Reading
`slot` instead of `1-slot` made the hook feed a stale delta and the cursor
never reached the target.

The UI cursor is `ViewManager+0x18/+0x1c`, written by `FUN_005bb4d0` from the
`WM_MOUSEMOVE` that `FUN_004457b0` posts, after the WndProc converts
client -> window coordinates (adds `clientOrigin - windowOrigin`; zero in
fullscreen, ~3x26 px in the bordered test window). The hook subtracts that
frame offset in `FUN_005bb4d0`, so the harness's posted move and the engine's
own ring move both land on the client coordinate:

```text
virtual mouse: cursor target=(395,234) current=(395,234) delta=(0,0)
virtual mouse: ui mouse_down ... ui=(395,234)
```

Evidence: `build/hook-probe-cursor-4.log`; `notes/harness/stack-probe.py`.

### 4. Level start resets to exclusive fullscreen; the load loop still stalls

After `loading_800x600.tga` + `ParkSoundDB.ini` the main thread stops making
progress. Stack samples show it either inside `IDirect3DDevice8::Present`
(`0x0040eb6a`, after `CALL [device_vtbl+0x3c]` in `FUN_0040eab0`) or cycling
through the loading-screen draw/present code without opening another file.

The Reset probe (`slot 14`, logged) shows why: at level start retail resets the
device to **exclusive fullscreen**, twice:

```text
Reset 800x600 fmt=23 (R5G6B5)   count=1 ms=0 swap=1(DISCARD)
      windowed=0 depth=1 flags=0x1(LOCKABLE) rate=75 interval=DEFAULT
Reset 800x600 fmt=22 (X8R8G8B8) ... same ...
```

Forcing `Windowed = 1` (and `rate = 0`) in the Reset hook under
`DINO_FORCE_INPUT_ACTIVE` removes the hard Present block: the thread keeps
cycling through Clear/Draw/Present and loader code instead of sitting in
Present. `DINO_SKIP_PRESENT=1` is not a substitute (dgVoodoo's queue fills and
a draw call blocks); `PresentationModel = discard` and null present rectangles
do not help either.

The load itself is still not progressing: x32dbg `CreateFileA` breakpoints stay
cold after `ParkSoundDB.ini` (no `.seq`, `.lnd`, `.mesh`, `.skin` opens) and the
input counters stay frozen, i.e. `FUN_00402460` never returns from the
park-start state. The app-update state machine (`app_data+0x20`; `0x29a` is the
in-house MPEG/movie state) or the loading-screen progress field is the next
thing to decode.

Ghidra reads `FUN_0051d1d0` as: open `loading_800x600.tga` -> `0x0051d34d
CALL 0x0040eab0(0,0,0)` (the present frame) -> `0x0051d375` LOD check
(`CMP [param_2+0x34], 3; JA invalid`) -> `0x0051d42c CALL 0x0053a400` (first
world I/O). So the stall is between `0x51d29e` and `0x51d42c`; the loader
returns 0 there and the one-shot start request (`0x0040256a` clears it) is
never retried. A first attempt to read `[app+0x48]+0x10`, `[app+0x60]+0x34`
and `[app+0x5c]+0x40` from the proxy sampled null/invalid pointers (the
notation needs a runtime read at `0x0051d375`, e.g. dump `[param_2+0x34]`),
so that field mapping is unverified.

Evidence: `notes/harness/stack-probe.py` (module/EBP walk),
`build/present-x32-1.out.log`, `build/probe-present-1.out.log`,
`build/probe-present-2.out.log`, `build/probe-nullrect-1.out.log`,
`build/probe-discard-1.out.log`.

### 5. Focus steal

When the free game starts, retail brings its window to the foreground
(`game_fg=True` from the Start click on). `ShowWindow`/`SetWindowPos` IAT shims
and `WS_EX_NOACTIVATE` did not prevent it (likely dgVoodoo/D3D activation), so
those shims were dropped. Any harness run that starts a park takes the user's
foreground window until it is killed; restore the previous window afterwards.

## Working shim (2026-10-27)

The proxy (`src/hook/dinput8_proxy.cpp`, env `DINO_FORCE_INPUT_ACTIVE=1` only)
now does three things:

1. `IDirectInputDevice8::SetCooperativeLevel` (mouse vtable slot 13) is forced
   to `DISCL_BACKGROUND | DISCL_NONEXCLUSIVE`, clearing `DISCL_FOREGROUND` and
   `DISCL_NOWINKEY`. Log:
   `virtual mouse: SetCooperativeLevel flags=0x6 -> 0xa hr=0x0` and the
   former `0x16 -> 0x1a hr=0x80004001` (E_NOTIMPL) is gone.
2. A 5 ms thread forces the `miInputDx8` poll flags (`[0x675104]+8` mouse,
   `+0x30` keyboard) and `app+0x54` (active). It also patches the app-loop
   scheduler gate (`0x004024db`, see below), which makes the retail loop run
   the view update itself. The manual drive of `FUN_00524610` from the thread
   is kept only as a fallback when the gate patch fails; `app+0x1a`
   (`FUN_00412230`, the D3D8 `TestCooperativeLevel` result) is the detector.
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

## Earlier blocker (resolved by the gate patch): the menu never advanced

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

1. ~~Find the deactivation branch in the app loop and patch it~~ - done: the
   gate is `0x004024db` (`app+0x1a`), patched to `EB 0A` (section 1 above).
2. One foreground run (`-PhysicalInput`) to unblock the P3 provider probe
   (`start-campaign.ps1 -ProviderTrace`); this needs the user's explicit OK.
   The foreground path is documented as working (state calls 42+).
3. Find why the first Present after the loading screen blocks (section 4).

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
