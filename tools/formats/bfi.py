#!/usr/bin/env python3
"""Reader for the show-feature definitions ``Data/Design/attractions/features/*.bfi``.

The 62 files are a packed stream. The part that is decoded:

```text
0x00  u32  footprint width     4 for most; cages 8/12/16; car 2, bus 4
0x04  u32  footprint height    4 for most; beam 4x1, bus 4x8, car 2x4
0x08  f32  height              4.0 default; CageHuge 8.0; JudgeRace 12.0
0x0c  u32  variant             0 default; CageSmall..Huge 0..3
0x10  u8   flag                1 for the six odd-footprint obstacles
0x11  u32  100                 100 for cages/tiles, 200..500 for obstacles
0x15  u32  10                  build time / weight?
0x19  u32  50..1200           Cage 1200, Tile 300, AnimalBait/Judge 50,
                               Waypoint 10, ObstacleCar 800
0x1d  u32  flag                0..4
0x21  u32  flag                3 default, 0 for obstacles
0x25  u32  0 or 100
0x29  u32  0..2
0x2d  string                   class tag: Cage, Judge, AnimalBait, StaticBait,
                               Waypoint, Teleport, LunchBox, Tile, or empty
...   fields + strings        the nested record, not fully decoded
```

After the class tag the record interleaves integers and length-prefixed
strings. The useful strings are the action/animation names, the LOD mesh
paths, texture paths, explosion ``.ini`` names, billboard directory, sound
names and the construction-panel switch key (``SWITCH_*``). They are
extracted by scanning for length-prefixed printable strings.

See ``docs/formats/bfi.md``.
"""
from __future__ import annotations

import argparse
import json
import re
import struct
from dataclasses import dataclass, field
from pathlib import Path

CLASS_TAGS = (
    "Cage",
    "Judge",
    "AnimalBait",
    "StaticBait",
    "Waypoint",
    "Teleport",
    "LunchBox",
    "Tile",
)

_ALLOWED = re.compile(r"^[A-Za-z0-9_\\/.\- :]+$")


class BfiFormatError(ValueError):
    pass


@dataclass
class Bfi:
    path: str
    name: str
    width: int
    height: int
    vertical: float
    variant: int
    flag: int
    field_11: int
    field_15: int
    field_19: int
    field_1d: int
    field_21: int
    field_25: int
    field_29: int
    class_tag: str
    strings: list[tuple[int, str]] = field(default_factory=list)

    def _match(self, suffix: str) -> list[str]:
        return [text for _, text in self.strings if text.lower().endswith(suffix)]

    @property
    def meshes(self) -> list[str]:
        return self._match(".mesh")

    @property
    def textures(self) -> list[str]:
        return self._match(".tga")

    @property
    def switch_key(self) -> str:
        keys = [text for _, text in self.strings if text.startswith("SWITCH_")]
        return keys[-1] if keys else ""

    @property
    def identifiers(self) -> list[str]:
        out = []
        for _, text in self.strings:
            if text == self.class_tag:
                continue
            if any(text.lower().endswith(ext) for ext in (".mesh", ".tga", ".ini")):
                continue
            if "\\" in text or text.startswith("SWITCH_"):
                continue
            out.append(text)
        return out

    def summary(self) -> dict:
        return {
            "path": self.path,
            "name": self.name,
            "width": self.width,
            "height": self.height,
            "vertical": self.vertical,
            "variant": self.variant,
            "flag": self.flag,
            "field_11": self.field_11,
            "field_15": self.field_15,
            "field_19": self.field_19,
            "field_1d": self.field_1d,
            "field_21": self.field_21,
            "field_25": self.field_25,
            "field_29": self.field_29,
            "class_tag": self.class_tag,
            "meshes": self.meshes,
            "textures": self.textures,
            "switch_key": self.switch_key,
            "identifiers": self.identifiers,
            "strings": [text for _, text in self.strings],
        }


def _read_tag(data: bytes) -> tuple[str, int]:
    size = struct.unpack_from("<I", data, 0x2D)[0]
    raw = data[0x31: 0x31 + size]
    if not raw or raw[-1] != 0 or len(raw) != size:
        raise BfiFormatError("class tag is not a valid length-prefixed string")
    return raw[:-1].decode("latin-1"), 0x2D + 4 + size


def scan_strings(data: bytes, start: int) -> list[tuple[int, str]]:
    """Find length-prefixed printable strings in the nested body."""
    out: list[tuple[int, str]] = []
    limit = len(data) - 5
    for off in range(start, max(start, limit)):
        size = struct.unpack_from("<I", data, off)[0]
        if not 3 <= size <= 120 or off + 4 + size > len(data):
            continue
        raw = data[off + 4: off + 4 + size]
        if raw[-1] != 0:
            continue
        try:
            text = raw[:-1].decode("latin-1")
        except UnicodeDecodeError:
            continue
        if not _ALLOWED.match(text) or not any(c.isalpha() for c in text):
            continue
        out.append((off, text))
    return out


def read_bfi(path: Path) -> Bfi:
    data = path.read_bytes()
    if len(data) < 0x40:
        raise BfiFormatError(f"{path}: too short")
    width, height = struct.unpack_from("<II", data, 0)
    vertical = struct.unpack_from("<f", data, 8)[0]
    variant = struct.unpack_from("<I", data, 0x0C)[0]
    flag = data[0x10]
    field_11, field_15, field_19, field_1d, field_21, field_25, field_29 = (
        struct.unpack_from("<I", data, off)[0]
        for off in (0x11, 0x15, 0x19, 0x1D, 0x21, 0x25, 0x29)
    )
    tag, body = _read_tag(data)
    strings = [(0x2D, tag)] if tag else []
    strings.extend(scan_strings(data, body))
    return Bfi(
        path=str(path),
        name=path.stem,
        width=width,
        height=height,
        vertical=vertical,
        variant=variant,
        flag=flag,
        field_11=field_11,
        field_15=field_15,
        field_19=field_19,
        field_1d=field_1d,
        field_21=field_21,
        field_25=field_25,
        field_29=field_29,
        class_tag=tag,
        strings=strings,
    )


def scan(directory: Path) -> list[Bfi]:
    return [read_bfi(path) for path in sorted(directory.glob("*.bfi"))]


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("path", type=Path, help="a .bfi file or the features directory")
    args = ap.parse_args(argv)
    if args.path.is_dir():
        print(json.dumps([item.summary() for item in scan(args.path)], indent=2))
    else:
        print(json.dumps(read_bfi(args.path).summary(), indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
