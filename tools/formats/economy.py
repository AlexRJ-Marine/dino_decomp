#!/usr/bin/env python3
"""Budget ledger rows for the Dino Island ``spEconomy`` save block.

The 14-row budget panel is refreshed by ``0x00510430`` from the *yesterday*
counter block (``+0xc4`` .. ``+0x16c``), not from a single
``spPark+0x1c[14]`` array.  This module mirrors that refresh so a save can be
inspected without the game.

Offsets are relative to the serialized economy object, which starts at
``park_block + 0x28`` in a version-53 ``.sav`` (``sav_graph.ParkState.raw``).

See ``docs/decomp/economy.md``.
"""
from __future__ import annotations

import argparse
import json
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

BASE = 0x28

#: Panel rows, in the widget order built by ``0x0050fd30``.
ROWS = (
    "FOOD",
    "EFFECTIFS_FOOD",
    "EFFECTIFS_SECU",
    "DINOACHAT",
    "LABOACTION",
    "CONSTRUCTION",
    "ENTREES",
    "RESTOS",
    "BARAQUES",
    "SOUVENIRS",
    "BOITNOIRE",
    "POINTDEVUE",
    "GRADINS",
    "VENTES",
)

#: (amount offset, count offset) read by the refresh ``0x00510430``.
ROW_SOURCES = (
    (0xC4, 0xC8, "FOOD total/count"),
    (0xBC, None, "role-0 staff bucket + live employee count"),
    (0xCC, 0xD0, "EFFECTIFS_SECU total/count"),
    (0x154, 0x150, "DINOACHAT group A total/count"),
    (0x164, 0x160, "LABOACTION group C total/count"),
    (0x15C, 0x158, "CONSTRUCTION group B total/count"),
    (0x13C, 0x140, "ENTREES ticket total/count"),
    (0xD8, 0x10C, "RESTOS revenue/count (shop kind 1)"),
    (0xDC, 0x110, "BARAQUES revenue/count (shop kind 2)"),
    (0xE0, 0x114, "SOUVENIRS revenue/count (shop kind 3)"),
    (0xE4, 0x118, "BOITNOIRE revenue/count (shop kind 4)"),
    (0xE8, 0x11C, "POINTDEVUE revenue/count (shop kind 5)"),
    (0xEC, 0x120, "GRADINS revenue/count (shop kind 6)"),
    (0x16C, 0x168, "VENTES generic income total/count"),
)

TOTALS = {
    "expenses": 0x144,
    "income": 0x148,
    "net": 0x14C,
}

#: Retail saves use the debug fill pattern ``0xF00Dxxxx`` for counters that
#: have never been written.  The yesterday block is only filled by the first
#: day rollover, so a fresh save contains ``0xF00D0000`` / ``0xF00DBAAD``.
POISON_PREFIX = 0xF00D


class EconomyError(ValueError):
    pass


@dataclass
class LedgerRow:
    name: str
    amount: int | None
    count: int | None
    source: str

    def to_dict(self) -> dict:
        return {
            "row": self.name,
            "amount": self.amount,
            "count": self.count,
            "source": self.source,
        }


@dataclass
class Ledger:
    budget: int
    rows: list[LedgerRow]
    totals: dict[str, int | None]
    settled: bool

    def to_dict(self) -> dict:
        return {
            "budget": self.budget,
            "settled": self.settled,
            "rows": [r.to_dict() for r in self.rows],
            "totals": dict(self.totals),
        }


def _raw_u32(block: bytes, offset: int, what: str) -> int:
    pos = BASE + offset
    if pos + 4 > len(block):
        raise EconomyError(f"{what}: offset 0x{pos:x} outside {len(block)}-byte block")
    return struct.unpack_from("<I", block, pos)[0]


def _is_poison(value: int) -> bool:
    # 0xF00D0000 / 0xF00DBAAD fill patterns, and arithmetic on them (the
    # totals are computed as income - expenses, so poison - poison = 0xBAAD).
    return value >> 16 == POISON_PREFIX or (value & 0xFFFF) == 0xBAAD


def _counter(block: bytes, offset: int, what: str) -> int | None:
    value = _raw_u32(block, offset, what)
    return None if _is_poison(value) else value


def ledger(block: bytes) -> Ledger:
    """Decode the 14 budget rows from a serialized economy block.

    ``EFFECTIFS_FOOD``'s count is a live employee count in the game, not a
    save counter, so it is always ``None``.  Rows that still contain the
    ``0xF00Dxxxx`` fill pattern are reported as ``None`` and clear
    ``settled``; the yesterday block is only written by the first day
    rollover.
    """
    if len(block) < BASE + 0x150:
        raise EconomyError(f"economy block too short: {len(block)} bytes")
    rows: list[LedgerRow] = []
    settled = True
    for name, (amount_off, count_off, source) in zip(ROWS, ROW_SOURCES):
        amount = _counter(block, amount_off, f"{name} amount")
        count = None if count_off is None else _counter(block, count_off, f"{name} count")
        if amount is None or (count_off is not None and count is None):
            settled = False
        rows.append(LedgerRow(name, amount, count, source))
    totals = {name: _counter(block, off, name) for name, off in TOTALS.items()}
    if any(value is None for value in totals.values()):
        settled = False
    return Ledger(
        budget=_raw_u32(block, 0x08, "budget"),
        rows=rows,
        totals=totals,
        settled=settled,
    )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sav", type=Path, help="version-53 .sav")
    parser.add_argument("--json", action="store_true", help="machine-readable output")
    args = parser.parse_args(argv)

    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from sav_graph import read_sav_graph  # noqa: PLC0415

    graph = read_sav_graph(args.sav)
    if graph.park is None:
        print("no park block in save", file=sys.stderr)
        return 1
    result = ledger(graph.park.raw)
    if args.json:
        print(json.dumps(result.to_dict(), indent=2))
        return 0
    print(f"budget {result.budget:,}")
    if not result.settled:
        print("  (yesterday/total counters not written yet: 0xF00D fill pattern)")
    for row in result.rows:
        amount = "-" if row.amount is None else f"{row.amount:,}"
        count = "" if row.count is None else f"  x{row.count:,}"
        print(f"  {row.name:<16} {amount:>12}{count}")
    for name, value in result.totals.items():
        text = "-" if value is None else f"{value:,}"
        print(f"  {name:<16} {text:>12}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
