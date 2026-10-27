"""Dump the populated simulation ``list0`` of a campaign autosave.

``list0`` holds ``saDinosaur`` records. The typed record model is
``sav_campaign.py``/``read_campaign_dino``; the packed prefix is 0x19 bytes
and the tail is schema-driven, so there is no record-length prefix. This
scanner is the compact table view used by tests and recon.

The free-game save form leaves the lists empty.
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from sav import read_sav  # noqa: E402
from sav_campaign import (  # noqa: E402
    read_campaign_list0,
    read_campaign_list1,
    read_campaign_list2,
)
from wld import read_sav_world  # noqa: E402


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def scan(path: Path) -> int:
    save = read_sav(path)
    world = read_sav_world(path)
    data = save.data
    offset = save.state_offset
    roads = u32(data, offset)
    offset += 4 + roads * 16
    count = u32(data, offset)
    start = offset + 4
    print(f"sav campaign scan: version={save.version} name={save.name!r} "
          f"world_dinos={len(world.dinosaurs)} roads={roads} list0={count}")
    if count == 0:
        print("list0: empty (free-game form)")
        return 0

    records, end = read_campaign_list0(data, start, count)
    for index, record in enumerate(records):
        tail = record.size - 0x19 - len(record.name_bytes)
        print(f"{index:2d} id={record.object_id} x={record.x} y={record.y} "
              f"states={record.state_bytes} name={record.name!r} "
              f"tail={tail:#x} n={record.node_count} "
              f"end={record.offset + record.size:#x}")
    list1_count = u32(data, end)
    food, list1_end = read_campaign_list1(data, end + 4, list1_count)
    list2_count = u32(data, list1_end)
    employees, list2_end = read_campaign_list2(
        data, list1_end + 4, list2_count)
    visitor_count = u32(data, list2_end)
    visitor_end = list2_end + 4 + visitor_count * 0x50
    print(f"list0_end={end:#x} list1_count={list1_count}")
    for index, record in enumerate(food):
        print(f"L1 {index:2d} id={record.object_id} "
              f"cell=({record.cell_x},{record.cell_y}) "
              f"pos=({record.position[0]:.1f},{record.position[1]:.1f})")
    print(f"list1_end={list1_end:#x} list2_count={list2_count}")
    for index, record in enumerate(employees):
        print(f"L2 {index:2d} id={record.object_id} "
              f"cell=({record.cell_x},{record.cell_y}) state={record.state}")
    print(f"list2_end={list2_end:#x} visitor_count={visitor_count} "
          f"next_count={u32(data, visitor_end)}")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sav", type=Path)
    args = parser.parse_args(argv)
    try:
        return scan(args.sav)
    except Exception as exc:  # noqa: BLE001 - CLI should print one clean error
        print(f"sav campaign scan failed: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
