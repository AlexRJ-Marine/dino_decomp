# Quality-of-life layer

Status: **QOL-1 and QOL-3 complete** — window modes/persistence and portable
camera key rebinding. QOL-2 (UI scaling) waits for the overlay/UI layer.

## Window modes

- Default: resizable windowed, 1280×720.
- `--fullscreen`: SDL3 fullscreen (`SDL_WINDOW_FULLSCREEN`).
- `--borderless`: borderless window (`SDL_WINDOW_BORDERLESS`).
- `--windowed`: force windowed mode even if the config says otherwise.
- Runtime: **F11** toggles fullscreen, **F10** toggles borderless.

The viewport always uses the actual drawable size; the camera keeps a vertical
33° FOV, so widescreen shows more horizontally rather than stretching.

## Config persistence

Default config: `dino_viewer.cfg` in the working directory (gitignored).

```ini
width=1280
height=720
fullscreen=0
borderless=0
vsync=1
```

- The config is loaded before CLI parsing; CLI flags override it.
- It is saved on clean exit with the final window size and modes.
- `--config FILE` chooses another path; `--no-save-config` disables saving.
- `--validate` returns before creating a window and does not write config.

## Key rebinding

Default key map: `dino_keys.cfg` (gitignored).

```text
orbit_left=LEFT      orbit_right=RIGHT
orbit_up=UP          orbit_down=DOWN
zoom_in=PLUS         zoom_out=MINUS
reset=R
fullscreen=F11       borderless=F10      mute=M       quit=ESC
```

- `--keys FILE` chooses another map.
- `--rebind ACTION=KEY` (repeatable) changes one binding.
- `--list-keys` prints the effective map and exits.
- `--no-save-keys` avoids writing the file.
- Key names: `LEFT`, `RIGHT`, `UP`, `DOWN`, `ESC`, `SPACE`, `A`..`Z`,
  `0`..`9`, `F1`..`F12`, `PLUS`, `MINUS`, `KP_PLUS`, `KP_MINUS`, etc.
- Rebinds are applied before the window starts and persist to the file.

Example:

```powershell
.\build\mingw\dino_viewer.exe --keys dino_keys.cfg `
  --rebind orbit_left=J --rebind reset=H --list-keys
```

The original `Data/Config/KeyMapper.ini` action names can be mapped to these
port actions as game actions are ported.

## Evidence

| File | Size |
| --- | --- |
| `notes/qol-16x9.png` | 640×360 (16:9) |
| `notes/qol-4x3.png` | 640×480 (4:3) |

Tests (`tests/test_viewer_cpp.py`):

- `test_cpp_config_persist`: writes `width=400`/`height=300`, relaunches
  without size flags, checks the persisted 400×300 screenshot.
- `test_cpp_widescreen_and_4x3`: borderless 16:9 and 4:3 screenshots.
- `test_cpp_keymap_persist`: rebinds `orbit_left=J` and `reset=H`, checks the
  file and the reloaded map.
- `test_cpp_keymap_rejects_unknown_action`.

## Next

- **QOL-2**: UI scaling for the original overlay layout (needs the overlay/UI
  layer in Phase 3).
- **Phase 3**: hook the original exe and replace loaders one at a time.
