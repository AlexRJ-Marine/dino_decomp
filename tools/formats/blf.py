#!/usr/bin/env python3
"""Reader for Dino Island ``.blf`` landscape-object descriptors.

24/24 retail files share one layout::

    u32  version          2
    u32  lod_count        2
    u32  unknown          0
    u32  texture_count    2 (base + "_lod2" texture)
    lod_count x (u32 len + NUL-terminated .mesh path)
    1 x (u32 len + NUL-terminated texture directory)

Example ``palm1.blf``::

    DATA\\graphics\\landscape\\trees\\palmtrees\\palm1_lod1.mesh
    DATA\\graphics\\landscape\\trees\\palmtrees\\palm1_lod2.mesh
    DATA\\graphics\\TEXTURES\\buildings

The mesh's own material names the texture inside that directory
(``palm1.tga``), so a renderer can resolve ``dir + "\\" + material``.
"""
from __future__ import annotations

import argparse
import json
import struct
from dataclasses import dataclass, field
from pathlib import Path

HEADER = struct.Struct("<4I")


class BlfFormatError(ValueError):
    pass


@dataclass
class Blf:
    path: str
    version: int
    lod_count: int
    unknown: int
    texture_count: int
    lods: list[str] = field(default_factory=list)
    texture_dir: str = ""

    def summary(self) -> dict:
        return {
            "path": self.path,
            "version": self.version,
            "lod_count": self.lod_count,
            "unknown": self.unknown,
            "texture_count": self.texture_count,
            "lods": self.lods,
            "texture_dir": self.texture_dir,
        }


def _string(data: bytes, off: int, what: str) -> tuple[str, int]:
    if off + 4 > len(data):
        raise BlfFormatError(f"{what}: missing length at 0x{off:x}")
    n = struct.unpack_from("<I", data, off)[0]
    off += 4
    if not 1 <= n <= 4096 or off + n > len(data):
        raise BlfFormatError(f"{what}: bad length {n} at 0x{off - 4:x}")
    raw = data[off : off + n]
    if raw[-1] != 0:
        raise BlfFormatError(f"{what}: not NUL-terminated")
    return raw[:-1].decode("latin-1"), off + n


def read_blf(path: Path) -> Blf:
    data = path.read_bytes()
    if len(data) < HEADER.size:
        raise BlfFormatError(f"{path}: shorter than header")
    version, lod_count, unknown, texture_count = HEADER.unpack_from(data, 0)
    if version != 2:
        raise BlfFormatError(f"{path}: unsupported version {version}")
    if not 1 <= lod_count <= 16:
        raise BlfFormatError(f"{path}: bad LOD count {lod_count}")
    off = HEADER.size
    lods = []
    for i in range(lod_count):
        s, off = _string(data, off, f"lod[{i}]")
        lods.append(s)
    texture_dir, off = _string(data, off, "texture dir")
    if off != len(data):
        raise BlfFormatError(f"{path}: {len(data) - off} trailing bytes")
    return Blf(
        path=str(path),
        version=version,
        lod_count=lod_count,
        unknown=unknown,
        texture_count=texture_count,
        lods=lods,
        texture_dir=texture_dir,
    )


def scan(root: Path) -> tuple[int, list[tuple[str, str]], dict]:
    total = 0
    bad: list[tuple[str, str]] = []
    lods = 0
    for p in sorted(root.rglob("*.blf")):
        total += 1
        try:
            blf = read_blf(p)
        except Exception as e:
            bad.append((str(p), str(e)))
            continue
        lods += len(blf.lods)
    return total, bad, {"lods": lods}


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--dump", type=Path)
    g.add_argument("--scan", type=Path)
    args = ap.parse_args(argv)
    if args.dump:
        print(json.dumps(read_blf(args.dump).summary(), indent=2))
        return 0
    total, bad, sums = scan(args.scan)
    print(f"scanned {total} .blf files, {len(bad)} bad, {sums['lods']} LOD meshes")
    for path, why in bad[:20]:
        print(f"  FAIL {path}: {why}")
    return 0 if not bad else 1


if __name__ == "__main__":
    raise SystemExit(main())
