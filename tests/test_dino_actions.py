"""Dinosaur action chooser oracle (P3).

Mirrors ``FUN_0056e410`` / ``FUN_0056e5d0`` / ``FUN_0056e670``; see
``notes/dino-action-chooser-recon.md``.
"""
from __future__ import annotations

import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from dino_actions import (  # noqa: E402
    NEED_WEIGHTS,
    DinoActionLoop,
    LoopConfig,
    NeedBlock,
    action_score,
    choose,
    load_action_table,
    need_state,
    need_term,
    preconditions_ok,
    score_table,
)

ACTIONS = ROOT / "game" / "%MAINDIR%" / "Data" / "Design" / "attractions" / "actions"

# Retail actions whose only gates/effects involve the four needs and size, so
# their chooser behaviour can be asserted without a world target.
SELF_NODES = ("Exploration", "Eat", "EatForSmallies", "Sleep")


@pytest.fixture(scope="module")
def actions():
    if not (ACTIONS / "ActionList.ini").exists():
        pytest.skip("retail actions missing")
    table = load_action_table(ACTIONS)
    assert len(table) == 64
    return table


def test_need_state_thresholds():
    assert need_state(99) == 3
    assert need_state(90) == 3
    assert need_state(81) == 3
    assert need_state(80) == 0
    assert need_state(61) == 0
    assert need_state(60) == 1
    assert need_state(59) == 1
    assert need_state(31) == 1
    assert need_state(30) == 2
    assert need_state(0) == 2


def test_weight_tables_are_the_retail_values():
    assert NEED_WEIGHTS == (
        (2, 15, 300, 1),
        (1, 10, 60, 1),
        (2, 15, 65, 1),
        (5, 20, 90, 1),
    )


def test_need_term_clamps_and_weights():
    # BAD food: 300 x 30.
    assert need_term(NeedBlock(food=20), 0, 30) == 9000
    # Full food: +30 is clamped to +9, then weight 1.
    assert need_term(NeedBlock(food=90), 0, 30) == 9
    # Negative deltas are not lower-clamped: BAD food x -5.
    assert need_term(NeedBlock(food=0), 0, -5) == -1500
    # MID rest (40): +100 clamps to +59, weight 20.
    assert need_term(NeedBlock(repos=40), 3, 100) == 20 * 59


def test_eat_wins_when_food_is_bad(actions):
    block = NeedBlock(food=20, violence=90, secu=50, repos=90, size=250)
    # Eat carries consume_flag=1: the feeder target supplies the item.
    choice = choose(actions, block, node_names=SELF_NODES, inventory=True)
    assert choice is not None
    assert (choice.name, choice.score) == ("Eat", 9009)


def test_sleep_wins_when_rest_is_bad(actions):
    block = NeedBlock(food=70, violence=90, secu=50, repos=20, size=250)
    choice = choose(actions, block, node_names=SELF_NODES, inventory=True)
    assert choice is not None
    assert choice.name == "Sleep"
    assert choice.score == 15 * -50 + 90 * 79


def test_both_bad_food_wins(actions):
    block = NeedBlock(food=20, violence=90, secu=50, repos=20, size=250)
    choice = choose(actions, block, node_names=SELF_NODES, inventory=True)
    assert choice is not None
    assert (choice.name, choice.score) == ("Eat", 9009)


def test_size_gate_selects_small_eater(actions):
    small = NeedBlock(food=20, violence=90, secu=50, repos=90, size=150)
    large = NeedBlock(food=20, violence=90, secu=50, repos=90, size=250)
    assert choose(actions, small, node_names=SELF_NODES,
                  inventory=True).name == "EatForSmallies"
    assert choose(actions, large, node_names=SELF_NODES,
                  inventory=True).name == "Eat"
    eat = next(a for a in actions if a.name == "Eat")
    assert preconditions_ok(eat, NeedBlock(size=199), inventory=True) is False
    assert preconditions_ok(eat, NeedBlock(size=200), inventory=True) is True
    assert preconditions_ok(eat, NeedBlock(size=200)) is False


def test_full_table_needs_node_restriction(actions):
    """Scoring all 64 actions is not the retail node set.

    ``Victory`` carries ``field_32 = 50000`` and ``Smell`` four +500 need
    deltas, so they outrank Eat when food is bad. Retail only scores the nodes
    present in the dinosaur's behavior graph (perceived targets + usable
    actions), which is why ``choose`` takes ``node_names``.
    """
    block = NeedBlock(food=20, violence=90, secu=50, repos=90, size=250)
    top = score_table(actions, block)
    assert top[0] == ("Victory", 50000)
    assert top[1][0] == "Smell"
    assert top[1][1] == 5000 + 300 * 79 + 1 * 9 + 15 * 49 + 1 * 9
    choice = choose(actions, block, node_names=SELF_NODES, inventory=True)
    assert choice.name == "Eat"


def test_all_needs_full_means_idle(actions):
    """At 99 the +100/+30/+10 deltas clamp to 0; Sleep is negative. No node
    reaches the score >= 1 threshold, so the dinosaur stays idle."""
    assert choose(actions, NeedBlock(), node_names=SELF_NODES,
                  inventory=True) is None


def test_action_loop_timeline(actions):
    """The port loop: Eat -> Sleep -> Eat -> Exploration, matching the C++
    `--dino-actions` timeline. `action_scale=0.1` makes each 30 s action 3 s."""
    loop = DinoActionLoop(
        actions,
        NeedBlock(food=20, violence=90, secu=50, repos=20, size=250),
        LoopConfig(action_scale=0.1),
    )
    assert loop.action_id == 0x1F
    changes: list[tuple[float, str]] = []
    previous = loop.action_name
    t = 0.0
    for _ in range(100):
        loop.tick(0.1)
        t += 0.1
        if loop.action_name != previous:
            changes.append((round(t, 1), loop.action_name))
            previous = loop.action_name
    assert changes[:4] == [
        (0.1, "Eat"),
        (3.1, "Sleep"),
        (6.1, "Eat"),
        (9.1, "Exploration"),
    ]


def test_action_loop_decay(actions):
    loop = DinoActionLoop(actions, NeedBlock())
    loop.tick(10.0)
    assert (loop.needs.food, loop.needs.violence, loop.needs.secu,
            loop.needs.repos) == (94, 97, 99, 94)
    assert loop.action_name == "Exploration"


def test_action_loop_deltas_apply_on_completion(actions):
    loop = DinoActionLoop(
        actions,
        NeedBlock(food=20, violence=90, secu=50, repos=90, size=250),
        LoopConfig(action_scale=0.1),
    )
    loop.tick(0.1)
    assert loop.action_name == "Eat"
    loop.tick(3.0)  # Eat finishes: +30 FOOD, +10 VIOLENCE (clamped)
    assert (loop.needs.food, loop.needs.violence) == (49, 99)


def test_action_loop_starts_idle_when_full(actions):
    loop = DinoActionLoop(actions, NeedBlock())
    loop.tick(0.1)
    assert loop.action_name == "idle"
    assert loop.action_id == 0x1F


def test_action_order_and_field_32(actions):
    assert actions[0].name == "Exploration"
    assert actions[-1].name == "EatBaitForSmallies"
    assert next(a for a in actions if a.name == "Victory").field_32 == 50000
    assert action_score(
        next(a for a in actions if a.name == "Victory"),
        NeedBlock(),
    ) == 50000
