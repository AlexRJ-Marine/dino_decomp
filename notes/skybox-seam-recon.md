# Skybox bottom/side seam (R-1, WORLD-3)

Date: 2026-10-28. Binary: `DinoIsland.exe` (capstone disassembly via
`pefile`; Ghidra MCP was not running).

## Symptom

The port drew the six sky quads on an 8000-unit half-size cube centred on the
**world origin**. With the orbit camera thousands of units from the origin,
the cube was off-centre: looking 60 degrees down, the light-blue horizon band
of the side faces showed in the top corners of the frame and the side/bottom
edge cut across the view as a curved seam; looking 60 degrees up, the band
climbed the bottom of the frame. The sky changed with orbit distance.

## Retail draw `0x00600df0`

```text
00600e0e  mov ecx, [esp+0x58]       ; arg (scene/camera owner)
00600e17  add ecx, 0x70
00600e1a  call 0x447650             ; -> vec3 position of the camera node
00600e24  call 0x4663f0             ; identity, row 3 = (x, y, z): Translate
00600e32  mov [esp+0xc..0x14], 0x42c80000 x3   ; (100, 100, 100)
00600e4a  call 0x466c20             ; this = Scaling(100) * this
00600e5a  call 0x40e320             ; set world matrix
...                                  ; six faces at +0x30..+0x5c, then +0x60/+0x68
```

`0x4663f0` zeroes 16 floats, writes 1.0 on the diagonal and copies the vec3
to `+0x30/+0x34/+0x38` (D3D row-vector translation). `0x466c20` builds a
scaling matrix from the vec3 argument and multiplies it in front. So
`world = Scale(100) * Translate(camera)`: the sky cube follows the eye.

The builder `0x00600f80` uses unit (`0x3f800000` / `0xbf800000`) corners, so
the retail box is ±100 units around the camera.

Confidence: high for the translate/scale; `0x447650` reading the camera
node's position is inferred from its use (lookup via `[0x674528]+0x70`), not
runtime-traced.

## Port change

`src/viewer/main.cpp`: `kSkyboxHalfSize = 100` and `draw_scene` draws the sky
with `mvp * Translate(eye)`, restoring `u_mvp` afterwards. Depth test/write
stay off, so the box size only matters for the near plane (0.1).

## Evidence

- `notes/skybox-seam-ab.png` — isl01, `--camera-orbit --yaw 45 --distance
  3000`, pitch +60 (top row) and -60 (bottom row); left is the old
  origin-centred cube, right the camera-centred one.
- Old build, pitch +60: top-left pixel `(117,187,218)` (horizon band); new:
  `(3,0,61)` (bottom face) at all four corners.
- `tests/test_viewer_cpp.py::test_cpp_skybox_follows_camera` asserts the
  dark corners at +60 and an identical sky strip at -60 for distance 1500
  vs 3000.

## Not done

- Retail side-by-side at a steep pitch (the retail park camera does not reach
  ±60; `p5-side-by-side.png` is still the reference frame).
- The sky quads still take the default vertex colour 220 (0.86 tint) from
  `build_vertices`; whether retail draws them at full brightness is unproven.
