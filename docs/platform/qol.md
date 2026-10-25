# Quality-of-life layer

Status: **QOL-1 complete** — window modes, resolution persistence and widescreen
framing. QOL-2/3 are next.

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

## Evidence

| File | Size |
| --- | --- |
| `notes/qol-16x9.png` | 640×360 (16:9) |
| `notes/qol-4x3.png` | 640×480 (4:3) |

Tests (`tests/test_viewer_cpp.py`):

- `test_cpp_config_persist`: writes `width=400`/`height=300`, relaunches
  without size flags, checks the persisted 400×300 screenshot.
- `test_cpp_widescreen_and_4x3`: borderless 16:9 and 4:3 screenshots.

## Next

- **QOL-2**: UI scaling for the original overlay layout (needs the overlay/UI
  layer in Phase 3).
- **QOL-3**: portable key map and rebinding (camera actions first, then game
  actions as they are ported).
