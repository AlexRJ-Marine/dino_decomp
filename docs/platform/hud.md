# HUD and bitmap text (UI-0)

Status: **working** — retail `.fnt` text renders on screen, with a live HUD,
a start screen and a controls panel.

> **Retail menu is now the default.** `--menu` and interactive launches use the
> original `MenuInterface.Ini` overlay; see
> `docs/platform/ui-original-menu.md`. The old DINO ISLAND screen is available
> only with `--placeholder-menu`; the Tab controls panel is still port-style.

## `.fnt` reader

`src/formats/fnt.cpp` mirrors the validated Python parser
(`docs/formats/fnt.md`):

```text
0x00  9 x u32: version, atlas w/h, font size, cell height, base, line height,
             glyph count, hash
0x24  glyph_count x u16 codes
...   glyph_count x 8-byte metrics (x0,y0,x1,y1)
...   atlas: w*h*2 bytes, 16-bit texels
```

`codes[i]` maps to `metrics[i + 2]`; the last two code words are padding.
`arial12.fnt` gives 245 glyphs.

## Rendering

- The atlas is uploaded as `GL_R8`, taking the **high byte** of each 16-bit
  texel (the coverage value).
- `build_text_mesh` emits one quad per glyph in screen pixels (top-left
  origin), with UVs converted to the atlas (`v - 1` because the shared vertex
  builder adds 1).
- Drawn with the main shader in `u_unlit = 2` mode: fragment alpha is
  `texture(...).r * u_alpha`, colour comes from the vertex colour.
- Orthographic MVP maps pixels to clip space. Note the translation lives at
  `m[3]`/`m[7]` for this upload convention.
- Text uses alpha blending, depth test off; state is restored afterwards.

## Start screen and panels

- Interactive runs (`--frames 0`, no screenshot) open a start screen over the
  island: `DINO ISLAND`, `Press ENTER to begin`, `Esc quits  Tab help`.
  ENTER/SPACE starts, ESC quits.
- `--menu` forces the start screen for screenshots; `--no-menu` skips it.
- **Tab** toggles a controls panel (also `--help-panel` for screenshots).
- Both use the same `draw_2d` path: a translucent screen-space rectangle plus
  text quads.

## HUD

```text
Dino Island   FPS 99
visitors 12   paths 24   fences 0
```

- **F1** toggles the HUD; `--no-hud` starts hidden.
- `--hud-font FILE` selects another `.fnt` (default
  `Data/Fonte/arial12.fnt`).
- The HUD is rebuilt each frame; this is a small mesh (212 verts for the
  current two lines) but a dynamic-buffer path is a future optimisation.

## Park HUD (`ParkInterface.Ini`)

When an island is loaded, the viewer also loads
`ParkInterface.Ini` + `ParkGraphicDB.Ini` (2,694 widgets, 38 roots, 53
visible at park start) and draws the always-on panels over the scene:

- visible roots and descendants only; `RENDERVIEW` is skipped;
- normal state = frame 4 (frame 0 is the gray disabled art);
- `.fnt` text with `TEXT_HEIGHT` scaling and `TEXT_JUSTIFY`;
- runtime fields: budget 76050, visitor/dino/show counts, 09:44, day 1;
- `--no-park-hud` disables it.

At 800×600 this matches the Python oracle (`notes/hud-preview.png`):
right panel MAD 10.8, gauge 4.0, map 2.1 (oracle vs retail: 8.8 / 3.65).

## Evidence

- `notes/hud.png` — HUD over the isl01 view.
- `notes/menu-retail.png` — retail main menu.
- `notes/menu-free.png` — retail Free Game page.
- `notes/park-hud.png` — retail park HUD over isl01.
- `notes/hud-preview.png` — Python oracle for the park HUD.
- `tests/test_viewer_cpp.py`:
  - `test_cpp_hud` checks the font load line and bright glyph pixels.
  - `test_cpp_park_hud_load` / `test_cpp_park_hud_shot` check the park HUD.
  - `test_cpp_retail_menu_*` check the retail menu.
- `src/formats/fnt.cpp`, `src/formats/fnt.h`.

## Open items

- Kerning/advance beyond glyph width; coloured/shadowed text.
- HUD scaling for high-DPI and park re-anchoring at 1024×768 (QOL-2).
- Save/load panels and park button bindings (UI-2 Phase D).
- Minimap render target.
- `draw_ui_mesh` re-uploads its mesh per widget per frame; batching is the
  next performance step for the park HUD.
