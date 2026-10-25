#!/usr/bin/env python3
"""Deeper look at one mesh: words, padding, candidate index buffers."""
from __future__ import annotations

import struct
import sys
from pathlib import Path

MAGIC = b"\x01\x03\x00\x00\x00"


def parse(data: bytes):
    name_len = struct.unpack_from("<I", data, 17)[0]
    name = data[21 : 21 + name_len].rstrip(b"\0").decode("ascii", "replace")
    return name, 21 + name_len


def dump_words(data: bytes, start: int, n: int = 96):
    for i in range(0, n, 8):
        off = start + i * 4
        if off + 32 > len(data):
            break
        words = struct.unpack_from("<8I", data, off)
        floats = struct.unpack_from("<8f", data, off)
        print(f"  {off:5d} 0x{off:04x}  " + " ".join(f"{w:10d}" for w in words))
        print(f"              " + " ".join(f"{f:10.4f}" for f in floats))


def main():
    root = Path(sys.argv[1] if len(sys.argv) > 1 else "game/%MAINDIR%")
    name = sys.argv[2] if len(sys.argv) > 2 else "cow_lod1.mesh"
    p = next(p for p in root.rglob(name) if p.name == name)
    data = p.read_bytes()
    name, start = parse(data)
    count = struct.unpack_from("<I", data, start)[0]
    body = start + 4
    print(f"{p}\nsize={len(data)} name={name} count={count} body=0x{body:x} body_len={len(data)-body}")
    print(f"size mod 16={len(data)%16} mod 64={len(data)%64}")
    end = len(data.rstrip(b"\x00"))
    print(f"last nonzero={end} trailing_zeros={len(data)-end} data_len={end-body}")

    print("\nfirst 128 body words:")
    dump_words(data, body, 128)

    print("\ntail 128 bytes:")
    t = data[-128:]
    for i in range(0, len(t), 16):
        print(f"  {len(data)-128+i:5d}  " + " ".join(f"{b:02x}" for b in t[i:i+16]))

    # candidate u16 index buffers: longest run of values < count in second half
    print("\ncandidate u16 index runs (values < count) in second half:")
    half = body + (len(data) - body) // 2
    run = 0
    best = (0, 0)
    for i in range(half, len(data) - 1, 2):
        v = struct.unpack_from("<H", data, i)[0]
        if v < count:
            run += 1
            if run > best[0]:
                best = (run, i - 2 * (run - 1))
        else:
            run = 0
    print(f"  longest run: {best[0]} values at 0x{best[1]:x}" if best[0] else "  none")

    # u32 values equal to count or multiples, useful for locating sub-chunks
    print("\nu32 occurrences of count-ish values:")
    targets = {count, count * 2, count * 3, count * 4, count // 3}
    for i in range(body, len(data) - 3):
        v = struct.unpack_from("<I", data, i)[0]
        if v in targets:
            print(f"  0x{i:x}: {v}")


if __name__ == "__main__":
    main()
