#!/usr/bin/env python3
"""Reader for ``simu_trans.dic`` — the genotype/phenotype database.

The file has ten parts (``// Part N : title``):

1. Gene translation/dominance/rarity blocks (18 genes) — see ``gene_trans.py``.
2. Merge ratio tables: Head, ArmLeg, Body, Tail. Each is 20 diagonal values
   plus a 20x20 matrix. These matrices are identical to ``anim_trans.dic``.
3. Attributes linked to members: 11 attributes x 5 parts = 55 tables, each a
   20-value diagonal and a 20x20 merge matrix.
4. Dummy disposition table (5 parts x 48 rows x 6 ints).
5. Dummy type table (5 parts x 48 rows x 5 ints).
6. Dummy influence table (50 blocks of 18 ints).
7. Skin translation/influence (8 types x 3 values, then 582 rows x 22 x 18).
8. Carapace translation/influence (6 types x 3 values, then 16 rows x 18).
9. Min/max attributes (20 values each).
10. Default animation family for the 20 base dinosaurs.

This module parses parts 1, 2, 3 and 10 and exposes the part headers for the
rest. See ``docs/formats/simu-trans.md``.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from gene_trans import GeneTable, read_gene_trans  # noqa: E402

PART_RE = re.compile(r"^//\s*Part\s+(\d+)\s*:\s*(.*)$")
ATTR_RE = re.compile(r"^//\s*([A-Za-z]+)\s*:\s*([A-Za-z]+)\s*$")
MERGE_RE = re.compile(r"^//\s*([A-Za-z]+)\s*:\s*merge ratio\s*$")

FAMILIES = (
    "HighQuadruped",
    "LightQuadruped",
    "HeavyBiped",
    "LightBiped",
    "LowQuadruped",
)

PARTS = (
    "Gene translation table",
    "Merge ratio table",
    "Attributes linked to members",
    "Dummy disposition table",
    "Dummy type table",
    "Dummy influence table",
    "Skin translation and influence table",
    "Carapace translation and influence table",
    "Min & max attributes",
    "Default family for each base dinosaur",
)


class SimuTransError(ValueError):
    pass


@dataclass
class MergeTable:
    name: str
    diagonal: list[int]
    matrix: list[list[int]]

    def to_dict(self) -> dict:
        return {
            "name": self.name,
            "diagonal": len(self.diagonal),
            "matrix": [len(self.matrix), len(self.matrix[0]) if self.matrix else 0],
        }


@dataclass
class SimuTrans:
    path: str
    genes: dict[str, GeneTable]
    merge: dict[str, MergeTable]
    attributes: dict[tuple[str, str], MergeTable]
    default_families: list[int]
    part_headers: list[tuple[int, str]] = field(default_factory=list)

    def family_of(self, dinosaur_index: int) -> str:
        return FAMILIES[self.default_families[dinosaur_index]]

    def summary(self) -> dict:
        return {
            "path": self.path,
            "genes": len(self.genes),
            "merge": {name: table.to_dict() for name, table in self.merge.items()},
            "attributes": len(self.attributes),
            "attribute_keys": [f"{p}:{a}" for p, a in self.attributes],
            "default_families": {
                FAMILIES[i]: self.default_families.count(i)
                for i in range(len(FAMILIES))
            },
            "part_headers": [f"{n}: {title}" for n, title in self.part_headers],
        }


def _part_offsets(lines: list[str]) -> list[tuple[int, int, str]]:
    headers = []
    for index, line in enumerate(lines):
        match = PART_RE.match(line)
        if match:
            headers.append((int(match.group(1)), index, match.group(2).strip()))
    headers.sort()
    if [n for n, _, _ in headers] != list(range(1, 11)):
        raise SimuTransError(f"expected parts 1..10, got {[n for n, _, _ in headers]}")
    return headers


def _block(lines: list[str], start: int, end: int, pattern: re.Pattern[str]):
    """Yield (match, body lines) for a header regex inside [start, end)."""
    index = start
    while index < end:
        match = pattern.match(lines[index])
        if not match:
            index += 1
            continue
        body_start = index + 1
        index = body_start
        while index < end and not lines[index].startswith("//"):
            index += 1
        yield match, lines[body_start:index]


def _table(name: str, body: list[str], expected: int) -> MergeTable:
    tokens = [line.strip() for line in body if line.strip()]
    if not tokens:
        raise SimuTransError(f"{name}: empty table")
    try:
        count = int(tokens[0])
    except ValueError as e:
        raise SimuTransError(f"{name}: bad count {tokens[0]!r}") from e
    if count != expected:
        raise SimuTransError(f"{name}: count {count}, expected {expected}")
    values: list[int] = []
    for line in tokens[1:]:
        values.extend(int(token) for token in line.split())
    if len(values) != count + count * count:
        raise SimuTransError(
            f"{name}: {len(values)} values, expected {count + count * count}"
        )
    diagonal = values[:count]
    matrix_values = values[count:]
    matrix = [
        matrix_values[row * count:(row + 1) * count] for row in range(count)
    ]
    return MergeTable(name=name, diagonal=diagonal, matrix=matrix)


def read_simu_trans(path: Path) -> SimuTrans:
    lines = path.read_text(encoding="cp1252", errors="replace").splitlines()
    headers = _part_offsets(lines)
    starts = {n: index for n, index, _ in headers}
    part2_start = starts[2]
    part3_start = starts[3]
    part4_start = starts[4]
    part10_start = starts[10]
    part10_end = len(lines)

    merge: dict[str, MergeTable] = {}
    for match, body in _block(lines, part2_start, part3_start, MERGE_RE):
        merge[match.group(1)] = _table(match.group(1), body, 20)
    if set(merge) != {"Head", "ArmLeg", "Body", "Tail"}:
        raise SimuTransError(f"merge tables: {sorted(merge)}")

    attributes: dict[tuple[str, str], MergeTable] = {}
    for match, body in _block(lines, part3_start, part4_start, ATTR_RE):
        part, attribute = match.group(1), match.group(2)
        attributes[(part, attribute)] = _table(
            f"{part}:{attribute}", body, 20
        )
    if len(attributes) != 55:
        raise SimuTransError(f"expected 55 attribute tables, got {len(attributes)}")

    tail = [
        line.strip()
        for line in lines[part10_start + 1:part10_end]
        if line.strip() and not line.strip().startswith("//")
    ]
    if not tail:
        raise SimuTransError("missing Part 10 list")
    count = int(tail[0])
    values = [int(token) for line in tail[1:] for token in line.split()]
    if count != 20 or len(values) != 20:
        raise SimuTransError(f"default families: {count} entries, {len(values)} values")
    if any(not 0 <= value < len(FAMILIES) for value in values):
        raise SimuTransError("default family index out of range")

    return SimuTrans(
        path=str(path),
        genes=read_gene_trans(path),
        merge=merge,
        attributes=attributes,
        default_families=values,
        part_headers=[(n, title) for n, _, title in headers],
    )


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--dump", type=Path, required=True)
    args = ap.parse_args(argv)
    print(json.dumps(read_simu_trans(args.dump).summary(), indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
