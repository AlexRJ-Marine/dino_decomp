import struct
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from sav import SavFormatError, read_sav  # noqa: E402

SLOT = ROOT / "run" / "save" / "slot001.sav"
AUTO = ROOT / "run" / "save" / "auto" / "auto.sav"


def test_retail_save_catalog():
    if not SLOT.exists():
        pytest.skip("retail save not present (run the patched game first)")
    save = read_sav(SLOT)
    assert save.version == 53
    assert save.name == "agenttest"
    assert len(save.shows) == 39
    assert save.shows[0].path == "DATA\\design\\scenario\\show\\Cpg1_Race_1.shw"
    assert save.shows[0].name == "The Crazy Course"
    assert save.shows[0].params == (1800, 4, 1, 19, 39, 1)
    assert save.shows[-1].path == "DATA\\design\\scenario\\show\\Cpg7_obst_2.shw"
    assert save.shows[-1].name == "The Ultimate Race"
    assert save.state_bytes == 3023


def test_auto_save_catalog():
    if not AUTO.exists():
        pytest.skip("retail auto save not present")
    save = read_sav(AUTO)
    assert save.version == 53
    assert save.name == ""
    assert len(save.shows) == 39
    assert save.state_bytes == 2829


def test_synthetic_save(tmp_path):
    def utf16(text: str) -> bytes:
        return text.encode("utf-16-le") + b"\x00\x00"

    name = utf16("slot")
    out = bytearray()
    out += struct.pack("<III", 53, 1, len(name))
    out += name + b"\x00" * 8
    shows = [
        ("DATA\\design\\scenario\\show\\A.shw", "Show A", (1, 2, 3, 4, 5, 6)),
        ("DATA\\design\\scenario\\show\\B.shw", "Show B", (7, 8, 9, 10, 11, 12)),
    ]
    out += struct.pack("<I", len(shows))
    for path, title, params in shows:
        p = path.encode("ascii") + b"\x00"
        n = utf16(title)
        out += struct.pack("<I", len(p)) + p
        out += struct.pack("<I", len(n)) + n
        out += struct.pack("<6I", *params)
    out += b"\xde\xad\xbe\xef"
    p = tmp_path / "synthetic.sav"
    p.write_bytes(out)

    save = read_sav(p)
    assert save.name == "slot"
    assert [s.name for s in save.shows] == ["Show A", "Show B"]
    assert save.shows[1].params == (7, 8, 9, 10, 11, 12)
    assert save.state == b"\xde\xad\xbe\xef"


def test_rejects_without_catalog(tmp_path):
    p = tmp_path / "bad.sav"
    p.write_bytes(struct.pack("<III", 53, 0, 0) + b"\x00" * 32)
    with pytest.raises(SavFormatError):
        read_sav(p)
