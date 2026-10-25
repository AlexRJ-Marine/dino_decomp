# SDL3 + OpenGL viewer

Status: **VIEW-1 + VIEW-2 complete** — the window opens, loads a retail
`.mesh` + `.tga`, and draws it with a GLSL 3.3 lit pipeline and orbit camera.

## Toolchain

- SDL3 **3.4.18** MinGW development package, installed under
  `C:\Tools\SDL3\SDL3-3.4.18\x86_64-w64-mingw32`.
- Install/reinstall:

```powershell
.\scripts\setup-sdl3.ps1
```

- `scripts/build.ps1` finds the newest `C:\Tools\SDL3\SDL3-*` prefix (or uses
  `$env:SDL3_ROOT`) and passes it to CMake as `-DSDL3_ROOT=...`.
- CMake target `dino_viewer` links `SDL3::SDL3` + `opengl32` and copies
  `SDL3.dll` next to the executable.

## Build/run

```powershell
.\scripts\build.ps1
.\build\mingw\dino_viewer.exe --mesh "game/%MAINDIR%/Data/Graphics/Actors/Dinos/allo/allo_lod1.mesh" `
  --texture "game/%MAINDIR%/Data/Graphics/Textures/actors/dinos/allo.tga"
```

Options:

```text
--mesh FILE              retail .mesh to draw
--texture FILE           retail .tga texture
--width N --height N     window size (default 1280x720)
--frames N               exit after N frames (0 = run until closed)
--screenshot FILE.bmp    save the last frame as a 24-bit BMP
--yaw DEG --pitch DEG     initial orbit camera
--distance N             camera distance (0 = auto)
--validate               parse inputs and print counts, no window
--title TEXT             window title
--no-vsync               disable vsync
```

Controls: **left-drag** orbit, **wheel** zoom, **arrows** orbit, **R** reset,
**Esc** or the close button quits.

Example smoke run:

```powershell
.\build\mingw\dino_viewer.exe --frames 3 `
  --screenshot notes\viewer-allo.bmp --width 1200 --height 760 --no-vsync `
  --mesh "game/%MAINDIR%/Data/Graphics/Actors/Dinos/allo/allo_lod1.mesh" `
  --texture "game/%MAINDIR%/Data/Graphics/Textures/actors/dinos/allo.tga"
```

Result: `notes/viewer-allo.png`, OpenGL 3.3 core (NVIDIA), SDL 3.4.18,
317 vertices / 382 triangles.

## Implementation notes

- `src/formats/mesh.cpp` reads the retail `.mesh`: header, positions, indices,
  UVs, normals and the optional per-vertex RGBA tail. `--validate` prints the
  same counts as the Python parser.
- `src/formats/tga.cpp` reads uncompressed/RLE 24/32-bit and 8-bit grayscale
  TGA, returning top-left RGBA8.
- `src/viewer/main.cpp`:
  - SDL3 window/events/timing; GL 3.3 core context.
  - GLSL 3.3 vertex/fragment shaders with per-vertex normal, UV, color,
    texture modulation, key + fill lighting and a small gamma lift.
  - Interleaved VBO/VAO + EBO; UV `v_tex = 1 + v` as in the Python renderer.
  - Orbit camera, resize handling, `glReadPixels` BMP screenshots.
- `tests/test_viewer_cpp.py` checks the window smoke test, `.mesh`/`.tga`
  validation and a textured mesh screenshot.

## Next

VIEW-3: load `.skel` + `.skin` + `.anim`, add bone indices/weights and the
rigid skinning matrix to the shader, and play the allosaurus clip.

