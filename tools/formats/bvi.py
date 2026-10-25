#!/usr/bin/env python3
"""Reader for ``.bvi`` visitor profiles.

Each file starts with a 96-byte header, then a table of length-prefixed ASCII
``DATA\\...`` strings (26 mesh paths + texture directory + texture file),
then a small undecoded tail (91–113 bytes).

Header (24 x 32-bit, mixed int/float):

    0  u32  personality id (0 Surfer, 1 Hooligan, 2 Parent, 3 Blase,
             4 Old, 5 Riche, 6 Scientist, 7 Fan, 8 Spd, 9 Kid)
    1  u32  1
    2  u32  1
    3  f32  height (1.7 m)
    4  f32  walk speed (1.2 .. 2.5)
    5  u32  10
    6  u32  2
    7..11 f32  five preference weights (-1 .. 1)
    12 u32  576
    13 f32  unknown per-profile scale (1.0 .. 2.5)
    14..20 f32 seven constant tuning weights, identical in all 24 files
              (1, 1, 1, 500, 300, -3000, 3000); the first six are the global
              attraction-scoring vector.
    21..23    header tail (0, 0, 2)

See ``docs/formats/bvi.md``.
"""
from __future__ import annotations

import argparse
import json
import struct
from dataclasses import dataclass, field
from pathlib import Path

HEADER_SIZE = 96
PERSONALITIES = {
    0: "Surfer",
    1: "Hooligan",
    2: "Parent",
    3: "Blase",
    4: "Old",
    5: "Riche",
    6: "Scientist",
    7: "Fan",
    8: "Spd",
    9: "Kid",
}


class BviFormatError(ValueError):
    pass


@dataclass
class VisitorProfile:
    path: str
    personality_id: int
    height: float
    walk_speed: float
    preferences: tuple[float, ...]
    header_tail: tuple[int, ...]
    unknown_0x34: float = 0.0
    scoring_weights: tuple[float, ...] = ()
    strings: list[str] = field(default_factory=list)
    tail: bytes = b""

    @property
    def personality(self) -> str:
        return PERSONALITIES.get(self.personality_id, f"unknown({self.personality_id})")

    @property
    def meshes(self) -> list[str]:
        return [s for s in self.strings if s.lower().endswith(".mesh")]

    @property
    def texture_dir(self) -> str:
        for s in self.strings:
            if not s.lower().endswith((".mesh", ".tga")):
                return s
        return ""

    @property
    def texture(self) -> str:
        for s in self.strings:
            if s.lower().endswith(".tga"):
                return s
        return ""

    def summary(self) -> dict:
        return {
            "path": self.path,
            "personality": self.personality,
            "personality_id": self.personality_id,
            "height": self.height,
            "walk_speed": self.walk_speed,
            "preferences": list(self.preferences),
            "unknown_0x34": self.unknown_0x34,
            "scoring_weights": list(self.scoring_weights),
            "meshes": len(self.meshes),
            "texture": Path(self.texture).name,
            "tail_bytes": len(self.tail),
        }


def read_bvi(path: Path) -> VisitorProfile:
    data = path.read_bytes()
    if len(data) < HEADER_SIZE:
        raise BviFormatError(f"{path}: shorter than header")
    words = struct.unpack_from("<24I", data, 0)
    floats = struct.unpack_from("<24f", data, 0)
    off = HEADER_SIZE
    strings: list[str] = []
    while off + 4 <= len(data):
        length = struct.unpack_from("<I", data, off)[0]
        if not 8 <= length <= 300 or off + 4 + length > len(data):
            break
        raw = data[off + 4:off + 4 + length]
        if not raw.startswith(b"DATA\\") or not raw.endswith(b"\x00"):
            break
        if not all(32 <= b < 127 or b == 0 for b in raw):
            break
        strings.append(raw.rstrip(b"\x00").decode("ascii"))
        off += 4 + length
    if not strings:
        raise BviFormatError(f"{path}: no string table")
    return VisitorProfile(
        path=str(path),
        personality_id=words[0],
        height=floats[3],
        walk_speed=floats[4],
        preferences=tuple(round(floats[i], 4) for i in range(7, 12)),
        header_tail=tuple(words[12:24]),
        unknown_0x34=round(floats[13], 4),
        scoring_weights=tuple(round(floats[i], 4) for i in range(14, 21)),
        strings=strings,
        tail=data[off:],
    )


def scan(root: Path) -> tuple[int, list[tuple[str, str]], dict]:
    total = 0
    bad: list[tuple[str, str]] = []
    sums = {"meshes": 0}
    for path in sorted(root.rglob("*.bvi")):
        total += 1
        try:
            profile = read_bvi(path)
        except Exception as e:
            bad.append((str(path), str(e)))
            continue
        sums["meshes"] += len(profile.meshes)
    return total, bad, sums


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--dump", type=Path)
    ap.add_argument("--scan", type=Path)
    args = ap.parse_args(argv)
    if args.dump:
        print(json.dumps(read_bvi(args.dump).summary(), indent=2))
        return 0
    if args.scan:
        total, bad, sums = scan(args.scan)
        print(f"scanned {total} visitor profiles, {len(bad)} bad, "
              f"{sums['meshes']} mesh paths")
        for path, why in bad[:20]:
            print(f"  FAIL {path}: {why}")
        return 0 if not bad else 1
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
