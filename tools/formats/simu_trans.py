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
    dummy_disposition: dict[str, list[list[int]]] = field(default_factory=dict)
    dummy_type: dict[str, list[list[int]]] = field(default_factory=dict)
    dummy_influence: list[list[list[int]]] = field(default_factory=list)
    member_reference_widths: list[int] = field(default_factory=list)
    skin_map: list[list[int]] = field(default_factory=list)
    skin_unknown: int = 0
    skin_influence: list[list[list[int]]] = field(default_factory=list)
    carapace_map: list[list[int]] = field(default_factory=list)
    carapace_influence: list[list[list[int]]] = field(default_factory=list)
    min_attributes: list[float] = field(default_factory=list)
    max_attributes: list[float] = field(default_factory=list)
    part_headers: list[tuple[int, str]] = field(default_factory=list)
    attribute_order: list[str] = field(default_factory=list)
    part_order: list[str] = field(default_factory=list)

    def family_of(self, dinosaur_index: int) -> str:
        return FAMILIES[self.default_families[dinosaur_index]]

    def summary(self) -> dict:
        return {
            "path": self.path,
            "genes": len(self.genes),
            "merge": {name: table.to_dict() for name, table in self.merge.items()},
            "attributes": len(self.attributes),
            "attribute_keys": [f"{p}:{a}" for p, a in self.attributes],
            "attribute_order": self.attribute_order,
            "part_order": self.part_order,
            "dummy_disposition": {
                name: [len(rows), len(rows[0]) if rows else 0]
                for name, rows in self.dummy_disposition.items()
            },
            "dummy_type": {
                name: [len(rows), len(rows[0]) if rows else 0]
                for name, rows in self.dummy_type.items()
            },
            "dummy_influence": [
                [len(table), len(table[0]) if table else 0]
                for table in self.dummy_influence
            ],
            "member_reference_widths": self.member_reference_widths,
            "skin_map": [len(self.skin_map), len(self.skin_map[0]) if self.skin_map else 0],
            "skin_unknown": self.skin_unknown,
            "skin_influence": [
                [len(table), len(table[0]) if table else 0]
                for table in self.skin_influence
            ],
            "carapace_map": [
                len(self.carapace_map), len(self.carapace_map[0]) if self.carapace_map else 0
            ],
            "carapace_influence": [
                [len(table), len(table[0]) if table else 0]
                for table in self.carapace_influence
            ],
            "min_attributes": len(self.min_attributes),
            "max_attributes": len(self.max_attributes),
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


def _dummy_subtables(
    lines: list[str], start: int, end: int, row_width: int
) -> dict[str, list[list[int]]]:
    parts: dict[str, list[list[int]]] = {}
    index = start
    while index < end:
        text = lines[index].strip()
        if text in ("// Head", "// Body", "// Tail"):
            name = text[3:]
            index += 1
            while index < end and not lines[index].strip():
                index += 1
            count = int(lines[index].strip())
            index += 1
            rows: list[list[int]] = []
            while len(rows) < count:
                if lines[index].strip():
                    row = [int(token) for token in lines[index].split()]
                    if len(row) != row_width:
                        raise SimuTransError(
                            f"{name}: row {len(rows)} has {len(row)} values, "
                            f"expected {row_width}"
                        )
                    rows.append(row)
                index += 1
            parts[name] = rows
        else:
            index += 1
    if set(parts) != {"Head", "Body", "Tail"}:
        raise SimuTransError(f"dummy subparts: {sorted(parts)}")
    return parts


def _dummy_influence(
    lines: list[str], start: int, end: int
) -> tuple[list[list[list[int]]], list[int]]:
    tokens = [
        line.strip()
        for line in lines[start + 1:end]
        if line.strip() and not line.strip().startswith("//")
    ]
    index = 0
    tables: list[list[list[int]]] = []
    while index < len(tokens) and tokens[index].endswith(":"):
        count = int(tokens[index].rstrip(":"))
        index += 1
        blocks: list[list[int]] = []
        for _ in range(count):
            width = int(tokens[index].rstrip(":"))
            index += 1
            values: list[int] = []
            while len(values) < width:
                values.extend(int(token) for token in tokens[index].split())
                index += 1
            if len(values) != width:
                raise SimuTransError("dummy influence row width mismatch")
            blocks.append(values)
        tables.append(blocks)
    widths = [int(token) for token in tokens[index:]]
    return tables, widths


def _counted_blocks(
    tokens: list[str], index: int
) -> tuple[list[list[int]], int]:
    """Read ``N:`` then N × (``W:`` + W ints); return blocks and next index."""
    count = int(tokens[index].rstrip(":"))
    index += 1
    blocks: list[list[int]] = []
    for _ in range(count):
        width = int(tokens[index].rstrip(":"))
        index += 1
        values: list[int] = []
        while len(values) < width:
            values.extend(int(token) for token in tokens[index].split())
            index += 1
        if len(values) != width:
            raise SimuTransError("counted block width mismatch")
        blocks.append(values)
    return blocks, index


def read_simu_trans(path: Path) -> SimuTrans:
    lines = path.read_text(encoding="cp1252", errors="replace").splitlines()
    headers = _part_offsets(lines)
    starts = {n: index for n, index, _ in headers}
    part2_start = starts[2]
    part3_start = starts[3]
    part4_start = starts[4]
    part10_start = starts[10]
    part10_end = len(lines)
    part5_start = starts[5]
    part6_start = starts[6]
    part7_start = starts[7]
    part8_start = starts[8]
    part9_start = starts[9]

    merge: dict[str, MergeTable] = {}
    for match, body in _block(lines, part2_start, part3_start, MERGE_RE):
        merge[match.group(1)] = _table(match.group(1), body, 20)
    if set(merge) != {"Head", "ArmLeg", "Body", "Tail"}:
        raise SimuTransError(f"merge tables: {sorted(merge)}")

    attributes: dict[tuple[str, str], MergeTable] = {}
    attribute_order: list[str] = []
    part_order: list[str] = []
    for match, body in _block(lines, part3_start, part4_start, ATTR_RE):
        part, attribute = match.group(1), match.group(2)
        attributes[(part, attribute)] = _table(
            f"{part}:{attribute}", body, 20
        )
        if attribute not in attribute_order:
            attribute_order.append(attribute)
        if part not in part_order:
            part_order.append(part)
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

    dummy_disposition = _dummy_subtables(lines, part4_start, part5_start, 6)
    dummy_type = _dummy_subtables(lines, part5_start, part6_start, 5)
    dummy_influence, reference_widths = _dummy_influence(
        lines, part6_start, part7_start
    )
    if len(dummy_influence) != 2 or any(
        len(table) != 50 or any(len(row) != 18 for row in table)
        for table in dummy_influence
    ):
        raise SimuTransError("dummy influence: expected two 50x18 tables")
    if len(reference_widths) != 5:
        raise SimuTransError(
            f"member reference widths: {len(reference_widths)} values"
        )

    def tokens_between(start: int, end: int) -> list[str]:
        return [
            line.strip()
            for line in lines[start + 1:end]
            if line.strip() and not line.strip().startswith("//")
        ]

    skin_tokens = tokens_between(part7_start, part8_start)
    skin_map, index = _counted_blocks(skin_tokens, 0)
    skin_unknown = int(skin_tokens[index])
    index += 1
    skin_influence: list[list[list[int]]] = []
    for _ in range(2):
        table, index = _counted_blocks(skin_tokens, index)
        skin_influence.append(table)
    if index != len(skin_tokens):
        raise SimuTransError("skin part: trailing tokens")

    carapace_tokens = tokens_between(part8_start, part9_start)
    carapace_map, index = _counted_blocks(carapace_tokens, 0)
    carapace_influence: list[list[list[int]]] = []
    for _ in range(2):
        table, index = _counted_blocks(carapace_tokens, index)
        carapace_influence.append(table)
    if index != len(carapace_tokens):
        raise SimuTransError("carapace part: trailing tokens")

    minmax = [float(token) for token in tokens_between(part9_start, part10_start)]
    if len(minmax) != 40:
        raise SimuTransError(f"min/max attributes: {len(minmax)} values")
    min_attributes = minmax[:20]
    max_attributes = minmax[20:40]

    return SimuTrans(
        path=str(path),
        genes=read_gene_trans(path),
        merge=merge,
        attributes=attributes,
        default_families=values,
        dummy_disposition=dummy_disposition,
        dummy_type=dummy_type,
        dummy_influence=dummy_influence,
        member_reference_widths=reference_widths,
        skin_map=skin_map,
        skin_unknown=skin_unknown,
        skin_influence=skin_influence,
        carapace_map=carapace_map,
        carapace_influence=carapace_influence,
        min_attributes=min_attributes,
        max_attributes=max_attributes,
        part_headers=[(n, title) for n, _, title in headers],
        attribute_order=attribute_order,
        part_order=part_order,
    )


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--dump", type=Path, required=True)
    args = ap.parse_args(argv)
    print(json.dumps(read_simu_trans(args.dump).summary(), indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
