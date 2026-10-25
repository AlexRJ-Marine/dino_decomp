#!/usr/bin/env python3
"""Reader for ``.bsi`` food-source definitions.

Ten files in ``Data/Design/foodsource``, one per ``FoodSourceList.ini`` entry
(Flower … Cow).

Layout::

    0   f32 nutrition / food value
    4   f32 quantity / portion size
    8   u32 price
    12  u32 kind id (FoodSourceList order)
    16  u32 0
    20  u32 0
    24  u32 flags (256 in all retail files)
    28  u8  flag (0)
    29  four length-prefixed strings:
          food LOD1 mesh, part1 mesh, part2 mesh, texture directory
    ... tail: numeric bytes + feeder mesh + ``FOOD_DISTRIBUTION_*_BUTTON``

The tail strings are interleaved; scan for them. See ``docs/formats/bsi.md``.
"""
from __future__ import annotations

import argparse
import json
import struct
from dataclasses import dataclass, field
from pathlib import Path

HEADER_SIZE = 29
ASSET_STRINGS = 4


class BsiFormatError(ValueError):
    pass


@dataclass
class FoodSource:
    path: str
    name: str
    nutrition: float
    quantity: float
    price: int
    kind_id: int
    flags: int
    flag28: int
    assets: list[str] = field(default_factory=list)
    tail_strings: list[str] = field(default_factory=list)
    tail: bytes = b""

    @property
    def feeder_mesh(self) -> str:
        for text in self.tail_strings:
            if text.lower().endswith(".mesh") and "feed" in text.lower():
                return text
        return ""

    @property
    def ui_name(self) -> str:
        for text in self.tail_strings:
            if text.startswith("FOOD_"):
                return text
        return ""

    def summary(self) -> dict:
        return {
            "path": self.path,
            "name": self.name,
            "nutrition": self.nutrition,
            "quantity": self.quantity,
            "price": self.price,
            "kind_id": self.kind_id,
            "flags": self.flags,
            "flag28": self.flag28,
            "assets": self.assets,
            "feeder_mesh": self.feeder_mesh,
            "ui_name": self.ui_name,
            "tail_bytes": len(self.tail),
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


def _scan_tail_strings(data: bytes, start: int) -> list[str]:
    out = []
    off = start
    while off + 4 <= len(data):
        parsed = _read_string(data, off)
        if parsed is not None:
            text, end = parsed
            if (text.startswith(("DATA\\", "FOOD_"))
                    or text.lower().endswith((".mesh", ".tga", ".ini"))
                    or text.isalpha()):
                if len(text) >= 4:
                    out.append(text)
                    off = end
                    continue
        off += 1
    return out


def read_bsi(path: Path) -> FoodSource:
    data = path.read_bytes()
    if len(data) < HEADER_SIZE:
        raise BsiFormatError(f"{path}: shorter than header")
    nutrition, quantity = struct.unpack_from("<2f", data, 0)
    price, kind_id, reserved0, reserved1, flags = struct.unpack_from("<5I", data, 8)
    flag28 = data[28]
    off = HEADER_SIZE
    assets = []
    for index in range(ASSET_STRINGS):
        parsed = _read_string(data, off)
        if parsed is None:
            raise BsiFormatError(f"{path}: asset {index} is not a string at {off}")
        text, off = parsed
        assets.append(text)
    tail_strings = _scan_tail_strings(data, off)
    return FoodSource(
        path=str(path),
        name=path.stem,
        nutrition=nutrition,
        quantity=quantity,
        price=price,
        kind_id=kind_id,
        flags=flags,
        flag28=flag28,
        assets=assets,
        tail_strings=tail_strings,
        tail=data[off:],
    )


def read_food_list(path: Path) -> list[str]:
    lines = path.read_text(encoding="cp1252", errors="replace").splitlines()
    names = [line.strip() for line in lines if line.strip() and not line.strip().startswith("//")]
    if names and names[0].isdigit():
        names = names[1:]
    return names


def scan(root: Path) -> tuple[int, list[tuple[str, str]], dict]:
    total = 0
    bad: list[tuple[str, str]] = []
    sums = {"assets": 0, "ui": 0}
    for path in sorted(root.rglob("*.bsi")):
        total += 1
        try:
            food = read_bsi(path)
        except Exception as e:
            bad.append((str(path), str(e)))
            continue
        sums["assets"] += len(food.assets)
        sums["ui"] += int(bool(food.ui_name))
    return total, bad, sums


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--dump", type=Path)
    ap.add_argument("--scan", type=Path)
    args = ap.parse_args(argv)
    if args.dump:
        print(json.dumps(read_bsi(args.dump).summary(), indent=2))
        return 0
    if args.scan:
        total, bad, sums = scan(args.scan)
        print(f"scanned {total} food sources, {len(bad)} bad, "
              f"{sums['assets']} asset paths, {sums['ui']} UI names")
        for path, why in bad[:20]:
            print(f"  FAIL {path}: {why}")
        return 0 if not bad else 1
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
