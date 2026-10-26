"""Decoding for the 64 ``.bai`` action definitions."""
from __future__ import annotations

import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from bai import (  # noqa: E402
    HEADER_SIZE,
    BaiFormatError,
    EffectWeight,
    NeedDelta,
    Precondition,
    StatGrant,
    read_action_list,
    read_bai,
    scan,
)

ACTIONS = ROOT / "game" / "%MAINDIR%" / "Data" / "Design" / "attractions" / "actions"


def test_scan_all_bai_files():
    count, parsed, errors = scan(ACTIONS)
    assert count == 64
    assert errors == []
    assert len(parsed) == 64
    for bai in parsed:
        # The parser enforces exact EOF; keep the raw-tail check too.
        assert len(bai.tail) == Path(bai.path).stat().st_size - HEADER_SIZE
        assert bai.tail


def test_tail_totals():
    _, parsed, _ = scan(ACTIONS)
    assert sum(len(b.preconditions) for b in parsed) == 46
    assert sum(len(b.effect_weights) for b in parsed) == 43
    assert sum(len(b.need_deltas) for b in parsed) == 134
    assert sum(len(b.stat_grants) for b in parsed) == 24
    assert sum(len(b.effects) for b in parsed) == 204
    assert sum(len(e.events) for b in parsed for e in b.effects) == 171


def test_eat_flag_covers_four_actions():
    _, parsed, _ = scan(ACTIONS)
    eat = {b.name for b in parsed if b.eat_flag}
    assert eat == {"Eat", "EatBait", "EatForSmallies", "EatBaitForSmallies"}


def test_consume_flag_covers_five_actions():
    _, parsed, _ = scan(ACTIONS)
    consume = {b.name for b in parsed if b.consume_flag}
    assert consume == {
        "Eat",
        "EatBait",
        "EatForSmallies",
        "EatBaitForSmallies",
        "DinoAttackForCarniv",
    }


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


def test_preconditions_use_runtime_order():
    carn = read_bai(ACTIONS / "DinoAttackForCarnivore.bai")
    herb = read_bai(ACTIONS / "DinoAttackForHerbivore.bai")
    assert carn.preconditions == (Precondition(1, 22, 1),)
    assert herb.preconditions == (Precondition(0, 22, 0),)
    assert carn.preconditions[0].describe() == "carnivore >= 1"
    assert herb.preconditions[0].describe() == "carnivore <= 0"
    assert carn.preconditions[0].holds(1)
    assert not carn.preconditions[0].holds(0)
    assert herb.preconditions[0].holds(0)
    assert not herb.preconditions[0].holds(1)


def test_length_and_height_band_preconditions():
    assert read_bai(ACTIONS / "Eat.bai").preconditions == (Precondition(200, 4, 1),)
    assert read_bai(ACTIONS / "EatForSmallies.bai").preconditions == (
        Precondition(200, 4, 0),
    )
    assert read_bai(ACTIONS / "Jump1m_Size1.bai").preconditions == (
        Precondition(200, 4, 0),
    )
    assert read_bai(ACTIONS / "Jump1m_Size2.bai").preconditions == (
        Precondition(201, 4, 1),
        Precondition(620, 4, 0),
    )
    assert read_bai(ACTIONS / "Jump1m_Size3.bai").preconditions == (
        Precondition(621, 4, 1),
        Precondition(950, 4, 0),
    )
    assert read_bai(ACTIONS / "JumpForBiggies.bai").preconditions == (
        Precondition(951, 4, 1),
    )
    assert read_bai(ACTIONS / "ForceField.bai").preconditions == (
        Precondition(250, 5, 1),
    )
    assert read_bai(ACTIONS / "ForceFieldSmall.bai").preconditions == (
        Precondition(250, 5, 0),
    )


def test_effect_weights_use_runtime_order():
    armor = read_bai(ACTIONS / "GetArmor.bai")
    assert armor.effect_weights == (
        EffectWeight(attribute=11, weight=1, unused=20),
    )
    field = read_bai(ACTIONS / "ForceField.bai")
    assert field.effect_weights == (
        EffectWeight(attribute=14, weight=1, unused=50),
        EffectWeight(attribute=20, weight=1, unused=50),
    )


def test_need_deltas():
    eat = read_bai(ACTIONS / "Eat.bai")
    assert eat.need_deltas == (NeedDelta(0, 30), NeedDelta(1, 10))
    sleep = read_bai(ACTIONS / "Sleep.bai")
    assert sleep.need_deltas == (NeedDelta(2, -50), NeedDelta(3, 100))
    bait = read_bai(ACTIONS / "EatBaitForSmallies.bai")
    assert bait.need_deltas == tuple(NeedDelta(i, 80) for i in range(4))


def test_grants_ramp_and_element_events():
    armor = read_bai(ACTIONS / "GetArmor.bai")
    assert armor.stat_grants == (StatGrant(16, 50),)
    assert armor.effect_class == 0
    assert [e.time for e in armor.effects] == [20, 35, 75, 90, 100]
    events = [ev for e in armor.effects for ev in e.events]
    assert [ev.kind for ev in events] == [16] * 5
    assert [ev.value for ev in events] == [10, 20, 30, 40, 50]
    assert all(ev.percent == 100 for ev in events)
    assert all(e.ramp_percent == 100 for e in armor.effects)
    assert all(e.ramp_value == 0.0 for e in armor.effects)


def test_jump_and_smell_ramp_params():
    jump = read_bai(ACTIONS / "Jump1m_Size1.bai")
    assert [e.ramp_percent for e in jump.effects] == [50, 75, 25, 25, 5]
    assert [e.ramp_value for e in jump.effects] == [10.0, 5.0, 20.0, 20.0, 20.0]
    assert jump.effects[0].floats == (-20.0, 15.0, 20.0, -10.0, -20.0)

    smell = read_bai(ACTIONS / "Smell.bai")
    (element,) = smell.effects
    assert element.ramp_percent == 1
    assert element.ramp_value == pytest.approx(30.0)
    assert element.floats == (2.0, 2.0, 2.0, 2.0, 2.0)


def test_element_bones_and_classes():
    _, parsed, _ = scan(ACTIONS)
    for bai in parsed:
        for element in bai.effects:
            assert len(element.params) == 9
            assert element.element_class in {0, 1, 2}
            assert all(0 <= bone < 80 for bone in element.bones)
            assert element.time >= 0
            assert element.duration >= 0
    classes = {
        bai.name: bai.effects[0].element_class_name
        for bai in parsed
        if bai.effects
    }
    assert classes["GetInvincibility"] == "invincibility"
    assert classes["Fall"] == "fall"
    assert classes["JumpHole"] == "fall"


def test_effect_class_values():
    _, parsed, _ = scan(ACTIONS)
    by_name = {bai.name: bai.effect_class for bai in parsed}
    assert by_name["Eat"] == 0
    assert by_name["KickTheirAss"] == 1
    assert by_name["MineExplosion"] == 2
    assert by_name["Teleport"] == 3
    # ``GetJump`` (action 18) has no pending-effect class; the jump family
    # stores class 5 in the tail.  ``FUN_00569660`` uses the separate
    # ``special_action`` group for the obstacle-attack branch.
    assert by_name["GetJump"] == 0
    assert by_name["Jump1m_Size1"] == 5
    assert by_name["LinkObstAttackSize1"] == 5


def test_event_kinds_are_effect_ids():
    """Event kinds select ``FUN_005727b0`` stat blocks, not action ids."""
    jump = read_bai(ACTIONS / "GetJump.bai")
    events = [ev for e in jump.effects for ev in e.events]
    assert {ev.kind for ev in events} == {9}
    assert {ev.label for ev in events} == {"jump"}
    assert [ev.value for ev in events] == [10, 20, 30, 40, 50]

    # ``GetQuad`` scales the current five reward weights by 125..400 %, which
    # proves the third field is a percentage, not a probability.
    quad = read_bai(ACTIONS / "GetQuad.bai")
    percents = [ev.percent for e in quad.effects for ev in e.events]
    assert percents == [125, 150, 200, 300, 400]
    assert all(ev.value == 0 for e in quad.effects for ev in e.events)


def test_attribute_labels_match_get_attribute():
    from bai import ATTRIBUTE_LABELS

    assert ATTRIBUTE_LABELS[4] == "length"
    assert ATTRIBUTE_LABELS[5] == "height"
    assert ATTRIBUTE_LABELS[7] == "length_f"
    assert ATTRIBUTE_LABELS[8] == "height_f"
    assert ATTRIBUTE_LABELS[13] == "weight_total"
    assert ATTRIBUTE_LABELS[14] == "weight_2"


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


def test_rejects_truncated_tail(tmp_path):
    data = (ACTIONS / "Eat.bai").read_bytes()
    short = tmp_path / "Eat.bai"
    short.write_bytes(data[:-4])
    with pytest.raises(BaiFormatError, match="truncated"):
        read_bai(short)


def test_rejects_trailing_bytes(tmp_path):
    data = (ACTIONS / "Eat.bai").read_bytes()
    long = tmp_path / "Eat.bai"
    long.write_bytes(data + b"\x00")
    with pytest.raises(BaiFormatError, match="trailing"):
        read_bai(long)
