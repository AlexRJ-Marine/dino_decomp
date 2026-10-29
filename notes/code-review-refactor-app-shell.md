# Review: app-shell refactor (`2f6c7db..5fcd5ce`)

Date: 2026-10-29. Reviewer: independent read-only pass (fast diff tier).
Scope: 8 commits, `2f6c7db` ... `5fcd5ce`; `main.cpp` 10,729 -> 10 lines.
Verdict: **no behavioural regression found in the final tree.** The one real
regression this series introduced (`eye`/`view_center` shadowing, commits
`598f050`..`5fcd5ce^`) is fixed and its evidence reproduces. Remaining items are
readability/doc/test-coverage findings, all LOW or NIT.

## Findings

### MEDIUM - the refactor's own failure mode has no test

The only real regression was invisible to every gate: `notes/refactor-camera-
shadowing.md` records 45,364 vs 152,879 distinct colours, i.e. a fully broken
3D scene, while `--selftest` (194) and the fast suite (350) stayed green.
`tests/test_app_cpp.py` park-shot tests sample HUD panels only
(`test_app_cpp.py:1570-1576`: right panel and bottom gauge); no test asserts a
terrain/scene pixel. A split of this kind can re-introduce exactly the same
class of bug (a local in a phase shadowing a member another phase reads) and
still pass CI.

Minimal fix: the guard already planned as MAINT-3 in `notes/NOW.md` - render
`--distance 500 --no-vegetation` and assert that the centre region is not the
clear colour, plus a static check that no `Game::` method declares a
function-scope local whose name is a `game.h` member.

Where the bug lived: `src/app/game.cpp:1161` (`update_camera_view`), dead
locals removed in `5fcd5ce`.

### LOW - two frame-phase names describe the wrong thing

Both are faithful slices of the old loop, so there is no behaviour bug, but the
stated point of MAINT-1 is that the names are the documentation:

- `Game::update_audio()` (`src/app/game.cpp:691-704`) also applies
  `minimap_zoom_in`/`minimap_zoom_out` to `minimap_scale`. `docs/platform/app-
  shell.md:91` repeats the mismatch ("UI sound and music players, minimap zoom
  keys"). Minimap zoom is a camera/HUD concern, not audio.
- `Game::update_diagnostics()` (`src/app/game.cpp:949-953`) also refills the
  looping WAV stream (`SDL_PutAudioStreamData`), which is audio, not
  diagnostics.

Minimal fix: move each block verbatim - minimap zoom into `update_camera()` (or
a new `update_minimap_controls()` called from the same slot), the WAV refill
into `update_audio()` - and update the two table rows.

### LOW - one remaining function-scope shadow of a member

`src/app/game_scene.cpp:79`:

```cpp
const dino::Vec3 eye{focus.x, focus.y, 4000.0f};
```

`render_minimap()` deliberately builds its own top-down eye and never reads
`Game::eye`, so this is benign today and it was a local in the pre-refactor
lambda too. It is still the exact shape of the `5fcd5ce` bug, and it is the only
such shadow left: a brace-depth scan of all 120 `Game::` methods against all 284
`game.h` members found no other function-scope collision.

Minimal fix: rename the local to `minimap_eye`.

### LOW - the shadow-scan claim in the note is incomplete

`notes/refactor-camera-shadowing.md:67-71` says the scan "reports only
function-local variables with no cross-phase use (`fence_texture` path string,
`sky_faces`, a `texture` upload handle, menu frame indices, `spawn_x/spawn_y` in
the hire loop)". It omits `game_scene.cpp:79` (`eye`), and the listed examples
are all *nested-scope* locals (`game_boot.cpp:807`, `game_boot.cpp:931`,
`game_build.cpp:131`, `game_boot_park.cpp:890`), not the function-scope class
that caused the regression. The substantive claim holds - there is exactly one
function-scope shadow and it is harmless - but the note should say that, rather
than list a different set.

### LOW - docs overstate how the new structure is tested

`notes/evidence-index.md:139` and the Project row in
`docs/platform/feature-status.md` cite `tests/test_naming.py`, `--selftest (194)`
and `tests/test_app_cpp.py` as the evidence for "`Game` owns the session;
`run()` = boot (21 phases) -> 14 frame phases -> shutdown". None of them checks
that:

- `tests/test_naming.py:66-90` only bans the old `viewer` identifiers and
  asserts `src/app/main.cpp` exists;
- `--selftest` does not touch the app shell.

The counts are true today (21 declared / 21 called in `boot()`, 14 declared / 14
called in `run()`), but nothing would catch a phase dropped from the call list.
Minimal fix: a small guard test - count `boot_*`/frame-phase declarations in
`game.h` and their call sites in `boot()`/`run()` - which is the MAINT-3 item.

### LOW - `naming.md` contradicts the code it was committed with

`docs/platform/naming.md` is new in this series and describes code that does not
exist:

- "Private class state gets a trailing underscore ... new code follows
  `ui::Park`": `Game` is the new class and has **no** trailing-underscore
  member (`game.h:154-443`, 284 members, zero `_` fields). The `Game` header
  comment does say it mirrors the retail globals; the rule should carry the
  same exemption.
- "Functions ... verb first, from a small closed set: `load_`, `read_`,
  `write_`, `save_`, `parse_`, `build_`, `make_`, `apply_`, `update_`,
  `handle_`, `draw_`, `render_`, `run_`": the app shell adds `boot_`,
  `ensure_`, `refresh_`, `sync_`, `present_`, `advance_`, `clear_`, `place_`,
  `spawn_`, `print_`, `enter_`, `fill_`, `queue_`, `activate_`, `arm_`, `do_`,
  `sell_`, `import_`, `auto_assign_`, `find_` (see the method map,
  `game.h:445-595`), plus a noun-first `minimap_click_zoom` (`game.h:582`).
  Either widen the set or state that `Game` is exempt.

### NIT - split plan omits the largest remaining bodies

`docs/platform/app-shell.md:122` says "A phase must fit on a screen or two",
while `update_session()` is 434 lines (`game.cpp:255-690`) and is not in the
MAINT-2 list in `notes/NOW.md`. For the next pass, the bodies over ~300 lines
are: `draw_park_hud` 757, `handle_input` 637, `update_placement` 637,
`update_session` 434, `draw_menu` 427, `load_island` 385, `apply_port_state`
320. Adding the last three keeps the rule and the plan consistent.

### NIT - two statements on one physical line

`src/app/game_boot.cpp:1002` has
`dino::build_placement_meshes(...);            std::printf("build: cell ...`.
Pre-existing (original `main.cpp:1449`), not introduced here, so no action is
required if the line is meant to stay verbatim.

### NIT - `app-shell.md` table line counts

Several counts are 5-15% low (`asset_paths` 73+190 vs actual 57+179, `capture`
16+55 vs 12+47, `park_camera` 173+65 vs 161+62). The doc already says the
numbers drift, so this is informational only.

## Checks performed (all read-only)

Correctness / equivalence
- Token-level set diff of every stripped, comment-free line of
  `2f6c7db^:src/app/main.cpp` against all current `src/app/*.cpp|*.h`:
  306 "missing" and 536 "added" lines, all classified as member declarations,
  lambda->method conversions, `using` declarations, `dt`->`frame_dt`,
  `kPi`->member, `ui_geometry` extraction, `.data()`/`[this]` captures. No
  dropped statement, no new control flow.
- Phase order: the first original line of each of the 14 frame phases is
  strictly increasing in `run()` order (8010, 8650, 8692, 8724, 9158, 9169,
  9395, 9416, 9420, 9619, 9634, 10271, 10371, 10571), so no phase was moved.
- `boot()` calls 21 phases in declaration order; `run()` calls 14.
- Depth-0 shadow scan (brace-depth parser, not grep): 120 methods x 284
  members -> one hit (`game_scene.cpp:79`), reported above. The `5fcd5ce`
  regression class is otherwise eliminated.
- All 115 declared methods are defined; no definitions without declarations
  (5 initial hits were pointer-returning declarations the regex missed).
- No dead members added: every member is read somewhere. The three
  write-only-looking fields (`attraction_selected_slot`, `hud_font_ready`,
  `cam_mouse_valid`) are byte-identical in behaviour to the original
  (`main.cpp:455`, `983`, `8605`).
- All 6 original `static` locals map 1:1 to one new location
  (`kLodNames`, `kFaceNames` x2, `suffix` x2, `kProfileNames`) - no duplicated
  static state, no lifetime change.
- `ui_geometry.{h,cpp}` is a verbatim move of `main.cpp:103-257` (`Bounds`,
  `build_screen_rect`, `UiTexture`, `UiSourceRect`, `map_park_pointer`,
  `ui_frame_rect`, `build_ui_quad`, `MinimapTarget`): no signature or body
  change; only linkage changed (internal -> `dino::app`), and every one of the
  120 free helpers in `game_*.cpp` is declared in a header, so external linkage
  is intended.
- `.data()` conversions are correct: original `float selected_tint[3]`
  (`main.cpp:3141`) -> `std::array<float,3>::data()` for
  `fence_pulse_colors(double, float[3], float[3])` (`src/world/build.h:173`).
- `kPi`: original `constexpr float` local -> `static constexpr` member, same
  value.
- Early construction of all members before `boot()` (a genuine semantic change
  vs progressive locals) is inert: `MusicPlayer`, `UiSoundPlayer`, `GlFns`,
  `Menu`, `ParkHud`, `Terrain`, `Vegetation` are aggregates/default-initialised
  with no constructors that touch SDL/GL/files.
- `Game::update_camera_view()` (`game.cpp:1161`) assigns both `eye` and
  `view_center` on both branches; no indeterminate read.

Build / tests
- `build/refactor/dino_port.exe --selftest` -> `selftest: PASS (194 checks)`,
  exit 0.
- `scripts/test.ps1 -Exe build/refactor/dino_port.exe -Fast` ->
  `350 passed, 156 deselected in 50.85s`.
- Binary is newer than every source; no rebuild needed.
- `CMakeLists.txt:102-113` lists all 12 new sources exactly once (10
  `game_*.cpp` + `ui_geometry.cpp` + `main.cpp`); no duplicate, no missing.
- `src/app/main.cpp` is 10 lines; `Game::run()` is 35 (`game.cpp:15-49`).

Evidence reproduction
- `notes/harness/cam-before.bmp` measures 45,364 distinct colours and centre
  `(17,27,41)`; `cam-after.bmp` measures 152,879 and `(67,127,21)` - exactly the
  note's table.
- Re-ran the note's command on the current binary against retail
  `game/%MAINDIR%/Data/Design/landscape/isl01/isl01.lnd`: 152,928 distinct
  colours, centre `(67,127,21)`. The fix still holds.

Read-only discipline
- The 19 changed paths are `CMakeLists.txt`, `src/app/*`, `docs/platform/*`,
  `notes/refactor-camera-shadowing.md`. No `original/`, no `game/`, no binary
  addition, no tracked `notes/*.png`, no asset copied into `src/`.
- No writes were made by this review; the only artefacts are this note and a
  temporary screenshot under the gitignored `notes/harness/`.

Non-intrusive runtime / background
- No `SetCursorPos`, `mouse_event`, `SetForegroundWindow`, `AppActivate`,
  `SendInput` or `keybd_event` was added anywhere in the range; no harness
  script changed, so the windowed/virtual harness and background operation are
  untouched.

## Range note

The brief said 11 commits; the range is 8 (`git rev-list --count
2f6c7db^..5fcd5ce`). The MAINT-1 leaf extractions that took `main.cpp` from
15,266 to 10,729 lines happened before `2f6c7db`, so the "was 15,266 / 11,145"
figures in `notes/NOW.md:51` refer to the start of MAINT-1, not to this range's
baseline.

## Follow-up (2026-10-29, same session)

All findings were addressed or explicitly deferred:

- MEDIUM: `test_cpp_park_hud_shot` now asserts the centre crop (1 colour on
  the regression image, >1,000 now) and `tests/test_app_shell.py` rejects
  function-scope member shadows and pins the phase map; verified by
  temporarily reintroducing `dino::Vec3 eye;` (caught) and against the
  before image. MAINT-3 done.
- LOW `update_audio` / `update_diagnostics`: minimap zoom and the loop-stream
  refill moved into named helpers called at the same point in the frame, so
  the phase names are accurate and the order is unchanged.
- LOW `game_scene.cpp:79`: `eye` -> `minimap_eye`; `draw_ui_mesh`'s `mesh`
  parameter -> `geometry` (the guard now reports zero shadows).
- LOW camera note: scan enumeration corrected; colour variance noted.
- LOW evidence claims: the evidence index now points at the phase-map test.
- LOW naming doc: `Game` fields documented as the deliberate no-underscore
  exception; the verb list no longer pretends to be closed.
- NIT phase size: `update_session` and the next tier are listed in MAINT-2 /
  `notes/refactor-maint2-plan.md`.
- FAST gate (`-Fast` skips every app test): tracked as MAINT-4, not done here.
- NITs left as-is: two statements on `game_boot.cpp:1002` (pre-existing),
  approximate table counts (the doc says they drift).
