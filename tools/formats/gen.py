#!/usr/bin/env python3
"""Reader for Dino Island ``.gen`` genotypes.

The files are Windows-1252 text. Some carry the full French documentation
comments; others (breed variants) are just the number of genes followed by
allele pairs. Both forms are::

    18                  <- gene count
    a b                 <- gene 1: allele A, allele B
    ...                 <- 18 pairs

The 18 genes (from the documented header in the full files):

     0  head type                  0..19
     1  legs type                  0..19
     2  body type                  0..19
     3  tail type                  0..19
     4  head dummy position        0..39
     5  head dummy type            0..39
     6  body dummy position        0..39
     7  body dummy type            0..39
     8  tail dummy position        0..39
     9  tail dummy type            0..39
    10  skin type                  0..7
    11  skin subtype               0..2
    12  carapace type              0..5
    13  carapace subtype           0..3
    14  diet (0 herbivore, 1 carnivore)
    15  aggressiveness             0..9  (display 1..10)
    16  stamina                    0..9  (display 10..100)
    17  courage                    0..9  (display 10..80)

See ``docs/formats/gen.md``.
"""
from __future__ import annotations

import argparse
import json
import re
from dataclasses import dataclass
from pathlib import Path

GENE_NAMES = [
    "head_type", "legs_type", "body_type", "tail_type",
    "head_dummy_position", "head_dummy_type",
    "body_dummy_position", "body_dummy_type",
    "tail_dummy_position", "tail_dummy_type",
    "skin_type", "skin_subtype", "carapace_type", "carapace_subtype",
    "diet", "aggressiveness", "stamina", "courage",
]


class GenFormatError(ValueError):
    pass


@dataclass
class Genotype:
    path: str
    name: str
    gene_count: int
    alleles: list[tuple[int, int]]
    documented: bool = False

    def summary(self) -> dict:
        return {
            "path": self.path,
            "name": self.name,
            "genes": self.gene_count,
            "documented": self.documented,
            "alleles": self.alleles,
        }


def read_gen(path: Path) -> Genotype:
    text = path.read_text(encoding="cp1252", errors="replace")
    documented = "//" in text
    data = re.sub(r"//[^\r\n]*", "", text)
    tokens = data.split()
    if not tokens:
        raise GenFormatError(f"{path}: empty")
    try:
        count = int(tokens[0])
    except ValueError as e:
        raise GenFormatError(f"{path}: bad gene count {tokens[0]!r}") from e
    if count <= 0 or count > 128:
        raise GenFormatError(f"{path}: bad gene count {count}")
    if len(tokens) != 1 + count * 2:
        raise GenFormatError(
            f"{path}: expected {1 + count * 2} tokens, got {len(tokens)}"
        )
    values = [int(t) for t in tokens[1:]]
    alleles = [(values[i], values[i + 1]) for i in range(0, len(values), 2)]
    return Genotype(
        path=str(path),
        name=path.stem,
        gene_count=count,
        alleles=alleles,
        documented=documented,
    )


def scan(root: Path) -> tuple[int, list[tuple[str, str]], dict]:
    total = 0
    bad: list[tuple[str, str]] = []
    sums = {"genes": 0, "documented": 0}
    for path in sorted(root.rglob("*.gen")):
        total += 1
        try:
            gen = read_gen(path)
        except Exception as e:
            bad.append((str(path), str(e)))
            continue
        sums["genes"] += gen.gene_count
        sums["documented"] += int(gen.documented)
    return total, bad, sums


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--dump", type=Path)
    ap.add_argument("--scan", type=Path)
    args = ap.parse_args(argv)
    if args.dump:
        gen = read_gen(args.dump)
        data = gen.summary()
        data["genes_named"] = {
            name: pair for name, pair in zip(GENE_NAMES, gen.alleles)
        }
        print(json.dumps(data, indent=2))
        return 0
    if args.scan:
        total, bad, sums = scan(args.scan)
        print(f"scanned {total} genotypes, {len(bad)} bad, {sums['genes']} genes, "
              f"{sums['documented']} documented")
        for path, why in bad[:20]:
            print(f"  FAIL {path}: {why}")
        return 0 if not bad else 1
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
