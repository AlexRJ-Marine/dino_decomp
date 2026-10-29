# Refactor regression: the camera view never left (0,0,0)

Date: 2026-10-29. Found while splitting `game.cpp`; fixed in the same session.
Applies to the port only — retail is unaffected.

## Symptom

Every park/scene render after commit `598f050` (`refactor(app): split the
frame loop into named phases`) showed only the clear colour: the terrain,
water, skybox, buildings and dinosaurs were never drawn.

## Cause

In the pre-refactor `main()` the frame loop declared the camera view inside the
loop body:

```cpp
dino::Vec3 eye;
dino::Vec3 view_center;
if (free_camera) { ... } else { eye = orbit_eye(); view_center = center; }
```

Used later in the same iteration by `update_placement()` and `draw_frame()`.
When the loop was sliced into phases, `eye`/`view_center` were promoted to
`Game` members (they are read by two different phases), but the two local
declarations stayed in `update_camera_view()`. Every assignment went to the
locals; the members stayed default-constructed, so `draw_scene()` received
`eye == view_center == (0,0,0)` and a degenerate `look_at`.

Why the tests missed it: the park screenshot tests only assert HUD pixels
(`test_cpp_park_hud_shot` checks the right panel and the bottom gauge), and
the scene is drawn behind those panels. `--selftest` and the fast suite never
sample terrain pixels.

## Evidence

Command (isl01, no vegetation, fixed distance):

```powershell
.\build\refactor\dino_port.exe --frames 2 --no-menu `
  --screenshot notes\harness\cam-before.bmp --width 800 --height 600 `
  --no-vsync --no-save-config `
  --lnd "game\%MAINDIR%\Data\Design\landscape\isl01\isl01.lnd" `
  --asset-root "game\%MAINDIR%" --no-vegetation --distance 500
```

| Build | Distinct colours | Centre pixel (400,300) |
| --- | ---: | --- |
| before (locals shadowing the members) | 45,364 | `(17, 27, 41)` — clear colour |
| after (locals removed) | 152,879 (152,928 on a re-run) | `(67, 127, 21)` — island grass |

The distinct-colour count varies by a few dozen between runs because the
clear colour pulses with elapsed time; the centre pixel and the centre crop
are stable. The crop `(300,200)-(500,400)` has exactly 1 colour in the before
image and >1,000 in the after image.

`notes/harness/cam-before.bmp` / `cam-after.bmp` (gitignored) are the two
frames; the after image shows isl01 with water, skybox, HUD and minimap, i.e.
the pre-refactor view.

## Fix

`Game::update_camera_view()` now assigns the members directly:

```cpp
void Game::update_camera_view() {
    if (free_camera) { ... eye = free_pos; view_center = ...; }
    else { eye = orbit_eye(); view_center = center; }
}
```

A repeat scan of all `src/app/game*.cpp` for function-scope declarations and
parameters whose name matches a `Game` member found one more function-scope
collision: `render_minimap()` built its own top-down `eye` (now
`minimap_eye`), which was benign but is the same bug class. The remaining
same-name locals are inside nested blocks, where they cannot hide the member
for the rest of the method (the store preview and port-state restore build
their own camera that way on purpose).

## Follow-up (done)

`tests/test_app_cpp.py::test_cpp_park_hud_shot` now samples the centre crop
(`(300,200)-(500,400)`): it must have >1,000 colours and a green-dominant
centre pixel. On the before image the crop has 1 colour and the centre is
`(17,27,41)`.

`tests/test_app_shell.py` additionally rejects any function-scope local or
parameter that shadows a `Game` member, and pins the 21 boot and 14 frame
phase calls in order. It catches the `update_camera_view()` bug directly
(verified by reintroducing `dino::Vec3 eye;` temporarily).
