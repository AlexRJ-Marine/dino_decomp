import sys
from pathlib import Path

import pytest
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from tga import read_tga, scan  # noqa: E402

GAME = ROOT / "game" / "%MAINDIR%"
CURSOR = GAME / "Data" / "Graphics" / "Cursors" / "CursorMain.tga"
COW = GAME / "Data" / "Graphics" / "Textures" / "actors" / "animals" / "cow_lod1.tga"
CASE = GAME / "Data" / "Graphics" / "Textures" / "helper" / "case.tga"


def _assert_matches_pil(path):
    t = read_tga(path)
    im = Image.open(path).convert("RGBA")
    assert (t.width, t.height) == im.size
    assert t.pixels == im.tobytes()


def test_cursor_32bit():
    if not CURSOR.exists():
        pytest.skip("retail sample not present")
    t = read_tga(CURSOR)
    assert (t.width, t.height, t.pixel_depth) == (32, 32, 32)
    assert t.image_type == 2
    _assert_matches_pil(CURSOR)


def test_cow_24bit():
    if not COW.exists():
        pytest.skip("retail sample not present")
    t = read_tga(COW)
    assert (t.width, t.height, t.pixel_depth) == (128, 128, 24)
    _assert_matches_pil(COW)


def test_case_8bit_grayscale():
    if not CASE.exists():
        pytest.skip("retail sample not present")
    t = read_tga(CASE)
    assert (t.width, t.height, t.pixel_depth) == (32, 32, 8)
    assert t.image_type == 3
    _assert_matches_pil(CASE)


def test_header_only_does_not_decode():
    if not CURSOR.exists():
        pytest.skip("retail sample not present")
    t = read_tga(CURSOR, decode=False)
    assert t.pixels == b""
    assert (t.width, t.height) == (32, 32)


def test_scan_all_tgas():
    if not GAME.exists():
        pytest.skip("retail game tree not present")
    total, bad, sums = scan(GAME)
    assert total == 649
    assert not bad, bad[:5]
    assert sums["pixels"] == 37_486_449
