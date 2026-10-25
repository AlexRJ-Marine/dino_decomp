#!/usr/bin/env python3
"""Probe the suspected mesh block chain: positions, indices, normals, UVs."""
from __future__ import annotations

import struct
import sys
from pathlib import Path

MAGIC = b"\x01\x03\x00\x00\x00"


def parse(data: bytes):
    name_len = struct.unpack_from("<I", data, 17)[0]
    name = data[21 : 21 + name_len].rstrip(b"\0").decode("ascii", "replace")
    return name, 21 + name_len


def vecs(data, off, count, n):
    return [struct.unpack_from("<%df" % n, data, off + i * n * 4) for i in range(count)]


def stats(vec, n):
    return [(round(min(v[i] for v in vec), 3), round(max(v[i] for v in vec), 3)) for i in range(n)]


def main():
    root = Path(sys.argv[1] if len(sys.argv) > 1 else "game/%MAINDIR%")
    name = sys.argv[2] if len(sys.argv) > 2 else "cow_lod1.mesh"
    p = next(p for p in root.rglob(name) if p.name == name)
    data = p.read_bytes()
    name, start = parse(data)
    count = struct.unpack_from("<I", data, start)[0]
    off = start + 4
    print(f"{name}: count={count} size={len(data)}")

    pos = vecs(data, off, count, 3)
    print("positions:", stats(pos, 3), "first", pos[0])
    off += count * 12

    idx_count = struct.unpack_from("<I", data, off)[0]
    off += 4
    idx = struct.unpack_from("<%dI" % idx_count, data, off)
    print(f"index_count={idx_count} max={max(idx)} distinct={len(set(idx))}")
    print("  first 18:", idx[:18])
    off += idx_count * 4

    print(f"next byte at 0x{off:x}: 0x{data[off]:02x}")
    block_count = struct.unpack_from("<I", data, off + 1)[0]
    print(f"next count at 0x{off+1:x}: {block_count}")
    off += 5

    normals = vecs(data, off, block_count, 3)
    lens = [sum(c * c for c in n) ** 0.5 for n in normals]
    print("normals:", stats(normals, 3), "len min/avg/max:",
          round(min(lens), 3), round(sum(lens) / len(lens), 3), round(max(lens), 3))
    off += block_count * 12

    print(f"next byte at 0x{off:x}: 0x{data[off]:02x}")
    block_count2 = struct.unpack_from("<I", data, off + 1)[0]
    print(f"next count at 0x{off+1:x}: {block_count2}")
    off += 5

    uvs = vecs(data, off, block_count2, 2)
    print("uvs:", stats(uvs, 2), "first", uvs[0])
    off += block_count2 * 8

    print(f"after UVs: 0x{off:x}, remaining={len(data)-off}")
    print("next 64 bytes:", data[off : off + 64].hex(" "))
    print("next u32:", struct.unpack_from("<8I", data, off) if off + 32 <= len(data) else None)
    print("next f32:", [round(x, 4) for x in struct.unpack_from("<8f", data, off)] if off + 32 <= len(data) else None)


if __name__ == "__main__":
    main()
