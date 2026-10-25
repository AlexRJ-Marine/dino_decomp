#!/usr/bin/env python3
"""Reader for ``anim_trans.dic``, ``graphic_trans.dic`` and ``proportion.dat``.

All three live in ``Data/Design/Genotype``.  The two ``.dic`` files are plain
cp1252 text with ``//`` section headers; ``proportion.dat`` is binary.

``anim_trans.dic``::

    // Dinosaurs
    20
    <20 species names>
    // Families
    5
    HighQuadruped LightQuadruped HeavyBiped LightBiped LowQuadruped
    // Animations
    51
    <51 clip names>
    // Head merge translation table
    <20 x 20 ints>
    // ArmLeg merge translation table
    <20 x 20 ints>
    // Body merge translation table
    <20 x 20 ints>
    // Tail merge translation table
    <20 x 20 ints>

Each matrix is indexed by species in the ``Dinosaurs`` order.  The diagonal
is 0.  Head/Body/Tail use 50 for every cross-species pair; ArmLeg uses
0/1/50/99 and is asymmetric.

``graphic_trans.dic`` is a comment plus five reference widths
(100, 100, 100, 300, 150), one per family in the order above.

``proportion.dat`` is a u32 count (20) then 20 records of five floats in the
``Dinosaurs`` order.  Column 3 is always 1.0 (legs are the reference); the
other columns track head, arm, body and tail part scales (T-rex arms 0.235,
Diplodocus tail 2.093).

See ``docs/formats/anim-trans.md``.
"""
from __future__ import annotations

import argparse
import json
import struct
from dataclasses import dataclass, field
from pathlib import Path


class AnimTransError(ValueError):
    pass


MERGE_TABLES = (
    "Head merge translation table",
    "ArmLeg merge translation table",
    "Body merge translation table",
    "Tail merge translation table",
)


@dataclass
class AnimTrans:
    path: str
    dinosaurs: list[str]
    families: list[str]
    animations: list[str]
    tables: dict[str, list[list[int]]] = field(default_factory=dict)

    @property
    def size(self) -> int:
        return len(self.dinosaurs)

    def merge(self, part: str, source: str, target: str) -> int:
        """Return the merge code for ``part`` (head/armleg/body/tail)."""
        table = self.tables[f"{part.capitalize()} merge translation table"]
        return table[self.dinosaurs.index(source)][self.dinosaurs.index(target)]

    def summary(self) -> dict:
        values = {
            name: sorted({v for row in table for v in row})
            for name, table in self.tables.items()
        }
        return {
            "path": self.path,
            "dinosaurs": len(self.dinosaurs),
            "families": self.families,
            "animations": len(self.animations),
            "tables": {name: len(table) for name, table in self.tables.items()},
            "table_values": values,
        }


def _lines(path: Path) -> list[str]:
    return [line.strip() for line in path.read_text(encoding="cp1252").splitlines()]


def read_anim_trans(path: Path) -> AnimTrans:
    lines = _lines(path)
    index = 0

    def skip_blank() -> None:
        nonlocal index
        while index < len(lines) and not lines[index]:
            index += 1

    def header(expected: str) -> None:
        nonlocal index
        skip_blank()
        if index >= len(lines) or lines[index] != f"// {expected}":
            found = lines[index] if index < len(lines) else "<eof>"
            raise AnimTransError(f"{path}: expected '// {expected}', got {found!r}")
        index += 1

    def names(expected: str) -> list[str]:
        nonlocal index
        header(expected)
        if index >= len(lines):
            raise AnimTransError(f"{path}: missing count after {expected}")
        try:
            count = int(lines[index])
        except ValueError as e:
            raise AnimTransError(f"{path}: bad count {lines[index]!r}") from e
        index += 1
        items = lines[index:index + count]
        if len(items) != count or any(not item for item in items):
            raise AnimTransError(f"{path}: expected {count} {expected} names")
        index += count
        return items

    dinosaurs = names("Dinosaurs")
    families = names("Families")
    animations = names("Animations")
    size = len(dinosaurs)
    tables: dict[str, list[list[int]]] = {}
    while True:
        skip_blank()
        if index >= len(lines):
            break
        if not lines[index].startswith("// "):
            raise AnimTransError(f"{path}: expected a table header at line {index + 1}")
        name = lines[index][3:].strip()
        index += 1
        rows: list[list[int]] = []
        while index < len(lines) and lines[index] and not lines[index].startswith("//"):
            try:
                row = [int(token) for token in lines[index].split()]
            except ValueError as e:
                raise AnimTransError(f"{path}: bad matrix row {lines[index]!r}") from e
            rows.append(row)
            index += 1
        if len(rows) != size or any(len(row) != size for row in rows):
            raise AnimTransError(
                f"{path}: {name} is {len(rows)} rows, expected {size}x{size}"
            )
        tables[name] = rows
    missing = [name for name in MERGE_TABLES if name not in tables]
    if missing:
        raise AnimTransError(f"{path}: missing tables {missing}")
    return AnimTrans(
        path=str(path),
        dinosaurs=dinosaurs,
        families=families,
        animations=animations,
        tables=tables,
    )


def read_graphic_trans(path: Path) -> list[int]:
    widths = []
    for line in _lines(path):
        if not line or line.startswith("//"):
            continue
        try:
            widths.append(int(line))
        except ValueError as e:
            raise AnimTransError(f"{path}: bad width {line!r}") from e
    if not widths:
        raise AnimTransError(f"{path}: no widths")
    return widths


PART_NAMES = ("head", "arm", "body", "leg", "tail")


@dataclass
class Proportions:
    """Per-species part scale factors from ``proportion.dat``."""

    path: str
    dinosaurs: list[str]
    scales: list[tuple[float, ...]]

    def scale(self, dinosaur: str, part: str) -> float:
        try:
            column = PART_NAMES.index(part.lower())
        except ValueError as e:
            raise AnimTransError(f"unknown part {part!r}") from e
        return self.scales[self.dinosaurs.index(dinosaur)][column]

    def summary(self) -> dict:
        return {
            "path": self.path,
            "records": len(self.scales),
            "parts": list(PART_NAMES),
            "leg_column_constant": all(
                abs(row[3] - 1.0) < 1e-6 for row in self.scales
            ),
            "sample": {
                name: [round(v, 4) for v in self.scales[i]]
                for i, name in enumerate(self.dinosaurs[:3])
            },
        }


def read_proportion(path: Path, dinosaurs: list[str] | None = None) -> Proportions:
    data = path.read_bytes()
    if len(data) < 4:
        raise AnimTransError(f"{path}: shorter than count")
    count = struct.unpack_from("<I", data, 0)[0]
    record = 5 * 4
    if len(data) != 4 + count * record:
        raise AnimTransError(
            f"{path}: {len(data)} bytes, expected {4 + count * record} for {count} records"
        )
    scales = [struct.unpack_from("<5f", data, 4 + i * record) for i in range(count)]
    if dinosaurs is None:
        dinosaurs = [f"dino{i}" for i in range(count)]
    if len(dinosaurs) != count:
        raise AnimTransError(f"{path}: {count} records but {len(dinosaurs)} names")
    return Proportions(path=str(path), dinosaurs=list(dinosaurs), scales=scales)


def scan(root: Path) -> tuple[int, list[tuple[str, str]], dict]:
    total = 0
    bad: list[tuple[str, str]] = []
    sums = {"dinosaurs": 0, "families": 0, "animations": 0, "tables": 0,
            "proportions": 0}
    for path in sorted(root.rglob("anim_trans.dic")):
        total += 1
        try:
            data = read_anim_trans(path)
        except Exception as e:
            bad.append((str(path), str(e)))
            continue
        sums["dinosaurs"] += len(data.dinosaurs)
        sums["families"] += len(data.families)
        sums["animations"] += len(data.animations)
        sums["tables"] += len(data.tables)
    for path in sorted(root.rglob("graphic_trans.dic")):
        total += 1
        try:
            read_graphic_trans(path)
        except Exception as e:
            bad.append((str(path), str(e)))
    anim_path = root / "anim_trans.dic"
    names = read_anim_trans(anim_path).dinosaurs if anim_path.exists() else None
    for path in sorted(root.rglob("proportion.dat")):
        total += 1
        try:
            sums["proportions"] += len(read_proportion(path, names).scales)
        except Exception as e:
            bad.append((str(path), str(e)))
    return total, bad, sums


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--dump", type=Path)
    ap.add_argument("--dump-proportion", type=Path)
    ap.add_argument("--scan", type=Path)
    args = ap.parse_args(argv)
    if args.dump:
        data = read_anim_trans(args.dump)
        print(json.dumps(data.summary(), indent=2))
        return 0
    if args.dump_proportion:
        names = None
        anim_path = args.dump_proportion.parent / "anim_trans.dic"
        if anim_path.exists():
            names = read_anim_trans(anim_path).dinosaurs
        print(json.dumps(read_proportion(args.dump_proportion, names).summary(), indent=2))
        return 0
    if args.scan:
        total, bad, sums = scan(args.scan)
        print(f"scanned {total} dictionary files, {len(bad)} bad, {sums}")
        for path, why in bad[:20]:
            print(f"  FAIL {path}: {why}")
        return 0 if not bad else 1
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
