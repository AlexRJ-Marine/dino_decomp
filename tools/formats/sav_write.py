#!/usr/bin/env python3
"""Re-serialize a retail `.sav` park block (SAVE-0).

The park record is the first writer-ordered structure in the save and the one
whose field order is fully documented (`docs/formats/sav.md`,
`tools/formats/sav_graph.py::_read_park_stream`).  This module rebuilds the
0x293-byte record from the parsed model and splices it back into the original
file, so a byte-identical result proves the writer order before any C++ port.

CLI:
    python sav_write.py run/save/slot001.sav [--out build/roundtrip.sav]

Exit code 0 means byte-identical; 1 prints the first mismatch and a hexdump.
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from sav import read_sav
from sav_graph import ParkState, read_sav_graph


def _u32(value: int) -> bytes:
    return struct.pack("<I", value & 0xFFFFFFFF)


def write_park(park: ParkState) -> bytes:
    """Serialize the park record in retail writer order (0x00585780)."""
    out = bytearray()
    out += _u32(park.class_id)
    out += _u32(park.header_a)
    out += _u32(park.header_b)
    out += struct.pack("<f", park.header_c)
    out += park.header_reserved
    out += _u32(park.budget)
    out += struct.pack("<f", park.sellback_factor)
    out += _u32(park.food_total)
    out += _u32(park.food_count)
    out += _u32(park.staff_secu_total)
    out += _u32(park.staff_secu_count)
    out += _u32(park.ticket_total)
    out += _u32(park.ticket_count)
    out += bytes(park.bankruptcy_flags)
    out += _u32(park.staff_buckets[0])
    out += _u32(park.staff_buckets[1])
    for value in park.shop_revenue:
        out += _u32(value)
    for value in park.shop_sales:
        out += _u32(value)
    for value in (park.group_a_count, park.group_a_total,
                  park.group_b_count, park.group_b_total,
                  park.group_c_count, park.group_c_total,
                  park.generic_count, park.generic_total):
        out += _u32(value)
    for pair in (park.yesterday_staff_buckets, park.yesterday_food,
                 park.yesterday_secu):
        out += _u32(pair[0])
        out += _u32(pair[1])
    for values in (park.yesterday_revenue, park.yesterday_sales,
                   park.yesterday_totals):
        for value in values:
            out += _u32(value)
    for pair in (park.cumulative_staff_buckets, park.cumulative_food,
                 park.cumulative_secu):
        out += _u32(pair[0])
        out += _u32(pair[1])
    for values in (park.cumulative_revenue, park.cumulative_sales,
                   park.cumulative_totals):
        for value in values:
            out += _u32(value)
    out += _u32(park.schedule_day)
    out += struct.pack("<f", park.schedule_elapsed)
    out += struct.pack("<f", park.schedule_current)
    out += _u32(park.schedule_field)
    for task in park.schedule_tasks:
        out += _u32(task)
    out += park.trailer
    out += bytes([park.tail_final])
    return bytes(out)


def rewrite_sav(path: Path) -> tuple[bytes, bytes, int]:
    """Rebuild the park block in `path`; return (original, rebuilt, offset)."""
    original = path.read_bytes()
    state_offset = read_sav(path).state_offset
    park = read_sav_graph(path).park
    park_offset = state_offset + park.offset
    rebuilt_park = write_park(park)
    rebuilt = (original[:park_offset] + rebuilt_park +
               original[park_offset + len(park.raw):])
    return original, rebuilt, park_offset


def first_diff(a: bytes, b: bytes) -> int:
    for index in range(min(len(a), len(b))):
        if a[index] != b[index]:
            return index
    return min(len(a), len(b))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sav", type=Path)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args(argv)

    original, rebuilt, offset = rewrite_sav(args.sav)
    if args.out:
        args.out.write_bytes(rebuilt)
    length_ok = len(original) == len(rebuilt)
    diff = first_diff(original, rebuilt)
    if length_ok and diff == len(original):
        print(f"{args.sav}: byte-identical park block at 0x{offset:x} "
              f"({len(rebuilt)} bytes)")
        return 0
    print(f"{args.sav}: MISMATCH len {len(original)} -> {len(rebuilt)}, "
          f"first diff at 0x{diff:x} (park at 0x{offset:x})")
    start = max(0, diff - 16)
    print("  original:", original[start:start + 32].hex(" "))
    print("  rebuilt :", rebuilt[start:start + 32].hex(" "))
    return 1


if __name__ == "__main__":
    sys.exit(main())
