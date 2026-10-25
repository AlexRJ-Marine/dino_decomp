#!/usr/bin/env python3
"""Reader for Dino Island .skel files.

Layout (validated on the 20 dinosaur skeletons and the smaller prop skeletons):

    0x00  5   magic          01 03 00 00 00
    0x05  4   hash
    0x09  4   sentinel       0x56789abc
    0x0d  4   bone_count
    0x11  4   global_scale   f32 (0.01 on dinosaurs)
    0x15  4   unknown        (0)
    0x19  76*bone_count      bone records, 19 x f32 each
    ...       names          bone_count x (u32 length + NUL-terminated string)
    ...       4 + 4*bone_count  u32 count + one u32 flag per bone (all 1)

The 19-float record is a transform of some kind (rotation/translation), but the
exact packing and the parent hierarchy are still open. The raw values are kept.
"""
from __future__ import annotations

import argparse
import json
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path

MAGIC = b"\x01\x03\x00\x00\x00"
SENTINEL = 0x56789ABC
RECORD_FLOATS = 19
RECORD_BYTES = RECORD_FLOATS * 4


class SkelFormatError(ValueError):
    pass


@dataclass
class SkelBone:
    index: int
    name: str
    values: tuple[float, ...]
    flag: int = 0

    def to_dict(self) -> dict:
        return {
            "index": self.index,
            "name": self.name,
            "values": [round(v, 6) for v in self.values],
            "flag": self.flag,
        }


@dataclass
class Skeleton:
    path: str
    hash: int
    bone_count: int
    global_scale: float
    unknown: int
    bones: list[SkelBone] = field(default_factory=list)
    trailing: bytes = b""

    def summary(self) -> dict:
        return {
            "path": self.path,
            "hash": f"{self.hash:#010x}",
            "bone_count": self.bone_count,
            "global_scale": self.global_scale,
            "unknown": self.unknown,
            "bones": [b.to_dict() for b in self.bones],
            "trailing_bytes": len(self.trailing),
        }


def read_skel(path: Path) -> Skeleton:
    data = path.read_bytes()
    if data[:5] != MAGIC:
        raise SkelFormatError(f"bad magic at 0x00: {data[:5].hex(' ')}")
    hash_, sentinel, count = struct.unpack_from("<3I", data, 5)
    if sentinel != SENTINEL:
        raise SkelFormatError(f"bad sentinel at 0x09: {sentinel:#010x}")
    if not 1 <= count <= 10_000:
        raise SkelFormatError(f"implausible bone_count {count}")

    global_scale, unknown = struct.unpack_from("<fI", data, 0x11)
    rec_start = 0x19
    rec_end = rec_start + count * RECORD_BYTES
    if rec_end > len(data):
        raise SkelFormatError(f"bone records run past EOF at 0x{rec_end:x}")

    raw_values = [
        struct.unpack_from("<19f", data, rec_start + i * RECORD_BYTES) for i in range(count)
    ]

    off = rec_end
    names: list[str] = []
    for i in range(count):
        if off + 4 > len(data):
            raise SkelFormatError(f"name[{i}] length past EOF")
        ln = struct.unpack_from("<I", data, off)[0]
        off += 4
        if not 1 <= ln <= 256 or off + ln > len(data):
            raise SkelFormatError(f"bad name[{i}] length {ln} at 0x{off-4:x}")
        raw = data[off : off + ln]
        if raw[-1] != 0:
            raise SkelFormatError(f"name[{i}] is not NUL-terminated")
        names.append(raw[:-1].decode("latin-1"))
        off += ln

    if off + 4 > len(data):
        raise SkelFormatError("missing flag count")
    flag_count = struct.unpack_from("<I", data, off)[0]
    off += 4
    if flag_count != count:
        raise SkelFormatError(f"flag count {flag_count} != bone count {count}")
    if off + count * 4 > len(data):
        raise SkelFormatError("flag table runs past EOF")
    flags = struct.unpack_from("<%dI" % count, data, off)
    off += count * 4

    bones = [
        SkelBone(index=i, name=names[i], values=raw_values[i], flag=flags[i])
        for i in range(count)
    ]
    return Skeleton(
        path=str(path),
        hash=hash_,
        bone_count=count,
        global_scale=global_scale,
        unknown=unknown,
        bones=bones,
        trailing=data[off:],
    )


def scan(root: Path) -> tuple[int, list[tuple[str, str]], dict]:
    total = 0
    bad: list[tuple[str, str]] = []
    bones = 0
    for p in sorted(root.rglob("*.skel")):
        total += 1
        try:
            s = read_skel(p)
        except Exception as e:
            bad.append((str(p), str(e)))
            continue
        bones += s.bone_count
    return total, bad, {"bones": bones}


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--dump", type=Path)
    g.add_argument("--scan", type=Path)
    args = ap.parse_args(argv)

    if args.dump:
        print(json.dumps(read_skel(args.dump).summary(), indent=2))
        return 0

    total, bad, sums = scan(args.scan)
    print(f"scanned {total} .skel files, {len(bad)} bad, {sums['bones']:,} bones total")
    for path, why in bad[:20]:
        print(f"  FAIL {path}: {why}")
    return 0 if not bad else 1


if __name__ == "__main__":
    raise SystemExit(main())
