"""Render a reference string with a decoded .fnt atlas."""
from __future__ import annotations

import sys
from pathlib import Path

from PIL import Image

FORMATS = Path(__file__).resolve().parents[1] / "formats"
if str(FORMATS) not in sys.path:
    sys.path.insert(0, str(FORMATS))

from fnt import Fnt  # noqa: E402


def render_text(
    font: Fnt,
    text: str,
    *,
    color: tuple[int, int, int] = (20, 25, 35),
    background: tuple[int, int, int] = (237, 240, 243),
    padding: int = 4,
    scale: int = 1,
) -> Image.Image:
    lines = text.split("\n")
    widths = []
    for line in lines:
        pen = 0
        for ch in line:
            glyph = font.glyph(ch)
            pen += glyph.width if glyph else max(1, font.font_size // 2)
        widths.append(pen)
    width = max(widths, default=1) + padding * 2
    height = font.line_height * max(1, len(lines)) + padding * 2

    image = Image.new("RGB", (width, height), background)
    pixels = image.load()
    for line_index, line in enumerate(lines):
        pen_x = padding
        pen_y = padding + line_index * font.line_height
        for ch in line:
            glyph = font.glyph(ch)
            if glyph is None:
                pen_x += max(1, font.font_size // 2)
                continue
            for yy in range(glyph.height):
                for xx in range(glyph.width):
                    alpha = font.alpha_at(glyph.x0 + xx, glyph.y0 + yy) / 255.0
                    if alpha <= 0:
                        continue
                    r, g, b = pixels[pen_x + xx, pen_y + yy]
                    pixels[pen_x + xx, pen_y + yy] = (
                        int(r * (1 - alpha) + color[0] * alpha),
                        int(g * (1 - alpha) + color[1] * alpha),
                        int(b * (1 - alpha) + color[2] * alpha),
                    )
            pen_x += glyph.width
    if scale > 1:
        image = image.resize((width * scale, height * scale), Image.NEAREST)
    return image
