#!/usr/bin/env python3
"""Test the hypothesis: positions, normals, UVs, then 16-bit indices."""
from __future__ import annotations

import struct
import sys
from pathlib import Path

MAGIC = b"\x01\x03\x00\x00\x00"


def parse(data: bytes):
    name_len = struct.unpack_from("<I", data, 17)[0]
    name = data[21 : 21 + name_len].rstrip(b"\0").decode("ascii", "replace")
    return name, 21 + name_len


def main():
    root = Path(sys.argv[1] if len(sys.argv) > 1 else "game/%MAINDIR%")
    name = sys.argv[2] if len(sys.argv) > 2 else "cow_lod1.mesh"
    p = next(p for p in root.rglob(name) if p.name == name)
    data = p.read_bytes()
    name, start = parse(data)
    count = struct.unpack_from("<I", data, start)[0]
    body = start + 4
    print(f"{name}: count={count} body=0x{body:x} len={len(data)}")

    # hypothesis A: count vertices, 3f positions, 3f normals, 2f UVs
    pos_size = count * 12
    nrm_size = count * 12
    uv_size = count * 8
    if body + pos_size + nrm_size + uv_size <= len(data):
        pos = [struct.unpack_from("<3f", data, body + i * 12) for i in range(count)]
        nrm = [struct.unpack_from("<3f", data, body + pos_size + i * 12) for i in range(count)]
        uv = [struct.unpack_from("<2f", data, body + pos_size + nrm_size + i * 8) for i in range(count)]
        print("bbox:", tuple(round(min(v[i] for v in pos), 3) for i in range(3)),
              tuple(round(max(v[i] for v in pos), 3) for i in range(3)))
        lens = [sum(c * c for c in n) ** 0.5 for n in nrm]
        print("normal length min/avg/max:", round(min(lens), 3), round(sum(lens) / len(lens), 3), round(max(lens), 3))
        print("uv min/max:", tuple(round(min(v[i] for v in uv), 3) for i in range(2)),
              tuple(round(max(v[i] for v in uv), 3) for i in range(2)))
        rest = len(data) - (body + pos_size + nrm_size + uv_size)
        print(f"remaining bytes after pos/nrm/uv: {rest}")
        off = body + pos_size + nrm_size + uv_size
        print("next 32 bytes:", data[off : off + 32].hex(" "))
        if rest >= 4:
            print("u32 at rest:", struct.unpack_from("<I", data, off)[0])
    else:
        print("hypothesis A does not fit")

    # hypothesis B: positions interleaved with normals/uv per vertex, 32 bytes
    stride = 32
    if body + count * stride <= len(data):
        rows = [struct.unpack_from("<8f", data, body + i * stride) for i in range(count)]
        print("stride32 first 3:", rows[:3])

    # scan the second half for 16-bit indices excluding the zero padding
    end = len(data.rstrip(b"\x00"))
    print(f"data end (last nonzero) = {end} (0x{end:x})")
    for start_off in range(body, end - 2, 2):
        vals = struct.unpack_from("<%dH" % ((end - start_off) // 2), data, start_off)
        if len(vals) >= 30 and max(vals) < count and min(vals) == 0:
            # score: many distinct, sensible triangle indices
            if len(set(vals)) > count // 2:
                print(f"candidate index buffer at 0x{start_off:x}: {len(vals)} values, max={max(vals)}, distinct={len(set(vals))}")
                print("  first 30:", vals[:30])
                break


if __name__ == "__main__":
    main()
