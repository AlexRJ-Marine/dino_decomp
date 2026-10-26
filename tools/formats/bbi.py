#!/usr/bin/env python3
"""Reader for ``.bbi`` building definitions.

39 files in ``Data/Design/buildings`` (one per ``BuildingList.ini`` entry):

    0..120   121-byte packed header:
               0  u32 footprint width
               4  u32 footprint height
               8  f32 scale
              12  u8  flag
              13  27 x unaligned u32 (13, 17, ... 117)
    121..    ten length-prefixed ASCII paths:
               skin, crash LOD1, destroy LOD1, construction LOD1,
               LOD2 x4, skel, anim
    ...      tail: more length-prefixed strings (texture dir, explosion
             files, sounds, ``CONSTRUC_*`` build-panel widget names) then
             numeric bytes (profile/cost values; not decoded)

The header is **not** 30 aligned u32: the first four fields are
`u32, u32, f32, u8`, so every later field is 1 byte out of phase with a
30-u32 view. The named offsets below come from the retail loader
``FUN_0053fb00`` (see ``notes/building-def-record-recon.md``).

See ``docs/formats/bbi.md``.
"""
from __future__ import annotations

import argparse
import json
import struct
from dataclasses import dataclass, field
from pathlib import Path

HEADER_SIZE = 121
MESH_COUNT = 10


class BbiFormatError(ValueError):
    pass


@dataclass
class Building:
    path: str
    name: str
    header: bytes
    footprint_w: int
    footprint_h: int
    scale: float
    flag: int
    header_words: tuple[int, ...]  # 27 unaligned u32 from byte 13
    meshes: list[str] = field(default_factory=list)
    tail_strings: list[str] = field(default_factory=list)
    tail: bytes = b""

    def _u32(self, offset: int) -> int:
        return struct.unpack_from("<I", self.header, offset)[0]

    def _f32(self, offset: int) -> float:
        return struct.unpack_from("<f", self.header, offset)[0]

    @property
    def category(self) -> int:
        return self._u32(49)

    @property
    def duration(self) -> float:
        return self._f32(37)

    @property
    def price(self) -> int:
        return self._u32(25)

    @property
    def appeal_scale(self) -> float:
        return self._f32(73)

    @property
    def radius_squared(self) -> int:
        return self._u32(105)

    @property
    def model0(self) -> int:
        return self._u32(109)

    @property
    def model1(self) -> float:
        return self._f32(113)

    @property
    def model_count(self) -> int:
        return self._u32(117)

    @property
    def ui_names(self) -> list[str]:
        return [s for s in self.tail_strings if s.startswith("CONSTRUC_")]

    def summary(self) -> dict:
        return {
            "path": self.path,
            "name": self.name,
            "footprint": [self.footprint_w, self.footprint_h],
            "scale": self.scale,
            "flag": self.flag,
            "header_words": list(self.header_words),
            "category": self.category,
            "duration": self.duration,
            "price": self.price,
            "appeal_scale": self.appeal_scale,
            "radius_squared": self.radius_squared,
            "model0": self.model0,
            "model1": self.model1,
            "model_count": self.model_count,
            "meshes": self.meshes,
            "tail_strings": self.tail_strings,
            "tail_bytes": len(self.tail),
            "ui_names": self.ui_names,
        }


def _read_string(data: bytes, off: int) -> tuple[str, int] | None:
    if off + 4 > len(data):
        return None
    length = struct.unpack_from("<I", data, off)[0]
    if not 2 <= length <= 400 or off + 4 + length > len(data):
        return None
    raw = data[off + 4:off + 4 + length]
    if not raw.endswith(b"\x00") or not all(32 <= b < 127 or b == 0 for b in raw):
        return None
    return raw.rstrip(b"\x00").decode("ascii"), off + 4 + length


def _plausible_tail_string(text: str) -> bool:
    if len(text) < 4:
        return False
    if text.startswith(("DATA\\", "CONSTRUC_")):
        return True
    if text.lower().endswith((".ini", ".wav", ".tga", ".mesh", ".skin", ".skel", ".anim")):
        return True
    return text.isalpha()


def _scan_tail_strings(data: bytes, start: int) -> list[str]:
    out: list[str] = []
    off = start
    while off + 4 <= len(data):
        parsed = _read_string(data, off)
        if parsed is not None:
            text, end = parsed
            if _plausible_tail_string(text):
                out.append(text)
                off = end
                continue
        off += 1
    return out


def read_bbi(path: Path) -> Building:
    data = path.read_bytes()
    if len(data) < HEADER_SIZE:
        raise BbiFormatError(f"{path}: shorter than header")
    off = HEADER_SIZE
    meshes = []
    for index in range(MESH_COUNT):
        parsed = _read_string(data, off)
        if parsed is None:
            raise BbiFormatError(f"{path}: mesh {index} is not a string at {off}")
        text, off = parsed
        meshes.append(text)

    tail_strings = _scan_tail_strings(data, off)
    return Building(
        path=str(path),
        name=path.stem,
        header=data[:HEADER_SIZE],
        footprint_w=struct.unpack_from("<I", data, 0)[0],
        footprint_h=struct.unpack_from("<I", data, 4)[0],
        scale=struct.unpack_from("<f", data, 8)[0],
        flag=data[12],
        header_words=struct.unpack_from("<27I", data, 13),
        meshes=meshes,
        tail_strings=tail_strings,
        tail=data[off:],
    )


def read_building_list(path: Path) -> list[str]:
    lines = path.read_text(encoding="cp1252", errors="replace").splitlines()
    names = [line.strip() for line in lines if line.strip() and not line.strip().startswith("//")]
    if names and names[0].isdigit():
        names = names[1:]
    return names


def scan(root: Path) -> tuple[int, list[tuple[str, str]], dict]:
    total = 0
    bad: list[tuple[str, str]] = []
    sums = {"meshes": 0, "ui": 0}
    for path in sorted(root.rglob("*.bbi")):
        total += 1
        try:
            building = read_bbi(path)
        except Exception as e:
            bad.append((str(path), str(e)))
            continue
        sums["meshes"] += len(building.meshes)
        sums["ui"] += len(building.ui_names)
    return total, bad, sums


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--dump", type=Path)
    ap.add_argument("--scan", type=Path)
    args = ap.parse_args(argv)
    if args.dump:
        print(json.dumps(read_bbi(args.dump).summary(), indent=2))
        return 0
    if args.scan:
        total, bad, sums = scan(args.scan)
        print(f"scanned {total} buildings, {len(bad)} bad, "
              f"{sums['meshes']} mesh paths, {sums['ui']} UI names")
        for path, why in bad[:20]:
            print(f"  FAIL {path}: {why}")
        return 0 if not bad else 1
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
