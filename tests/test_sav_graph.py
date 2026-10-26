import struct
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from sav_graph import (  # noqa: E402
    PARK_BLOCK_SIZE,
    VISITOR_RECORD_SIZE,
    SavGraphError,
    read_sav_graph,
)

SLOT = ROOT / "run" / "save" / "slot001.sav"
AUTO = ROOT / "run" / "save" / "auto" / "auto.sav"


def _utf16(text: str) -> bytes:
    return text.encode("utf-16-le") + b"\x00\x00"


def _visitor(object_id: int, x: int, y: int, visitor_id: int, needs: tuple[int, ...]) -> bytes:
    out = bytearray(VISITOR_RECORD_SIZE)
    struct.pack_into("<IiiII", out, 0, object_id, x, y, 0x00010001, 0)
    struct.pack_into("<I", out, 0x14, 1)
    struct.pack_into("<4f", out, 0x18, x + 0.5, y + 0.5, 0.0, 1.0)
    struct.pack_into("<II", out, 0x28, 0, 0)
    struct.pack_into("<I", out, 0x30, visitor_id)
    struct.pack_into("<7I", out, 0x34, *needs)
    return bytes(out)


def _park_block(budget: int) -> bytes:
    block = bytearray(PARK_BLOCK_SIZE)
    struct.pack_into("<I", block, 0x30, budget)
    struct.pack_into("<f", block, 0x34, 1.0)
    struct.pack_into("<I", block, 0x4C, 2)  # base + 0x24
    return bytes(block)


def _dino(name: str, store_id: int, path: str, desc: str) -> bytes:
    n = _utf16(name)
    p = path.encode("ascii") + b"\x00"
    d = desc.encode("ascii") + b"\x00"
    return (
        struct.pack("<I", len(n)) + n
        + struct.pack("<I", store_id)
        + struct.pack("<I", len(p)) + p
        + struct.pack("<I", len(d)) + d
    )


def _state() -> bytes:
    out = bytearray()
    out += struct.pack("<I", 1) + struct.pack("<4i", 524, 392, 524, 452)
    out += struct.pack("<3I", 0, 0, 0)
    out += struct.pack("<I", 1) + _visitor(7, 500, 400, 42, (90, 91, 92, 93, 94, 95, 96))
    out += struct.pack("<I", 1) + _park_block(75000)
    out += struct.pack("<I", 2)
    out += _dino("Allosaurus", 9000, "DATA\\Design\\Scenario\\Genotype\\allo.gen", "&allo_description")
    out += _dino("Velociraptor", 15000, "DATA\\Design\\Scenario\\Genotype\\velo.gen", "&velo_description")
    out += struct.pack("<5I", 3, 3, 3, 3, 3)
    out += struct.pack("<I", 2) + b"\x01\x01"
    out += struct.pack("<4I", 0, 0, 0, 0)
    out += struct.pack("<I", 0)  # fifth AI list, non-auto saves only
    out += struct.pack("<10f", *([0.0] * 10))
    out += b"\xaa\xbb\xcc"
    return bytes(out)


def _synthetic_save(path: Path) -> None:
    name = _utf16("slot")
    out = bytearray()
    out += struct.pack("<III", 53, 1, len(name))
    out += name + b"\x00" * 8
    show_path = b"DATA\\design\\scenario\\show\\A.shw\x00"
    show_name = _utf16("Show")
    out += struct.pack("<I", 1)
    out += struct.pack("<I", len(show_path)) + show_path
    out += struct.pack("<I", len(show_name)) + show_name
    out += struct.pack("<6I", 1, 2, 3, 4, 5, 6)
    out += _state()
    path.write_bytes(out)


def test_retail_manual_graph():
    if not SLOT.exists():
        pytest.skip("retail save not present (run the patched game first)")
    graph = read_sav_graph(SLOT)
    assert len(graph.save.shows) == 39
    assert [r.to_dict() for r in graph.road] == [
        {"x0": 524, "y0": 392, "x1": 524, "y1": 452}
    ]
    assert graph.park is not None
    assert graph.park.budget == 75125
    assert graph.park.ticket_total == 125
    assert graph.park.ticket_count == 2
    assert graph.park.staff_buckets == (0, 0)
    assert graph.park.yesterday_totals[2:5] == (0xBAADF00D,) * 3
    assert graph.park.cumulative_totals[2:5] == (0xBAADF00D,) * 3
    assert graph.park.stream_end == 0x24A
    assert len(graph.park.tail) == 0x49
    assert len(graph.visitors) == 2
    assert [v.visitor_id for v in graph.visitors] == [38, 39]
    assert graph.visitors[0].needs == (97, 96, 99, 99, 99, 99, 99)
    assert len(graph.dino_store) == 20
    assert graph.dino_store[0].name == "Allosaurus"
    assert graph.dino_store[0].store_id == 9000
    assert graph.dino_store[-1].name == "Velociraptor"
    assert graph.visitor_model == (3, 3, 3, 3, 3)
    assert graph.lab_count == 8
    assert graph.lab_flags == b"\x01" * 8
    assert graph.ai_counts == (0, 0, 0, 0, 2)
    assert len(graph.ai_entries) == 2
    assert len(graph.ai_entries[0]) == 17
    assert len(graph.camera) == 10
    assert graph.camera[0] == pytest.approx(551.0, abs=0.01)
    assert graph.tail_extra == b""
    assert len(graph.tail) == 94


def test_retail_auto_graph():
    if not AUTO.exists():
        pytest.skip("retail auto save not present")
    graph = read_sav_graph(AUTO)
    assert graph.park is not None
    assert graph.park.budget == 75000
    assert graph.park.ticket_total == 0
    assert graph.park.cumulative_totals[2:5] == (0xBAADF00D,) * 3
    assert graph.park.stream_end == 0x24A
    assert len(graph.park.tail) == 0x49
    assert graph.visitors == []
    assert len(graph.dino_store) == 20
    assert graph.ai_counts == (0, 0, 0, 0, 0)
    assert graph.ai_entries == []
    assert graph.camera[:3] == pytest.approx((128.0, 128.0, 250.0))
    assert graph.tail_extra == b""
    assert len(graph.tail) == 60


def test_synthetic_graph(tmp_path):
    path = tmp_path / "synthetic.sav"
    _synthetic_save(path)
    graph = read_sav_graph(path)
    assert len(graph.road) == 1
    assert graph.park is not None
    assert graph.park.budget == 75000
    assert graph.park.ticket_count == 2
    assert graph.park.stream_end == 0x24A
    assert len(graph.park.tail) == 0x49
    assert graph.park.cumulative_revenue == (0,) * 13
    assert len(graph.visitors) == 1
    assert graph.visitors[0].object_id == 7
    assert graph.visitors[0].visitor_id == 42
    assert graph.visitors[0].needs[0] == 90
    assert [d.name for d in graph.dino_store] == ["Allosaurus", "Velociraptor"]
    assert graph.visitor_model == (3, 3, 3, 3, 3)
    assert graph.lab_count == 2
    assert graph.ai_counts == (0, 0, 0, 0, 0)
    assert graph.ai_entries == []
    assert graph.camera == (0.0,) * 10
    assert graph.tail_extra == b"\xaa\xbb\xcc"


def test_rejects_unknown_object_list(tmp_path):
    path = tmp_path / "bad.sav"
    _synthetic_save(path)
    data = bytearray(path.read_bytes())
    state_off = data.find(struct.pack("<4i", 524, 392, 524, 452)) + 16
    struct.pack_into("<I", data, state_off, 3)  # list0 count
    path.write_bytes(data)
    with pytest.raises(SavGraphError, match="list0"):
        read_sav_graph(path)


def test_rejects_populated_lab_record(tmp_path):
    path = tmp_path / "bad-lab.sav"
    _synthetic_save(path)
    graph = read_sav_graph(path)
    flag_offset = graph.save.state_offset + graph.tail_offset - graph.lab_count
    data = bytearray(path.read_bytes())
    data[flag_offset] = 0
    path.write_bytes(data)
    with pytest.raises(SavGraphError, match="lab store"):
        read_sav_graph(path)
