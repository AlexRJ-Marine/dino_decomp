#!/usr/bin/env python3
"""Reader for the show-feature definitions ``Data/Design/attractions/features/*.bfi``.

The complete body is now decoded as a strict sequential stream: every one of
the 62 retail files parses to an exact EOF.  The layout is the same for all
class tags; the tag only selects the construction-kit category and the
``tagged`` flag at record ``+0x34``.

```text
0x00  u32   footprint width     4 default; cages 8/12/16; car 2, bus 4
0x04  u32   footprint height    4 default; beam 4x1, bus 4x8, car 2x4
0x08  f32   vertical size       4.0 default; CageHuge 8.0; judges 12.0
0x0c  u32   variant             0 default; CageSmall..Huge 0..3
0x10  u8    mesh flag           1 for the mesh props (beams, cars, rock...)
0x11  u32   field_11            100 default; 200..500 for large props
0x15  u32   field_15            10 default; 1..5 for large props
0x19  u32   price               10..2500 (bait 10..50, cages 400..2400)
0x1d  u32   waypoint team       1..4 for WaypointTeam1..4, else 0
0x21  u32   display type        0 mesh prop/judge, 1 cage, 3 ground tile
0x25  u32   field_25            100 only for ObstacleMine, else 0
0x29  u32   field_29            0..2
0x2d  string  class tag
-- body (same shape for every class) --
u32    judge_kind          0 = not a judge, 1 Race .. 6 Capture
u32 n + n bytes              show-type list (1..6)
u32    food_kind           .bsi/.bni kind id, 0 for non-food
u32    field_3c            10000, Waypoint 50000
u32    field_40            100, bait 70, Waypoint 500
u32 n + n strings            interaction action names (ActionList.ini)
u32    anim_flag           0 for cages, else 1
f32    anim_speed          0.0 / 0.1 / 0.25
u32    state_count         1..3 visual states (renderer loops over it)
8 strings                   mesh/texture slots (3 states x 2 LODs + 2)
string                      texture directory
f32    model_scale         1.0 / 0.25
7 strings                   effect/explosion .ini names
3 x u32                     field_154..15c (0; judge third = 8.0)
string                      billboard directory
u32 n + n x (u32 m + m strings)  attribute-name lists (5 show levels)
u32 n + n x u32             score steps (200/400/600/800/1000)
3 strings                   sound / event names
f32, f32                    duration pair (second = 15 x first, bait 12 x)
u32    category            2 default, 4 LunchBox, 1 StoneWall
string                      construction-kit switch key (SWITCH_*)
string                      trailing string (empty)
u8     tail_flag           1 only LunchBox
```

Mesh slot order (renderer ``FUN_00621770``: model *i* uses slot *i* and
``i + 3``; ``state_count`` limits *i*):

```text
slot 0  state 0 LOD1      slot 3  state 0 LOD2
slot 1  state 1 LOD1      slot 4  state 1 LOD2
slot 2  state 2 LOD1      slot 5  state 2 LOD2
slot 6  ground/effect texture (tiles, baits, cages)
slot 7  second waypoint texture
```

See ``docs/formats/bfi.md``.
"""
from __future__ import annotations

import argparse
import json
import struct
from dataclasses import dataclass, field
from pathlib import Path

CLASS_TAGS = (
    "Cage",
    "Judge",
    "AnimalBait",
    "StaticBait",
    "Waypoint",
    "Teleport",
    "LunchBox",
    "Tile",
)

JUDGE_KINDS = {
    0: "",
    1: "Race",
    2: "Obstacle",
    3: "Fight",
    4: "Destruction",
    5: "Team",
    6: "Capture",
}


class BfiFormatError(ValueError):
    pass


@dataclass
class Bfi:
    path: str
    name: str
    # header
    width: int
    height: int
    vertical: float
    variant: int
    flag: int
    field_11: int
    field_15: int
    price: int
    waypoint_team: int
    display_type: int
    field_25: int
    field_29: int
    # body
    class_tag: str
    judge_kind: int
    show_types: list[int]
    food_kind: int
    field_3c: int
    field_40: int
    actions: list[str]
    anim_flag: int
    anim_speed: float
    state_count: int
    mesh_slots: list[str] = field(default_factory=list)
    texture_dir: str = ""
    model_scale: float = 1.0
    effects: list[str] = field(default_factory=list)
    field_154: int = 0
    field_158: int = 0
    field_15c: int = 0
    billboard_dir: str = ""
    attribute_sets: list[list[str]] = field(default_factory=list)
    score_steps: list[float] = field(default_factory=list)
    sounds: list[str] = field(default_factory=list)
    duration: float = 0.0
    duration_frames: float = 0.0
    category: int = 0
    switch_key: str = ""
    trailing_string: str = ""
    tail_flag: int = 0
    # file order list of every string, kept for the old API
    strings: list[tuple[int, str]] = field(default_factory=list)

    @property
    def judge(self) -> str:
        return JUDGE_KINDS.get(self.judge_kind, "")

    # Header aliases kept from the first parser revision.
    @property
    def field_19(self) -> int:
        return self.price

    @property
    def field_1d(self) -> int:
        return self.waypoint_team

    @property
    def field_21(self) -> int:
        return self.display_type

    @property
    def meshes(self) -> list[str]:
        # Slot meshes plus the judge course mesh stored in the effect list.
        return [s for s in (*self.mesh_slots, *self.effects) if s.lower().endswith(".mesh")]

    @property
    def textures(self) -> list[str]:
        return [s for s in (*self.mesh_slots, *self.effects) if s.lower().endswith(".tga")]

    @property
    def identifiers(self) -> list[str]:
        out = []
        for _, text in self.strings:
            if text == self.class_tag:
                continue
            if any(text.lower().endswith(ext) for ext in (".mesh", ".tga", ".ini")):
                continue
            if "\\" in text or text.startswith("SWITCH_"):
                continue
            out.append(text)
        return out

    def summary(self) -> dict:
        return {
            "path": self.path,
            "name": self.name,
            "width": self.width,
            "height": self.height,
            "vertical": self.vertical,
            "variant": self.variant,
            "flag": self.flag,
            "field_11": self.field_11,
            "field_15": self.field_15,
            "price": self.price,
            "waypoint_team": self.waypoint_team,
            "display_type": self.display_type,
            "field_25": self.field_25,
            "field_29": self.field_29,
            "class_tag": self.class_tag,
            "judge_kind": self.judge_kind,
            "judge": self.judge,
            "show_types": self.show_types,
            "food_kind": self.food_kind,
            "field_3c": self.field_3c,
            "field_40": self.field_40,
            "actions": self.actions,
            "anim_flag": self.anim_flag,
            "anim_speed": self.anim_speed,
            "state_count": self.state_count,
            "mesh_slots": self.mesh_slots,
            "texture_dir": self.texture_dir,
            "model_scale": self.model_scale,
            "effects": self.effects,
            "field_154": self.field_154,
            "field_158": self.field_158,
            "field_15c": self.field_15c,
            "billboard_dir": self.billboard_dir,
            "attribute_sets": self.attribute_sets,
            "score_steps": self.score_steps,
            "sounds": self.sounds,
            "duration": self.duration,
            "duration_frames": self.duration_frames,
            "category": self.category,
            "switch_key": self.switch_key,
            "trailing_string": self.trailing_string,
            "tail_flag": self.tail_flag,
            "meshes": self.meshes,
            "textures": self.textures,
            "identifiers": self.identifiers,
            "strings": [text for _, text in self.strings],
        }


class _Reader:
    def __init__(self, data: bytes, offset: int):
        self.data = data
        self.offset = offset

    def need(self, size: int) -> None:
        if self.offset + size > len(self.data):
            raise BfiFormatError(
                f"body overruns file at 0x{self.offset:x} (need {size}, "
                f"have {len(self.data) - self.offset})"
            )

    def u32(self) -> int:
        self.need(4)
        value = struct.unpack_from("<I", self.data, self.offset)[0]
        self.offset += 4
        return value

    def f32(self) -> float:
        self.need(4)
        value = struct.unpack_from("<f", self.data, self.offset)[0]
        self.offset += 4
        return value

    def u8(self) -> int:
        self.need(1)
        value = self.data[self.offset]
        self.offset += 1
        return value

    def string(self) -> str:
        size = self.u32()
        if size > 4096:
            raise BfiFormatError(f"implausible string length {size} at 0x{self.offset - 4:x}")
        self.need(size)
        raw = self.data[self.offset: self.offset + size]
        self.offset += size
        if not raw or raw[-1] != 0:
            raise BfiFormatError(f"string at 0x{self.offset - size:x} is not NUL-terminated")
        return raw[:-1].decode("latin-1")


def read_bfi(path: Path) -> Bfi:
    data = path.read_bytes()
    if len(data) < 0x35:
        raise BfiFormatError(f"{path}: too short")
    width, height = struct.unpack_from("<II", data, 0)
    vertical = struct.unpack_from("<f", data, 8)[0]
    variant = struct.unpack_from("<I", data, 0x0C)[0]
    flag = data[0x10]
    field_11, field_15, price, waypoint_team, display_type, field_25, field_29 = (
        struct.unpack_from("<I", data, off)[0]
        for off in (0x11, 0x15, 0x19, 0x1D, 0x21, 0x25, 0x29)
    )

    tag_reader = _Reader(data, 0x2D)
    tag = tag_reader.string()
    reader = _Reader(data, tag_reader.offset)

    strings: list[tuple[int, str]] = [(0x2D, tag)]

    def take_string() -> str:
        offset = reader.offset
        text = reader.string()
        strings.append((offset, text))
        return text

    judge_kind = reader.u32()
    show_count = reader.u32()
    reader.need(show_count)
    show_types = list(data[reader.offset: reader.offset + show_count])
    reader.offset += show_count
    food_kind = reader.u32()
    field_3c = reader.u32()
    field_40 = reader.u32()
    action_count = reader.u32()
    actions = [take_string() for _ in range(action_count)]
    anim_flag = reader.u32()
    anim_speed = reader.f32()
    state_count = reader.u32()
    mesh_slots = [take_string() for _ in range(8)]
    texture_dir = take_string()
    model_scale = reader.f32()
    effects = [take_string() for _ in range(7)]
    field_154 = reader.u32()
    field_158 = reader.u32()
    field_15c = reader.u32()
    billboard_dir = take_string()

    attribute_sets: list[list[str]] = []
    for _ in range(reader.u32()):
        attribute_sets.append([take_string() for _ in range(reader.u32())])
    score_steps = [reader.f32() for _ in range(reader.u32())]
    sounds = [take_string() for _ in range(3)]
    duration = reader.f32()
    duration_frames = reader.f32()
    category = reader.u32()
    switch_key = take_string()
    trailing_string = take_string()
    tail_flag = reader.u8()

    if reader.offset != len(data):
        raise BfiFormatError(
            f"{path.name}: {len(data) - reader.offset} trailing bytes at 0x{reader.offset:x}"
        )

    return Bfi(
        path=str(path),
        name=path.stem,
        width=width,
        height=height,
        vertical=vertical,
        variant=variant,
        flag=flag,
        field_11=field_11,
        field_15=field_15,
        price=price,
        waypoint_team=waypoint_team,
        display_type=display_type,
        field_25=field_25,
        field_29=field_29,
        class_tag=tag,
        judge_kind=judge_kind,
        show_types=show_types,
        food_kind=food_kind,
        field_3c=field_3c,
        field_40=field_40,
        actions=actions,
        anim_flag=anim_flag,
        anim_speed=anim_speed,
        state_count=state_count,
        mesh_slots=mesh_slots,
        texture_dir=texture_dir,
        model_scale=model_scale,
        effects=effects,
        field_154=field_154,
        field_158=field_158,
        field_15c=field_15c,
        billboard_dir=billboard_dir,
        attribute_sets=attribute_sets,
        score_steps=score_steps,
        sounds=sounds,
        duration=duration,
        duration_frames=duration_frames,
        category=category,
        switch_key=switch_key,
        trailing_string=trailing_string,
        tail_flag=tail_flag,
        strings=strings,
    )


def scan(directory: Path) -> list[Bfi]:
    return [read_bfi(path) for path in sorted(directory.glob("*.bfi"))]


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("path", type=Path, help="a .bfi file or the features directory")
    args = ap.parse_args(argv)
    if args.path.is_dir():
        print(json.dumps([item.summary() for item in scan(args.path)], indent=2))
    else:
        print(json.dumps(read_bfi(args.path).summary(), indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
