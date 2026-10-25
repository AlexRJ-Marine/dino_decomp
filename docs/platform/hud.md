# HUD and bitmap text (UI-0)

Status: **working** — the retail `.fnt` font renders on screen and the viewer
shows a live HUD.

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

## Evidence

- `notes/hud.png` — HUD over the isl01 view.
- `tests/test_viewer_cpp.py::test_cpp_hud` checks the font load line and that
  the top-left corner contains bright glyph pixels.
- `src/formats/fnt.cpp`, `src/formats/fnt.h`.

## Open items

- Kerning/advance beyond glyph width; coloured/shadowed text.
- HUD scaling for high-DPI (QOL-2).
- Menus/panels (UI-1) on top of this text path.
