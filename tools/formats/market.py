#!/usr/bin/env python3
"""Reader for the dinosaur store catalog ``Data/Design/Scenario/Market.ini``.

The file includes ``MissionBase.ini`` and carries one ``[DINOSTORE]`` section
with one key per store page (``DinoStore0..6``; page 1 is absent):

```text
[DINOSTORE]
DinoStore0="Allosaurus 9000 allo.gen Allosaurus_Leoparus 10000 allo2.gen ..."
DinoStore2="..."
```

Each store value is a flat list of **triplets**:

```text
<display name> <price> <genotype file>
```

Underscores stand in for spaces in the display names (``Tyrannosaurus_Rex``).
The names are raw strings, not localisation ids.

See ``docs/formats/market.md``.
"""
from __future__ import annotations

import argparse
import json
import re
from dataclasses import dataclass
from pathlib import Path

from overlay import parse_ini

_STORE_RE = re.compile(r"^DINOSTORE(\d+)$")


class MarketError(ValueError):
    pass


@dataclass
class MarketEntry:
    store: int
    name: str
    price: int
    genotype: str

    def summary(self) -> dict:
        return {
            "store": self.store,
            "name": self.name,
            "price": self.price,
            "genotype": self.genotype,
        }


@dataclass
class Market:
    path: str
    entries: list[MarketEntry]

    @property
    def stores(self) -> list[int]:
        return sorted({entry.store for entry in self.entries})

    def by_genotype(self) -> dict[str, MarketEntry]:
        return {entry.genotype: entry for entry in self.entries}

    def summary(self) -> dict:
        return {
            "path": self.path,
            "stores": self.stores,
            "entries": len(self.entries),
            "prices": {
                entry.name: entry.price for entry in self.entries
            },
        }


def read_market(path: Path) -> Market:
    entries: list[MarketEntry] = []
    store_sections = [
        section
        for section in parse_ini(path)
        if section.name.upper() == "DINOSTORE"
    ]
    if not store_sections:
        raise MarketError(f"{path}: no [DINOSTORE] section")
    for section in store_sections:
        keys = sorted(
            section.values,
            key=lambda key: int(_STORE_RE.match(key).group(1)),
        )
        for key in keys:
            match = _STORE_RE.match(key)
            if not match:
                raise MarketError(f"{path}: unexpected key {key}")
            store = int(match.group(1))
            tokens = section.values[key].split()
            if len(tokens) % 3:
                raise MarketError(
                    f"{path}: {key} has {len(tokens)} tokens, not a multiple of 3"
                )
            for index in range(0, len(tokens), 3):
                name, price, genotype = tokens[index:index + 3]
                if not price.isdigit():
                    raise MarketError(f"{path}: bad price {price!r} for {name}")
                if not genotype.lower().endswith(".gen"):
                    raise MarketError(f"{path}: bad genotype {genotype!r}")
                entries.append(
                    MarketEntry(
                        store=store,
                        name=name,
                        price=int(price),
                        genotype=genotype,
                    )
                )
    return Market(path=str(path), entries=entries)


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("path", type=Path)
    args = ap.parse_args(argv)
    print(json.dumps(read_market(args.path).summary(), indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
