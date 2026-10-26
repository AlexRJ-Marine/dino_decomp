# Environment: skybox and water (WORLD-3)

Status: **working** - six-face PNG skybox and a tiled water plane. The water
height is the `.lnd` float, runtime-confirmed at 16.0 (isl01) and 20.0
(isl02); see `notes/calibration-2026-10-26.md`.

## Skybox

`.lnd` points at `DATA\graphics\TEXTURES\skybox\isl01`. The directory holds
`front.PNG`, `back.PNG`, `left.PNG`, `right.PNG`, `top.PNG`, `bottom.PNG`
(128×128). These are **PNG**, not TGA, so the port vendors `stb_image`
(`src/third_party/stb_image.h`, public domain) behind
`dino::load_image()`; TGA still goes through our validated reader.

Rendering: six quads on a 5000-unit cube, one texture each, drawn unlit
(`u_unlit = 1`) before the terrain with depth test off and depth writes
disabled. `--skybox DIR` overrides the directory.

## Water

- Level: the `.lnd` float (16.0 on isl01, 20.0 on isl02), runtime-confirmed
  through the water-quad Z at `FUN_00602cf0`; `--water-level N` overrides.
- Texture: the first `.lnd` water path (`Water.tga`, 256×256), repeat-wrapped,
  tiled 48× across a plane with a 6144-unit half-extent (12,288 units across) so it reaches the skybox horizon.
- Drawn after the terrain with alpha 0.78 and depth writes off, so the
  shoreline shows through.

## Evidence

- `notes/world3-sky2.png` — island, ocean plane and sky from 900 units.
- `notes/world3-hero.png` — low-angle island shot.
- `notes/world3-up.png` — top-down island with sky at the corners.
- `notes/skybox-isl01-sheet.png` — the six faces (front/back/left, right/top/
  bottom).
- `notes/water-10.png`, `notes/water-16.png` — level comparison.
- `tests/test_viewer_cpp.py::test_cpp_skybox_and_water` asserts
  `environment: skybox 6/6 faces, water yes at z=16.0 (256x256)`.

## Open questions

- Face orientation: the current mapping looks plausible but the horizon glow
  may be vertically flipped. The retail engine's exact cube convention needs a
  trace or a side-by-side screenshot.
- Find the water-surface Z for the remaining islands; isl01 (16.0) and isl02
  (20.0) match the `.lnd` float at runtime. Skybox face orientation and water
  animation remain.
- Cloud layer (`cloud.ini`, `nuage*.tga` on isl04) is not implemented.
