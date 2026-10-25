import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))
sys.path.insert(0, str(ROOT / "tools" / "viewer"))

from fnt import FntFormatError, read_fnt, scan  # noqa: E402
from text_render import render_text  # noqa: E402

GAME = ROOT / "game" / "%MAINDIR%"
ARIAL12 = GAME / "Data" / "Fonte" / "arial12.fnt"
FONT19 = GAME / "Data" / "Fonte" / "font19.fnt"


def test_arial12_header():
    if not ARIAL12.exists():
        pytest.skip("retail sample not present")
    font = read_fnt(ARIAL12)
    assert font.version == 1
    assert (font.atlas_width, font.atlas_height) == (256, 256)
    assert (font.font_size, font.cell_height, font.base, font.line_height) == (12, 10, 9, 12)
    assert font.glyph_count == 247
    assert font.hash == 0x97FB30A4
    assert len(font.glyphs) == 245
    assert font.padding_codes == (247, 0)
    assert len(font.atlas) == 256 * 256 * 2


def test_glyph_metrics():
    if not ARIAL12.exists():
        pytest.skip("retail sample not present")
    font = read_fnt(ARIAL12)
    a = font.glyph("A")
    h = font.glyph("H")
    assert a is not None and h is not None
    assert (a.x0, a.y0, a.x1, a.y1) == (242, 0, 248, 12)
    assert (a.width, a.height) == (7, 13)
    assert (h.x0, h.y0, h.x1, h.y1) == (38, 13, 45, 25)
    assert (h.width, h.height) == (8, 13)


def test_render_text():
    if not ARIAL12.exists():
        pytest.skip("retail sample not present")
    font = read_fnt(ARIAL12)
    image = render_text(font, "Hello, Dino Island!")
    assert image.size[0] > 80 and image.size[1] == 20
    colors = image.convert("RGB").getcolors(maxcolors=100_000)
    assert colors is not None
    foreground = sum(count for count, color in colors if color != (237, 240, 243))
    assert foreground > 100


def test_font19_atlas():
    if not FONT19.exists():
        pytest.skip("retail sample not present")
    font = read_fnt(FONT19)
    assert (font.atlas_width, font.atlas_height) == (512, 512)
    assert len(font.atlas) == 512 * 512 * 2
    assert len(font.glyphs) == 245


def test_scan_all_fonts():
    if not GAME.exists():
        pytest.skip("retail game tree not present")
    total, bad, sums = scan(GAME)
    assert total == 5
    assert not bad, bad[:5]
    assert sums["glyphs"] == 1_225


def test_rejects_bad_atlas_size(tmp_path):
    if not ARIAL12.exists():
        pytest.skip("retail sample not present")
    data = bytearray(ARIAL12.read_bytes())
    data[-1] ^= 0xFF
    data = data[:-1]
    p = tmp_path / "bad.fnt"
    p.write_bytes(data)
    with pytest.raises(FntFormatError):
        read_fnt(p)
