#!/usr/bin/env python3
"""Budget ledger rows for the Dino Island ``spEconomy`` save block.

The 14-row budget panel is refreshed by ``0x00510430`` from the *yesterday*
counter block (``+0xc4`` .. ``+0x16c``), not from a single
``spPark+0x1c[14]`` array.  This module mirrors that refresh so a save can be
inspected without the game.

The park block is a 0x30-byte header followed by the economy field stream
written by ``0x00585780`` in **writer order** (budget, sellback, FOOD, staff,
ticket, staff buckets, shop arrays, groups, yesterday block).  The stream is
parsed by ``sav_graph._read_park_stream``.

See ``docs/decomp/economy.md`` and ``docs/formats/sav.md``.
"""
from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from sav_graph import ParkState, _read_park_stream  # noqa: E402

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

#: Uninitialized counters use the debug fill ``0xBAADF00D`` (the three totals
#: are only computed by the panel refresh, and the yesterday block is only
#: written by the first day rollover).
POISON = 0xBAADF00D


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


def _counter(value: int) -> int | None:
    return None if value == POISON else value


def _row(name: str, amount: int, count: int | None, source: str) -> LedgerRow:
    return LedgerRow(name, _counter(amount), None if count is None else _counter(count), source)


def ledger_from_state(park: ParkState) -> Ledger:
    y = park
    rows = [
        _row("FOOD", y.yesterday_food[0], y.yesterday_food[1], "FOOD total/count"),
        _row("EFFECTIFS_FOOD", y.yesterday_staff_buckets[0], None, "role-0 staff bucket + live employee count"),
        _row("EFFECTIFS_SECU", y.yesterday_secu[0], y.yesterday_secu[1], "EFFECTIFS_SECU total/count"),
        _row("DINOACHAT", y.yesterday_totals[6], y.yesterday_totals[5], "DINOACHAT group A total/count"),
        _row("LABOACTION", y.yesterday_totals[10], y.yesterday_totals[9], "LABOACTION group C total/count"),
        _row("CONSTRUCTION", y.yesterday_totals[8], y.yesterday_totals[7], "CONSTRUCTION group B total/count"),
        _row("ENTREES", y.yesterday_totals[0], y.yesterday_totals[1], "ENTREES ticket total/count"),
        _row("RESTOS", y.yesterday_revenue[1], y.yesterday_sales[1], "RESTOS revenue/count (shop kind 1)"),
        _row("BARAQUES", y.yesterday_revenue[2], y.yesterday_sales[2], "BARAQUES revenue/count (shop kind 2)"),
        _row("SOUVENIRS", y.yesterday_revenue[3], y.yesterday_sales[3], "SOUVENIRS revenue/count (shop kind 3)"),
        _row("BOITNOIRE", y.yesterday_revenue[4], y.yesterday_sales[4], "BOITNOIRE revenue/count (shop kind 4)"),
        _row("POINTDEVUE", y.yesterday_revenue[5], y.yesterday_sales[5], "POINTDEVUE revenue/count (shop kind 5)"),
        _row("GRADINS", y.yesterday_revenue[6], y.yesterday_sales[6], "GRADINS revenue/count (shop kind 6)"),
        _row("VENTES", y.yesterday_totals[12], y.yesterday_totals[11], "VENTES generic income total/count"),
    ]
    totals = {
        "expenses": _counter(y.yesterday_totals[2]),
        "income": _counter(y.yesterday_totals[3]),
        "net": _counter(y.yesterday_totals[4]),
    }
    settled = all(r.amount is not None for r in rows) and all(
        value is not None for value in totals.values()
    )
    return Ledger(budget=park.budget, rows=rows, totals=totals, settled=settled)


def ledger(block: bytes) -> Ledger:
    """Decode the 14 budget rows from a serialized park block."""
    try:
        park = _read_park_stream(block, 0)
    except Exception as exc:  # noqa: BLE001 - normalize the parser error
        raise EconomyError(str(exc)) from exc
    return ledger_from_state(park)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sav", type=Path, help="version-53 .sav")
    parser.add_argument("--json", action="store_true", help="machine-readable output")
    args = parser.parse_args(argv)

    from sav_graph import read_sav_graph  # noqa: PLC0415

    graph = read_sav_graph(args.sav)
    if graph.park is None:
        print("no park block in save", file=sys.stderr)
        return 1
    result = ledger_from_state(graph.park)
    if args.json:
        print(json.dumps(result.to_dict(), indent=2))
        return 0
    print(f"budget {result.budget:,}")
    if not result.settled:
        print("  (yesterday/total counters not written yet: 0xBAADF00D fill)")
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
