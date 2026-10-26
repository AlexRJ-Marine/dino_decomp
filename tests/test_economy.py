"""Budget-ledger decoding from the serialized ``spEconomy`` block."""
from __future__ import annotations

import struct
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from economy import ROWS, EconomyError, ledger  # noqa: E402

SLOT = ROOT / "run" / "save" / "slot001.sav"
BLOCK_SIZE = 0x293
STREAM = 0x30
POISON = 0xBAADF00D


def build_park_stream(
    *,
    budget: int = 75_000,
    sellback: float = 1.0,
    food=(0, 0),
    secu=(0, 0),
    ticket=(0, 0),
    staff=(0, 0),
    revenue=(0,) * 13,
    sales=(0,) * 13,
    groups=((0, 0), (0, 0), (0, 0), (0, 0)),
    yesterday_staff=(0, 0),
    yesterday_food=(0, 0),
    yesterday_secu=(0, 0),
    yesterday_revenue=(0,) * 13,
    yesterday_sales=(0,) * 13,
    yesterday_totals=(0,) * 13,
    cumulative_staff=(0, 0),
    cumulative_food=(0, 0),
    cumulative_secu=(0, 0),
    cumulative_revenue=(0,) * 13,
    cumulative_sales=(0,) * 13,
    cumulative_totals=(0,) * 13,
) -> bytes:
    """Build a block using the writer order of ``0x00585780``."""
    block = bytearray(BLOCK_SIZE)
    pos = STREAM

    def u32(*values: int) -> None:
        nonlocal pos
        for value in values:
            struct.pack_into("<I", block, pos, value)
            pos += 4

    def u8(*values: int) -> None:
        nonlocal pos
        for value in values:
            block[pos] = value
            pos += 1

    u32(budget)
    struct.pack_into("<f", block, pos, sellback)
    pos += 4
    u32(*food)
    u32(*secu)
    u32(*ticket)
    u8(0, 0)
    u32(*staff)
    u32(*revenue)
    u32(*sales)
    for count, total in groups:
        u32(count, total)
    u32(*yesterday_staff)
    u32(*yesterday_food)
    u32(*yesterday_secu)
    u32(*yesterday_revenue)
    u32(*yesterday_sales)
    u32(*yesterday_totals)
    u32(*cumulative_staff)
    u32(*cumulative_food)
    u32(*cumulative_secu)
    u32(*cumulative_revenue)
    u32(*cumulative_sales)
    u32(*cumulative_totals)
    return bytes(block)


def test_ledger_rows_match_panel_refresh():
    block = build_park_stream(
        budget=75_125,
        ticket=(125, 2),
        staff=(11, 12),
        revenue=(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13),
        sales=(101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113),
        groups=((17, 16), (19, 18), (21, 20), (51, 50)),
        yesterday_staff=(13, 14),
        yesterday_food=(15, 16),
        yesterday_secu=(17, 18),
        yesterday_revenue=tuple(range(30, 43)),
        yesterday_sales=tuple(range(40, 53)),
        yesterday_totals=(
            22, 23, 60, 61, 1, 16, 17, 20, 21, 18, 19, 51, 50
        ),
    )
    result = ledger(block)
    assert result.budget == 75_125
    assert result.settled is True
    assert [r.name for r in result.rows] == list(ROWS)
    by_name = {r.name: r for r in result.rows}
    assert (by_name["FOOD"].amount, by_name["FOOD"].count) == (15, 16)
    assert (by_name["EFFECTIFS_FOOD"].amount, by_name["EFFECTIFS_FOOD"].count) == (13, None)
    assert (by_name["EFFECTIFS_SECU"].amount, by_name["EFFECTIFS_SECU"].count) == (17, 18)
    assert (by_name["DINOACHAT"].amount, by_name["DINOACHAT"].count) == (17, 16)
    assert (by_name["LABOACTION"].amount, by_name["LABOACTION"].count) == (19, 18)
    assert (by_name["CONSTRUCTION"].amount, by_name["CONSTRUCTION"].count) == (21, 20)
    assert (by_name["ENTREES"].amount, by_name["ENTREES"].count) == (22, 23)
    assert (by_name["RESTOS"].amount, by_name["RESTOS"].count) == (31, 41)
    assert (by_name["BARAQUES"].amount, by_name["BARAQUES"].count) == (32, 42)
    assert (by_name["SOUVENIRS"].amount, by_name["SOUVENIRS"].count) == (33, 43)
    assert (by_name["BOITNOIRE"].amount, by_name["BOITNOIRE"].count) == (34, 44)
    assert (by_name["POINTDEVUE"].amount, by_name["POINTDEVUE"].count) == (35, 45)
    assert (by_name["GRADINS"].amount, by_name["GRADINS"].count) == (36, 46)
    assert (by_name["VENTES"].amount, by_name["VENTES"].count) == (50, 51)
    assert result.totals == {"expenses": 60, "income": 61, "net": 1}


def test_cumulative_block_is_separate_from_yesterday():
    block = build_park_stream(
        yesterday_food=(10, 1),
        yesterday_totals=(0, 0, 100, 200, 100) + (0,) * 8,
        cumulative_food=(999, 7),
        cumulative_revenue=tuple(range(100, 113)),
        cumulative_sales=tuple(range(200, 213)),
        cumulative_totals=(1, 2, 3, 4, 5) + (0,) * 8,
    )
    from sav_graph import _read_park_stream

    park = _read_park_stream(block, 0)
    assert park.yesterday_food == (10, 1)
    assert park.cumulative_food == (999, 7)
    assert park.cumulative_revenue == tuple(range(100, 113))
    assert park.cumulative_sales == tuple(range(200, 213))
    assert park.stream_end == 0x24A
    result = ledger(block)
    # The panel reads the yesterday block only.
    assert (result.rows[0].amount, result.rows[0].count) == (10, 1)


def test_ledger_marks_poison_totals_unsettled():
    block = build_park_stream(yesterday_totals=(0, 0, POISON, POISON, POISON) + (0,) * 8)
    result = ledger(block)
    assert result.settled is False
    assert all(value is None for value in result.totals.values())


def test_ledger_rejects_short_block():
    with pytest.raises(EconomyError):
        ledger(b"\0" * 0x100)


def test_retail_save_ticket_pair():
    if not SLOT.exists():
        pytest.skip("retail save not present (run the patched game first)")
    from sav_graph import read_sav_graph

    graph = read_sav_graph(SLOT)
    assert graph.park is not None
    park = graph.park
    assert park.budget == 75_125
    assert (park.ticket_total, park.ticket_count) == (125, 2)
    assert park.staff_buckets == (0, 0)
    assert park.stream_end == 0x24A
    assert len(park.tail) == 0x49
    assert park.cumulative_totals[2:5] == (0xBAADF00D,) * 3
    result = ledger(park.raw)
    assert result.budget == 75_125
    # A fresh park has never rolled a day: the yesterday rows are zero and the
    # three totals are still the 0xBAADF00D debug fill.
    assert result.settled is False
    assert all(row.amount in (0, None) for row in result.rows)
    assert all(row.count in (None, 0) for row in result.rows)
    assert all(value is None for value in result.totals.values())
