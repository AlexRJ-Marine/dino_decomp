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
        u32 price
        u32 path_bytes (ASCII, NUL included)
        ASCII path
        u32 description_bytes (ASCII, NUL included)
        ASCII description

    u32 visitor_model[5]        # five capability/profile values
    u32 lab_count
    lab_count x record:
        u8  present_flag        # 1 = empty slot; 0 = populated, rest follows
        if present_flag == 0:
            18 x (u32 allele_a, u32 allele_b)   # 0x90-byte genotype
            u32 name_bytes (ASCII, NUL included)
            ASCII name
    ...                         # AI/scenario tail, not covered here

The park block has a 0x30-byte header.  The economy field stream begins at
``block + 0x30`` and stores fields in **writer order**, not at their runtime
offsets (writer ``0x00585780``): budget, sellback, FOOD, staff, ticket,
staff buckets, shop arrays, groups, the yesterday block and the cumulative
block.  The stream is 0x21a bytes long.  See ``docs/formats/sav.md``.

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
GENOTYPE_GENES = 18


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
PARK_TRAILER_OFFSET = 0x3D  # relative to the schedule tail (block + 0x24a)
SHOP_KINDS = 13


@dataclass
class ParkState:
    offset: int
    class_id: int
    header_a: int
    header_b: int
    header_c: float
    header_reserved: bytes
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
    cumulative_staff_buckets: tuple[int, int]
    cumulative_food: tuple[int, int]
    cumulative_secu: tuple[int, int]
    cumulative_revenue: tuple[int, ...]
    cumulative_sales: tuple[int, ...]
    cumulative_totals: tuple[int, ...]
    stream_end: int
    schedule_day: int
    schedule_elapsed: float
    schedule_current: float
    schedule_field: int
    schedule_tasks: tuple[int, ...]
    trailer: bytes
    tail_final: int
    tail: bytes
    raw: bytes

    def to_dict(self) -> dict:
        return {
            "offset": self.offset,
            "class_id": self.class_id,
            "header": {
                "a": self.header_a,
                "b": self.header_b,
                "c": self.header_c,
                "reserved": self.header_reserved.hex(),
            },
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
            "cumulative": {
                "staff_buckets": list(self.cumulative_staff_buckets),
                "food": list(self.cumulative_food),
                "secu": list(self.cumulative_secu),
                "revenue": list(self.cumulative_revenue),
                "sales": list(self.cumulative_sales),
                "totals": list(self.cumulative_totals),
            },
            "stream_end": self.stream_end,
            "schedule": {
                "day": self.schedule_day,
                "elapsed": self.schedule_elapsed,
                "current": self.schedule_current,
                "field_18": self.schedule_field,
                "tasks": list(self.schedule_tasks),
                "trailer": self.trailer.hex(),
                "final": self.tail_final,
            },
            "tail": self.tail.hex(),
        }


@dataclass
class DinoStoreEntry:
    name: str
    price: int
    path: str
    description: str

    def to_dict(self) -> dict:
        return {
            "name": self.name,
            "price": self.price,
            "path": self.path,
            "description": self.description,
        }


@dataclass
class LabRecord:
    """One ``spLab`` genotype-store slot.

    ``flag`` is the byte read by ``0x00592a00``: 1 marks an empty slot and 0
    marks a populated one.  A populated slot is followed by 18 allele pairs
    (the same 18-gene genotype as ``.gen``) and an ASCII display name.
    """

    flag: int
    genes: tuple[tuple[int, int], ...] = ()
    name: str = ""

    @property
    def empty(self) -> bool:
        return self.flag != 0

    def to_dict(self) -> dict:
        return {
            "flag": self.flag,
            "empty": self.empty,
            "genes": [[a, b] for a, b in self.genes],
            "name": self.name,
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
    lab_records: list[LabRecord] = field(default_factory=list)
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
            "lab_populated": sum(1 for record in self.lab_records if not record.empty),
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
    """Decode the 0x293-byte park record.

    The record is a fixed header followed by three writer-ordered streams::

        0x000  u32 class id (2)
        0x004  u32 header_a (0 manual / 3 autosave)
        0x008  u32 header_b (100 in both retail saves)
        0x00c  f32 header_c (10.0 manual / 0.0 autosave)
        0x010  header_reserved (32 bytes, zero in retail saves)

        0x030  economy stream, 0x21a bytes (writer ``0x00585780``)
        0x24a  schedule manager, 0x3c bytes (writer ``0x00594cb0``):
               u32 day, f32 elapsed, f32 current, u32 field_18,
               11 x u32 task slots
        0x286  12-byte park field ``spPark+0x148``
        0x292  1 trailing byte (0x01 in both retail saves)

    The economy stream stores fields in writer order, not at their runtime
    offsets, and ends with the cumulative counter block (the 180-byte "second
    block" that ``0x00585240`` reads back into the yesterday fields in the
    retail binary -- a loader bug, not reproduced here).
    """
    if len(block) < PARK_HEADER_SIZE:
        raise SavGraphError(f"park block too short: {len(block)} bytes")
    class_id = struct.unpack_from("<I", block, 0x00)[0]
    header_a = struct.unpack_from("<I", block, 0x04)[0]
    header_b = struct.unpack_from("<I", block, 0x08)[0]
    header_c = struct.unpack_from("<f", block, 0x0C)[0]
    header_reserved = block[0x10:PARK_HEADER_SIZE]
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
    cumulative_staff = (u32("cumulative staff 0"), u32("cumulative staff 1"))
    cumulative_food = (u32("cumulative food total"), u32("cumulative food count"))
    cumulative_secu = (u32("cumulative secu total"), u32("cumulative secu count"))
    cumulative_revenue = tuple(u32(f"cumulative revenue[{i}]") for i in range(SHOP_KINDS))
    cumulative_sales = tuple(u32(f"cumulative sales[{i}]") for i in range(SHOP_KINDS))
    cumulative_totals = tuple(u32(f"cumulative total[{i}]") for i in range(13))
    stream_end = pos
    tail = block[pos:]

    # Schedule manager tail: 12-byte time manager, then the task vector's
    # +0x18 field and its fixed 11 slots, then the 12-byte spPark+0x148 field
    # and one trailing byte.  The vector length is not stored, so derive it
    # from the tail size (0x49 bytes in both retail saves -> 11 slots).
    if len(tail) < 0x1D:
        raise SavGraphError(f"park schedule tail too short: {len(tail)} bytes")
    schedule_day = struct.unpack_from("<I", tail, 0x00)[0]
    schedule_elapsed = struct.unpack_from("<f", tail, 0x04)[0]
    schedule_current = struct.unpack_from("<f", tail, 0x08)[0]
    schedule_field = struct.unpack_from("<I", tail, 0x0C)[0]
    slot_count = (len(tail) - 0x1D) // 4
    schedule_tasks = tuple(
        struct.unpack_from("<I", tail, 0x10 + i * 4)[0] for i in range(slot_count)
    )
    trailer_at = 0x10 + slot_count * 4
    trailer = tail[trailer_at:trailer_at + 12]
    tail_final = tail[trailer_at + 12]

    return ParkState(
        offset=file_offset,
        class_id=class_id,
        header_a=header_a,
        header_b=header_b,
        header_c=header_c,
        header_reserved=header_reserved,
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
        cumulative_staff_buckets=cumulative_staff,
        cumulative_food=cumulative_food,
        cumulative_secu=cumulative_secu,
        cumulative_revenue=cumulative_revenue,
        cumulative_sales=cumulative_sales,
        cumulative_totals=cumulative_totals,
        stream_end=stream_end,
        schedule_day=schedule_day,
        schedule_elapsed=schedule_elapsed,
        schedule_current=schedule_current,
        schedule_field=schedule_field,
        schedule_tasks=schedule_tasks,
        trailer=trailer,
        tail_final=tail_final,
        tail=tail,
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
        price = _u32(data, off, f"dino {i} price")
        off += 4
        path_len = _u32(data, off, f"dino {i} path length")
        off += 4
        dino_path, off = _string(data, off, path_len, "ascii", f"dino {i} path")
        desc_len = _u32(data, off, f"dino {i} description length")
        off += 4
        desc, off = _string(data, off, desc_len, "ascii", f"dino {i} description")
        dinos.append(DinoStoreEntry(name, price, dino_path, desc))

    visitor_model = tuple(_u32(data, off + i * 4, f"visitor model {i}") for i in range(5))
    off += 20

    lab_count = _u32(data, off, "lab count")
    off += 4
    if lab_count > 4096:
        raise SavGraphError(f"lab count {lab_count} is implausible")
    labs: list[LabRecord] = []
    for i in range(lab_count):
        _need(data, off, 1, f"lab {i} flag")
        flag = data[off]
        off += 1
        if flag != 0:
            labs.append(LabRecord(flag=flag))
            continue
        gene_bytes = GENOTYPE_GENES * 8
        _need(data, off, gene_bytes, f"lab {i} genotype")
        genes = tuple(
            struct.unpack_from("<II", data, off + g * 8)
            for g in range(GENOTYPE_GENES)
        )
        off += gene_bytes
        name_len = _u32(data, off, f"lab {i} name length")
        off += 4
        if name_len > 1024:
            raise SavGraphError(f"lab {i} name length {name_len} is implausible")
        name, off = _string(data, off, name_len, "ascii", f"lab {i} name")
        labs.append(LabRecord(flag=flag, genes=genes, name=name))
    lab_flags = bytes(record.flag for record in labs)

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
        lab_records=labs,
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
    ap.add_argument("--park", action="store_true", help="dump the decoded park state")
    ap.add_argument("--labs", action="store_true", help="list lab genotype-store records")
    args = ap.parse_args(argv)
    graph = read_sav_graph(args.save)
    if args.park and graph.park is not None:
        print(json.dumps(graph.park.to_dict(), indent=2))
    if args.labs:
        for record in graph.lab_records:
            print(json.dumps(record.to_dict(), indent=2))
    if args.visitors:
        for v in graph.visitors:
            print(json.dumps(v.to_dict(), indent=2))
    if args.dinos:
        for d in graph.dino_store:
            print(f"{d.name:16} price={d.price:6} {d.path}")
    print(json.dumps(graph.summary(), indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
