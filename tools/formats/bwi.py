#!/usr/bin/env python3
"""Reader for the wall definitions ``Data/Design/attractions/walls/*.bwi``.

The 24 files are a packed little-endian stream (``WallList.ini`` order):

```text
0x00  u32   version            4
0x04  f32   height             2 / 4 / 6 / 8
0x08  u32   100                resistance / hit points?
0x0c  u32   build cost?        10 wood, 15 others, 30 plates
0x10  u32   30                 visibility?
0x14  u8    electrified        1 for the four *Elect walls
0x15  f32 x 7                  texture/UV scales (unaligned!)
0x31  string x 7               4 textures, 2 meshes, texture dir
      u32   4                  constant
      string                    material build-panel widget
      string                    height build-panel widget
```

Each string is a ``u32`` byte length (including the NUL) followed by that many
bytes, so the whole stream is unaligned after 0x14.

See ``docs/formats/bwi.md``.
"""
from __future__ import annotations

import argparse
import json
import struct
from dataclasses import dataclass, field
from pathlib import Path


class BwiFormatError(ValueError):
    pass


@dataclass
class Bwi:
    path: str
    name: str
    version: int
    height: float
    field_08: int
    field_0c: int
    field_10: int
    electrified: bool
    scales: list[float] = field(default_factory=list)
    textures: list[str] = field(default_factory=list)
    meshes: list[str] = field(default_factory=list)
    texture_dir: str = ""
    field_tail: int = 0
    switch_material: str = ""
    switch_height: str = ""

    def summary(self) -> dict:
        return {
            "path": self.path,
            "name": self.name,
            "version": self.version,
            "height": self.height,
            "field_08": self.field_08,
            "field_0c": self.field_0c,
            "field_10": self.field_10,
            "electrified": self.electrified,
            "scales": self.scales,
            "textures": self.textures,
            "meshes": self.meshes,
            "texture_dir": self.texture_dir,
            "field_tail": self.field_tail,
            "switch_material": self.switch_material,
            "switch_height": self.switch_height,
        }


def _read_string(data: bytes, off: int, what: str) -> tuple[str, int]:
    if off + 4 > len(data):
        raise BwiFormatError(f"{what}: truncated length at 0x{off:x}")
    size = struct.unpack_from("<I", data, off)[0]
    if size == 0 or size > 256 or off + 4 + size > len(data):
        raise BwiFormatError(f"{what}: bad string length {size} at 0x{off:x}")
    raw = data[off + 4: off + 4 + size]
    if raw[-1] != 0:
        raise BwiFormatError(f"{what}: string not NUL-terminated at 0x{off:x}")
    return raw[:-1].decode("latin-1"), off + 4 + size


def read_bwi(path: Path) -> Bwi:
    data = path.read_bytes()
    if len(data) < 0x40:
        raise BwiFormatError(f"{path}: too short")
    version, height, f08, f0c, f10 = struct.unpack_from("<IfIII", data, 0)
    if version != 4:
        raise BwiFormatError(f"{path}: version {version}")
    electrified = data[0x14] != 0
    scales = [struct.unpack_from("<f", data, 0x15 + 4 * i)[0] for i in range(7)]
    off = 0x31
    strings = []
    for index in range(7):
        text, off = _read_string(data, off, f"{path}: string {index}")
        strings.append(text)
    tail = struct.unpack_from("<I", data, off)[0]
    off += 4
    material, off = _read_string(data, off, f"{path}: material widget")
    height_widget, off = _read_string(data, off, f"{path}: height widget")
    if off != len(data):
        raise BwiFormatError(f"{path}: {len(data) - off} trailing bytes")
    return Bwi(
        path=str(path),
        name=path.stem,
        version=version,
        height=height,
        field_08=f08,
        field_0c=f0c,
        field_10=f10,
        electrified=electrified,
        scales=scales,
        textures=strings[:4],
        meshes=strings[4:6],
        texture_dir=strings[6],
        field_tail=tail,
        switch_material=material,
        switch_height=height_widget,
    )


def scan(directory: Path) -> list[Bwi]:
    return [read_bwi(path) for path in sorted(directory.glob("*.bwi"))]


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("path", type=Path, help="a .bwi file or the walls directory")
    args = ap.parse_args(argv)
    if args.path.is_dir():
        print(json.dumps([wall.summary() for wall in scan(args.path)], indent=2))
    else:
        print(json.dumps(read_bwi(args.path).summary(), indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
