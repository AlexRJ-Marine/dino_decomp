#!/usr/bin/env python3
"""Reader for the simulation-state prefix of Dino Island ``.sav`` files.

The save is a sequence of manager sections.  ``sav.read_sav`` already returns
the header and the 39-show catalog; the catalog is the tail of the
``pwdWorldManager`` section.  This module decodes the next manager section,
``spSimuManager``, up to the point where the AI/scenario tail starts.

Layout (version 53, as produced by the retail game)::

    u32 road_count
    road_count x (s32 x0, s32 y0, s32 x1, s32 y1)

    u32 list0_count             # usually 0 in retail saves
    u32 list1_count             # usually 0
    u32 list2_count             # usually 0

    u32 visitor_count
    visitor_count x 0x50-byte visitor record

    u32 park_count              # 1 in retail saves
    park_count x 0x293-byte park block

    u32 dino_store_count        # 20
    dino_store_count x record:
        u32 name_bytes (UTF-16LE, NUL included)
        UTF-16LE name
        u32 store_id
        u32 path_bytes (ASCII, NUL included)
        ASCII path
        u32 description_bytes (ASCII, NUL included)
        ASCII description

    u32 visitor_model[5]        # five capability/profile values
    u32 lab_count
    lab_count x u8 present_flag # 1 = empty; 0 = genotype data follows
    ...                         # AI/scenario tail, not covered here

The park block has a 0x30-byte header.  The economy field stream begins at
``block + 0x30`` and stores fields in **writer order**, not at their runtime
offsets (writer ``0x00585780``): budget, sellback, FOOD, staff, ticket,
staff buckets, shop arrays, groups.  See ``docs/formats/sav.md``.

See ``docs/formats/sav.md`` and ``notes/save-graph-recon.md``.
"""
from __future__ import annotations

import argparse
import json
import struct
from dataclasses import dataclass, field
from pathlib import Path

from sav import SaveFile, SavFormatError, read_sav

PARK_BLOCK_SIZE = 0x293
VISITOR_RECORD_SIZE = 0x50
DINO_COUNT = 20


class SavGraphError(SavFormatError):
    pass


@dataclass
class RoadRect:
    x0: int
    y0: int
    x1: int
    y1: int

    def to_dict(self) -> dict:
        return {"x0": self.x0, "y0": self.y0, "x1": self.x1, "y1": self.y1}


@dataclass
class Visitor:
    object_id: int
    x: int
    y: int
    flags: int
    unknown_10: int
    state: int
    fx: float
    fy: float
    dx: float
    dy: float
    unknown_28: int
    zero_2c: int
    visitor_id: int
    needs: tuple[int, ...]

    def to_dict(self) -> dict:
        return {
            "object_id": self.object_id,
            "x": self.x,
            "y": self.y,
            "flags": self.flags,
            "state": self.state,
            "fx": self.fx,
            "fy": self.fy,
            "dx": self.dx,
            "dy": self.dy,
            "visitor_id": self.visitor_id,
            "needs": list(self.needs),
        }


PARK_HEADER_SIZE = 0x30
SHOP_KINDS = 13


@dataclass
class ParkState:
    offset: int
    budget: int
    sellback_factor: float
    food_total: int
    food_count: int
    staff_secu_total: int
    staff_secu_count: int
    ticket_total: int
    ticket_count: int
    bankruptcy_flags: tuple[int, int]
    staff_buckets: tuple[int, int]
    shop_revenue: tuple[int, ...]
    shop_sales: tuple[int, ...]
    group_a_count: int
    group_a_total: int
    group_b_count: int
    group_b_total: int
    group_c_count: int
    group_c_total: int
    generic_count: int
    generic_total: int
    yesterday_staff_buckets: tuple[int, int]
    yesterday_food: tuple[int, int]
    yesterday_secu: tuple[int, int]
    yesterday_revenue: tuple[int, ...]
    yesterday_sales: tuple[int, ...]
    yesterday_totals: tuple[int, ...]
    raw: bytes

    def to_dict(self) -> dict:
        return {
            "offset": self.offset,
            "budget": self.budget,
            "sellback_factor": self.sellback_factor,
            "food": [self.food_total, self.food_count],
            "staff_secu": [self.staff_secu_total, self.staff_secu_count],
            "ticket": [self.ticket_total, self.ticket_count],
            "bankruptcy_flags": list(self.bankruptcy_flags),
            "staff_buckets": list(self.staff_buckets),
            "shop_revenue": list(self.shop_revenue),
            "shop_sales": list(self.shop_sales),
            "groups": {
                "dinoachat": [self.group_a_count, self.group_a_total],
                "construction": [self.group_b_count, self.group_b_total],
                "laboaction": [self.group_c_count, self.group_c_total],
                "ventes": [self.generic_count, self.generic_total],
            },
            "yesterday": {
                "staff_buckets": list(self.yesterday_staff_buckets),
                "food": list(self.yesterday_food),
                "secu": list(self.yesterday_secu),
                "revenue": list(self.yesterday_revenue),
                "sales": list(self.yesterday_sales),
                "totals": list(self.yesterday_totals),
            },
        }


@dataclass
class DinoStoreEntry:
    name: str
    store_id: int
    path: str
    description: str

    def to_dict(self) -> dict:
        return {
            "name": self.name,
            "store_id": self.store_id,
            "path": self.path,
            "description": self.description,
        }


@dataclass
class SavGraph:
    save: SaveFile
    road: list[RoadRect] = field(default_factory=list)
    visitors: list[Visitor] = field(default_factory=list)
    park: ParkState | None = None
    dino_store: list[DinoStoreEntry] = field(default_factory=list)
    visitor_model: tuple[int, ...] = ()
    lab_count: int = 0
    lab_flags: bytes = b""
    ai_counts: tuple[int, ...] = ()
    ai_entries: list[bytes] = field(default_factory=list)
    camera: tuple[float, ...] = ()
    tail_offset: int = 0
    tail_extra: bytes = b""

    @property
    def tail(self) -> bytes:
        """Raw AI + camera + trailing bytes (kept for older callers)."""
        raw = b""
        for value in self.ai_counts:
            raw += struct.pack("<I", value)
        raw += b"".join(self.ai_entries)
        raw += struct.pack("<" + "f" * len(self.camera), *self.camera)
        return raw + self.tail_extra

    def summary(self) -> dict:
        return {
            "path": self.save.path,
            "version": self.save.version,
            "shows": len(self.save.shows),
            "road_rects": len(self.road),
            "visitors": len(self.visitors),
            "budget": self.park.budget if self.park else None,
            "dino_store": len(self.dino_store),
            "visitor_model": list(self.visitor_model),
            "lab_count": self.lab_count,
            "ai_counts": list(self.ai_counts),
            "ai_entries": len(self.ai_entries),
            "camera": list(self.camera),
            "tail_bytes": len(self.tail),
        }


def _need(data: bytes, off: int, size: int, what: str) -> None:
    if off < 0 or off + size > len(data):
        raise SavGraphError(f"{what}: need {size} bytes at {off:#x}")


def _u32(data: bytes, off: int, what: str) -> int:
    _need(data, off, 4, what)
    return struct.unpack_from("<I", data, off)[0]


def _i32(data: bytes, off: int, what: str) -> int:
    _need(data, off, 4, what)
    return struct.unpack_from("<i", data, off)[0]


def _f32(data: bytes, off: int, what: str) -> float:
    _need(data, off, 4, what)
    return struct.unpack_from("<f", data, off)[0]


def _string(data: bytes, off: int, size: int, encoding: str, what: str) -> tuple[str, int]:
    _need(data, off, size, what)
    raw = data[off:off + size]
    try:
        text = raw.decode(encoding)
    except UnicodeDecodeError as exc:
        raise SavGraphError(f"{what}: {exc}") from exc
    return text.rstrip("\x00"), off + size


def _read_visitor(data: bytes, off: int) -> Visitor:
    _need(data, off, VISITOR_RECORD_SIZE, "visitor record")
    return Visitor(
        object_id=_u32(data, off, "visitor object id"),
        x=_i32(data, off + 0x04, "visitor x"),
        y=_i32(data, off + 0x08, "visitor y"),
        flags=_u32(data, off + 0x0C, "visitor flags"),
        unknown_10=_u32(data, off + 0x10, "visitor +0x10"),
        state=_u32(data, off + 0x14, "visitor state"),
        fx=_f32(data, off + 0x18, "visitor fx"),
        fy=_f32(data, off + 0x1C, "visitor fy"),
        dx=_f32(data, off + 0x20, "visitor dx"),
        dy=_f32(data, off + 0x24, "visitor dy"),
        unknown_28=_u32(data, off + 0x28, "visitor +0x28"),
        zero_2c=_u32(data, off + 0x2C, "visitor +0x2c"),
        visitor_id=_u32(data, off + 0x30, "visitor id"),
        needs=tuple(_u32(data, off + 0x34 + i * 4, f"visitor need {i}") for i in range(7)),
    )


def _read_park_stream(block: bytes, file_offset: int) -> ParkState:
    """Decode the economy field stream written by ``0x00585780``.

    The stream starts after the 0x30-byte park header and stores fields in
    writer order, not at their runtime offsets.
    """
    if len(block) < PARK_HEADER_SIZE:
        raise SavGraphError(f"park block too short: {len(block)} bytes")
    pos = PARK_HEADER_SIZE

    def need(size: int, what: str) -> None:
        _need(block, pos, size, what)

    def u32(what: str) -> int:
        nonlocal pos
        need(4, what)
        value = struct.unpack_from("<I", block, pos)[0]
        pos += 4
        return value

    def f32(what: str) -> float:
        nonlocal pos
        need(4, what)
        value = struct.unpack_from("<f", block, pos)[0]
        pos += 4
        return value

    def u8(what: str) -> int:
        nonlocal pos
        need(1, what)
        value = block[pos]
        pos += 1
        return value

    budget = u32("budget")
    sellback = f32("sellback")
    food_total = u32("food total")
    food_count = u32("food count")
    secu_total = u32("staff secu total")
    secu_count = u32("staff secu count")
    ticket_total = u32("ticket total")
    ticket_count = u32("ticket count")
    flags = (u8("bankruptcy flag 0"), u8("bankruptcy flag 1"))
    staff_buckets = (u32("staff bucket 0"), u32("staff bucket 1"))
    shop_revenue = tuple(u32(f"shop revenue[{i}]") for i in range(SHOP_KINDS))
    shop_sales = tuple(u32(f"shop sales[{i}]") for i in range(SHOP_KINDS))
    group_a_count = u32("dinoachat count")
    group_a_total = u32("dinoachat total")
    group_b_count = u32("construction count")
    group_b_total = u32("construction total")
    group_c_count = u32("laboaction count")
    group_c_total = u32("laboaction total")
    generic_count = u32("ventes count")
    generic_total = u32("ventes total")
    yesterday_staff = (u32("yesterday staff 0"), u32("yesterday staff 1"))
    yesterday_food = (u32("yesterday food total"), u32("yesterday food count"))
    yesterday_secu = (u32("yesterday secu total"), u32("yesterday secu count"))
    yesterday_revenue = tuple(u32(f"yesterday revenue[{i}]") for i in range(SHOP_KINDS))
    yesterday_sales = tuple(u32(f"yesterday sales[{i}]") for i in range(SHOP_KINDS))
    yesterday_totals = tuple(u32(f"yesterday total[{i}]") for i in range(13))
    return ParkState(
        offset=file_offset,
        budget=budget,
        sellback_factor=sellback,
        food_total=food_total,
        food_count=food_count,
        staff_secu_total=secu_total,
        staff_secu_count=secu_count,
        ticket_total=ticket_total,
        ticket_count=ticket_count,
        bankruptcy_flags=flags,
        staff_buckets=staff_buckets,
        shop_revenue=shop_revenue,
        shop_sales=shop_sales,
        group_a_count=group_a_count,
        group_a_total=group_a_total,
        group_b_count=group_b_count,
        group_b_total=group_b_total,
        group_c_count=group_c_count,
        group_c_total=group_c_total,
        generic_count=generic_count,
        generic_total=generic_total,
        yesterday_staff_buckets=yesterday_staff,
        yesterday_food=yesterday_food,
        yesterday_secu=yesterday_secu,
        yesterday_revenue=yesterday_revenue,
        yesterday_sales=yesterday_sales,
        yesterday_totals=yesterday_totals,
        raw=block,
    )


def read_sav_graph(path: Path) -> SavGraph:
    save = read_sav(path)
    data = save.state
    off = 0

    road_count = _u32(data, off, "road count")
    off += 4
    if road_count > 4096:
        raise SavGraphError(f"road count {road_count} is implausible")
    road: list[RoadRect] = []
    for i in range(road_count):
        _need(data, off, 16, f"road rect {i}")
        road.append(RoadRect(*struct.unpack_from("<4i", data, off)))
        off += 16

    for name in ("list0", "list1", "list2"):
        count = _u32(data, off, f"{name} count")
        off += 4
        if count != 0:
            raise SavGraphError(
                f"{name} has {count} objects; only the empty retail form is decoded"
            )

    visitor_count = _u32(data, off, "visitor count")
    off += 4
    if visitor_count > 10000:
        raise SavGraphError(f"visitor count {visitor_count} is implausible")
    visitors = [_read_visitor(data, off + i * VISITOR_RECORD_SIZE) for i in range(visitor_count)]
    off += visitor_count * VISITOR_RECORD_SIZE

    park_count = _u32(data, off, "park count")
    off += 4
    if park_count != 1:
        raise SavGraphError(f"park count {park_count}; only one park block is decoded")

    _need(data, off, PARK_BLOCK_SIZE, "park block")
    block = data[off:off + PARK_BLOCK_SIZE]
    park = _read_park_stream(block, off)
    off += PARK_BLOCK_SIZE

    dino_count = _u32(data, off, "dino store count")
    off += 4
    if dino_count > 1000:
        raise SavGraphError(f"dino store count {dino_count} is implausible")
    dinos: list[DinoStoreEntry] = []
    for i in range(dino_count):
        name_len = _u32(data, off, f"dino {i} name length")
        off += 4
        name, off = _string(data, off, name_len, "utf-16-le", f"dino {i} name")
        store_id = _u32(data, off, f"dino {i} store id")
        off += 4
        path_len = _u32(data, off, f"dino {i} path length")
        off += 4
        dino_path, off = _string(data, off, path_len, "ascii", f"dino {i} path")
        desc_len = _u32(data, off, f"dino {i} description length")
        off += 4
        desc, off = _string(data, off, desc_len, "ascii", f"dino {i} description")
        dinos.append(DinoStoreEntry(name, store_id, dino_path, desc))

    visitor_model = tuple(_u32(data, off + i * 4, f"visitor model {i}") for i in range(5))
    off += 20

    lab_count = _u32(data, off, "lab count")
    off += 4
    if lab_count > 4096:
        raise SavGraphError(f"lab count {lab_count} is implausible")
    _need(data, off, lab_count, "lab flags")
    lab_flags = data[off:off + lab_count]
    off += lab_count
    if any(flag == 0 for flag in lab_flags):
        raise SavGraphError(
            f"lab store at {off - lab_count:#x} contains populated genotype records; "
            "their variable-length layout is not decoded"
        )

    tail_offset = off
    ai_counts = tuple(_u32(data, off + i * 4, f"AI list {i} count") for i in range(5))
    off += 20
    if any(count != 0 for count in ai_counts[:4]):
        raise SavGraphError(
            f"AI lists {ai_counts[:4]} contain objects; only the empty retail form is decoded"
        )

    # The fifth AI list has a count; only non-auto saves carry its 17-byte
    # entries.  Auto saves go straight to the 40-byte camera state.
    ai_entries: list[bytes] = []
    auto_save = path.name.lower() == "auto.sav" or path.parent.name.lower() == "auto"
    if not auto_save:
        count = ai_counts[4]
        if count > 10000:
            raise SavGraphError(f"AI entry count {count} is implausible")
        _need(data, off, count * 17, "AI entries")
        ai_entries = [data[off + i * 17:off + (i + 1) * 17] for i in range(count)]
        off += count * 17

    _need(data, off, 40, "camera state")
    camera = tuple(struct.unpack_from("<10f", data, off))
    off += 40
    tail_extra = data[off:]

    return SavGraph(
        save=save,
        road=road,
        visitors=visitors,
        park=park,
        dino_store=dinos,
        visitor_model=visitor_model,
        lab_count=lab_count,
        lab_flags=lab_flags,
        ai_counts=ai_counts,
        ai_entries=ai_entries,
        camera=camera,
        tail_offset=tail_offset,
        tail_extra=tail_extra,
    )


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("save", type=Path)
    ap.add_argument("--visitors", action="store_true", help="list visitor records")
    ap.add_argument("--dinos", action="store_true", help="list dinosaur-store records")
    args = ap.parse_args(argv)
    graph = read_sav_graph(args.save)
    if args.visitors:
        for v in graph.visitors:
            print(json.dumps(v.to_dict(), indent=2))
    if args.dinos:
        for d in graph.dino_store:
            print(f"{d.name:16} id={d.store_id:6} {d.path}")
    print(json.dumps(graph.summary(), indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
