# Environment: skybox and water (WORLD-3)

Status: **working** - six-face PNG skybox (retail face order) and a tiled
water plane with the retail additive blend. The water height is the `.lnd`
float, runtime-confirmed at 16.0 (isl01) and 20.0 (isl02); see
`notes/calibration-2026-10-26.md`.

## Skybox

`.lnd` points at `DATA\graphics\TEXTURES\skybox\isl01`. The directory holds
`front.PNG`, `back.PNG`, `left.PNG`, `right.PNG`, `top.PNG`, `bottom.PNG`
(128×128). These are **PNG**, not TGA, so the port vendors `stb_image`
(`src/third_party/stb_image.h`, public domain) behind
`dino::load_image()`; TGA still goes through our validated reader.

Rendering: six quads on a 5000-unit cube, one texture each, drawn unlit
(`u_unlit = 1`) before the terrain with depth test off and depth writes
disabled. `--skybox DIR` overrides the directory.

Face mapping matches the retail geometry builder `0x00600f80` / draw
`0x00600df0`: **front = -X, left = +Y, back = +X, right = -Y, top = +Z,
bottom = -Z**. Each quad's vertices are in UV order (0,0), (1,0), (0,1),
(1,1); v=0 is the PNG top row (the port stores `v-1` because the shader adds
1). The cube is camera-centred and world-axis aligned; retail renders the
inside of the cube (the port does not enable culling). The old mapping had
front=+Y/left=-X, a ~90 degree rotation, now corrected. The current capture
is `notes/p5-sky.png`.

## Water

- Level: the `.lnd` float (16.0 on isl01, 20.0 on isl02), runtime-confirmed
  through the water-quad Z at `FUN_00602cf0`; `--water-level N` overrides.
- Texture: the first `.lnd` water path (`Water.tga`, 256?256), repeat-wrapped,
  tiled 38? across a plane with a 4,864-unit half-extent (9,728 units across).
- Drawn after the terrain with **additive blending** (`Data/States/
  WaterSurface.ini`: source `One`, destination `One`, op `Add`) and depth
  writes off, so the shoreline shows through. The old port used alpha 0.78,
  which is not the retail state.
- **Water fog** is per-material: `Data/States/FogParam_WaterSurface.txt` uses
  100..700 with a black colour, so the ocean fades to the sky before the plane
  edge. Scene fog (`FogParam.txt`, 1500..2000, colour 255,220,165) is applied
  to the other lit materials. Both are linear.
- Retail also binds the same water TGA to a second texture stage with
  `Modulate2x` (coordinate set 1, 28/30 tiling, 0.25 offset; the retail
  builder scrolls the two layers in opposite directions). The port implements
  the stage with a second UV set in the shader, clamped `2*texture*current`,
  and `--no-water2` disables it for A/B captures:
  `notes/water-modulate2x.png`. Water *animation* (the opposite scroll) is
  still open.

## Side-by-side (WORLD-3)

`notes/p5-side-by-side.png` compares the retail game (dgVoodoo2, isl01
Free Game, 09:42) with the port at the same camera (`--distance 65.6 --yaw 0
--pitch 53.4 --park-time 09:42`, 800×600). Terrain silhouette, basemap,
vegetation placement, HUD anchoring and the warm fog match closely.

Known differences in that frame:

- the retail park has two visitors (budget 75,150) and the port has none
  (75,000);
- the port minimap is camera-rotated and its detail render differs from the
  retail minimap;
- retail terrain shading is slightly smoother than the port's;
- the port has no water in this view (same as retail), so the second
  `Modulate2x` water stage is not exercised.

## Evidence

- `notes/p5-side-by-side.png` — retail vs port, same island/camera/time.
- `notes/world3-sky2.png` — island, ocean plane and sky from 900 units.
- `notes/world3-hero.png` — low-angle island shot.
- `notes/world3-up.png` — top-down island with sky at the corners.
- `notes/skybox-isl01-sheet.png` — the six faces (front/back/left, right/top/
  bottom).
- `notes/water-10.png`, `notes/water-16.png` — level comparison (old alpha
  blend).
- `notes/p5-water-additive.png` — additive blend without fog.
- `notes/p5-water-fog3.png` — additive water + per-material fog (current).
- `tests/test_viewer_cpp.py::test_cpp_skybox_and_water` asserts
  `environment: skybox 6/6 faces, water yes at z=16.0 (256x256) water2
  256x256`; `test_cpp_water_modulate2x` compares the sea with and without
  `--no-water2` (`notes/water-modulate2x.png`).

## Open questions

- The skybox face order/UVs are solved from the retail builder; a side-by-side
  screenshot against the original remains for final visual confirmation.
- Water animation (the retail opposite scroll of the two `Modulate2x` layers)
  is not implemented; the stage itself, additive blend, scene fog and water fog
  are now retail-matched. The skybox bottom/side seam is still visible at some
  angles.
- Cloud layer (`cloud.ini`, `nuage*.tga` on isl04) is not implemented.
