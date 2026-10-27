"""Dump the populated simulation lists of a campaign autosave.

The free-game save form leaves the three simulation lists after the roads
empty. A live campaign autosave populates ``list0`` with one record per
dinosaur; this tool walks that list using the packed 0x19-byte prefix
documented in ``docs/formats/sav.md`` and prints the id/position/needs/name
plus the variable tail length.

The record tail itself is not decoded yet; the next record start is found by
scanning for the next plausible packed name and validating the x/y prefix.
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from sav import read_sav  # noqa: E402
from wld import read_sav_world  # noqa: E402

PREFIX = 0x19


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def plausible_name(data: bytes, start: int) -> str | None:
    if start < 4:
        return None
    length = u32(data, start - 4)
    if length < 4 or length > 80 or length % 2:
        return None
    if start + length > len(data):
        return None
    try:
        text = data[start:start + length].decode("utf-16-le")
    except UnicodeDecodeError:
        return None
    text = text.rstrip("\x00")
    if len(text) < 3:
        return None
    if not all(ch.isalnum() or ch in " _" for ch in text):
        return None
    return text


def find_next_record(data: bytes, record_start: int, name_end: int) -> int | None:
    for name_start in range(name_end, len(data) - 4):
        if plausible_name(data, name_start) is None:
            continue
        candidate = name_start - PREFIX
        if candidate <= record_start:
            continue
        x = u32(data, candidate + 4)
        y = u32(data, candidate + 8)
        if x < 4096 and y < 4096:
            return candidate
    return None


def scan(path: Path) -> int:
    save = read_sav(path)
    world = read_sav_world(path)
    data = save.data
    offset = save.state_offset
    roads = u32(data, offset)
    offset += 4 + roads * 16
    count = u32(data, offset)
    offset += 4
    print(f"sav campaign scan: version={save.version} name={save.name!r} "
          f"world_dinos={len(world.dinosaurs)} roads={roads} list0={count}")
    if count == 0:
        print("list0: empty (free-game form)")
        return 0

    start = offset
    for index in range(count):
        object_id = u32(data, start)
        x = u32(data, start + 4)
        y = u32(data, start + 8)
        states = tuple(data[start + 0x0C:start + 0x10])
        name_len = u32(data, start + 0x15)
        name = data[start + 0x19:start + 0x19 + name_len].decode(
            "utf-16-le").rstrip("\x00")
        name_end = start + 0x19 + name_len
        next_start = (find_next_record(data, start, name_end)
                      if index + 1 < count else None)
        tail = None if next_start is None else next_start - name_end
        print(f"{index:2d} id={object_id} x={x} y={y} states={states} "
              f"name={name!r} "
              f"tail={'?' if tail is None else hex(tail)}")
        if next_start is None:
            break
        start = next_start
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
