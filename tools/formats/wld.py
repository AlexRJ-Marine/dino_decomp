#!/usr/bin/env python3
"""Reader for Dino Island world-object blocks (``.wld`` and ``.sav``).

The campaign ``WorldFile`` and the save game share one serialized
``pwdWorldManager`` block.  The block is written by ``0x0053c9d0`` and read by
``0x0053c560``.  The ``.wld`` variant is loaded with the campaign flag set
(``0x00403e10`` -> ``0x0051e6c0(reader, version, 1)``), while a save game is
loaded with the flag clear (``0x00402bc0`` / ``0x00402f10``).

World object classes are selected by the list, not by an inline tag.  The class
table is installed by ``0x0053a140``; each class exposes a name getter on
vtable slot 2:

===== ============ ==================================================
 id    class        evidence
===== ============ ==================================================
 0x12  drsDinosaur  ctor 0x005e8e30, 0x11c bytes, name 0x0065e810
 0x13  drsVisitor   ctor 0x005e8c60, 0x03c bytes, name 0x0065e4c4
 0x14  drsEmployee  ctor 0x005e8b40, 0x034 bytes, name 0x00670070
 0x15  drsAnimal    ctor 0x005e8a00, 0x030 bytes, name 0x00670030
 0x16  drsShow      ctor 0x005e74f0, 0x070 bytes, name 0x0065e464
 0x17  drsFence     ctor 0x005e87a0, 0x05c bytes, name 0x0066fff4
 0x18  drsBuilding  ctor 0x005e8590, 0x034 bytes, name 0x0065e3ac
===== ============ ==================================================

Block layout (version 50..53, campaign flag as noted)::

    u32 campaign_id                  # save games only; -1 in free game
    u32 mission_id                   # save games only; -1 in free game
    u32 objective_count              # save games only
    objective_count x (u32 kind, u32 value, u32 target)
    u32 fence_count
    fence_count x drsFence
    u32 show_count
    show_count x drsShow
    u32 animal_count                 # version > 1
    animal_count x drsAnimal
    u32 employee_count               # version > 2
    employee_count x drsEmployee
    u32 building_count
    building_count x drsBuilding + u32 link
    u32 visitor_count                # save games only
    visitor_count x drsVisitor
    u32 dinosaur_count               # version > 1
    dinosaur_count x drsDinosaur
    u32 feature_flag_count  + bytes  # 62 in retail; save games only
    u32 building_flag_count + bytes  # 39 in retail; save games only
    u32 food_flag_count     + bytes  # 10 in retail; save games only
    u32 misc_flag_count     + bytes  # 6 in retail; save games only
    u32 show_catalog_count           # save games only (drs show catalog)

The ``.wld`` file continues with the simulation roads (``0x0058ccc0``)::

    u32 road_count
    road_count x (s32 x0, s32 y0, s32 x1, s32 y1)

and a 40-byte camera state for versions >= 50.

Save-game header (``0x00403e10`` / ``0x00402bc0``)::

    0x00  u32 version
    0x04  u32 slot
    0x08  u32 name_len
    0x0c  UTF-16LE name (name_len bytes)
    ...   12 reserved zero bytes
    ...   world block

See ``docs/formats/wld.md`` and ``notes/world-objects-recon.md``.
"""
from __future__ import annotations

import argparse
import json
import struct
from dataclasses import dataclass, field
from pathlib import Path

KNOWN_VERSIONS = {2, 50, 51, 52, 53}
CAMERA_BYTES = 40
SAV_HEADER_EXTRA = 12

WORLD_CLASSES = {
    0x12: "drsDinosaur",
    0x13: "drsVisitor",
    0x14: "drsEmployee",
    0x15: "drsAnimal",
    0x16: "drsShow",
    0x17: "drsFence",
    0x18: "drsBuilding",
}


class WldFormatError(ValueError):
    pass


@dataclass
class Waypoint:
    """One 0x18-byte point on a fence/show polyline."""

    flags: int
    key: int
    x: int
    y: int
    z: int
    extra: int

    def to_dict(self) -> dict:
        return {
            "flags": self.flags,
            "key": self.key,
            "x": self.x,
            "y": self.y,
            "z": self.z,
            "extra": self.extra,
        }


@dataclass
class Fence:
    object_id: int
    field_14: int
    start: tuple[int, int]
    end: tuple[int, int]
    waypoints: list[Waypoint]
    class_tag: int
    field_44: int
    field_48: int
    field_14_first: int = 0
    extra16: bytes = b""

    def to_dict(self) -> dict:
        return {
            "object_id": self.object_id,
            "field_14": self.field_14,
            "start": list(self.start),
            "end": list(self.end),
            "waypoint_count": len(self.waypoints),
            "class_tag": self.class_tag,
            "field_44": self.field_44,
            "field_48": self.field_48,
        }


@dataclass
class Show:
    object_id: int
    field_14: int
    start: tuple[int, int]
    end: tuple[int, int]
    waypoints: list[Waypoint]
    class_tag: int
    field_44: int
    field_48: int

    def to_dict(self) -> dict:
        return {
            "object_id": self.object_id,
            "field_14": self.field_14,
            "start": list(self.start),
            "end": list(self.end),
            "waypoint_count": len(self.waypoints),
            "class_tag": self.class_tag,
            "field_44": self.field_44,
            "field_48": self.field_48,
        }


@dataclass
class Animal:
    object_id: int
    field_1c: int
    field_20: int | None

    def to_dict(self) -> dict:
        return {
            "object_id": self.object_id,
            "field_1c": self.field_1c,
            "field_20": self.field_20,
        }


@dataclass
class Employee:
    object_id: int
    field_1c: int

    def to_dict(self) -> dict:
        return {"object_id": self.object_id, "field_1c": self.field_1c}


@dataclass
class Building:
    object_id: int
    linked_id: int
    kind: int
    cell_x: int
    cell_y: int
    state: int
    link: int
    class_tag: int = 0x18

    @property
    def kind_name(self) -> str | None:
        return BUILDING_NAMES.get(self.kind)

    def to_dict(self) -> dict:
        return {
            "object_id": self.object_id,
            "linked_id": self.linked_id,
            "kind": self.kind,
            "kind_name": self.kind_name,
            "class_tag": self.class_tag,
            "cell_x": self.cell_x,
            "cell_y": self.cell_y,
            "state": self.state,
            "link": self.link,
        }


@dataclass
class Visitor:
    object_id: int
    field_1c: int
    class_tag: int
    field_20: int
    field_24: int
    field_28: int | None

    def to_dict(self) -> dict:
        return {
            "object_id": self.object_id,
            "field_1c": self.field_1c,
            "class_tag": self.class_tag,
            "field_20": self.field_20,
            "field_24": self.field_24,
            "field_28": self.field_28,
        }


@dataclass
class Dinosaur:
    object_id: int
    field_1c: int
    field_24: int | None
    field_28: int | None
    field_2c: int | None
    class_tag: int
    #: 18 (allele A, allele B) pairs in the same order as a design ``.gen``.
    genotype: list[tuple[int, int]] = field(default_factory=list)

    def to_dict(self) -> dict:
        return {
            "object_id": self.object_id,
            "field_1c": self.field_1c,
            "field_24": self.field_24,
            "field_28": self.field_28,
            "field_2c": self.field_2c,
            "class_tag": self.class_tag,
            "genotype": [list(pair) for pair in self.genotype],
        }


@dataclass
class WorldBlock:
    offset: int
    end: int
    campaign_id: int | None
    mission_id: int | None
    objectives: list[tuple[int, int, int]] = field(default_factory=list)
    fences: list[Fence] = field(default_factory=list)
    shows: list[Show] = field(default_factory=list)
    animals: list[Animal] = field(default_factory=list)
    employees: list[Employee] = field(default_factory=list)
    buildings: list[Building] = field(default_factory=list)
    visitors: list[Visitor] = field(default_factory=list)
    dinosaurs: list[Dinosaur] = field(default_factory=list)
    feature_flags: bytes = b""
    building_flags: bytes = b""
    food_flags: bytes = b""
    misc_flags: bytes = b""
    show_catalog_count: int | None = None

    def summary(self) -> dict:
        return {
            "offset": self.offset,
            "end": self.end,
            "campaign_id": self.campaign_id,
            "mission_id": self.mission_id,
            "objectives": [list(o) for o in self.objectives],
            "fences": len(self.fences),
            "shows": len(self.shows),
            "animals": len(self.animals),
            "employees": len(self.employees),
            "buildings": len(self.buildings),
            "visitors": len(self.visitors),
            "dinosaurs": len(self.dinosaurs),
            "feature_flags": len(self.feature_flags),
            "building_flags": len(self.building_flags),
            "food_flags": len(self.food_flags),
            "misc_flags": len(self.misc_flags),
            "show_catalog_count": self.show_catalog_count,
            "building_kinds": [b.kind for b in self.buildings],
        }


@dataclass
class RoadRect:
    x0: int
    y0: int
    x1: int
    y1: int

    def to_dict(self) -> dict:
        return {"x0": self.x0, "y0": self.y0, "x1": self.x1, "y1": self.y1}


@dataclass
class WldFile:
    path: str
    version: int
    mode: int | None
    world: WorldBlock
    roads: list[RoadRect] = field(default_factory=list)
    camera: tuple[float, ...] | None = None
    size: int = 0
    simulation_offset: int = 0
    simulation_dinos: list = field(default_factory=list)
    food_sources: list = field(default_factory=list)
    employees: list = field(default_factory=list)
    attraction_count: int = 0
    simulation_end: int = 0

    def summary(self) -> dict:
        return {
            "path": self.path,
            "version": self.version,
            "mode": self.mode,
            "size": self.size,
            "world": self.world.summary(),
            "roads": len(self.roads),
            "simulation_offset": self.simulation_offset,
            "simulation_dinos": len(self.simulation_dinos),
            "food_sources": len(self.food_sources),
            "employees": len(self.employees),
            "attraction_count": self.attraction_count,
            "simulation_end": self.simulation_end,
            "camera": list(self.camera) if self.camera else None,
        }


_BUILDING_NAMES: list[str] | None = None


def _building_names() -> dict[int, str]:
    global _BUILDING_NAMES
    if _BUILDING_NAMES is not None:
        return _BUILDING_NAMES
    root = Path(__file__).resolve().parents[2]
    path = root / "game" / "%MAINDIR%" / "Data" / "Design" / "buildings" / "BuildingList.ini"
    names: dict[int, str] = {}
    if path.exists():
        lines = [ln.strip() for ln in path.read_text(encoding="latin-1").splitlines()]
        for line in lines:
            line = line.split("//", 1)[0].strip()
            if not line:
                continue
            try:
                int(line)
                continue
            except ValueError:
                names[len(names)] = line
    _BUILDING_NAMES = names
    return names


BUILDING_NAMES = _building_names()


class _Reader:
    def __init__(self, data: bytes, off: int, end: int, version: int, what: str):
        self.data = data
        self.off = off
        self.end = end
        self.version = version
        self.what = what

    def need(self, size: int) -> None:
        if size < 0 or self.off + size > self.end:
            raise WldFormatError(
                f"{self.what}: need {size} bytes at {self.off:#x} (end {self.end:#x})"
            )

    def u32(self) -> int:
        self.need(4)
        value = struct.unpack_from("<I", self.data, self.off)[0]
        self.off += 4
        return value

    def i32(self) -> int:
        self.need(4)
        value = struct.unpack_from("<i", self.data, self.off)[0]
        self.off += 4
        return value

    def u16(self) -> int:
        self.need(2)
        value = struct.unpack_from("<H", self.data, self.off)[0]
        self.off += 2
        return value

    def skip(self, size: int) -> None:
        self.need(size)
        self.off += size

    def count(self, limit: int = 200_000) -> int:
        value = self.u32()
        if value > limit:
            raise WldFormatError(f"{self.what}: implausible count {value} at {self.off - 4:#x}")
        return value


def _read_waypoints(r: _Reader) -> list[Waypoint]:
    count = r.count(10_000)
    points: list[Waypoint] = []
    for _ in range(count):
        # Version 2 omits the first u32 (it is synthesised as 0); versions
        # below 0x25 also store the second field as an offset from 0x1e61.
        flags = 0 if r.version < 3 else r.u32()
        key = r.u32()
        if r.version < 0x25:
            key += 0x1E61
        x = r.u32()
        y = r.u32()
        z = r.u32()
        extra = r.u32()
        points.append(Waypoint(flags, key, x, y, z, extra))
    return points


def _read_placed_base(r: _Reader, version: int) -> tuple[int, int, tuple[int, int], tuple[int, int], list[Waypoint]]:
    object_id = r.u32()
    field_14 = r.u32()
    start = (r.u32(), r.u32())
    end = (r.u32(), r.u32())
    if version > 1:
        waypoints = _read_waypoints(r)
    else:
        waypoints = []
    return object_id, field_14, start, end, waypoints


def _read_fence(r: _Reader) -> Fence:
    object_id, field_14, start, end, waypoints = _read_placed_base(r, r.version)
    first_14 = field_14
    kind = r.u32()
    field_14 = r.u32()
    # The subclass repeats the placement rectangle in every version; keep the
    # raw bytes so a writer can reproduce them.
    r.need(16)
    extra16 = bytes(r.data[r.off:r.off + 16])
    r.skip(16)
    field_44 = r.u32() if r.version >= 2 else 0
    field_48 = r.u32() if r.version >= 0x25 else 0
    return Fence(object_id, field_14, start, end, waypoints, kind, field_44,
                 field_48, first_14, extra16)


def _read_show(r: _Reader) -> Show:
    object_id, field_14, start, end, waypoints = _read_placed_base(r, r.version)
    class_tag = r.u32()
    field_44 = r.u32() if r.version >= 2 else 0
    field_48 = r.u32() if r.version >= 0x25 else 0
    if r.version < 2:
        # Legacy list of extra 0x18-byte records.
        count = r.count(10_000)
        r.skip(count * 0x18)
    return Show(object_id, field_14, start, end, waypoints, class_tag, field_44, field_48)


def _read_animal(r: _Reader) -> Animal:
    object_id = r.u32()
    field_1c = r.u32()
    field_20 = r.u32() if r.version > 0x32 else None
    return Animal(object_id, field_1c, field_20)


def _read_employee(r: _Reader) -> Employee:
    return Employee(r.u32(), r.u32())


def _read_building(r: _Reader) -> Building:
    object_id = r.u32()
    linked_id = r.u32()
    class_tag = r.u32()
    kind = r.u32()
    cell_x = r.u32()
    cell_y = r.u32()
    state = r.u32()
    # The per-object link is written by the list loop, not by the record.
    return Building(object_id, linked_id, kind, cell_x, cell_y, state, 0, class_tag)


def _read_visitor(r: _Reader) -> Visitor:
    object_id = r.u32()
    field_1c = r.u32()
    class_tag = r.u32()
    field_20 = r.u32()
    field_24 = r.u32()
    field_28 = r.u32() if r.version >= 5 else None
    return Visitor(object_id, field_1c, class_tag, field_20, field_24, field_28)


def _read_dinosaur(r: _Reader) -> Dinosaur:
    object_id = r.u32()
    field_1c = r.u32()
    if r.version >= 0x25:
        field_24 = r.u32()
        field_28 = r.u32()
        field_2c = r.u32()
    else:
        field_24 = field_28 = field_2c = None
    class_tag = r.u32()
    pairs = 18 if r.version > 0x15 else 16
    genotype = [(r.u32(), r.u32()) for _ in range(pairs)]
    return Dinosaur(object_id, field_1c, field_24, field_28, field_2c, class_tag, genotype)


def read_world_block(
    data: bytes,
    version: int,
    offset: int = 0,
    *,
    campaign: bool,
    end: int | None = None,
    what: str = "world",
) -> WorldBlock:
    """Parse one ``pwdWorldManager`` block.

    ``campaign`` is the value passed to ``0x0051e6c0``: ``True`` for a
    campaign ``.wld``, ``False`` for a save game.  Save games carry an
    objective list and a visitor list; campaign worlds omit both.
    """
    if version not in KNOWN_VERSIONS:
        raise WldFormatError(f"{what}: unknown version {version}")
    limit = len(data) if end is None else end
    r = _Reader(data, offset, limit, version, what)
    start = offset

    campaign_id: int | None = None
    mission_id: int | None = None
    objectives: list[tuple[int, int, int]] = []
    if not campaign:
        # 0x0055d710: campaign/mission ids, followed by the objective list
        # (0x0053c560's first list) for save games.
        campaign_id = r.u32()
        mission_id = r.u32()
        if version > 9:
            n = r.count(10_000)
            objectives = [(r.u32(), r.u32(), r.u32()) for _ in range(n)]

    fences = [_read_fence(r) for _ in range(r.count(10_000))]
    shows = [_read_show(r) for _ in range(r.count(10_000))]
    animals = [_read_animal(r) for _ in range(r.count(10_000))] if version > 1 else []
    employees = [_read_employee(r) for _ in range(r.count(10_000))] if version > 2 else []
    buildings = []
    for _ in range(r.count(10_000)):
        buildings.append(_read_building(r))
        buildings[-1].link = r.u32()  # the per-object link follows the record

    visitors: list[Visitor] = []
    if not campaign and not (6 <= version <= 0x10):
        visitors = [_read_visitor(r) for _ in range(r.count(100_000))]

    dinosaurs = [_read_dinosaur(r) for _ in range(r.count(100_000))] if version > 1 else []

    feature_flags = b""
    building_flags = b""
    food_flags = b""
    misc_flags = b""
    show_catalog_count = None
    if not campaign:
        # 0x00543d20: unlock flags for features, buildings, food sources and
        # six misc flags.  Campaign worlds skip this whole function.
        feature_count = r.count(10_000)
        feature_flags = bytes(r.data[r.off:r.off + feature_count])
        r.skip(feature_count)
        building_count = r.count(10_000)
        building_flags = bytes(r.data[r.off:r.off + building_count])
        r.skip(building_count)
        food_count = r.count(10_000)
        food_flags = bytes(r.data[r.off:r.off + food_count])
        r.skip(food_count)
        misc_count = r.count(10_000)
        misc_flags = bytes(r.data[r.off:r.off + misc_count])
        r.skip(misc_count)
        # The show catalog itself is parsed by sav.py; stop before its count.
        if version > 0x27:
            show_catalog_count = None

    return WorldBlock(
        offset=start,
        end=r.off,
        campaign_id=campaign_id,
        mission_id=mission_id,
        objectives=objectives,
        fences=fences,
        shows=shows,
        animals=animals,
        employees=employees,
        buildings=buildings,
        visitors=visitors,
        dinosaurs=dinosaurs,
        feature_flags=feature_flags,
        building_flags=building_flags,
        food_flags=food_flags,
        misc_flags=misc_flags,
        show_catalog_count=show_catalog_count,
    )


def read_wld(path: Path) -> WldFile:
    data = path.read_bytes()
    if len(data) < 8:
        raise WldFormatError(f"{path}: shorter than header")
    version = struct.unpack_from("<I", data, 0)[0]
    if version not in KNOWN_VERSIONS:
        raise WldFormatError(f"{path}: unknown .wld version {version}")
    off = 4
    mode = None
    if version > 10:
        if len(data) < 8:
            raise WldFormatError(f"{path}: missing world mode")
        mode = struct.unpack_from("<I", data, off)[0]
        off += 4
    body_end = len(data)
    camera = None
    if version >= 50:
        if len(data) < off + CAMERA_BYTES:
            raise WldFormatError(f"{path}: missing camera tail")
        camera = struct.unpack_from("<10f", data, len(data) - CAMERA_BYTES)
        body_end = len(data) - CAMERA_BYTES
    world = read_world_block(
        data, version, off, campaign=True, end=body_end, what=str(path)
    )
    roads: list[RoadRect] = []
    r = _Reader(data, world.end, body_end, version, str(path))
    road_count = r.count(100_000)
    for _ in range(road_count):
        roads.append(RoadRect(r.i32(), r.i32(), r.i32(), r.i32()))
    simulation_offset = r.off
    dinos: list = []
    food: list = []
    employees: list = []
    attraction_count = 0
    simulation_end = simulation_offset
    if version > 2:
        from sav_campaign import (  # local import keeps wld.py standalone
            read_campaign_list0,
            read_campaign_list1,
            read_campaign_list2,
        )

        dino_count = r.count(10_000)
        dinos, end = read_campaign_list0(data, r.off, dino_count)
        food_count = struct.unpack_from("<I", data, end)[0]
        food, end = read_campaign_list1(data, end + 4, food_count)
        employee_count = struct.unpack_from("<I", data, end)[0]
        employees, simulation_end = read_campaign_list2(
            data, end + 4, employee_count)
        if simulation_end + 4 <= body_end:
            attraction_count = struct.unpack_from(
                "<I", data, simulation_end)[0]
    return WldFile(
        str(path),
        version,
        mode,
        world,
        roads,
        camera,
        size=len(data),
        simulation_offset=simulation_offset,
        simulation_dinos=dinos,
        food_sources=food,
        employees=employees,
        attraction_count=attraction_count,
        simulation_end=simulation_end,
    )


def read_sav_world(path: Path) -> WorldBlock:
    """Parse the world block of a retail ``.sav`` and verify its end."""
    from sav import read_sav  # local import keeps wld.py standalone

    save = read_sav(path)
    data = save.data
    name_len = struct.unpack_from("<I", data, 8)[0]
    off = 0x18 + name_len
    world = read_world_block(
        data,
        save.version,
        off,
        campaign=False,
        end=save.count_offset,
        what=str(path),
    )
    if world.end != save.count_offset:
        raise WldFormatError(
            f"{path}: world block ends at {world.end:#x}, show catalog at {save.count_offset:#x}"
        )
    return world


def scan(root: Path) -> tuple[list[WldFile], list[tuple[str, str]]]:
    good: list[WldFile] = []
    bad: list[tuple[str, str]] = []
    for path in sorted(root.rglob("*.wld")):
        try:
            good.append(read_wld(path))
        except Exception as exc:
            bad.append((str(path), str(exc)))
    return good, bad


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--dump", type=Path)
    ap.add_argument("--scan", type=Path)
    ap.add_argument("--sav", type=Path)
    ap.add_argument("--objects", action="store_true", help="list every object")
    args = ap.parse_args(argv)
    if args.dump:
        print(json.dumps(read_wld(args.dump).summary(), indent=2))
        return 0
    if args.sav:
        print(json.dumps(read_sav_world(args.sav).summary(), indent=2))
        return 0
    if args.scan:
        good, bad = scan(args.scan)
        print(f"scanned {len(good) + len(bad)} .wld files, {len(bad)} bad")
        for w in good:
            s = w.summary()
            print(
                f"{w.path}: v{w.version}, {w.size} bytes, mode {w.mode}, "
                f"fences {s['world']['fences']}, shows {s['world']['shows']}, "
                f"buildings {s['world']['buildings']}, dinos {s['world']['dinosaurs']}, "
                f"roads {s['roads']}"
            )
        for path, why in bad:
            print(f"FAIL {path}: {why}")
        return 0 if not bad else 1
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
