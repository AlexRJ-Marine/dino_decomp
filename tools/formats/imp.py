#!/usr/bin/env python3
"""Reader for Dino Island ``.imp`` landscape-import placement files.

Layout (all 10 retail islands, 78 files, 16,558 instances)::

    u32  path_len
    n    model path, NUL-terminated (e.g. "DATA\\Design\\Landscape\\Objects\\palm1.blf")
    u32  instance_count
    count x 16 bytes:
        f32 x       world X, 0 .. 1024
        f32 y       world Y, 0 .. 1024
        f32 scale   uniform scale (palms 0.5 .. 2.0, ferns 0.8 .. 1.2)
        f32 angle   Z rotation in radians, 0 .. 2*pi

The 0..1024 coordinate range is the same on every island, which is why the
terrain grid is a 1024-unit square regardless of the ``.lnd`` width/height
fields.
"""
from __future__ import annotations

import argparse
import json
import math
import struct
from dataclasses import dataclass, field
from pathlib import Path

WORLD_SIZE = 1024.0


class ImpFormatError(ValueError):
    pass


@dataclass
class ImpInstance:
    x: float
    y: float
    scale: float
    angle: float

    def to_dict(self) -> dict:
        return {"x": self.x, "y": self.y, "scale": self.scale, "angle": self.angle}


@dataclass
class Imp:
    path: str
    model: str
    instances: list[ImpInstance] = field(default_factory=list)

    def summary(self) -> dict:
        xs = [i.x for i in self.instances] or [0.0]
        ys = [i.y for i in self.instances] or [0.0]
        scales = [i.scale for i in self.instances] or [0.0]
        angles = [i.angle for i in self.instances] or [0.0]
        return {
            "path": self.path,
            "model": self.model,
            "instances": len(self.instances),
            "bounds": {
                "x": [min(xs), max(xs)],
                "y": [min(ys), max(ys)],
                "scale": [min(scales), max(scales)],
                "angle": [min(angles), max(angles)],
            },
            "first": self.instances[0].to_dict() if self.instances else None,
        }


def read_imp(path: Path) -> Imp:
    data = path.read_bytes()
    if len(data) < 8:
        raise ImpFormatError(f"{path}: shorter than header")
    name_len = struct.unpack_from("<I", data, 0)[0]
    if not 1 <= name_len <= 4096 or 4 + name_len + 4 > len(data):
        raise ImpFormatError(f"{path}: bad path length {name_len}")
    raw = data[4 : 4 + name_len]
    if raw[-1] != 0:
        raise ImpFormatError(f"{path}: model path is not NUL-terminated")
    model = raw[:-1].decode("latin-1")
    off = 4 + name_len
    count = struct.unpack_from("<I", data, off)[0]
    off += 4
    if count > 1_000_000:
        raise ImpFormatError(f"{path}: implausible instance count {count}")
    if off + count * 16 != len(data):
        raise ImpFormatError(
            f"{path}: {count} instances need {count * 16} bytes, have {len(data) - off}"
        )

    instances = []
    for i in range(count):
        x, y, scale, angle = struct.unpack_from("<4f", data, off + i * 16)
        if not (0.0 <= x <= WORLD_SIZE and 0.0 <= y <= WORLD_SIZE):
            raise ImpFormatError(f"{path}: instance {i} outside world: {x}, {y}")
        if not (0.01 <= scale <= 100.0):
            raise ImpFormatError(f"{path}: instance {i} bad scale {scale}")
        if not (0.0 <= angle <= 2.0 * math.pi + 1e-3):
            raise ImpFormatError(f"{path}: instance {i} bad angle {angle}")
        instances.append(ImpInstance(x=x, y=y, scale=scale, angle=angle))
    return Imp(path=str(path), model=model, instances=instances)


def scan(root: Path) -> tuple[int, list[tuple[str, str]], dict]:
    total = 0
    bad: list[tuple[str, str]] = []
    instances = 0
    models: set[str] = set()
    for p in sorted(root.rglob("*.imp")):
        total += 1
        try:
            imp = read_imp(p)
        except Exception as e:
            bad.append((str(p), str(e)))
            continue
        instances += len(imp.instances)
        models.add(imp.model)
    return total, bad, {"instances": instances, "models": len(models)}


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--dump", type=Path)
    g.add_argument("--scan", type=Path)
    args = ap.parse_args(argv)
    if args.dump:
        print(json.dumps(read_imp(args.dump).summary(), indent=2))
        return 0
    total, bad, sums = scan(args.scan)
    print(f"scanned {total} .imp files, {len(bad)} bad, "
          f"{sums['instances']:,} instances, {sums['models']} models")
    for path, why in bad[:20]:
        print(f"  FAIL {path}: {why}")
    return 0 if not bad else 1


if __name__ == "__main__":
    raise SystemExit(main())
