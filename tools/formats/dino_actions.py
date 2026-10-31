"""Dinosaur action chooser (P3 reference implementation).

Mirrors the retail code recovered in ``notes/dino-action-chooser-recon.md``:

- ``FUN_0056e5d0`` — one need term: upper-clamp the delta, look up a
  per-need weight by the need's GOOD/MID/BAD state, multiply.
- ``FUN_0056e410`` — sum the action's ``.bai`` need deltas, add the node rest
  cost and the optional threat term, apply the node percentage, then add the
  action's ``field_32`` base score.
- ``FUN_0056e670`` — pick the highest score ``>= 1`` among the nodes whose
  preconditions hold (``FUN_0056e0e0``).

The node **set** is a separate input: in the retail game the behavior graph
only contains the actions/targets a dinosaur can currently use. This module
therefore takes an explicit list of action nodes; scoring them is exact, the
availability policy is the caller's.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, Mapping, Sequence

from bai import Bai, read_action_list, read_bai

NEED_NAMES = ("food", "violence", "secu", "repos")

# DAT_00643054 / 0x64 / 0x74 / 0x84 in DinoIsland.exe; index = need state.
NEED_WEIGHTS: tuple[tuple[int, int, int, int], ...] = (
    (2, 15, 300, 1),   # FOOD
    (1, 10, 60, 1),    # VIOLENCE
    (2, 15, 65, 1),    # SECU
    (5, 20, 90, 1),    # REPOS
)

# FUN_00570590 thresholds, descending.
NEED_THRESHOLDS = (80, 60, 30)


def need_state(value: int) -> int:
    """GOOD/MID/BAD/full index from ``FUN_005714b0``.

    ``value > 80`` -> 3 (full), ``60..80`` -> 0, ``30..60`` -> 1,
    ``<= 30`` -> 2. Note the non-monotonic order: 3 is the best state.
    """
    if value > NEED_THRESHOLDS[0]:
        return 3
    if value > NEED_THRESHOLDS[1]:
        return 0
    if value > NEED_THRESHOLDS[2]:
        return 1
    return 2


def clamp_need(value: int) -> int:
    return max(0, min(99, value))


@dataclass
class NeedBlock:
    """The four needs plus the attribute inputs used by preconditions.

    Attribute ids and actor fields come from ``FUN_00572620``. Unknown ids
    return 0, exactly like the retail accessor.
    """

    food: int = 99
    violence: int = 99
    secu: int = 99
    repos: int = 99
    # Food gauge (`dino+0x25c`): max = weight, gauge init max/2.
    max_food: float = 0.0
    food_gauge: float = 0.0
    food_consumption: float = 0.0
    size: int = 300          # attribute 4, actor+0x50 (centimetres)
    size_class: int = 3      # attribute 12, actor+0x98
    aggression: int = 50     # attribute 18, actor+0xbc
    stamina: int = 50        # attribute 19, actor+0xc0
    agility: int = 50        # attribute 29, actor+0x1e0
    aggression_timer: int = 0   # attribute 15, actor+0xb4
    extra: Mapping[int, int] = field(default_factory=dict)

    def need(self, index: int) -> int:
        return (self.food, self.violence, self.secu, self.repos)[index]

    def state(self, index: int) -> int:
        return need_state(self.need(index))

    def attribute(self, attribute: int) -> int:
        if 0 <= attribute <= 3:
            return self.need(attribute)
        values = {
            4: self.size,
            9: self.extra.get(9, 0),
            10: self.extra.get(10, 0),
            11: self.extra.get(11, 0),
            12: self.size_class,
            13: self.extra.get(13, 0),
            14: self.extra.get(14, 0),
            15: self.aggression_timer,
            16: self.extra.get(16, 0),
            18: self.aggression,
            19: self.stamina,
            20: self.extra.get(20, 0),
            22: self.extra.get(22, 0),
            24: self.extra.get(24, 0),
            29: self.agility,
            30: self.extra.get(30, 0),
        }
        return self.extra.get(attribute, values.get(attribute, 0))


def need_term(block: NeedBlock, need: int, delta: int) -> int:
    """``FUN_0056e5d0``: one need-delta score term."""
    if not 0 <= need <= 3:
        return 0
    value = block.need(need)
    if value + delta > 99:
        delta = 99 - value
    weight = NEED_WEIGHTS[need][block.state(need)]
    if delta < 1 and weight < 1:
        return -(weight * delta)
    return weight * delta


def action_score(
    action: Bai,
    block: NeedBlock,
    *,
    node_percent: int = 100,
    node_rest: int = 0,
    threat_score: int = 0,
) -> int:
    """Score one action node exactly like ``FUN_0056e410``.

    ``node_rest`` is the node's ``+0x10`` field (already the raw value that the
    retail code divides by 100); ``threat_score`` is the decoded threat term
    that the retail code divides by 10 before feeding need 2 (SECU).
    """
    score = 0
    for delta in action.need_deltas:
        score += need_term(block, delta.need, delta.delta)
    if node_rest:
        score += need_term(block, 3, int(node_rest / 100))
    if threat_score:
        score += need_term(block, 2, int(threat_score / 10))
    if node_percent != 100 and score > 0:
        score = node_percent * score // 100
    return action.field_32 + score


def preconditions_ok(
    action: Bai,
    block: NeedBlock,
    *,
    inventory: bool = False,
) -> bool:
    """``FUN_0056e0e0``: the ``.bai`` gates plus the consume/item gate.

    The item gate needs the target's vtable index, which is not data-driven
    here; ``inventory`` says whether the caller has the required item.
    """
    for precondition in action.preconditions:
        if not precondition.holds(block.attribute(precondition.attribute)):
            return False
    if action.consume_flag and not inventory:
        return False
    return True


@dataclass(frozen=True)
class Choice:
    index: int
    name: str
    score: int


def choose(
    actions: Sequence[Bai],
    block: NeedBlock,
    *,
    node_names: Iterable[str] | None = None,
    node_percent: int = 100,
    node_rest: int = 0,
    threat_score: int = 0,
    inventory: bool = False,
) -> Choice | None:
    """Return the highest-scoring available action, or ``None`` if idle.

    ``node_names`` limits the candidate set to the actions actually present in
    the dinosaur's behavior graph (retail builds these from usable actions and
    perceived targets). Ties keep the first action, matching the retail
    ``max_element`` pass.
    """
    wanted = None if node_names is None else set(node_names)
    best: Choice | None = None
    for index, action in enumerate(actions):
        if wanted is not None and action.name not in wanted:
            continue
        if not preconditions_ok(action, block, inventory=inventory):
            continue
        score = action_score(
            action,
            block,
            node_percent=node_percent,
            node_rest=node_rest,
            threat_score=threat_score,
        )
        if score < 1:
            continue
        if best is None or score > best.score:
            best = Choice(index=index, name=action.name, score=score)
    return best


def load_action_table(actions_dir: str | Path) -> list[Bai]:
    """Load the 64 ``.bai`` actions in ``ActionList.ini`` order."""
    directory = Path(actions_dir)
    names = read_action_list(directory / "ActionList.ini")
    return [read_bai(directory / f"{name}.bai") for name in names]


def score_table(
    actions: Sequence[Bai],
    block: NeedBlock,
) -> list[tuple[str, int]]:
    """Score every action, for probes/tests. Descending."""
    scored = [(action.name, action_score(action, block)) for action in actions]
    return sorted(scored, key=lambda item: item[1], reverse=True)


@dataclass
class LoopConfig:
    """Port tuning for the need/action loop.

    The decay rates are not calibrated against retail yet; the chooser and the
    action completion rule are the retail algorithm. ``action_scale``
    multiplies the `.bai` 30 s duration (the probe uses 0.1).
    """

    node_names: tuple[str, ...] = (
        "Exploration", "Eat", "EatForSmallies", "Sleep")
    decision_interval: float = 0.5
    food_decay: float = 0.5
    violence_decay: float = 0.2  # unused: the exact 10-point 10 s pass wins
    secu_decay: float = 0.0
    repos_decay: float = 0.5
    action_scale: float = 1.0


class DinoActionLoop:
    """Per-dinosaur need decay + chooser + action countdown.

    Mirrors ``src/world/dino_actions.cpp``. Retail runs the element events
    when the action starts and never applies the `.bai` need-delta table;
    the delta loop was removed after the whole-image writer census
    (``notes/dino-ai-gap-audit.md`` §4.2).
    """

    def __init__(
        self,
        actions: Sequence[Bai],
        block: NeedBlock,
        config: LoopConfig | None = None,
    ) -> None:
        self.actions = list(actions)
        self.needs = block
        self.config = config or LoopConfig()
        self._decay = [0.0, 0.0, 0.0, 0.0]
        self._ten = 0.0
        self._action: int | None = None
        self._elapsed = 0.0
        self._duration = 0.0
        self.last_score = 0
        self.last_needs = (
            block.food,
            block.violence,
            block.secu,
            block.repos,
        )
        # Decide on the first tick so a forced need state shows immediately.
        self._decision = self.config.decision_interval

    @property
    def action_name(self) -> str:
        if self._action is None:
            return "idle"
        return self.actions[self._action].name

    @property
    def action_id(self) -> int:
        return 0x1F if self._action is None else self._action

    @property
    def busy(self) -> bool:
        return self._action is not None

    def tick(self, dt: float) -> None:
        if dt <= 0.0:
            return
        rates = (
            self.config.food_decay,
            self.config.violence_decay,
            self.config.secu_decay,
            self.config.repos_decay,
        )
        axes = ("food", "violence", "secu", "repos")
        for axis, rate in enumerate(rates):
            if axis == 1:
                # `FUN_00571410`: exact 10-point decay on the 10 s pass below.
                self._decay[axis] = 0.0
                continue
            value = self.needs.need(axis)
            if value <= 0 or rate <= 0.0:
                self._decay[axis] = 0.0
                continue
            self._decay[axis] += rate * dt
            while self._decay[axis] >= 1.0 and self.needs.need(axis) > 0:
                setattr(self.needs, axes[axis], self.needs.need(axis) - 1)
                self._decay[axis] -= 1.0
        self._ten += dt
        while self._ten >= 10.0:
            self._ten -= 10.0
            if self.needs.violence > 0:
                self.needs.violence = max(0, self.needs.violence - 10)
            if self.needs.max_food > 0.0:
                amount = 10.0 * self.needs.food_consumption
                gauge = max(0.0, self.needs.food_gauge - amount)
                self.needs.food_gauge = gauge
                self.needs.food = clamp_need(
                    int(gauge * 99.0 / self.needs.max_food)
                )
        if self._action is not None:
            self._elapsed += dt
            if self._elapsed >= self._duration:
                self._finish()
        self._decision += dt
        if self._action is None and self._decision >= self.config.decision_interval:
            self._decide()

    def _decide(self) -> None:
        choice = choose(
            self.actions,
            self.needs,
            node_names=self.config.node_names,
            inventory=True,
        )
        if choice is None:
            return
        self._action = choice.index
        self._elapsed = 0.0
        action = self.actions[choice.index]
        self._duration = max(0.1, action.field_2e * self.config.action_scale)
        self.last_score = choice.score
        self.last_needs = (
            self.needs.food,
            self.needs.violence,
            self.needs.secu,
            self.needs.repos,
        )
        apply_action_events(action, self.needs)
        self._decision = 0.0

    def _finish(self) -> None:
        # Retail does not apply the `.bai` need deltas; the element events
        # already ran in `_decide`.
        self._action = None
        self._elapsed = 0.0
        self._duration = 0.0
        self._decision = self.config.decision_interval


def apply_action_events(action: Bai, block: NeedBlock) -> None:
    """`FUN_00569A00` + `FUN_005727B0`: apply the action element events.

    Ported kinds: 1 = VIOLENCE delta, 25 = REPOS from the rest-gauge percent.
    Other kinds write actor/phenotype stats not modelled by ``NeedBlock``.
    """
    for element in action.effects:
        for event in element.events:
            if event.kind == 1:
                block.violence = clamp_need(block.violence + event.value)
            elif event.kind == 25:
                block.repos = clamp_need(
                    event.percent if event.percent else event.value
                )
