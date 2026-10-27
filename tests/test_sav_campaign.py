import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
LIVE = ROOT / "run-test" / "save" / "auto" / "auto.sav"
STABLE = ROOT / "build" / "campaign-autosave.sav"

sys.path.insert(0, str(ROOT / "tools" / "formats"))

from sav import read_sav  # noqa: E402
from sav_campaign import (  # noqa: E402
    read_campaign_list0,
    read_campaign_list1,
    read_campaign_list2,
    write_campaign_dino,
    write_campaign_food_source,
    write_campaign_employee,
    write_campaign_list0,
    write_campaign_list1,
    write_campaign_list2,
)
from wld import read_sav_world  # noqa: E402


def _sample() -> Path | None:
    campaign = LIVE if LIVE.exists() and LIVE.stat().st_size > 20000 else STABLE
    return campaign if campaign.exists() and campaign.stat().st_size > 20000 else None


def _list0_offset(campaign: Path) -> tuple[bytes, int, int]:
    import struct

    save = read_sav(campaign)
    data = campaign.read_bytes()
    off = save.state_offset
    roads = struct.unpack_from("<I", data, off)[0]
    off += 4 + roads * 16
    return data, off, struct.unpack_from("<I", data, off)[0]


def test_campaign_list0_typed_roundtrip():
    """Every list0 record re-encodes byte-identically from the typed model."""
    campaign = _sample()
    if campaign is None:
        pytest.skip("campaign autosave not present")
    data, list0_at, count = _list0_offset(campaign)
    assert count == 20
    records, end = read_campaign_list0(data, list0_at + 4, count)
    assert len(records) == 20
    assert end == 0x6942
    assert write_campaign_list0(records) == data[list0_at:end]
    for index, record in enumerate(records):
        raw = data[record.offset:record.offset + record.size]
        assert write_campaign_dino(record) == raw, f"record {index} differs"

    # The simulation id is the world record's second dword (`+0x04`, exposed
    # as field_1c), not the world record's primary object_id.
    world = read_sav_world(campaign)
    assert [d.field_1c for d in world.dinosaurs] == [
        record.object_id for record in records]

    first = records[0]
    assert first.object_id == 63823
    assert first.name == "Ankylosaur1"
    assert (first.x, first.y) == (517, 450)
    assert first.state_bytes == (3, 3, 1, 2)
    assert (first.field_10, first.field_12, first.flag) == (425, 200, 0)
    assert first.thresholds == (80, 60, 30) * 4
    assert (first.food, first.violence, first.security, first.rest) == (41, 80, 99, 95)
    assert first.need_states == (1, 0, 3, 3)
    assert first.action_id == 0x1F
    assert first.node_count == 3
    assert first.path.position == (516.0, 450.0)
    assert first.path.cells == (537, 450)
    assert (first.path.cell_x, first.path.cell_y) == (517, 450)
    assert first.path.waypoints[0] == (516, 450)
    assert first.path.waypoints[43] == (0, 0)
    assert (first.path.field_1c8, first.path.field_1cc) == (18, 1)

    last = records[-1]
    assert last.object_id == 63932
    assert last.name == "Coelophysi2"
    assert (last.x, last.y) == (530, 379)
    assert last.node_count == 0
    assert last.size == 0x19 + len(last.name_bytes) + 0x302


def test_campaign_list1_list2_roundtrip():
    """list1 is 2 saFoodSource records (0x2C), list2 is 6 employees (0x2B)."""
    campaign = _sample()
    if campaign is None:
        pytest.skip("campaign autosave not present")
    import struct

    data, list0_at, count = _list0_offset(campaign)
    records, end = read_campaign_list0(data, list0_at + 4, count)
    assert len(records) == 20 and end == 0x6942

    list1_at = end
    assert struct.unpack_from("<I", data, list1_at)[0] == 2
    food, list1_end = read_campaign_list1(data, list1_at + 4, 2)
    assert list1_end == 0x699E
    assert [(r.object_id, r.cell_x, r.cell_y) for r in food] == [
        (264, 461, 461), (267, 461, 461)]
    assert food[0].position == (461.5, 421.5)
    assert food[1].position == (461.5, 445.5)
    assert write_campaign_list1(food) == data[list1_at:list1_end]
    for record in food:
        assert write_campaign_food_source(record) == data[
            record.offset:record.offset + 0x2C]

    list2_at = list1_end
    assert struct.unpack_from("<I", data, list2_at)[0] == 6
    employees, list2_end = read_campaign_list2(data, list2_at + 4, 6)
    assert list2_end == 0x6AA4
    assert [r.object_id for r in employees] == [13, 15, 337, 375, 411, 63785]
    assert all(r.state == 2 for r in employees)
    assert write_campaign_list2(employees) == data[list2_at:list2_end]
    for record in employees:
        assert write_campaign_employee(record) == data[
            record.offset:record.offset + 0x2B]

    assert struct.unpack_from("<I", data, list2_end)[0] == 0  # visitors
    assert struct.unpack_from("<I", data, list2_end + 4)[0] == 57  # next list


def test_campaign_whole_file_roundtrip():
    """Header/world/catalog/roads/list0 rebuild; list1+ tail stays verbatim."""
    campaign = _sample()
    if campaign is None:
        pytest.skip("campaign autosave not present")
    from sav_write import rewrite_sav_campaign

    original, rebuilt, list0_at = rewrite_sav_campaign(campaign)
    assert list0_at == 0x2632
    assert len(rebuilt) == len(original)
    assert rebuilt == original
    result = subprocess.run(
        [sys.executable, str(ROOT / "tools" / "formats" / "sav_write.py"),
         str(campaign), "--campaign"],
        capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stderr
    assert ("byte-identical whole file (campaign list0) at 0x2632 "
            "(53338 bytes)") in result.stdout


def test_campaign_tool_check():
    campaign = _sample()
    if campaign is None:
        pytest.skip("campaign autosave not present")
    result = subprocess.run(
        [sys.executable, str(ROOT / "tools" / "formats" / "sav_campaign.py"),
         str(campaign), "--check", "--limit", "2"],
        capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stderr
    assert ("list0=20 start=0x2632 end=0x6942 list1=2 end=0x699e "
            "list2=6 end=0x6aa4 visitors=0 next=57") in result.stdout
    assert "list0 round-trip: 20 records identical (17168 bytes)" in result.stdout
    assert "list1/list2 round-trip: 2 + 6 records identical (354 bytes)" in result.stdout
