"""Dump the populated simulation ``list0`` of a campaign autosave.

``list0`` holds ``saDinosaur`` records. The packed prefix is 0x19 bytes; the
tail is schema-driven and its length is
``0x2D1 + threshold_block + texture_bytes + 0x16*node_count``, where
``threshold_block`` is 0x30 when the flag at ``+0x14`` is zero. The path-node
count is the u32 at ``path_start + 0x1C6`` (tail ``+0x2C7`` in the retail
form). See ``notes/campaign-list0-tail-recon.md`` and ``docs/formats/sav.md``.

The free-game save form leaves the lists empty.
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from sav import read_sav  # noqa: E402
from wld import read_sav_world  # noqa: E402


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def parse_record(data: bytes, start: int) -> tuple[int, int, int, bytes, str, int, int, int]:
    object_id = u32(data, start)
    x = u32(data, start + 4)
    y = u32(data, start + 8)
    states = tuple(data[start + 0x0C:start + 0x10])
    flag = data[start + 0x14]
    name_len = u32(data, start + 0x15)
    name = data[start + 0x19:start + 0x19 + name_len].decode(
        "utf-16-le").rstrip("\x00")
    name_end = start + 0x19 + name_len
    cursor = name_end + 4  # +0x38, always 0
    if flag == 0:
        cursor += 0x30  # four [80,60,30] threshold triples
    cursor += 0xB0  # +0x214 gauge/action block
    cursor += 0x0C  # +0x208 state dwords
    cursor += 3     # +0x514..+0x516 flags
    texture_bytes = u32(data, cursor)
    cursor += 4 + texture_bytes
    cursor += 4 + 1 + 4  # f524, f517, f1d0
    path_start = cursor
    nodes = u32(data, path_start + 0x1C6)
    tail = (path_start - name_end) + 0x201 + 0x16 * nodes
    return object_id, x, y, states, name, tail, nodes, name_end + tail


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
        (object_id, x, y, states, name, tail, nodes,
         end) = parse_record(data, start)
        print(f"{index:2d} id={object_id} x={x} y={y} states={states} "
              f"name={name!r} tail={tail:#x} n={nodes} end={end:#x}")
        start = end
    print(f"list0_end={start:#x} list1_count={u32(data, start)}")
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
