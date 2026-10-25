#!/usr/bin/env python3
"""Reader for Dino Island ``.lnd`` landscape descriptors.

Layout (10/10 retail islands)::

    u32 len + NUL-terminated string   x 8
        0  name key (e.g. "&Isl01_Name")
        1  basemap TGA
        2  detailmap TGA
        3  heightmap RAW
        4  skybox prefix
        5  water TGA
        6  second water TGA
        7  occupation RAW
    f32   unknown (16.0 .. 26.0; likely water level or detail scale)
    u32   width   (heightmap/basemap width)
    u32   height
    u32   unknown (0, 1 or 2; likely biome/theme id)
    u32 len + NUL-terminated music name
    25 x (u32 len + NUL-terminated vegetation .imp path or "None")
"""
from __future__ import annotations

import argparse
import json
import struct
from dataclasses import dataclass, field
from pathlib import Path


class LndFormatError(ValueError):
    pass


@dataclass
class Lnd:
    path: str
    name_key: str
    basemap: str
    detailmap: str
    heightmap: str
    skybox: str
    water: str
    water2: str
    occupation: str
    unknown_float: float
    width: int
    height: int
    unknown_int: int
    music: str
    vegetation: list[str] = field(default_factory=list)

    def summary(self) -> dict:
        return {
            "path": self.path,
            "name_key": self.name_key,
            "basemap": self.basemap,
            "detailmap": self.detailmap,
            "heightmap": self.heightmap,
            "skybox": self.skybox,
            "water": self.water,
            "water2": self.water2,
            "occupation": self.occupation,
            "unknown_float": self.unknown_float,
            "width": self.width,
            "height": self.height,
            "unknown_int": self.unknown_int,
            "music": self.music,
            "vegetation_count": len(self.vegetation),
            "vegetation": self.vegetation,
        }


def _read_string(data: bytes, off: int, what: str) -> tuple[str, int]:
    if off + 4 > len(data):
        raise LndFormatError(f"{what}: missing length at 0x{off:x}")
    n = struct.unpack_from("<I", data, off)[0]
    off += 4
    if not 1 <= n <= 4096 or off + n > len(data):
        raise LndFormatError(f"{what}: bad length {n} at 0x{off-4:x}")
    raw = data[off : off + n]
    if raw[-1] != 0:
        raise LndFormatError(f"{what}: string is not NUL-terminated")
    return raw[:-1].decode("latin-1"), off + n


def read_lnd(path: Path) -> Lnd:
    data = path.read_bytes()
    off = 0
    strings = []
    for i in range(8):
        s, off = _read_string(data, off, f"string[{i}]")
        strings.append(s)
    if off + 16 > len(data):
        raise LndFormatError("missing landscape header")
    unknown_float, width, height, unknown_int = struct.unpack_from("<f3I", data, off)
    off += 16
    music, off = _read_string(data, off, "music")

    vegetation: list[str] = []
    while off < len(data):
        s, off = _read_string(data, off, f"vegetation[{len(vegetation)}]")
        vegetation.append(s)
    if not vegetation:
        raise LndFormatError("missing vegetation list")

    return Lnd(
        path=str(path),
        name_key=strings[0],
        basemap=strings[1],
        detailmap=strings[2],
        heightmap=strings[3],
        skybox=strings[4],
        water=strings[5],
        water2=strings[6],
        occupation=strings[7],
        unknown_float=unknown_float,
        width=width,
        height=height,
        unknown_int=unknown_int,
        music=music,
        vegetation=vegetation,
    )


def scan(root: Path) -> tuple[int, list[tuple[str, str]], dict]:
    total = 0
    bad: list[tuple[str, str]] = []
    vegetation = 0
    for p in sorted(root.rglob("*.lnd")):
        total += 1
        try:
            lnd = read_lnd(p)
        except Exception as e:
            bad.append((str(p), str(e)))
            continue
        vegetation += len(lnd.vegetation)
    return total, bad, {"vegetation": vegetation}


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--dump", type=Path)
    g.add_argument("--scan", type=Path)
    args = ap.parse_args(argv)
    if args.dump:
        print(json.dumps(read_lnd(args.dump).summary(), indent=2))
        return 0
    total, bad, sums = scan(args.scan)
    print(f"scanned {total} .lnd files, {len(bad)} bad, "
          f"{sums['vegetation']} vegetation slots")
    for path, why in bad[:20]:
        print(f"  FAIL {path}: {why}")
    return 0 if not bad else 1


if __name__ == "__main__":
    raise SystemExit(main())
