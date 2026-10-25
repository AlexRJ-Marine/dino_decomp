#!/usr/bin/env python3
"""Reader for ``simu_trans.dic`` — the genotype translation/dominance tables.

Each of the 18 genes has a block:

    // Translation of the gene <Code>
    <translation_count>            (0 for identity genes: Head/ArmLeg/Body/Tail)
    <translation values>           (translation_count of them)
    <translation_count^2 ints>     (dominance matrix, row-major; omitted when 0)

    // Rarities of the alleles of the gene <Code>
    <allele_count>
    <allele_count rarity weights>

The allele count is the rarity count (20 for simple genes, 48 for dummies, 8
for skin type, …). For a dominant gene, the expressed value for allele pair
(a, b) is ``matrix[a][b]``; simple genes have no matrix and their expression
rule is still open.

See ``docs/formats/gen.md``.
"""
from __future__ import annotations

import argparse
import json
from dataclasses import dataclass
from pathlib import Path

TRANSLATION_RE = "// Translation of the gene <"
RARITY_RE = "// Rarities of the alleles of the gene <"


class GeneTransFormatError(ValueError):
    pass


@dataclass
class GeneTable:
    code: str
    allele_count: int
    translation: list[str]
    matrix: list[list[int]] | None
    rarities: list[int]

    def express(self, allele_a: int, allele_b: int) -> int | None:
        if not (0 <= allele_a < self.allele_count and 0 <= allele_b < self.allele_count):
            raise ValueError(f"{self.code}: allele out of range")
        if self.matrix is None:
            return None
        return self.matrix[allele_a][allele_b]

    def summary(self) -> dict:
        return {
            "code": self.code,
            "alleles": self.allele_count,
            "translated": bool(self.translation),
            "matrix": bool(self.matrix),
            "rarities": self.rarities,
        }


class _Cursor:
    def __init__(self, lines: list[str]) -> None:
        self.lines = lines
        self.index = 0

    def next_line(self) -> str:
        while self.index < len(self.lines):
            line = self.lines[self.index].strip()
            self.index += 1
            if line:
                return line
        raise GeneTransFormatError("unexpected EOF")

    def next_header(self, prefix: str) -> str:
        while self.index < len(self.lines):
            line = self.lines[self.index].strip()
            self.index += 1
            if line.startswith(prefix):
                start = line.index("<") + 1
                end = line.index(">")
                return line[start:end]
        raise GeneTransFormatError(f"missing header {prefix}")


def read_gene_trans(path: Path) -> dict[str, GeneTable]:
    lines = path.read_text(encoding="cp1252", errors="replace").splitlines()
    cursor = _Cursor(lines)
    tables: dict[str, GeneTable] = {}
    while True:
        try:
            code = cursor.next_header(TRANSLATION_RE)
        except GeneTransFormatError:
            break
        translation_count = int(cursor.next_line())
        translation: list[str] = []
        for _ in range(translation_count):
            translation.extend(cursor.next_line().split())
        if len(translation) != translation_count:
            raise GeneTransFormatError(f"{code}: {len(translation)} translation values")

        matrix = None
        if translation_count:
            values: list[int] = []
            while len(values) < translation_count * translation_count:
                line = cursor.next_line()
                if line.startswith("//"):
                    raise GeneTransFormatError(f"{code}: matrix ended early")
                values.extend(int(token) for token in line.split())
            matrix = [
                values[i * translation_count:(i + 1) * translation_count]
                for i in range(translation_count)
            ]

        rarity_code = cursor.next_header(RARITY_RE)
        if rarity_code != code:
            raise GeneTransFormatError(f"{code}: rarity block is {rarity_code}")
        allele_count = int(cursor.next_line())
        rarities = []
        while len(rarities) < allele_count:
            line = cursor.next_line()
            if line.startswith("//"):
                raise GeneTransFormatError(f"{code}: rarities ended early")
            rarities.extend(int(token) for token in line.split())
        if len(rarities) != allele_count:
            raise GeneTransFormatError(f"{code}: {len(rarities)} rarities")

        if translation_count and translation_count != allele_count:
            raise GeneTransFormatError(f"{code}: translation/rarity count mismatch")
        tables[code] = GeneTable(
            code=code,
            allele_count=allele_count,
            translation=translation,
            matrix=matrix,
            rarities=rarities,
        )
    return tables


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("dic", type=Path)
    args = ap.parse_args(argv)
    tables = read_gene_trans(args.dic)
    print(json.dumps({code: table.summary() for code, table in tables.items()}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
