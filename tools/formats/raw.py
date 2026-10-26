#!/usr/bin/env python3
"""Readers for the island ``.raw`` resources.

The ``.lnd`` header points at two raw files:

    heightmap   Data\\graphics\\TEXTURES\\landscape\\islands\\islNN\\heightmap_NN.raw
    occupation  Data\\design\\landscape\\islNN\\islNN_occupation.raw

* Heightmap: 512 x 512 bytes, one unsigned byte per sample. It is a square
  8-bit heightmap (the port maps it to a 1024-unit world).
* Occupation: 1024 x 1024 bytes, one byte per world unit. Values are only
  ``0`` (buildable land) and ``255`` (blocked: water, cliffs, dense cover).
  The black region in the mask matches the island land in the basemap.

The basemap is 1024 x 1024 TGA and the detail map 512 x 512 TGA; both live
beside the heightmap. The ``Data/Graphics`` copy of the occupation mask is
almost identical to the ``Data/Design`` copy (only 1 of 10 islands is
byte-identical; the rest differ by <1%); ``.lnd`` references the Design copy.

See ``docs/formats/raw.md``.
"""
from __future__ import annotations

import argparse
import json
import math
from dataclasses import dataclass
from pathlib import Path


class RawFormatError(ValueError):
    pass


@dataclass
class Heightmap:
    path: str
    width: int
    height: int
    data: bytes

    def at(self, x: int, y: int) -> int:
        return self.data[y * self.width + x]

    def summary(self) -> dict:
        values = list(self.data)
        return {
            "path": self.path,
            "width": self.width,
            "height": self.height,
            "min": min(values),
            "max": max(values),
            "mean": round(sum(values) / len(values), 3),
        }


@dataclass
class Occupation:
    path: str
    width: int
    height: int
    data: bytes

    def at(self, x: int, y: int) -> int:
        return self.data[y * self.width + x]

    def is_buildable(self, x: int, y: int) -> bool:
        return self.at(x, y) == 0

    @property
    def buildable(self) -> int:
        return self.data.count(0)

    def summary(self) -> dict:
        values = set(self.data)
        return {
            "path": self.path,
            "width": self.width,
            "height": self.height,
            "values": sorted(values),
            "buildable": self.data.count(0),
            "blocked": self.data.count(255),
        }


def _square_side(size: int, what: str) -> int:
    side = math.isqrt(size)
    if side * side != size:
        raise RawFormatError(f"{what}: {size} bytes is not a square 8-bit map")
    return side


def read_heightmap(path: Path) -> Heightmap:
    data = path.read_bytes()
    if not data:
        raise RawFormatError(f"{path}: empty")
    side = _square_side(len(data), str(path))
    return Heightmap(path=str(path), width=side, height=side, data=data)


def read_occupation(path: Path, expected: int = 1024) -> Occupation:
    data = path.read_bytes()
    if not data:
        raise RawFormatError(f"{path}: empty")
    side = _square_side(len(data), str(path))
    if side != expected:
        raise RawFormatError(f"{path}: side {side}, expected {expected}")
    if set(data) - {0, 255}:
        raise RawFormatError(f"{path}: non-binary mask values {sorted(set(data))}")
    return Occupation(path=str(path), width=side, height=side, data=data)


def scan(design_root: Path, graphics_root: Path) -> dict:
    islands = []
    for index in range(1, 11):
        hm_path = (
            graphics_root
            / f"isl{index:02d}"
            / f"heightmap_{index:02d}.raw"
        )
        occ_path = (
            design_root / f"isl{index:02d}" / f"isl{index:02d}_occupation.raw"
        )
        if not hm_path.exists() or not occ_path.exists():
            continue
        heightmap = read_heightmap(hm_path)
        occupation = read_occupation(occ_path)
        # Sample the heightmap at each occupation cell's half-resolution point.
        zero = []
        blocked = []
        step = occupation.width // heightmap.width
        for y in range(0, occupation.height, step):
            for x in range(0, occupation.width, step):
                value = occupation.at(x, y)
                sample = heightmap.at(x // step, y // step)
                (zero if value == 0 else blocked).append(sample)
        islands.append(
            {
                "island": f"isl{index:02d}",
                "heightmap": heightmap.summary(),
                "occupation": occupation.summary(),
                "buildable_mean_height": round(sum(zero) / len(zero), 3),
                "blocked_mean_height": round(sum(blocked) / len(blocked), 3),
                "buildable_water_fraction": round(zero.count(0) / len(zero), 4),
            }
        )
    return {"islands": islands}


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--heightmap", type=Path)
    ap.add_argument("--occupation", type=Path)
    ap.add_argument("--scan", type=Path)
    args = ap.parse_args(argv)
    if args.heightmap:
        print(json.dumps(read_heightmap(args.heightmap).summary(), indent=2))
        return 0
    if args.occupation:
        print(json.dumps(read_occupation(args.occupation).summary(), indent=2))
        return 0
    if args.scan:
        design = args.scan / "Data" / "Design" / "landscape"
        graphics = args.scan / "Data" / "Graphics" / "Textures" / "landscape" / "islands"
        print(json.dumps(scan(design, graphics), indent=2))
        return 0
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
