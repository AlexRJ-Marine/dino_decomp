"""Budget-ledger decoding from the serialized ``spEconomy`` block."""
from __future__ import annotations

import struct
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from economy import BASE, ROWS, EconomyError, ledger  # noqa: E402

SLOT = ROOT / "run" / "save" / "slot001.sav"


def _make_block() -> bytearray:
    block = bytearray(0x293)
    struct.pack_into("<I", block, BASE + 0x08, 75_125)
    for i, (amount, count) in enumerate(
        zip(range(1, 15), range(101, 115))
    ):
        struct.pack_into("<I", block, BASE + 0x24 + i * 4, amount)
        struct.pack_into("<I", block, BASE + 0x58 + i * 4, count)
    # yesterday block, as read by 0x00510430
    struct.pack_into("<I", block, BASE + 0xC4, 11)
    struct.pack_into("<I", block, BASE + 0xC8, 12)
    struct.pack_into("<I", block, BASE + 0xBC, 13)
    struct.pack_into("<I", block, BASE + 0xCC, 14)
    struct.pack_into("<I", block, BASE + 0xD0, 15)
    struct.pack_into("<I", block, BASE + 0x154, 16)
    struct.pack_into("<I", block, BASE + 0x150, 17)
    struct.pack_into("<I", block, BASE + 0x164, 18)
    struct.pack_into("<I", block, BASE + 0x160, 19)
    struct.pack_into("<I", block, BASE + 0x15C, 20)
    struct.pack_into("<I", block, BASE + 0x158, 21)
    struct.pack_into("<I", block, BASE + 0x13C, 22)
    struct.pack_into("<I", block, BASE + 0x140, 23)
    for i in range(6):
        struct.pack_into("<I", block, BASE + 0xD8 + i * 4, 30 + i)
        struct.pack_into("<I", block, BASE + 0x10C + i * 4, 40 + i)
    struct.pack_into("<I", block, BASE + 0x16C, 50)
    struct.pack_into("<I", block, BASE + 0x168, 51)
    struct.pack_into("<I", block, BASE + 0x144, 60)
    struct.pack_into("<I", block, BASE + 0x148, 61)
    struct.pack_into("<I", block, BASE + 0x14C, 1)
    return block


def test_ledger_rows_match_panel_refresh():
    result = ledger(bytes(_make_block()))
    assert result.budget == 75_125
    assert result.settled is True
    assert [r.name for r in result.rows] == list(ROWS)
    by_name = {r.name: r for r in result.rows}
    assert (by_name["FOOD"].amount, by_name["FOOD"].count) == (11, 12)
    assert (by_name["EFFECTIFS_FOOD"].amount, by_name["EFFECTIFS_FOOD"].count) == (13, None)
    assert (by_name["EFFECTIFS_SECU"].amount, by_name["EFFECTIFS_SECU"].count) == (14, 15)
    assert (by_name["DINOACHAT"].amount, by_name["DINOACHAT"].count) == (16, 17)
    assert (by_name["LABOACTION"].amount, by_name["LABOACTION"].count) == (18, 19)
    assert (by_name["CONSTRUCTION"].amount, by_name["CONSTRUCTION"].count) == (20, 21)
    assert (by_name["ENTREES"].amount, by_name["ENTREES"].count) == (22, 23)
    assert (by_name["RESTOS"].amount, by_name["RESTOS"].count) == (30, 40)
    assert (by_name["BARAQUES"].amount, by_name["BARAQUES"].count) == (31, 41)
    assert (by_name["SOUVENIRS"].amount, by_name["SOUVENIRS"].count) == (32, 42)
    assert (by_name["BOITNOIRE"].amount, by_name["BOITNOIRE"].count) == (33, 43)
    assert (by_name["POINTDEVUE"].amount, by_name["POINTDEVUE"].count) == (34, 44)
    assert (by_name["GRADINS"].amount, by_name["GRADINS"].count) == (35, 45)
    assert (by_name["VENTES"].amount, by_name["VENTES"].count) == (50, 51)
    assert result.totals == {"expenses": 60, "income": 61, "net": 1}


def test_ledger_rejects_short_block():
    with pytest.raises(EconomyError):
        ledger(b"\0" * 0x100)


def test_retail_save_fresh_park_is_zero():
    if not SLOT.exists():
        pytest.skip("retail save not present (run the patched game first)")
    from sav_graph import read_sav_graph

    graph = read_sav_graph(SLOT)
    assert graph.park is not None
    result = ledger(graph.park.raw)
    assert result.budget == 75_125
    # A fresh park has never rolled a day, so the yesterday block is untouched
    # (0x00 or the 0xF00Dxxxx debug fill); totals are computed lazily by the
    # panel refresh and are still poison in the save.
    assert result.settled is False
    assert all(row.amount in (0, None) for row in result.rows)
    by_name = {r.name: r for r in result.rows}
    assert by_name["ENTREES"].count is None
    assert all(row.count in (None, 0) for row in result.rows)
    assert set(result.totals) == {"expenses", "income", "net"}
    assert all(value is None for value in result.totals.values())
