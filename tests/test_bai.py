"""Header decoding for the 64 ``.bai`` action definitions."""
from __future__ import annotations

import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from bai import HEADER_SIZE, BaiFormatError, read_action_list, read_bai, scan  # noqa: E402

ACTIONS = ROOT / "game" / "%MAINDIR%" / "Data" / "Design" / "attractions" / "actions"


def test_scan_all_bai_files():
    count, parsed, errors = scan(ACTIONS)
    assert count == 64
    assert errors == []
    assert len(parsed) == 64
    for bai in parsed:
        assert len(bai.tail) == Path(bai.path).stat().st_size - HEADER_SIZE


def test_eat_flag_covers_four_actions():
    _, parsed, _ = scan(ACTIONS)
    eat = {b.name for b in parsed if b.eat_flag}
    assert eat == {"Eat", "EatBait", "EatForSmallies", "EatBaitForSmallies"}


def test_header_value_sets():
    _, parsed, _ = scan(ACTIONS)
    assert {round(b.field_18, 4) for b in parsed} == {
        0.0, 0.1, 1.0, 1.25, 1.4, 1.5, 1.7, 2.0, 2.5, 3.0, 3.1
    }
    assert {v for b in parsed for v in b.params} <= set(range(10, 801))


def test_eat_header_and_params():
    bai = read_bai(ACTIONS / "Eat.bai")
    assert bai.name == "Eat"
    assert bai.group == "normal"
    assert bai.field_14 == 0
    assert bai.field_18 == pytest.approx(3.1)
    assert bai.field_1c == 3
    assert bai.flag_20 == 0
    assert bai.special_action == 0
    assert bai.eat_flag == 1
    assert bai.field_26 == 50
    assert bai.field_2a == pytest.approx(30.0)
    assert bai.field_2e == pytest.approx(30.0)
    assert bai.field_32 == 0
    assert bai.params == (100,) * 8


def test_sleep_params_and_special_group():
    bai = read_bai(ACTIONS / "Sleep.bai")
    assert bai.group == "sleep"
    assert bai.special_action == 3
    assert bai.params == (10,) * 7 + (100,)


def test_danse_flag_20():
    bai = read_bai(ACTIONS / "Danse.bai")
    assert bai.flag_20 == 1
    assert bai.special_action == 0


def test_victory_threshold():
    bai = read_bai(ACTIONS / "Victory.bai")
    assert bai.group == "victory"
    assert bai.special_action == 6
    assert bai.field_32 == 50_000


def test_reach_waypoint_special_values():
    bai = read_bai(ACTIONS / "ReachWaypoint.bai")
    assert bai.field_2a == pytest.approx(10.0)
    assert bai.special_action == 0
    assert bai.field_26 == 60
    assert bai.field_32 == 800


def test_long_names_are_truncated_in_the_file():
    carn = read_bai(ACTIONS / "DinoAttackForCarnivore.bai")
    herb = read_bai(ACTIONS / "DinoAttackForHerbivore.bai")
    assert carn.name == "DinoAttackForCarniv"
    assert herb.name == "DinoAttackForHerbiv"
    assert carn.group == herb.group == "dino attack"


def test_action_list_covers_every_file():
    names = read_action_list(ACTIONS / "ActionList.ini")
    assert len(names) == 64
    assert names[:6] == ["Exploration", "RunAway", "Sleep", "Victory", "Eat", "Fall"]
    files = {p.stem for p in ACTIONS.glob("*.bai")}
    assert set(names) == files


def test_rejects_short_file(tmp_path):
    short = tmp_path / "Short.bai"
    short.write_bytes(b"Short\0" + b"\0" * 10)
    with pytest.raises(BaiFormatError):
        read_bai(short)
