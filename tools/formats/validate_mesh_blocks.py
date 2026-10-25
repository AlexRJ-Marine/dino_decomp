#!/usr/bin/env python3
"""Validate the mesh block grammar across all .mesh files."""
from __future__ import annotations

import struct
import sys
from collections import Counter
from pathlib import Path

MAGIC = b"\x01\x03\x00\x00\x00"


def parse(data: bytes):
    name_len = struct.unpack_from("<I", data, 17)[0]
    name = data[21 : 21 + name_len].rstrip(b"\0").decode("ascii", "replace")
    off = 21 + name_len
    count = struct.unpack_from("<I", data, off)[0]
    off += 4
    pos_end = off + count * 12
    idx_count = struct.unpack_from("<I", data, pos_end)[0]
    idx_start = pos_end + 4
    idx_end = idx_start + idx_count * 4
    return name, count, off, idx_count, idx_start, idx_end


def main():
    root = Path(sys.argv[1] if len(sys.argv) > 1 else "game/%MAINDIR%")
    stats = Counter()
    examples = {}
    bad = []

    for p in sorted(root.rglob("*.mesh")):
        data = p.read_bytes()
        try:
            name, count, pos_start, idx_count, idx_start, off = parse(data)
        except Exception as e:
            bad.append((p.name, f"header: {e}"))
            continue

        if off > len(data):
            bad.append((p.name, "indices past EOF"))
            continue
        if idx_count:
            indices = struct.unpack_from("<%dI" % idx_count, data, idx_start)
            if max(indices) >= count:
                bad.append((p.name, f"index {max(indices)} >= vertex count {count}"))
                continue

        flags = []
        ok = True
        for label, stride in (("uv", 8), ("normal", 12)):
            if off + 5 > len(data):
                flags.append(f"{label}:eof")
                ok = False
                break
            flag = data[off]
            c = struct.unpack_from("<I", data, off + 1)[0]
            flags.append(f"{label}:{flag}/{c}")
            if flag == 1 and c == count:
                off += 5 + c * stride
            elif flag == 0:
                off += 5
            else:
                ok = False
                break

        stats[tuple(flags)] += 1
        examples.setdefault(tuple(flags), p.name)
        if not ok:
            bad.append((p.name, "block grammar " + ",".join(flags)))

    print(f"parsed {sum(stats.values())} meshes, {len(bad)} bad")
    for k, n in stats.most_common(20):
        print(f"  {n:4}  {k}   e.g. {examples[k]}")
    print("\nbad examples:")
    for name, why in bad[:30]:
        print(f"  {name}: {why}")


if __name__ == "__main__":
    main()
