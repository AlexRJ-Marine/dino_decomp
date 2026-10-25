import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from mesh_header import MAGIC, MeshHeader, parse_header, parse_header_bytes, scan  # noqa: E402

GAME = ROOT / "game" / "%MAINDIR%"


def test_sample_header():
    path = GAME / "Data" / "Graphics" / "Actors" / "animals" / "cow" / "cow_lod1.mesh"
    if not path.exists():
        pytest.skip("retail sample not present")
    h = parse_header(path)
    assert h.ok, h.errors
    assert h.sentinel == 0x12345678
    assert h.name == "cow_lod1"
    assert h.payload_count == 93
    assert h.chunk_count == 1
    assert len(h.bbox_min) == 3


def test_scan_all_meshes():
    if not GAME.exists():
        pytest.skip("retail game tree not present")
    result = scan(GAME)
    assert len(result.files) >= 500, f"only found {len(result.files)} .mesh files"
    assert not result.errors, result.errors[:5]


def test_bad_magic():
    h = parse_header_bytes(b"not a mesh" + b"\x00" * 32)
    assert not h.ok
    assert "bad magic" in h.errors[0]


def test_bad_sentinel():
    raw = bytearray(MAGIC + b"\x00" * 4 + b"\x00\x00\x00\x00" + b"\x01\x00\x00\x00")
    raw += b"\x02\x00\x00\x00c\x00"
    h = parse_header_bytes(bytes(raw))
    assert not h.ok
    assert any("sentinel" in e for e in h.errors)


def test_truncated():
    h = parse_header_bytes(MAGIC)
    assert not h.ok
    assert "too short" in h.errors[0]
