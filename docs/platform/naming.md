# Naming and layout conventions

Status: current. These rules describe the tree as it is; they exist so a new
file lands where a reader expects it. They are style, not architecture — the
layering that matters is "what may include SDL/OpenGL".

## Directories under `src/`

| Directory | Holds | May use SDL/GL |
| --- | --- | --- |
| `formats/` | Retail file readers/writers (`.mesh`, `.sav`, `.wld`, …), no game state | **no** |
| `world/` | Simulation and park state: terrain, build grid, visitors, economy, employees, shows, campaign, port save | **no** |
| `ui/` | Widgets, menus, HUD layout and state, INI/overlay parsing | **no** |
| `render/` | GPU: GL function table, shaders, mesh upload, scene/text drawing | yes |
| `audio/` | Sound and music players | yes (SDL audio) |
| `app/` | Process shell: CLI, window, frame loop, boot, reports, asset lookup | yes |
| `hook/` | The 32-bit `DINPUT8.dll` proxy injected into the retail exe | yes (Win32) |
| `third_party/` | Vendored third-party code, unmodified | — |

The three no-SDL directories are a hard rule: a file there that starts
including `SDL3/` or `GL/` is in the wrong place. It keeps the simulation
testable without a window and reusable outside the port.

## Files

- C++: `snake_case.h` / `snake_case.cpp`.
- One primary type per header where the type is the point
  (`occupancy_grid.h` → `OccupancyGrid`); free-function modules are named for
  their job (`asset_paths.cpp`, `ui_geometry.cpp`).
- Abbreviations only where the project already uses them for retail formats:
  `bbi`, `bfi`, `bwi`, `sav`, `wld`, `mesh`, `skin`, `skel`, `anim`. Do not
  invent new ones.
- Python tests: `tests/test_<topic>.py`; tests that launch the port binary are
  `tests/test_<topic>_cpp.py`.

## Types

- `PascalCase`, descriptive. `OccupancyGrid`, `ParkClock`, `RetailParkCamera`.
- Do not suffix `Manager` unless the type really owns a collection's lifetime.
- When a port type mirrors a retail class, keep the retail name in a comment
  (`// stFence`), never in the identifier. The port is readable C++, not a
  transliteration of the binary.

## Functions

- `snake_case`, verb first, from a small closed set: `load_`, `read_`,
  `write_`, `save_`, `parse_`, `build_`, `make_`, `apply_`, `update_`,
  `handle_`, `draw_`, `render_`, `run_`.
- `try_<verb>` returns `bool` and leaves state unchanged when it fails
  (`try_construct`).
- Predicates start with `is_` / `has_` / `can_` (`is_loaded`, `has_room`).
- A method named `update_<thing>()` is called once per frame for that thing;
  `boot_<area>()` is one startup step; `handle_<event>()` reacts to input.

## State and constants

- Fields `snake_case`. Private class state gets a trailing underscore
  (`loaded_`, `dialog_text_` in `ui::Park`). `ui::Menu` predates the rule and
  uses bare names; new code follows `ui::Park`.
- Compile-time constants `kCamelCase` (`kSkyboxHalfSize`).
- `enum class` values `PascalCase` (`VisitorState::Choose`).
- Booleans read as assertions: `build_ready`, `menu_loaded`, `sav_ready`.

## Evidence in code

- Address first, role second:
  `// 0x53aa30 (opcode 0x17): fence click dispatcher`.
- Never use `FUN_0053aa30` as an identifier. The address is a comment; the
  identifier says what the code does.
- Recon write-ups are `notes/<topic>-recon.md`; the settled version is
  `docs/formats/` or `docs/decomp/`.

## Process surface

- Environment variables: `DINO_*` (`DINO_EXE`, `DINO_ASSET_ROOT`).
- CLI options: `--kebab-case` (`--camera-orbit`, `--sav-write`).
- Config keys: `snake_case` mirroring the option (`music_volume`).
- Executable and target: `dino_port`.

## What keeps the old names

`tools/viewer/` (asset preview tools), `tests/test_viewer.py` and the
historical `notes/viewer-*.png` captures are genuinely viewers or evidence and
keep their names. The application layer is `src/app` with target `dino_port`;
there is no `src/viewer` and no `dino_viewer` anymore.
