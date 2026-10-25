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

The park block starts with a 0x28-byte object header; the serialized
``spPark`` fields begin at ``block + 0x28``.  The budget is the u32 at
``block + 0x30`` (relative park offset 0x08).

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


@dataclass
class ParkState:
    offset: int
    budget: int
    sellback_factor: float
    field_0c: int
    field_10: int
    field_14: int
    field_18: int
    field_20: int
    counts_24: tuple[int, ...]
    counts_58: tuple[int, ...]
    raw: bytes

    def to_dict(self) -> dict:
        return {
            "offset": self.offset,
            "budget": self.budget,
            "sellback_factor": self.sellback_factor,
            "field_0c": self.field_0c,
            "field_10": self.field_10,
            "field_14": self.field_14,
            "field_18": self.field_18,
            "field_20": self.field_20,
            "counts_24": list(self.counts_24),
            "counts_58": list(self.counts_58),
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
    base = 0x28
    park = ParkState(
        offset=off,
        budget=_u32(block, base + 0x08, "park budget"),
        sellback_factor=_f32(block, base + 0x0C, "park sellback factor"),
        field_0c=_u32(block, base + 0x0C, "park +0x0c"),
        field_10=_u32(block, base + 0x10, "park +0x10"),
        field_14=_u32(block, base + 0x14, "park +0x14"),
        field_18=_u32(block, base + 0x18, "park +0x18"),
        field_20=_u32(block, base + 0x20, "park +0x20"),
        counts_24=tuple(
            _u32(block, base + 0x24 + i * 4, f"park counts_24[{i}]") for i in range(13)
        ),
        counts_58=tuple(
            _u32(block, base + 0x58 + i * 4, f"park counts_58[{i}]") for i in range(13)
        ),
        raw=block,
    )
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
