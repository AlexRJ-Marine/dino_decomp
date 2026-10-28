# SDL3 + OpenGL app shell

Status: **VIEW-1 + VIEW-2 + VIEW-3 complete** — the window opens, loads a
retail `.mesh` + `.tga`, and draws a textured, lit, **rigid-skinned animated**
allosaurus with an orbit camera. The interactive mode is now the retail park
camera (`docs/decomp/camera.md`); `--camera-orbit` keeps the view described
here. This is the port's application layer (`src/app`, target `dino_port`).

## Toolchain

- SDL3 **3.4.18** MinGW development package, installed under
  `C:\Tools\SDL3\SDL3-3.4.18\x86_64-w64-mingw32`.
- Install/reinstall:

```powershell
.\scripts\setup-sdl3.ps1
```

- `scripts/build.ps1` finds the newest `C:\Tools\SDL3\SDL3-*` prefix (or uses
  `$env:SDL3_ROOT`) and passes it to CMake as `-DSDL3_ROOT=...`.
- CMake target `dino_port` links `SDL3::SDL3` + `opengl32` and copies
  `SDL3.dll` next to the executable.

## Build/run

```powershell
.\scripts\build.ps1
.\build\mingw\dino_port.exe --mesh "game/%MAINDIR%/Data/Graphics/Actors/Dinos/allo/allo_lod1.mesh" `
  --texture "game/%MAINDIR%/Data/Graphics/Textures/actors/dinos/allo.tga"
```

Options:

```text
--mesh FILE              retail .mesh to draw
--texture FILE           retail .tga texture
--skel FILE              retail .skel skeleton (required for skinning)
--skin FILE              retail .skin rigid-skin table
--anim FILE              retail .anim clip
--time N                 fixed animation time (default: play the loop)
--speed N                animation speed multiplier (default 1)
--width N --height N     window size (default 1280x720)
--frames N               exit after N frames (0 = run until closed)
--screenshot FILE.bmp    save the last frame as a 24-bit BMP
--capture-dir DIR        save every frame (fixed animation step)
--capture-prefix P       frame filename prefix (default frame)
--yaw DEG --pitch DEG    initial orbit camera
--distance N             camera distance (0 = auto)
--validate               parse inputs and print counts/bounds, no window
--title TEXT             window title
--no-vsync               disable vsync
```

Controls: **left-drag** orbit, **wheel** zoom, **arrows** orbit, **R** reset,
**Esc** or the close button quits.

Example animated run:

```powershell
.\build\mingw\dino_port.exe `
  --mesh "game/%MAINDIR%/Data/Graphics/Actors/Dinos/allo/allo_lod1.mesh" `
  --texture "game/%MAINDIR%/Data/Graphics/Textures/actors/dinos/allo.tga" `
  --skel "game/%MAINDIR%/Data/Graphics/Actors/Dinos/allo/allo.skel" `
  --skin "game/%MAINDIR%/Data/Graphics/Actors/Dinos/allo/allo_lod1.skin" `
  --anim "game/%MAINDIR%/Data/Graphics/Actors/Dinos/animations/HeavyBiped/HeavyBiped_Armor.anim"
```

Deterministic 25-frame capture:

```powershell
.\build\mingw\dino_port.exe --frames 25 `
  --capture-dir notes\anim-cpp --capture-prefix allo `
  --width 600 --height 400 --no-vsync --distance 16 `
  --mesh ... --texture ... --skel ... --skin ... --anim ...
```

Result: `notes/viewer-anim-1600.png`, `notes/viewer-anim-sheet.png`,
`notes/viewer-anim-cpp.gif`.

## Implementation notes

- `src/formats/mesh.cpp` reads the retail `.mesh`: header, positions, indices,
  UVs, normals and the optional per-vertex RGBA tail. `--validate` prints the
  same counts as the Python parser.
- `src/formats/tga.cpp` reads uncompressed/RLE 24/32-bit and 8-bit grayscale
  TGA, returning top-left RGBA8.
- `src/app/main.cpp`:
  - SDL3 window/events/timing; GL 3.3 core context.
  - GLSL 3.3 vertex/fragment shaders with per-vertex normal, UV, color,
    texture modulation, key + fill lighting and a small gamma lift.
  - Interleaved VBO/VAO + EBO; UV `v_tex = 1 + v` as in the Python renderer.
  - Orbit camera, resize handling, `glReadPixels` BMP screenshots.
- `src/formats/skel.cpp` reads the 80-bone skeleton (64-byte row-major
  matrices, parent table, names).
- `src/formats/skin.cpp` locates the full-mesh block by name, reads its
  centimetre vertices, and parses the 35 rigid skin records
  (vertex partition, weights, inverse-bind matrices).
- `src/formats/anim.cpp` reads the 35-bone clip (`u64` times + local 4×4
  matrices) and samples it with quaternion slerp.
- CPU rigid skinning per frame:
  `v_cm @ inverse_bind @ W_raw_anim`, then `× 0.01` for metres. Normals are
  transformed by the same 3×3 and renormalised. The 317-vertex VBO is updated
  with `glBufferData` each frame.
- `tests/test_app_cpp.py` checks the window smoke test, `.mesh`/`.tga`
  validation, skinned validation, a fixed animation pose, and frame capture.

## Next

Phase 2 QoL:
- QOL-1 resolution/widescreen/borderless.
- QOL-2 UI scaling.
- QOL-3 key rebinding.

Then Phase 3: hook the original exe and replace loaders one at a time.

