import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from loc import LocFormatError, read_loc, scan  # noqa: E402

GAME = ROOT / "game" / "%MAINDIR%"
MENU = GAME / "Data" / "Overlay" / "MenuEng.loc"
PARK = GAME / "Data" / "Overlay" / "ParkEng.loc"


def test_menu_loc():
    if not MENU.exists():
        pytest.skip("retail sample not present")
    loc = read_loc(MENU)
    assert loc.file_version == "2"
    assert len(loc.records) == 56
    first = loc.records[0]
    assert first.field_id == "&NoText"
    assert first.text == "NO TEXT"
    assert first.last_modified == "16/7/2001 12:27"
    assert loc.records[-1].field_id == "&Isl10_Name"
    assert loc.records[-1].text == "ANTARTIKA"


def test_park_loc():
    if not PARK.exists():
        pytest.skip("retail sample not present")
    loc = read_loc(PARK)
    assert loc.file_version == "2"
    assert len(loc.records) == 702
    assert loc.records[0].field_id == "&"
    assert loc.records[0].text == ""
    # A long value with literal \n escapes must be preserved byte-for-byte.
    assert any("\\n" in rec.text for rec in loc.records)


def test_scan_all_loc():
    if not GAME.exists():
        pytest.skip("retail game tree not present")
    total, bad, sums = scan(GAME)
    assert total == 2
    assert not bad, bad[:5]
    assert sums["records"] == 758


def test_rejects_bad_loc(tmp_path):
    p = tmp_path / "bad.loc"
    p.write_bytes(b"\xff\xfeTEXT=\"orphan\"\r\n")
    with pytest.raises(LocFormatError):
        read_loc(p)
