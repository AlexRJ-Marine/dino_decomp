#!/usr/bin/env python3
"""Dump a byte range of a file as hex, u16, u32, or f32."""
from __future__ import annotations

import argparse
import struct
from pathlib import Path


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("file", type=Path)
    ap.add_argument("offset", type=lambda s: int(s, 0))
    ap.add_argument("length", type=lambda s: int(s, 0), nargs="?", default=128)
    ap.add_argument("--as", dest="fmt", choices=["hex", "u16", "u32", "f32"], default="hex")
    args = ap.parse_args()

    data = args.file.read_bytes()
    chunk = data[args.offset : args.offset + args.length]
    print(f"{args.file}  offset=0x{args.offset:x}  length={len(chunk)}")

    if args.fmt == "hex":
        for i in range(0, len(chunk), 16):
            b = chunk[i : i + 16]
            print(f"  {args.offset+i:06x}  " + " ".join(f"{x:02x}" for x in b) + "  " +
                  "".join(chr(x) if 32 <= x < 127 else "." for x in b))
    elif args.fmt == "u16":
        vals = struct.unpack_from("<%dH" % (len(chunk) // 2), chunk)
        for i in range(0, len(vals), 12):
            print(f"  {args.offset+i*2:06x}  " + " ".join(f"{v:6d}" for v in vals[i:i+12]))
    elif args.fmt == "u32":
        vals = struct.unpack_from("<%dI" % (len(chunk) // 4), chunk)
        for i in range(0, len(vals), 8):
            print(f"  {args.offset+i*4:06x}  " + " ".join(f"{v:10d}" for v in vals[i:i+8]))
    else:
        vals = struct.unpack_from("<%df" % (len(chunk) // 4), chunk)
        for i in range(0, len(vals), 6):
            print(f"  {args.offset+i*4:06x}  " + " ".join(f"{v:12.5f}" for v in vals[i:i+6]))


if __name__ == "__main__":
    main()
