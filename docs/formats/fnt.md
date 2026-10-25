# `.fnt` format

Status: **solved** — 5/5 retail fonts parse; a reference string renders
correctly from the atlas.

## Inventory

| File | Atlas | Font size | Line height | Mapped glyphs |
| --- | --- | ---: | ---: | ---: |
| `arial12.fnt` | 256×256 | 12 | 12 | 245 |
| `font.fnt` | 256×256 | 12 | 12 | 245 |
| `font12.fnt` | 256×256 | 12 | 12 | 245 |
| `font14.fnt` | 256×256 | 14 | 14 | 245 |
| `font19.fnt` | 512×512 | 19 | 19 | 245 |

The glyph table has 247 entries; the last two code words are padding
(`247, 0`), leaving 245 real glyphs.

## Layout

```text
0x00  9 x u32 header:
        version         (1)
        atlas_width
        atlas_height
        font_size
        cell_height
        base
        line_height
        glyph_count     (247)
        hash
0x24  glyph_count x u16 character codes
...   glyph_count x 8 bytes metrics:
        u16 x0, y0, x1, y1
...   atlas: atlas_width * atlas_height * 2 bytes
```

The atlas is 16-bit per texel. In the retail fonts the high and low bytes are
identical; the high byte is used as coverage/alpha.

### Metric offset

The metrics array has **two leading entries** that the code table does not
address. Retail mapping is:

```text
glyph for codes[i] = metrics[i + 2]     (i = 0 .. glyph_count - 3)
```

The final two code words are padding. The first two metrics are likely
fallback/special glyphs; their exact role is open.

Glyph rectangles are **inclusive**::

```text
width  = x1 - x0 + 1
height = y1 - y0 + 1
```

and the advance is the glyph width (the atlas is packed row-major, 13-pixel
rows for a 12px font).

## Evidence

- Ghidra: `mgFontDx8::LoadFile` at `0x430fb0` (source
  `D:\Platform\Libs\mcLib\src\graph\mgFontDx8.cpp`). It checks version `1`,
  reads the atlas dimensions, font metrics, glyph count, code table and
  per-glyph data, then calls `LoadChar`.
- Strings: `Incorrect font version, (%s)`,
  `mgFontDx8::LoadFile(): Unicode value %d already loaded`,
  `mgFontDx8::CreateTexture() ERROR, Texture size too big`.
- Parser `tools/formats/fnt.py` validates the atlas size exactly
  `width * height * 2`.
- Rendering `notes/font-arial12-sample.png` reads
  `"Hello, Dino Island!"` from the atlas.

## Reader/API

`tools/formats/fnt.py`:

- `read_fnt(path)` → `Fnt` with header fields, codes, raw metrics, atlas, and
  `glyphs` keyed by Unicode code point.
- `Fnt.glyph(char)`, `Fnt.alpha_at(x, y)`.
- `scan(root)`.

`tools/viewer/text_render.py` renders a string with the atlas; the asset
viewer exposes it:

```text
python tools/viewer/asset_viewer.py \
  --font "game/%MAINDIR%/Data/Fonte/arial12.fnt" \
  --text "Hello, Dino Island!" --scale 2 --out notes/font-sample.png
```

Tests: `tests/test_fnt.py` (header, +2 metric mapping, reference render,
font19 atlas, 5-file scan).

## Open questions

- Exact names/semantics of the header fields `cell_height`, `base`.
- Role of the two leading metrics and the two padding code words.
- Whether the 16-bit texel ever differs between high and low bytes (none in
  retail).
