# App shell (`src/app`)

Status: **refactored 2026-10-29.** The application layer is a shell: `main.cpp`
parses the command line, `Game` owns the session, and `Game::run()` is
*boot → frame loop → shutdown*. Each domain has its own file, so a phase is a
few dozen lines with a name instead of a slice of a 10k-line function. See
`naming.md` for the conventions used here.

## File map

| File | Lines | Holds |
| --- | ---: | --- |
| `main.cpp` | 10 | entry point: `Game(argc, argv).run()` |
| `game.h` | ~600 | the `Game` object: session state by subsystem, then the method map (boot phases, frame phases, one banner per domain file) |
| `game.cpp` | ~1,400 | constructor/destructor, `run()`, `shutdown()`, the 14 frame phases |
| `game_boot.cpp` | ~1,140 | `boot()` itself and boot phases 1–11: options, keymap, budgets, campaign + save discovery, menu state, asset loading, print modes, SDL window + GL context |
| `game_boot_park.cpp` | ~1,280 | boot phases 12–21: scene resources, park objects, dinosaurs, food, employees, camera, tools, final restore |
| `game_input.cpp` | ~1,390 | `handle_input()`, `update_placement()`, `update_preview()` |
| `game_ui.cpp` | ~1,420 | UI texture/font lookup, `draw_menu()`, `draw_park_hud()`, layout helpers |
| `game_menu.cpp` | ~430 | menu/campaign actions, options, video playback |
| `game_show.cpp` | ~390 | show scoring, participants, show placement, attraction actions |
| `game_park.cpp` | ~1,180 | island load, campaign park load, `PortState` save/load |
| `game_build.cpp` | ~1,030 | buildings, fences, roads, store catalog, daily settlement |
| `game_scene.cpp` | ~860 | vegetation, minimap, extra items, dinosaur import, paddock queries |
| `cli.{h,cpp}` | 235 + 740 | `Options`, `parse_args`, config load/save, help text |
| `reports.{h,cpp}` | 18 + 860 | headless `--…-info` report modes, called once from boot |
| `asset_paths.{h,cpp}` | 73 + 190 | retail path resolution, island/profile/show helpers |
| `ui_geometry.{h,cpp}` | 67 + 135 | UI rectangles/quads and park pointer mapping |
| `keymap.{h,cpp}` | 57 + 200 | key bindings |
| `camera_config.{h,cpp}` | 39 + 75 | `camera.ini` reader |
| `capture.{h,cpp}` | 16 + 55 | BMP screenshots |
| `park_camera.{h,cpp}` | 173 + 65 | retail park camera model (`docs/decomp/camera.md`) |

The line counts are approximate and drift; the point of the table is the
boundary, not the number.

## Lifecycle

```cpp
int main(int argc, char** argv) {
    return dino::app::Game(argc, argv).run();
}
```

`Game::run()`:

1. `boot()` — the 21 `boot_*` phases below. Each returns
   `std::optional<int>`: the process exit code when the run is over (a print
   mode, `--validate`, a fatal load error), `std::nullopt` to continue.
2. the frame loop (`while (running)`): 14 phases, in order.
3. `shutdown()` — release GL/SDL resources, write the config and end-of-run
   output.

`boot()` is a list of calls, one per phase, in execution order. The exit-code
test lives in `boot()` only, so a phase body reads as plain setup.

## Boot phases

| Phase | Sets up |
| --- | --- |
| `boot_command_line` | report modes, keymap load/rebind, music default, `--list-keys` |
| `boot_budgets_and_clock` | difficulty budgets/sellback, island list, park clock and `--park-time` |
| `boot_campaign_and_saves` | campaign progress, save-slot discovery, `--list-sounds` |
| `boot_menu_state` | session flags, campaign unlock, initial menu screen, pending actions |
| `boot_menu_overlay` | `ParkInterface.Ini`/`ParkGraphicDB.Ini` overlay, menu overlay, hover seeding |
| `boot_demo_defaults` | demo-path asset fallbacks, LOD quality, water, show specs |
| `boot_load_assets` | every optional asset: mesh, landscape, skin, animation, textures, wav |
| `boot_build_textures` | build-tool textures and HUD font atlas |
| `boot_print_modes` | `--validate` and print-only modes (returns the exit code) |
| `boot_platform` | SDL init, window and GL context, shaders, audio device |
| `boot_scene_resources` | texture/mesh uploads, spawn and camera seed, vegetation |
| `boot_park_objects` | building focus, `.wld` park load, entrance cells |
| `boot_minimap_and_visitors` | minimap, navigation flags, extra items, visitor spawn |
| `boot_employee_profiles` | employee list and `camera.ini` |
| `boot_import_dinos` | `.wld` dinosaurs, `--buy-dino`, paddock totals |
| `boot_food_definitions` | food source definitions and `--place-food` |
| `boot_camera_setup` | camera target/distance/yaw/pitch, free-camera entry |
| `boot_park_session` | park session defaults and the `.sav` park restore |
| `boot_employee_session` | budget overrides, purchases, nutritionists, feeding, employee mesh |
| `boot_build_tools` | road tool, construct/store/show actions, HUD layout defaults |
| `boot_finalize` | port save/load, hover probes, loop timing and window size |

## Frame phases

| Phase | Does |
| --- | --- |
| `handle_input` | pumps SDL events, keyboard/mouse actions, menu and HUD clicks |
| `begin_frame` | window size, park layout sync, pending select/sell requests |
| `advance_time` | frame `dt`, elapsed time, park palette pulse |
| `update_session` | menu loading/timer, campaign or free-game start, session transitions |
| `update_audio` | UI sound and music players; delegates the minimap zoom keys to `update_minimap_zoom()` |
| `update_camera` | camera input and goals (retail park model or free camera) |
| `update_diagnostics` | `DINO_DEBUG_CAMERA` trace; calls `refill_looping_stream()` |
| `set_background` | clear colour (time-of-day pulse) |
| `update_simulation` | animation clock, visitors, employees, dinosaurs, shows |
| `update_camera_view` | the frame's `eye` and `view_center` |
| `update_placement` | build-tool placement, picking, paddock selection |
| `update_preview` | fence/road/ghost preview state |
| `draw_frame` | water scroll, LOD/cull, scene, minimap, HUD and menu |
| `present_frame` | `SDL_GL_SwapWindow`, capture/screenshot, frame counter |

`eye` and `view_center` are `Game` members because `update_placement()` and
`draw_frame()` both read them after the camera phase computed them.

## Where does new code go?

| If you are adding… | Put it in |
| --- | --- |
| a startup step | a new `boot_<area>()` phase, declared in `game.h` and called from `boot()` in order |
| per-frame work | a `update_<thing>()` phase, called from `run()` in the right slot |
| park simulation | `src/world/`, not `src/app/`; call it from a phase |
| a menu/HUD screen | layout and state in `src/ui/`, drawing in `game_ui.cpp` |
| GPU work | `src/render/`; the app keeps the GL handles |
| a headless report | `reports.cpp` (called from `boot_command_line`) |
| a CLI option | `cli.{h,cpp}` and one line in the help text |

Rules:

- `game.h` is a map: data members and declarations, no logic beyond inline
  data helpers.
- No SDL/GL in `src/formats`, `src/world`, `src/ui` (see `naming.md`).
- A phase should fit on a screen or two. When one grows, split it — do not
  let `boot()` or `run()` grow again. The four original long bodies are split
  (MAINT-2); the remaining long phase bodies are `update_session` (434),
  `load_island` (386) and `apply_port_state` (321), not queued.
- A function-body local must not share a name with a `Game` member: a local
  in `update_camera_view()` once hid `eye`/`view_center` and the scene went
  black. `tests/test_app_shell.py` enforces this.

## Verification

```powershell
.\scripts\build.ps1 -BuildDir build\refactor
.\build\refactor\dino_port.exe --selftest      # 194 checks
.\scripts\test.ps1 -Exe build\refactor\dino_port.exe -Fast   # 397 tests
```

`--selftest` covers the format readers, objectives and HUD math without a
window; `--camera-selftest` asserts the retail camera equations. The fast
suite runs every headless app CLI test (`--selftest`, `--validate`, `--sav-*`,
`--*-info`) plus the format and static guards; window and screenshot tests are
marked `@pytest.mark.slow` and stay opt-in. A change that only moves code must
leave all of it unchanged.

`tests/test_app_shell.py` is a static guard: it checks `main.cpp`, the 21 boot
and 14 frame phase calls in order, rejects a function-scope local that shadows
a `Game` member, and rejects a module-wide `slow` marker on
`tests/test_app_cpp.py`. In that module a window run must stay `slow` and the
headless anchor tests must stay unmarked, so `-Fast` keeps covering the app
(MAINT-4).

History: this file used to describe the single-file 2026 vertical-slice
"viewer". The mesh/TGA/skinning notes it held now live in `docs/formats/`
(`mesh.md`, `tga.md`, `skel.md`, `skin.md`, `anim.md`).
