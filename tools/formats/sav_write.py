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
from wld import read_sav_world


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


def write_visitor(visitor) -> bytes:
    out = bytearray()
    out += _u32(visitor.object_id)
    out += struct.pack("<i", visitor.x)
    out += struct.pack("<i", visitor.y)
    out += _u32(visitor.flags)
    out += _u32(visitor.unknown_10)
    out += _u32(visitor.state)
    out += struct.pack("<4f", visitor.fx, visitor.fy, visitor.dx, visitor.dy)
    out += _u32(visitor.unknown_28)
    out += _u32(visitor.zero_2c)
    out += _u32(visitor.visitor_id)
    for need in visitor.needs:
        out += _u32(need)
    return bytes(out)


def write_simulation(graph) -> bytes:
    """Serialize the whole simulation section (everything after the world block)."""
    out = bytearray()
    out += _u32(len(graph.road))
    for road in graph.road:
        out += struct.pack("<4i", road.x0, road.y0, road.x1, road.y1)
    for _ in range(3):
        out += _u32(0)
    out += _u32(len(graph.visitors))
    for visitor in graph.visitors:
        out += write_visitor(visitor)
    out += _u32(1)
    out += write_park(graph.park)
    out += _u32(len(graph.dino_store))
    for entry in graph.dino_store:
        name = entry.name.encode("utf-16-le") + b"\x00\x00"
        out += _u32(len(name)) + name
        out += _u32(entry.price)
        path = entry.path.encode("ascii") + b"\x00"
        out += _u32(len(path)) + path
        description = entry.description.encode("ascii") + b"\x00"
        out += _u32(len(description)) + description
    for value in graph.visitor_model:
        out += _u32(value)
    out += _u32(graph.lab_count)
    for record in graph.lab_records:
        out += bytes([record.flag])
        if record.flag != 0:
            continue
        for first, second in record.genes:
            out += struct.pack("<II", first, second)
        name = record.name.encode("ascii") + b"\x00"
        out += _u32(len(name)) + name
    for count in graph.ai_counts:
        out += _u32(count)
    for entry in graph.ai_entries:
        out += entry
    out += struct.pack("<10f", *graph.camera)
    out += graph.tail_extra
    return bytes(out)


def rewrite_sav_full(path: Path) -> tuple[bytes, bytes, int]:
    """Rebuild the whole simulation section; return (original, rebuilt, offset)."""
    original = path.read_bytes()
    graph = read_sav_graph(path)
    state_offset = graph.save.state_offset
    rebuilt = original[:state_offset] + write_simulation(graph)
    return original, rebuilt, state_offset


def write_header(save) -> bytes:
    out = bytearray()
    out += _u32(save.version)
    out += _u32(save.slot)
    out += _u32(save.name_len)
    out += save.data[12:12 + save.name_len]
    out += save.header_tail
    return bytes(out)


def write_waypoints(points) -> bytes:
    out = bytearray(_u32(len(points)))
    for w in points:
        out += _u32(w.flags) + _u32(w.key) + _u32(w.x) + _u32(w.y)
        out += _u32(w.z) + _u32(w.extra)
    return bytes(out)


def write_placed_base(obj) -> bytes:
    out = bytearray()
    out += _u32(obj.object_id)
    out += _u32(obj.field_14_first if hasattr(obj, "field_14_first")
                else obj.field_14)
    out += struct.pack("<4I", obj.start[0], obj.start[1],
                       obj.end[0], obj.end[1])
    out += write_waypoints(obj.waypoints)
    return bytes(out)


def write_world(world, version: int) -> bytes:
    out = bytearray()
    out += _u32(world.campaign_id or 0)
    out += _u32(world.mission_id or 0)
    if version > 9:
        out += _u32(len(world.objectives))
        for first, second, third in world.objectives:
            out += _u32(first) + _u32(second) + _u32(third)
    out += _u32(len(world.fences))
    for fence in world.fences:
        out += write_placed_base(fence)
        out += _u32(fence.class_tag)
        out += _u32(fence.field_14)
        out += fence.extra16
        out += _u32(fence.field_44) + _u32(fence.field_48)
    out += _u32(len(world.shows))
    for show in world.shows:
        out += write_placed_base(show)
        out += _u32(show.class_tag) + _u32(show.field_44) + _u32(show.field_48)
    if version > 1:
        out += _u32(len(world.animals))
        for animal in world.animals:
            out += _u32(animal.object_id) + _u32(animal.field_1c)
            if version > 0x32:
                out += _u32(animal.field_20 or 0)
    if version > 2:
        out += _u32(len(world.employees))
        for employee in world.employees:
            out += _u32(employee.object_id) + _u32(employee.field_1c)
    out += _u32(len(world.buildings))
    for building in world.buildings:
        out += _u32(building.object_id) + _u32(building.linked_id)
        out += _u32(building.class_tag) + _u32(building.kind)
        out += _u32(building.cell_x) + _u32(building.cell_y)
        out += _u32(building.state) + _u32(building.link)
    if not (6 <= version <= 0x10):
        out += _u32(len(world.visitors))
        for visitor in world.visitors:
            out += _u32(visitor.object_id) + _u32(visitor.field_1c)
            out += _u32(visitor.class_tag) + _u32(visitor.field_20)
            out += _u32(visitor.field_24)
            if version >= 5:
                out += _u32(visitor.field_28 or 0)
    if version > 1:
        out += _u32(len(world.dinosaurs))
        for dino in world.dinosaurs:
            out += _u32(dino.object_id) + _u32(dino.field_1c)
            if version >= 0x25:
                out += _u32(dino.field_24 or 0) + _u32(dino.field_28 or 0)
                out += _u32(dino.field_2c or 0)
            out += _u32(dino.class_tag)
            for first, second in dino.genotype:
                out += _u32(first) + _u32(second)
    for flags in (world.feature_flags, world.building_flags,
                  world.food_flags, world.misc_flags):
        out += _u32(len(flags)) + bytes(flags)
    return bytes(out)


def write_catalog(save) -> bytes:
    out = bytearray(_u32(len(save.shows)))
    for show in save.shows:
        path = show.path.encode("ascii") + b"\x00"
        out += _u32(len(path)) + path
        name = show.name.encode("utf-16-le") + b"\x00\x00"
        out += _u32(len(name)) + name
        out += struct.pack("<6I", *show.params)
    return bytes(out)


def rewrite_sav_whole(path: Path) -> tuple[bytes, bytes, int]:
    """Rebuild header + world block + show catalog + simulation."""
    original = path.read_bytes()
    save = read_sav(path)
    world = read_sav_world(path)
    graph = read_sav_graph(path)
    rebuilt = (write_header(save) + write_world(world, save.version) +
               write_catalog(save) + write_simulation(graph))
    return original, rebuilt, 0


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
    parser.add_argument("--full", action="store_true",
                        help="rebuild the whole simulation section")
    parser.add_argument("--whole", action="store_true",
                        help="rebuild header + world + catalog + simulation")
    args = parser.parse_args(argv)

    if args.whole:
        rewrite = rewrite_sav_whole
    elif args.full:
        rewrite = rewrite_sav_full
    else:
        rewrite = rewrite_sav
    original, rebuilt, offset = rewrite(args.sav)
    if args.out:
        args.out.write_bytes(rebuilt)
    length_ok = len(original) == len(rebuilt)
    diff = first_diff(original, rebuilt)
    if length_ok and diff == len(original):
        what = ("whole file" if args.whole
                else "simulation" if args.full else "park block")
        print(f"{args.sav}: byte-identical {what} at 0x{offset:x} "
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
