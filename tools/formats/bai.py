#!/usr/bin/env python3
"""Reader for Dino Island ``.bai`` action definitions.

64 files live in ``Data/Design/attractions/actions`` and are loaded in
``ActionList.ini`` order by ``FUN_0053e060`` into a 0xac-byte runtime record.
This module decodes the packed header and the whole variable tail.

Layout (little-endian, packed -- several fields are not 4-byte aligned)::

    0x00  char name[20]      # truncated to 19 chars + NUL for long names
    0x14  u32  field_14
    0x18  f32  field_18
    0x1c  u32  field_1c
    0x20  u8   flag_20
    0x21  u32  special_action  # 0 normal, 1..6 special groups
    0x25  u8   eat_flag
    0x26  u32  field_26        # 50 in most files, 60 in ReachWaypoint
    0x2a  f32  field_2a        # 30.0 in 63/64 files, 10.0 in ReachWaypoint
    0x2e  f32  field_2e        # 30.0 in all 64 files
    0x32  u32  field_32
    0x36  u32  params[8]
    0x56  u8   consume_flag
    0x57  u32  n_anim;  n_anim  x (u32 animation, u32 variant, u32 duration)
          u32  n_fx;    n_fx    x (u32 effect, u32 magnitude, u32 count)
          u32  n_need;  n_need  x (u32 need, i32 delta)
          u32  n_grant; n_grant x (u32 stat, u32 value)
          u32  effect_class
          u32  n_fxrec; n_fxrec x effect element

Effect element (0x58-byte runtime record, file order)::

    u32   time             # timeline position, ~centiseconds
    u32   n_bones;  u32 bone[n_bones]
    f32   size
    u32   n_texture; char texture[n_texture]
    u32   n_events; n_events x (u32 stat, i32 value, u32 probability)
    u32   params[9]        # runtime +0x54, +0x2c, +0x14..+0x24, +0x3c, +0x40

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

#: Stat ids used by ``stat_grants`` and ``StatEvent``.  Inferred from the
#: action that grants them (GetArmor -> 16, GetJump -> 9, ...) and from the
#: matching ``recs`` ramp in the same file.
STAT_LABELS = {
    7: "unknown_7",
    8: "unknown_8",
    9: "jump",
    10: "perception",
    11: "intelligence",
    12: "unknown_12",
    13: "quad_a",
    14: "quad_b",
    16: "armor",
    18: "unknown_18",
    19: "endurance",
    20: "courage",
    23: "unknown_23",
    24: "unknown_24",
    25: "unknown_25",
}

#: ``effect_class`` (runtime ``+0x88``).  Observed values and the files that
#: use them; the label is a working hypothesis.
EFFECT_CLASSES = {
    0: "none",
    1: "fight",
    2: "explosion",
    3: "teleport",
    5: "jump",
}


class BaiFormatError(ValueError):
    pass


@dataclass(frozen=True)
class AnimStep:
    """One animation phase of the action."""

    animation: int
    variant: int
    duration: int


@dataclass(frozen=True)
class FxStep:
    """One effect step; ``magnitude`` scales with the action size."""

    effect: int
    magnitude: int
    count: int


@dataclass(frozen=True)
class NeedDelta:
    """Satisfaction change applied to a dinosaur need slot."""

    need: int
    delta: int

    @property
    def label(self) -> str:
        return NEED_LABELS.get(self.need, f"need_{self.need}")


@dataclass(frozen=True)
class StatGrant:
    """Permanent stat change granted when the action completes."""

    stat: int
    value: int

    @property
    def label(self) -> str:
        return STAT_LABELS.get(self.stat, f"stat_{self.stat}")


@dataclass(frozen=True)
class StatEvent:
    """Timed stat change inside an effect element."""

    stat: int
    value: int
    probability: int

    @property
    def label(self) -> str:
        return STAT_LABELS.get(self.stat, f"stat_{self.stat}")


@dataclass(frozen=True)
class EffectElement:
    """One timed visual/audio effect attached to a bone."""

    time: int
    bones: tuple[int, ...]
    size: float
    texture: str
    events: tuple[StatEvent, ...]
    params: tuple[int, ...]

    @property
    def floats(self) -> tuple[float, ...]:
        """The five float params (runtime ``+0x14..+0x24``)."""
        return tuple(struct.unpack("<f", struct.pack("<I", v))[0] for v in self.params[2:7])

    @property
    def probability(self) -> int:
        """Runtime ``+0x3c``; 100 in every file except Smell."""
        return self.params[7]


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
    anim_steps: tuple[AnimStep, ...]
    fx_steps: tuple[FxStep, ...]
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
            "anim_steps": [
                [s.animation, s.variant, s.duration] for s in self.anim_steps
            ],
            "fx_steps": [[s.effect, s.magnitude, s.count] for s in self.fx_steps],
            "need_deltas": [[d.need, d.delta] for d in self.need_deltas],
            "stat_grants": [[g.stat, g.value] for g in self.stat_grants],
            "effect_class": self.effect_class,
            "effects": [
                {
                    "time": e.time,
                    "bones": list(e.bones),
                    "size": e.size,
                    "texture": e.texture,
                    "events": [[s.stat, s.value, s.probability] for s in e.events],
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
        StatEvent(stat=c.u32(), value=c.i32(), probability=c.u32())
        for _ in range(n_events)
    )
    params = tuple(c.u32() for _ in range(9))
    return EffectElement(
        time=time,
        bones=bones,
        size=size,
        texture=texture,
        events=events,
        params=params,
    )


def _parse_tail(data: bytes, name: str) -> dict:
    c = _Cursor(data, name)
    consume_flag = c.u8()

    n_anim = c.u32()
    anim_steps = tuple(
        AnimStep(animation=c.u32(), variant=c.u32(), duration=c.u32())
        for _ in range(n_anim)
    )

    n_fx = c.u32()
    fx_steps = tuple(
        FxStep(effect=c.u32(), magnitude=c.u32(), count=c.u32())
        for _ in range(n_fx)
    )

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
        "anim_steps": anim_steps,
        "fx_steps": fx_steps,
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
                f"anim={[(s.animation, s.variant, s.duration) for s in bai.anim_steps]} "
                f"fx={[(s.effect, s.magnitude, s.count) for s in bai.fx_steps]} "
                f"needs={[(d.label, d.delta) for d in bai.need_deltas]} "
                f"grants={[(g.label, g.value) for g in bai.stat_grants]} "
                f"effects={len(bai.effects)}"
            )
            for i, effect in enumerate(bai.effects):
                print(
                    f"        E{i} t={effect.time} bones={list(effect.bones)} "
                    f"size={effect.size:g} tex={effect.texture or '-'} "
                    f"events={[(e.label, e.value, e.probability) for e in effect.events]} "
                    f"floats={tuple(round(v, 3) for v in effect.floats)} "
                    f"p={effect.probability}"
                )
        for name, error in errors:
            print(f"  ERROR {name}: {error}")
        return 0 if not errors else 1

    bai = read_bai(args.path)
    print(json.dumps(bai.summary(), indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
