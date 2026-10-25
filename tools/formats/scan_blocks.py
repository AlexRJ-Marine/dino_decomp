#!/usr/bin/env python3
"""Try candidate strides for a counted block and report the next u32."""
from __future__ import annotations

import struct
import sys
from pathlib import Path

MAGIC = b"\x01\x03\x00\x00\x00"


def parse(data: bytes):
    name_len = struct.unpack_from("<I", data, 17)[0]
    return data[21 : 21 + name_len].rstrip(b"\0").decode("ascii", "replace"), 21 + name_len


def inspect(data, off, count, label):
    print(f"\n{label}: off=0x{off:x} count={count}")
    for stride in (4, 6, 8, 10, 12, 16, 20, 24, 32, 36, 40):
        end = off + count * stride
        if end + 4 > len(data):
            continue
        vals = [struct.unpack_from("<f", data, off + i * stride)[0] for i in range(min(count, 200))]
        inrange = sum(1 for v in vals if v == v and -2.0 <= v <= 2.0)
        nxt = struct.unpack_from("<I", data, end)[0]
        print(f"  stride {stride:2}: end=0x{end:x} float_in_range={inrange}/{len(vals)} next_u32={nxt} ({nxt:#x})")


def main():
    root = Path(sys.argv[1] if len(sys.argv) > 1 else "game/%MAINDIR%")
    name = sys.argv[2] if len(sys.argv) > 2 else "cow_lod1.mesh"
    p = next(p for p in root.rglob(name) if p.name == name)
    data = p.read_bytes()
    nm, start = parse(data)
    count = struct.unpack_from("<I", data, start)[0]
    body = start + 4
    pos_end = body + count * 12
    idx_count = struct.unpack_from("<I", data, pos_end)[0]
    off = pos_end + 4 + idx_count * 4
    print(f"{name}: count={count} idx_count={idx_count} after_indices=0x{off:x} byte={data[off]:#x}")
    c1 = struct.unpack_from("<I", data, off + 1)[0]
    inspect(data, off + 5, c1, "block1")
    # try the next count at each candidate end
    for stride in (8, 12, 16):
        nxt_off = off + 5 + c1 * stride
        if nxt_off + 4 <= len(data):
            nxt = struct.unpack_from("<I", data, nxt_off)[0]
            print(f"  block1 stride {stride}: next_off=0x{nxt_off:x} next_u32={nxt}")
            if 0 < nxt < 10000:
                inspect(data, nxt_off + 4, nxt, f"block2 (after stride {stride})")


if __name__ == "__main__":
    main()
