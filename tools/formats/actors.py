#!/usr/bin/env python3
"""Readers for ``.bni`` animals and ``.bei`` employees.

Both are actor profiles: a fixed header, then a body interleaving numeric
records with length-prefixed ASCII asset/sound strings.

``.bni`` (5 files, ``AnimalList.ini``):

    48-byte header (12 x 32-bit):
      0  u32 variant (Cow 3, others 1)
      1  u32 1
      2  f32 1.0
      3  f32 size (Sheep 1.0 .. Ostrich 4.0)
      4  u32 limit (50 in all)
      5  u32 food value (Ostrich 200, Sheep 400, Pig 600, Cow 1000)
      6  f32 quantity (0.25 .. 0.5)
      7  u32 food kind id (matches .bsi kind)
      8  f32 10.0
      9  u32 0
      10 u32 0
      11 u32 1

``.bei`` (2 files, ``EmployeeList.ini``):

    60-byte header (15 x 32-bit):
      0  u32 employee kind (Nutritionist 0, Guard 1)
      1  u32 1
      2  u32 1
      3  f32 height (1.7)
      4  f32 walk speed (1.0 / 2.0)
      5  f32 run/action speed (2.5)
      6  u32 10
      7  u32 2
      8  u32 500
      9  f32 50
      10 f32 3
      11 f32 100
      12..14 u32 0, 0, 1

See ``docs/formats/bni.md`` and ``docs/formats/bei.md``.
"""
from __future__ import annotations

import argparse
import json
import struct
from dataclasses import dataclass, field
from pathlib import Path

BNI_HEADER = 48
BEI_HEADER = 60


class ActorFormatError(ValueError):
    pass


@dataclass
class Animal:
    path: str
    name: str
    variant: int
    size: float
    limit: int
    food_value: int
    quantity: float
    food_kind: int
    speed: float
    flag: int
    strings: list[str] = field(default_factory=list)
    body: bytes = b""

    @property
    def assets(self) -> list[str]:
        return [s for s in self.strings if s.startswith("DATA\\")]

    @property
    def meshes(self) -> list[str]:
        return [s for s in self.assets if s.lower().endswith(".mesh")]

    @property
    def texture_dirs(self) -> list[str]:
        return [s for s in self.assets if not s.lower().endswith(".mesh")]

    def summary(self) -> dict:
        return {
            "path": self.path,
            "name": self.name,
            "variant": self.variant,
            "size": self.size,
            "limit": self.limit,
            "food_value": self.food_value,
            "quantity": self.quantity,
            "food_kind": self.food_kind,
            "speed": self.speed,
            "flag": self.flag,
            "meshes": self.meshes,
            "texture_dirs": self.texture_dirs,
        }


@dataclass
class Employee:
    path: str
    name: str
    kind_id: int
    height: float
    walk_speed: float
    action_speed: float
    fields: tuple[int, ...]
    strings: list[str] = field(default_factory=list)
    body: bytes = b""

    @property
    def assets(self) -> list[str]:
        return [s for s in self.strings if s.startswith("DATA\\")]

    @property
    def meshes(self) -> list[str]:
        return [s for s in self.assets if s.lower().endswith(".mesh")]

    @property
    def sounds(self) -> list[str]:
        return [s for s in self.strings if not s.startswith("DATA\\")]

    def summary(self) -> dict:
        return {
            "path": self.path,
            "name": self.name,
            "kind_id": self.kind_id,
            "height": self.height,
            "walk_speed": self.walk_speed,
            "action_speed": self.action_speed,
            "fields": list(self.fields),
            "meshes": self.meshes,
            "textures": [s for s in self.assets if not s.lower().endswith(".mesh")],
            "sounds": self.sounds,
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


def _scan_strings(data: bytes, start: int) -> list[str]:
    out: list[str] = []
    off = start
    while off + 4 <= len(data):
        parsed = _read_string(data, off)
        if parsed is not None:
            text, end = parsed
            if text.startswith("DATA\\") or text.lower().endswith(
                (".mesh", ".tga", ".ini", ".wav")
            ) or text.isalpha():
                if len(text) >= 4:
                    out.append(text)
                    off = end
                    continue
        off += 1
    return out


def read_bni(path: Path) -> Animal:
    data = path.read_bytes()
    if len(data) < BNI_HEADER:
        raise ActorFormatError(f"{path}: shorter than header")
    words = struct.unpack_from("<12I", data, 0)
    floats = struct.unpack_from("<12f", data, 0)
    if data.find(b"DATA\\") < BNI_HEADER:
        raise ActorFormatError(f"{path}: no body strings")
    return Animal(
        path=str(path),
        name=path.stem,
        variant=words[0],
        size=floats[3],
        limit=words[4],
        food_value=words[5],
        quantity=floats[6],
        food_kind=words[7],
        speed=floats[8],
        flag=words[11],
        strings=_scan_strings(data, BNI_HEADER),
        body=data[BNI_HEADER:],
    )


def read_bei(path: Path) -> Employee:
    data = path.read_bytes()
    if len(data) < BEI_HEADER:
        raise ActorFormatError(f"{path}: shorter than header")
    words = struct.unpack_from("<15I", data, 0)
    floats = struct.unpack_from("<15f", data, 0)
    if data.find(b"DATA\\") < BEI_HEADER:
        raise ActorFormatError(f"{path}: no body strings")
    return Employee(
        path=str(path),
        name=path.stem,
        kind_id=words[0],
        height=floats[3],
        walk_speed=floats[4],
        action_speed=floats[5],
        fields=words[6:15],
        strings=_scan_strings(data, BEI_HEADER),
        body=data[BEI_HEADER:],
    )


def scan_bni(root: Path) -> tuple[int, list[tuple[str, str]], dict]:
    total = 0
    bad: list[tuple[str, str]] = []
    sums = {"meshes": 0}
    for path in sorted(root.rglob("*.bni")):
        total += 1
        try:
            animal = read_bni(path)
        except Exception as e:
            bad.append((str(path), str(e)))
            continue
        sums["meshes"] += len(animal.meshes)
    return total, bad, sums


def scan_bei(root: Path) -> tuple[int, list[tuple[str, str]], dict]:
    total = 0
    bad: list[tuple[str, str]] = []
    sums = {"meshes": 0, "sounds": 0}
    for path in sorted(root.rglob("*.bei")):
        total += 1
        try:
            employee = read_bei(path)
        except Exception as e:
            bad.append((str(path), str(e)))
            continue
        sums["meshes"] += len(employee.meshes)
        sums["sounds"] += len(employee.sounds)
    return total, bad, sums


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--animal", type=Path)
    ap.add_argument("--employee", type=Path)
    ap.add_argument("--scan-animals", type=Path)
    ap.add_argument("--scan-employees", type=Path)
    args = ap.parse_args(argv)
    if args.animal:
        print(json.dumps(read_bni(args.animal).summary(), indent=2))
        return 0
    if args.employee:
        print(json.dumps(read_bei(args.employee).summary(), indent=2))
        return 0
    if args.scan_animals:
        total, bad, sums = scan_bni(args.scan_animals)
        print(f"scanned {total} animals, {len(bad)} bad, {sums['meshes']} meshes")
        for path, why in bad[:20]:
            print(f"  FAIL {path}: {why}")
        return 0 if not bad else 1
    if args.scan_employees:
        total, bad, sums = scan_bei(args.scan_employees)
        print(f"scanned {total} employees, {len(bad)} bad, "
              f"{sums['meshes']} meshes, {sums['sounds']} sounds")
        for path, why in bad[:20]:
            print(f"  FAIL {path}: {why}")
        return 0 if not bad else 1
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
