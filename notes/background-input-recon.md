# Background virtual input / activation trace

Status: **blocked on retail activation**, not on DirectInput delivery.

The campaign x32dbg harness now reaches every menu screen and writes real
frames (`notes/harness/campaign-*.png`), but the synthetic clicks are
ignored by the retail UI while the game is in the background.

## Evidence

- `run-test/dino_hook.log` shows the mouse device is captured and the button
  state is delivered:

  ```text
  virtual mouse: SetCooperativeLevel patched
  virtual mouse: SetCooperativeLevel flags=0x6 -> 0xa hr=0x0
  virtual mouse: SetCooperativeLevel flags=0x16 -> 0x1a hr=0x80004001
  virtual mouse: state button=down
  virtual mouse: left down (1)
  virtual mouse: state button=up
  ```

- `notes/harness/campaign-main.png`, `campaign-list.png` and
  `campaign-park.png` still show the main menu; the Campaigns click does not
  open the campaign select page.
- `build/harness-coop-console.log` reports:
  `park not reached: the clicks reached DirectInput ... but the retail menu
  did not act on them while the game is in the background`.

## What was tried

- `SetCooperativeLevel` (IDirectInputDevice8 vtable slot 13) forced to
  `DISCL_BACKGROUND | DISCL_NONEXCLUSIVE` with `DISCL_FOREGROUND` cleared.
  The call succeeds (`flags=0x6 -> 0xa hr=0`), but the UI still ignores the
  clicks.
- The virtual cursor ring was inspected in `FUN_004458f0`:
  - x is stored at `input + slot*0x10 + 0xc`, but the delta base is
    `input + 0x1c - slot*0x10`;
  - y is stored at `input + (slot+1)*0x10`, delta base
    `input + (2 - slot)*0x10`.
  `apply_virtual_cursor` reads the y base correctly but the x base was the
  newly written slot; changing it to the game's base did not make the menu
  act and was reverted.
- The harness posts `WM_ACTIVATE(WA_ACTIVE)` and `WM_SETFOCUS` before each
  click (`Wake-GameWindow`); the retail WndProc (`0x4235b0`) sets
  `app+0x54`/`app+0x31cec` on those messages. The window is restored with
  `SW_SHOWNOACTIVATE`, never foregrounded.

## Conclusion

`DISCL_BACKGROUND` is necessary but not sufficient. The retail UI still
gates clicks on the window activation/foreground state, or reads the cursor
position from a path other than the DirectInput ring. The next experiment is
one of:

1. a foreground run (`-PhysicalInput`, needs the user's OK) to prove the
   provider probe and separate activation from menu flow;
2. a deeper in-process activation shim (WndProc/activation flag or
   `GetForegroundWindow`/cursor path) in `src/hook/dinput8_proxy.cpp`.

The `SetCooperativeLevel` hook change is uncommitted for hook/oracle review.
The P3 target-provider breakpoint (`0x56e455`) and `-ProviderTrace` switch
are ready; they fire once the campaign actually reaches the park.
