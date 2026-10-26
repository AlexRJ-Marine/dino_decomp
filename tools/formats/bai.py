#!/usr/bin/env python3
"""Reader for Dino Island ``.bai`` action definitions.

64 files live in ``Data/Design/attractions/actions`` and are loaded in
``ActionList.ini`` order by ``FUN_0053e060`` into a 0xac-byte runtime record.
This module decodes the packed header and the whole variable tail.

File layout (little-endian, packed -- several fields are not 4-byte aligned)::

    0x00  char name[20]      # truncated to 19 chars + NUL for long names
    0x14  u32  field_14      # runtime +0x00: element-score random range
    0x18  f32  field_18      # runtime +0x04: base radius/scale
    0x1c  u32  field_1c      # runtime +0x84: radius scale mode
    0x20  u8   flag_20       # runtime +0x2c
    0x21  u32  special_action  # runtime +0x60; 0 normal, 1..6 special groups
    0x25  u8   eat_flag      # runtime +0x2e
    0x26  u32  field_26        # runtime +0x30; 50 in most files, 60 ReachWaypoint
    0x2a  f32  field_2a        # runtime +0x34; 30.0 in 63/64, 10.0 ReachWaypoint
    0x2e  f32  field_2e        # runtime +0x38; 30.0 in all 64 files
    0x32  u32  field_32        # runtime +0x08
    0x36  u32  params[8]       # runtime +0x0c..+0x28
    0x56  u8   consume_flag    # runtime +0x2d
    0x57  u32  n_pre;    n_pre    x (attribute, comparison, threshold)
          u32  n_weight; n_weight x (attribute, unused, weight)
          u32  n_need;   n_need   x (need, i32 delta)
          u32  n_grant;  n_grant  x (attribute, value)
          u32  effect_class
          u32  n_elem;   n_elem   x element

The loader rotates every 8/12-byte array while pushing it into the runtime
vectors (``0x0053e648`` preconditions, ``0x0053e70e`` weights, ``0x0053e7cf``
need deltas, ``0x0053e877`` grants).  The dataclasses expose the **runtime**
order::

    file (attribute, comparison, threshold) -> runtime (threshold, attribute, comparison)
    file (attribute, unused, weight)        -> runtime (unused, weight, attribute)
    file (need, i32 delta)                  -> runtime (delta, need)
    file (attribute, value)                 -> runtime (value, attribute)

Effect element (0x58-byte runtime record; the file stores the fields in a
different order)::

    u32   time             # selection threshold, not a playback time
    u32   n_bones;  u32 bone[n_bones]
    f32   size
    u32   n_texture; char texture[n_texture]
    u32   n_events; n_events x (kind, i32 value, percent)
    u32   params[9]        # class, power, 5 floats, ramp percent, ramp value (f32)

``kind`` in an element event selects a stat-effect block in
``FUN_005727b0`` (1 violence, 9 jump, 10 perception, 11 intelligence,
13 five weights, 16 armor, 20 courage, 23 food gauge, 24 stun, 25 rest gauge,
26 scale fields, 27 body length/height, 28 flag).  It is **not** an
``ActionList.ini`` action id, although several values coincide.  The third
field is a percentage of the actor's current value: the handler computes
``value + current * percent / 100`` and clamps it.

The tail is self-describing: every count is stored, so a parse must end
exactly at EOF.  All 64 retail files satisfy that invariant.

See ``docs/formats/bai.md``.
"""
from __future__ import annotations

import argparse
import json
import struct
from dataclasses import dataclass
from pathlib import Path

NAME_SIZE = 20
HEADER_SIZE = 0x56
PARAM_COUNT = 8
ELEMENT_PARAM_COUNT = 9

#: ``special_action`` values; matches the "special action" groups in
#: ActionList.ini (Exploration, RunAway, Sleep, attacks, Victory).
ACTION_GROUPS = {
    0: "normal",
    1: "exploration",
    2: "runaway",
    3: "sleep",
    4: "dino attack",
    5: "obstacle attack",
    6: "victory",
}

#: Need slots used by ``need_deltas``; order matches ``saDinosaur``
#: ``+0x244 + 4*i`` (see ``docs/decomp/dinosaur-actions.md``).
NEED_LABELS = {
    0: "food",
    1: "violence",
    2: "security",
    3: "rest",
}

#: Attribute ids accepted by ``saDinosaur::GetAttribute`` (``FUN_00572620``,
#: jump table at ``0x00572730``).  The offset is the actor field the function
#: reads.  Ids 4/5 are the integer size-class stats used by the ``.bai``
#: length/height preconditions; ids 7/8 are the rounded float body dimensions.
ATTRIBUTE_LABELS = {
    0: "food",          # +0x244
    1: "violence",      # +0x248
    2: "security",      # +0x24c
    3: "rest",          # +0x250
    4: "length",        # +0x50, integer cm size class
    5: "height",        # +0x64, integer cm size class
    6: "field_68",      # +0x68
    7: "length_f",      # f32 +0x268, rounded
    8: "height_f",      # f32 +0x26c, rounded
    9: "jump",          # +0x264
    10: "perception",   # +0x280
    11: "intelligence", # +0x284
    12: "size",         # +0x98
    13: "weight_total", # +0x29c, sum of the five weights
    14: "weight_2",     # +0x28c, second of the five +0x288..+0x298 weights
    15: "field_b4",     # +0xb4
    16: "armor",        # +0x270
    18: "aggression",   # +0xbc
    19: "stamina",      # +0xc0
    20: "courage",      # +0x2a4
    22: "carnivore",    # +0x200
    23: "food_gauge",   # f32 +0x25c
    24: "stun",         # int16 +0x16
    29: "agility",      # +0x1e0
    30: "flag_528",     # +0x528 != 0
}

#: Element-event kinds select a stat-effect block in ``FUN_005727b0``.  They
#: are effect ids, not ``ActionList.ini`` action ids (values 9/10/11/13/... can
#: coincide with action indices, which made the old naming misleading).
EVENT_LABELS = {
    1: "violence",
    9: "jump",
    10: "perception",
    11: "intelligence",
    13: "five_weights",
    16: "armor",
    20: "courage",
    23: "food_gauge",
    24: "stun",
    25: "rest_gauge",
    26: "scale_fields",
    27: "body_length_height",
    28: "flag_2a8",
}

#: ``effect_class`` (runtime ``+0x88``).  A non-zero class makes
#: ``FUN_00569660`` push ``{class, actor, param_4, element_index}`` into the
#: park-behavior pending-effect queue (``DAT_006756d4``).  Classes 1/2/3 belong
#: to ``KickTheirAss``/``MineExplosion``/``Teleport``; class 5 covers the jump
#: and linked-obstacle family and is filtered specially by
#: ``FUN_0056d980``/``FUN_0056db70``.
EFFECT_CLASSES = {
    0: "none",
    1: "fight",
    2: "explosion",
    3: "teleport",
    5: "jump_or_link",
}

#: Element class (element ``+0x54`` / ``params[0]``).
ELEMENT_CLASSES = {
    0: "none",
    1: "invincibility",
    2: "fall",
}


class BaiFormatError(ValueError):
    pass


@dataclass(frozen=True)
class Precondition:
    """One ``CanDoAction`` gate (runtime order: threshold, attribute, comparison).

    ``comparison`` 0 requires ``attribute <= threshold``, 1 requires
    ``attribute >= threshold``; any other value is unconstrained.  This is the
    exact test in ``FUN_0056e0e0``.
    """

    threshold: int
    attribute: int
    comparison: int

    @property
    def label(self) -> str:
        return ATTRIBUTE_LABELS.get(self.attribute, f"attribute_{self.attribute}")

    def holds(self, value: int) -> bool:
        if self.comparison == 0:
            return value <= self.threshold
        if self.comparison == 1:
            return value >= self.threshold
        return True

    def describe(self) -> str:
        op = {0: "<=", 1: ">="}.get(self.comparison, "?")
        return f"{self.label} {op} {self.threshold}"


@dataclass(frozen=True)
class EffectWeight:
    """One term of the effect-element score (runtime order: unused, weight, attribute).

    ``FUN_00569660`` computes ``sum(GetAttribute(attribute) * weight)`` and adds
    ``rand() % action.field_14``.  The first file dword (``unused``) is copied
    into runtime ``+0x54`` but is not read by the scorer.
    """

    attribute: int
    weight: int
    unused: int

    @property
    def label(self) -> str:
        return ATTRIBUTE_LABELS.get(self.attribute, f"attribute_{self.attribute}")


@dataclass(frozen=True)
class NeedDelta:
    """Satisfaction change applied while scoring the action (file: need, delta).

    The loader rotates the pair to ``(delta, need)`` at runtime ``+0x78``;
    ``FUN_0056e410`` calls ``FUN_0056e5d0(need, delta, scratch_actor)``, which
    adds the delta to need slot ``+0x30 + 4*need`` (cap 99) and scales it by a
    per-need factor table.
    """

    need: int
    delta: int

    @property
    def label(self) -> str:
        return NEED_LABELS.get(self.need, f"need_{self.need}")


@dataclass(frozen=True)
class StatGrant:
    """One 8-byte grant pair (file: attribute, value).

    The loader rotates it to ``(value, attribute)`` at runtime ``+0x90``.  The
    consumer of this vector has not been located; the values mirror the event
    base values (``Smell`` grants twelve attributes at 100).
    """

    stat: int
    value: int

    @property
    def label(self) -> str:
        return ATTRIBUTE_LABELS.get(self.stat, f"stat_{self.stat}")


@dataclass(frozen=True)
class ElementEvent:
    """Stat effect applied at an element's selection threshold.

    ``FUN_005727b0`` computes ``value + current * percent / 100`` and clamps
    the result to the actor's per-stat bounds.  ``percent`` is therefore not a
    probability and can exceed 100 (``GetQuad`` uses 125..400).
    """

    kind: int
    value: int
    percent: int

    @property
    def label(self) -> str:
        return EVENT_LABELS.get(self.kind, f"effect_{self.kind}")


@dataclass(frozen=True)
class EffectElement:
    """One timed visual/audio effect attached to a bone."""

    time: int
    bones: tuple[int, ...]
    size: float
    texture: str
    events: tuple[ElementEvent, ...]
    params: tuple[int, ...]

    @property
    def element_class(self) -> int:
        """Runtime ``+0x54``; 0 normal, 1 invincibility, 2 fall."""
        return self.params[0]

    @property
    def element_class_name(self) -> str:
        return ELEMENT_CLASSES.get(self.element_class, "unknown")

    @property
    def power(self) -> int:
        """Runtime ``+0x2c``; 0..100 (KickTheirAss ramps 10..100)."""
        return self.params[1]

    @property
    def floats(self) -> tuple[float, ...]:
        """The five float params (runtime ``+0x14..+0x24``)."""
        return tuple(
            struct.unpack("<f", struct.pack("<I", v))[0] for v in self.params[2:7]
        )

    @property
    def ramp_percent(self) -> int:
        """Runtime ``+0x3c``; 100 = no ramp (jumps use 50/75/25/25/5)."""
        return self.params[7]

    @property
    def ramp_value(self) -> float:
        """Runtime ``+0x40`` as ``f32`` (jumps 10/5/20, Smell 30.0)."""
        return struct.unpack("<f", struct.pack("<I", self.params[8]))[0]


@dataclass
class Bai:
    path: str
    name: str
    field_14: int
    field_18: float
    field_1c: int
    flag_20: int
    special_action: int
    eat_flag: int
    field_26: int
    field_2a: float
    field_2e: float
    field_32: int
    params: tuple[int, ...]
    consume_flag: int
    preconditions: tuple[Precondition, ...]
    effect_weights: tuple[EffectWeight, ...]
    need_deltas: tuple[NeedDelta, ...]
    stat_grants: tuple[StatGrant, ...]
    effect_class: int
    effects: tuple[EffectElement, ...]
    tail: bytes

    @property
    def group(self) -> str:
        return ACTION_GROUPS.get(self.special_action, "unknown")

    @property
    def effect_class_name(self) -> str:
        return EFFECT_CLASSES.get(self.effect_class, "unknown")

    def summary(self) -> dict:
        return {
            "path": self.path,
            "name": self.name,
            "group": self.group,
            "field_14": self.field_14,
            "field_18": self.field_18,
            "field_1c": self.field_1c,
            "flag_20": self.flag_20,
            "special_action": self.special_action,
            "eat_flag": self.eat_flag,
            "field_26": self.field_26,
            "field_2a": self.field_2a,
            "field_2e": self.field_2e,
            "field_32": self.field_32,
            "params": list(self.params),
            "consume_flag": self.consume_flag,
            "preconditions": [
                {
                    "threshold": p.threshold,
                    "attribute": p.attribute,
                    "comparison": p.comparison,
                }
                for p in self.preconditions
            ],
            "effect_weights": [
                {"attribute": w.attribute, "weight": w.weight, "unused": w.unused}
                for w in self.effect_weights
            ],
            "need_deltas": [[d.need, d.delta] for d in self.need_deltas],
            "stat_grants": [[g.stat, g.value] for g in self.stat_grants],
            "effect_class": self.effect_class,
            "effects": [
                {
                    "time": e.time,
                    "bones": list(e.bones),
                    "size": e.size,
                    "texture": e.texture,
                    "events": [[s.kind, s.value, s.percent] for s in e.events],
                    "params": list(e.params),
                }
                for e in self.effects
            ],
            "tail_bytes": len(self.tail),
        }


class _Cursor:
    """Bounds-checked little-endian reader; any overrun is a format error."""

    def __init__(self, data: bytes, name: str = "") -> None:
        self.data = data
        self.offset = 0
        self.name = name

    def _take(self, size: int) -> bytes:
        end = self.offset + size
        if end > len(self.data):
            raise BaiFormatError(
                f"{self.name}: truncated at 0x{self.offset:x} "
                f"(need {size} bytes, {len(self.data) - self.offset} left)"
            )
        chunk = self.data[self.offset:end]
        self.offset = end
        return chunk

    def u32(self) -> int:
        return struct.unpack("<I", self._take(4))[0]

    def i32(self) -> int:
        return struct.unpack("<i", self._take(4))[0]

    def f32(self) -> float:
        return struct.unpack("<f", self._take(4))[0]

    def u8(self) -> int:
        return self._take(1)[0]

    def text(self, size: int) -> str:
        return self._take(size).rstrip(b"\x00").decode("latin-1")

    def done(self) -> bool:
        return self.offset == len(self.data)


def _parse_effect(c: _Cursor) -> EffectElement:
    time = c.u32()
    n_bones = c.u32()
    bones = tuple(c.u32() for _ in range(n_bones))
    size = c.f32()
    n_texture = c.u32()
    texture = c.text(n_texture)
    n_events = c.u32()
    events = tuple(
        ElementEvent(kind=c.u32(), value=c.i32(), percent=c.u32())
        for _ in range(n_events)
    )
    params = tuple(c.u32() for _ in range(ELEMENT_PARAM_COUNT))
    return EffectElement(
        time=time,
        bones=bones,
        size=size,
        texture=texture,
        events=events,
        params=params,
    )


def _read_precondition(c: _Cursor) -> Precondition:
    """File order is (attribute, comparison, threshold); runtime is rotated."""
    attribute = c.u32()
    comparison = c.u32()
    threshold = c.u32()
    return Precondition(threshold=threshold, attribute=attribute, comparison=comparison)


def _read_effect_weight(c: _Cursor) -> EffectWeight:
    """File order is (attribute, unused, weight); runtime is rotated."""
    attribute = c.u32()
    unused = c.u32()
    weight = c.u32()
    return EffectWeight(attribute=attribute, weight=weight, unused=unused)


def _parse_tail(data: bytes, name: str) -> dict:
    c = _Cursor(data, name)
    consume_flag = c.u8()

    # File order (attribute, comparison, threshold); runtime order is rotated.
    n_pre = c.u32()
    preconditions = tuple(_read_precondition(c) for _ in range(n_pre))

    # File order (attribute, unused, weight); runtime order is rotated.
    n_weight = c.u32()
    effect_weights = tuple(_read_effect_weight(c) for _ in range(n_weight))

    n_need = c.u32()
    need_deltas = tuple(NeedDelta(need=c.u32(), delta=c.i32()) for _ in range(n_need))

    n_grant = c.u32()
    stat_grants = tuple(StatGrant(stat=c.u32(), value=c.u32()) for _ in range(n_grant))

    effect_class = c.u32()

    n_effects = c.u32()
    effects = tuple(_parse_effect(c) for _ in range(n_effects))

    if not c.done():
        raise BaiFormatError(
            f"{name}: {len(data) - c.offset} trailing bytes at 0x{c.offset:x}"
        )
    return {
        "consume_flag": consume_flag,
        "preconditions": preconditions,
        "effect_weights": effect_weights,
        "need_deltas": need_deltas,
        "stat_grants": stat_grants,
        "effect_class": effect_class,
        "effects": effects,
    }


def read_bai(path: Path) -> Bai:
    data = path.read_bytes()
    if len(data) < HEADER_SIZE:
        raise BaiFormatError(f"{path.name}: {len(data)} bytes < {HEADER_SIZE}")
    raw_name = data[:NAME_SIZE].split(b"\x00", 1)[0]
    try:
        name = raw_name.decode("ascii")
    except UnicodeDecodeError as exc:
        raise BaiFormatError(f"{path.name}: non-ASCII action name") from exc
    if not name:
        raise BaiFormatError(f"{path.name}: empty action name")
    field_14, field_18, field_1c = struct.unpack_from("<IfI", data, 0x14)
    flag_20 = data[0x20]
    (special_action,) = struct.unpack_from("<I", data, 0x21)
    eat_flag = data[0x25]
    (field_26,) = struct.unpack_from("<I", data, 0x26)
    field_2a, field_2e = struct.unpack_from("<ff", data, 0x2A)
    (field_32,) = struct.unpack_from("<I", data, 0x32)
    params = struct.unpack_from(f"<{PARAM_COUNT}I", data, 0x36)
    tail = data[HEADER_SIZE:]
    decoded = _parse_tail(tail, path.name)
    return Bai(
        path=str(path),
        name=name,
        field_14=field_14,
        field_18=field_18,
        field_1c=field_1c,
        flag_20=flag_20,
        special_action=special_action,
        eat_flag=eat_flag,
        field_26=field_26,
        field_2a=field_2a,
        field_2e=field_2e,
        field_32=field_32,
        params=params,
        tail=tail,
        **decoded,
    )


def read_action_list(path: Path) -> list[str]:
    names: list[str] = []
    for line in path.read_text(encoding="latin-1").splitlines():
        line = line.strip()
        if not line or line.startswith("//"):
            continue
        if not names and line.isdigit():
            continue
        names.append(line)
    return names


def scan(root: Path) -> tuple[int, list[Bai], list[tuple[str, str]]]:
    """Return (file_count, parsed, errors) for an actions directory."""
    parsed: list[Bai] = []
    errors: list[tuple[str, str]] = []
    files = sorted(root.glob("*.bai"))
    for path in files:
        try:
            parsed.append(read_bai(path))
        except (BaiFormatError, OSError) as exc:
            errors.append((path.name, str(exc)))
    return len(files), parsed, errors


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path", type=Path, help="one .bai or the actions directory")
    parser.add_argument("--json", action="store_true", help="machine-readable output")
    args = parser.parse_args(argv)

    if args.path.is_dir():
        count, parsed, errors = scan(args.path)
        if args.json:
            print(
                json.dumps(
                    {
                        "files": count,
                        "parsed": len(parsed),
                        "errors": errors,
                        "actions": [b.summary() for b in parsed],
                    },
                    indent=2,
                )
            )
            return 0 if not errors else 1
        print(f"{len(parsed)}/{count} .bai files parsed, {len(errors)} errors")
        for bai in parsed:
            print(
                f"  {bai.name:<20} group={bai.group:<14} "
                f"f14={bai.field_14:<4} f18={bai.field_18:<6g} "
                f"f1c={bai.field_1c} flag={bai.flag_20} "
                f"special={bai.special_action} eat={bai.eat_flag} "
                f"f26={bai.field_26} f32={bai.field_32:<6} "
                f"tail={len(bai.tail):<3} params={bai.params}"
            )
            print(
                f"      consume={bai.consume_flag} class={bai.effect_class_name} "
                f"pre={[p.describe() for p in bai.preconditions]} "
                f"weights={[(w.label, w.weight) for w in bai.effect_weights]} "
                f"needs={[(d.label, d.delta) for d in bai.need_deltas]} "
                f"grants={[(g.label, g.value) for g in bai.stat_grants]} "
                f"effects={len(bai.effects)}"
            )
            for i, effect in enumerate(bai.effects):
                print(
                    f"        E{i} t={effect.time} bones={list(effect.bones)} "
                    f"size={effect.size:g} tex={effect.texture or '-'} "
                    f"events={[(e.label, e.value, e.percent) for e in effect.events]} "
                    f"floats={tuple(round(v, 3) for v in effect.floats)} "
                    f"ramp={effect.ramp_percent}%/{effect.ramp_value:g}"
                )
        for name, error in errors:
            print(f"  ERROR {name}: {error}")
        return 0 if not errors else 1

    bai = read_bai(args.path)
    print(json.dumps(bai.summary(), indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
