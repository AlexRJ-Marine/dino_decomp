#!/usr/bin/env python3
"""Typed reader/writer for the campaign ``list0`` (``saDinosaur``) records.

The populated simulation ``list0`` of a campaign autosave holds ``saDinosaur``
objects serialized by ``0x00570c90`` and read by ``0x005706c0`` (class vtable
``0x00643124``, slots ``+0x14`` save / ``+0x10`` load). There is **no record
length prefix**: the record is the packed 0x19-byte prefix, the UTF-16 name,
and the schema-driven tail. The only variable-width tail fields are the name,
the ``+0x518`` ASCII string and the path-node vector; everything else is
fixed, so the reader consumes the schema in order.

Layout evidence and the full field map: ``notes/campaign-list0-tail-recon.md``.
Sample: ``build/campaign-autosave.sav`` (version 53, 20 records). The writer is
byte-identical on every record of that sample; ``tests/test_sav_campaign.py``
guards it.
"""
from __future__ import annotations

import argparse
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from sav import read_sav  # noqa: E402

GAUGE_BYTES = 0xB0
PATH_WAYPOINTS = 44
NODE_BYTES = 22


class SavCampaignError(ValueError):
    pass


def _need(data: bytes, off: int, size: int, what: str) -> None:
    if off < 0 or off + size > len(data):
        raise SavCampaignError(f"{what}: need {size} bytes at {off:#x}")


def _u8(data: bytes, off: int, what: str) -> int:
    _need(data, off, 1, what)
    return data[off]


def _u16(data: bytes, off: int, what: str) -> int:
    _need(data, off, 2, what)
    return struct.unpack_from("<H", data, off)[0]


def _u32(data: bytes, off: int, what: str) -> int:
    _need(data, off, 4, what)
    return struct.unpack_from("<I", data, off)[0]


def _i32(data: bytes, off: int, what: str) -> int:
    _need(data, off, 4, what)
    return struct.unpack_from("<i", data, off)[0]


def _f32(data: bytes, off: int, what: str) -> float:
    _need(data, off, 4, what)
    return struct.unpack_from("<f", data, off)[0]


def _take(data: bytes, off: int, size: int, what: str) -> bytes:
    _need(data, off, size, what)
    return data[off:off + size]


@dataclass
class CampaignPathNode:
    """One 22-byte path node (``f32 x, f32 y, f32 metric, u8, u8, u32, u32``)."""

    x: float
    y: float
    metric: float
    field_0c: int
    field_0d: int
    field_0e: int
    field_12: int

    def to_dict(self) -> dict:
        return {
            "x": self.x,
            "y": self.y,
            "metric": self.metric,
            "field_0c": self.field_0c,
            "field_0d": self.field_0d,
            "field_0e": self.field_0e,
            "field_12": self.field_12,
        }


@dataclass
class CampaignPath:
    """The embedded path object at runtime ``saDinosaur+0x2c8``.

    Serialization starts at runtime ``+0x04`` (the vtable pointer is not
    stored). The 44 waypoint slots are a fixed constructor-capacity array and
    carry no count; only the node vector is counted.
    """

    position: tuple[float, float]
    target: tuple[float, float]
    cells: tuple[int, int]
    direction: tuple[float, float]
    timeline: tuple[float, ...]        # 9 floats (runtime +0x24..+0x47)
    sentinel: int                     # 0x7f7fc99e in the sample
    field_4c: int
    field_4e: int
    field_50: int
    direction2: tuple[float, float]
    cell_x: int
    cell_y: int
    field_64: bytes                   # four bytes
    waypoints: tuple[tuple[int, int], ...]
    field_1c8: int
    field_1cc: int
    nodes: tuple[CampaignPathNode, ...]
    trail_1ec: int
    trail_1f0: int
    trail_1f4: int
    trail_1f8: int
    trail_208: int
    trail_20c: int
    trail_210: int
    trail_214: tuple[float, float]
    trail_21c: int
    trail_220: int
    trail_224: int
    trail_228: tuple[float, float]
    trail_1d0: int
    trail_1d4: int

    def to_dict(self) -> dict:
        return {
            "position": list(self.position),
            "target": list(self.target),
            "cells": list(self.cells),
            "direction": list(self.direction),
            "timeline": list(self.timeline),
            "sentinel": self.sentinel,
            "field_4c": self.field_4c,
            "field_4e": self.field_4e,
            "field_50": self.field_50,
            "direction2": list(self.direction2),
            "cell": [self.cell_x, self.cell_y],
            "field_64": self.field_64.hex(),
            "waypoints": [list(point) for point in self.waypoints],
            "field_1c8": self.field_1c8,
            "field_1cc": self.field_1cc,
            "nodes": [node.to_dict() for node in self.nodes],
            "trail": {
                "1ec": self.trail_1ec,
                "1f0": self.trail_1f0,
                "1f4": self.trail_1f4,
                "1f8": self.trail_1f8,
                "208": self.trail_208,
                "20c": self.trail_20c,
                "210": self.trail_210,
                "214": list(self.trail_214),
                "21c": self.trail_21c,
                "220": self.trail_220,
                "224": self.trail_224,
                "228": list(self.trail_228),
                "1d0": self.trail_1d0,
                "1d4": self.trail_1d4,
            },
        }


@dataclass
class CampaignDino:
    """One ``list0`` ``saDinosaur`` record."""

    object_id: int
    x: int
    y: int
    state_bytes: tuple[int, int, int, int]
    field_10: int
    field_12: int
    flag: int
    name: str
    name_bytes: bytes
    field_38: int
    thresholds: tuple[int, ...] | None   # 12 ints, None when flag != 0
    gauges: bytes                        # raw 0xB0 memory image
    state3: bytes                        # 12 bytes (+0x208..+0x213)
    anim_flags: tuple[int, int, int]     # +0x514..+0x516
    texture_name: bytes                  # ASCII incl NUL (+0x518)
    field_524: int
    field_517: int
    field_1d0: int
    path: CampaignPath
    offset: int = 0
    size: int = 0

    @property
    def node_count(self) -> int:
        return len(self.path.nodes)

    def gauge_i32(self, relative: int) -> int:
        return struct.unpack_from("<i", self.gauges, relative)[0]

    @property
    def food(self) -> int:
        return self.gauge_i32(0x30)

    @property
    def violence(self) -> int:
        return self.gauge_i32(0x34)

    @property
    def security(self) -> int:
        return self.gauge_i32(0x38)

    @property
    def rest(self) -> int:
        return self.gauge_i32(0x3C)

    @property
    def need_states(self) -> tuple[int, int, int, int]:
        return struct.unpack_from("<4i", self.gauges, 0x98)  # +0x2ac..+0x2b8

    @property
    def activity(self) -> int:
        return self.gauges[0xA8]             # +0x2bc

    @property
    def action_id(self) -> int:
        return self.gauge_i32(0xAC)          # +0x2c0

    def to_dict(self) -> dict:
        return {
            "offset": self.offset,
            "size": self.size,
            "object_id": self.object_id,
            "x": self.x,
            "y": self.y,
            "state_bytes": list(self.state_bytes),
            "field_10": self.field_10,
            "field_12": self.field_12,
            "flag": self.flag,
            "name": self.name,
            "field_38": self.field_38,
            "thresholds": list(self.thresholds) if self.thresholds is not None else None,
            "gauges": {
                "food": self.food,
                "violence": self.violence,
                "security": self.security,
                "rest": self.rest,
                "need_states": list(self.need_states),
                "activity": self.activity,
                "action_id": self.action_id,
            },
            "state3": self.state3.hex(),
            "anim_flags": list(self.anim_flags),
            "texture_name": self.texture_name.decode("latin-1"),
            "field_524": self.field_524,
            "field_517": self.field_517,
            "field_1d0": self.field_1d0,
            "nodes": self.node_count,
            "path": self.path.to_dict(),
        }


def _read_path(data: bytes, off: int) -> tuple[CampaignPath, int]:
    start = off
    position = struct.unpack_from("<2f", data, off)
    target = struct.unpack_from("<2f", data, off + 0x08)
    cells = struct.unpack_from("<2I", data, off + 0x10)
    direction = struct.unpack_from("<2f", data, off + 0x18)
    timeline = struct.unpack_from("<9f", data, off + 0x20)
    sentinel = _u32(data, off + 0x44, "path sentinel")
    field_4c = _u16(data, off + 0x48, "path +0x4c")
    field_4e = _u16(data, off + 0x4a, "path +0x4e")
    field_50 = _u8(data, off + 0x4c, "path +0x50")
    direction2 = struct.unpack_from("<2f", data, off + 0x4d)
    cell_x = _u32(data, off + 0x55, "path cell x")
    cell_y = _u32(data, off + 0x59, "path cell y")
    field_64 = _take(data, off + 0x5d, 4, "path +0x64")
    waypoints = tuple(
        struct.unpack_from("<2I", data, off + 0x61 + i * 8)
        for i in range(PATH_WAYPOINTS)
    )
    field_1c8 = _u32(data, off + 0x1c1, "path +0x1c8")
    field_1cc = _u8(data, off + 0x1c5, "path +0x1cc")
    count = _u32(data, off + 0x1c6, "path node count")
    if count > 4096:
        raise SavCampaignError(f"path node count {count} is implausible")
    cursor = off + 0x1ca
    nodes: list[CampaignPathNode] = []
    for index in range(count):
        _need(data, cursor, NODE_BYTES, f"path node {index}")
        nodes.append(CampaignPathNode(
            x=struct.unpack_from("<f", data, cursor)[0],
            y=struct.unpack_from("<f", data, cursor + 4)[0],
            metric=struct.unpack_from("<f", data, cursor + 8)[0],
            field_0c=data[cursor + 0x0c],
            field_0d=data[cursor + 0x0d],
            field_0e=_u32(data, cursor + 0x0e, f"path node {index} +0x0e"),
            field_12=_u32(data, cursor + 0x12, f"path node {index} +0x12"),
        ))
        cursor += NODE_BYTES

    def u32(what: str) -> int:
        nonlocal cursor
        value = _u32(data, cursor, what)
        cursor += 4
        return value

    def f32(what: str) -> float:
        nonlocal cursor
        value = _f32(data, cursor, what)
        cursor += 4
        return value

    trail_1ec = u32("path trail +0x1ec")
    trail_1f0 = u32("path trail +0x1f0")
    trail_1f4 = u32("path trail +0x1f4")
    trail_1f8 = u32("path trail +0x1f8")
    trail_208 = _u8(data, cursor, "path trail +0x208")
    cursor += 1
    trail_20c = u32("path trail +0x20c")
    trail_210 = u32("path trail +0x210")
    trail_214 = (f32("path trail +0x214 x"), f32("path trail +0x214 y"))
    trail_21c = _u8(data, cursor, "path trail +0x21c")
    cursor += 1
    trail_220 = u32("path trail +0x220")
    trail_224 = u32("path trail +0x224")
    trail_228 = (f32("path trail +0x228 x"), f32("path trail +0x228 y"))
    trail_1d0 = u32("path trail +0x1d0")
    trail_1d4 = _u8(data, cursor, "path trail +0x1d4")
    cursor += 1

    expected = 0x201 + NODE_BYTES * count
    if cursor - start != expected:
        raise SavCampaignError(
            f"path size {cursor - start:#x} != expected {expected:#x}")
    return CampaignPath(
        position=position,
        target=target,
        cells=cells,
        direction=direction,
        timeline=timeline,
        sentinel=sentinel,
        field_4c=field_4c,
        field_4e=field_4e,
        field_50=field_50,
        direction2=direction2,
        cell_x=cell_x,
        cell_y=cell_y,
        field_64=field_64,
        waypoints=waypoints,
        field_1c8=field_1c8,
        field_1cc=field_1cc,
        nodes=tuple(nodes),
        trail_1ec=trail_1ec,
        trail_1f0=trail_1f0,
        trail_1f4=trail_1f4,
        trail_1f8=trail_1f8,
        trail_208=trail_208,
        trail_20c=trail_20c,
        trail_210=trail_210,
        trail_214=trail_214,
        trail_21c=trail_21c,
        trail_220=trail_220,
        trail_224=trail_224,
        trail_228=trail_228,
        trail_1d0=trail_1d0,
        trail_1d4=trail_1d4,
    ), cursor


def read_campaign_dino(data: bytes, off: int) -> tuple[CampaignDino, int]:
    start = off
    object_id = _u32(data, off, "campaign dino object id")
    x = _u32(data, off + 0x04, "campaign dino x")
    y = _u32(data, off + 0x08, "campaign dino y")
    state_bytes = tuple(data[off + 0x0c:off + 0x10])
    if len(state_bytes) != 4:
        raise SavCampaignError(f"campaign dino state bytes at {off:#x}")
    field_10 = _u16(data, off + 0x10, "campaign dino +0x10")
    field_12 = _u16(data, off + 0x12, "campaign dino +0x12")
    flag = _u8(data, off + 0x14, "campaign dino flag")
    name_len = _u32(data, off + 0x15, "campaign dino name length")
    if name_len == 0 or name_len > 4096 or (name_len & 1) != 0:
        raise SavCampaignError(
            f"campaign dino name length {name_len} at {off:#x}")
    name_bytes = _take(data, off + 0x19, name_len, "campaign dino name")
    try:
        name = name_bytes.decode("utf-16-le").rstrip("\x00")
    except UnicodeDecodeError as exc:
        raise SavCampaignError(f"campaign dino name: {exc}") from exc
    cursor = off + 0x19 + name_len

    field_38 = _u32(data, cursor, "campaign dino +0x38")
    cursor += 4
    thresholds: tuple[int, ...] | None = None
    if flag == 0:
        _need(data, cursor, 0x30, "campaign dino need thresholds")
        thresholds = struct.unpack_from("<12i", data, cursor)
        cursor += 0x30
    gauges = _take(data, cursor, GAUGE_BYTES, "campaign dino gauges")
    cursor += GAUGE_BYTES
    state3 = _take(data, cursor, 12, "campaign dino state3")
    cursor += 12
    anim_flags = tuple(_take(data, cursor, 3, "campaign dino anim flags"))
    cursor += 3
    texture_len = _u32(data, cursor, "campaign dino texture length")
    cursor += 4
    if texture_len == 0 or texture_len > 4096:
        raise SavCampaignError(
            f"campaign dino texture length {texture_len} at {off:#x}")
    texture_name = _take(data, cursor, texture_len, "campaign dino texture")
    cursor += texture_len
    field_524 = _u32(data, cursor, "campaign dino +0x524")
    cursor += 4
    field_517 = _u8(data, cursor, "campaign dino +0x517")
    cursor += 1
    field_1d0 = _u32(data, cursor, "campaign dino +0x1d0")
    cursor += 4

    path, cursor = _read_path(data, cursor)
    record = CampaignDino(
        object_id=object_id,
        x=x,
        y=y,
        state_bytes=state_bytes,
        field_10=field_10,
        field_12=field_12,
        flag=flag,
        name=name,
        name_bytes=name_bytes,
        field_38=field_38,
        thresholds=thresholds,
        gauges=gauges,
        state3=state3,
        anim_flags=anim_flags,
        texture_name=texture_name,
        field_524=field_524,
        field_517=field_517,
        field_1d0=field_1d0,
        path=path,
        offset=start,
        size=cursor - start,
    )
    return record, cursor


def read_campaign_list0(
    data: bytes, off: int, count: int
) -> tuple[list[CampaignDino], int]:
    """Read ``count`` records starting at ``off`` (after the list count)."""
    if count > 10000:
        raise SavCampaignError(f"campaign list0 count {count} is implausible")
    records: list[CampaignDino] = []
    cursor = off
    for index in range(count):
        record, cursor = read_campaign_dino(data, cursor)
        records.append(record)
    return records, cursor


def write_path(path: CampaignPath) -> bytes:
    out = bytearray()
    out += struct.pack("<2f", *path.position)
    out += struct.pack("<2f", *path.target)
    out += struct.pack("<2I", *path.cells)
    out += struct.pack("<2f", *path.direction)
    out += struct.pack("<9f", *path.timeline)
    out += struct.pack("<I", path.sentinel)
    out += struct.pack("<H", path.field_4c)
    out += struct.pack("<H", path.field_4e)
    out += bytes([path.field_50])
    out += struct.pack("<2f", *path.direction2)
    out += struct.pack("<I", path.cell_x)
    out += struct.pack("<I", path.cell_y)
    out += path.field_64
    for x, y in path.waypoints:
        out += struct.pack("<2I", x, y)
    out += struct.pack("<I", path.field_1c8)
    out += bytes([path.field_1cc])
    out += struct.pack("<I", len(path.nodes))
    for node in path.nodes:
        out += struct.pack("<fff", node.x, node.y, node.metric)
        out += bytes([node.field_0c, node.field_0d])
        out += struct.pack("<II", node.field_0e, node.field_12)
    out += struct.pack("<IIII", path.trail_1ec, path.trail_1f0,
                       path.trail_1f4, path.trail_1f8)
    out += bytes([path.trail_208])
    out += struct.pack("<II", path.trail_20c, path.trail_210)
    out += struct.pack("<2f", *path.trail_214)
    out += bytes([path.trail_21c])
    out += struct.pack("<II", path.trail_220, path.trail_224)
    out += struct.pack("<2f", *path.trail_228)
    out += struct.pack("<I", path.trail_1d0)
    out += bytes([path.trail_1d4])
    return bytes(out)


def write_campaign_dino(record: CampaignDino) -> bytes:
    out = bytearray()
    out += struct.pack("<III", record.object_id, record.x, record.y)
    out += bytes(record.state_bytes)
    out += struct.pack("<HH", record.field_10, record.field_12)
    out += bytes([record.flag])
    out += struct.pack("<I", len(record.name_bytes))
    out += record.name_bytes
    out += struct.pack("<I", record.field_38)
    if record.flag == 0:
        if record.thresholds is None or len(record.thresholds) != 12:
            raise SavCampaignError(
                f"campaign dino {record.object_id}: missing thresholds")
        out += struct.pack("<12i", *record.thresholds)
    out += record.gauges
    out += record.state3
    out += bytes(record.anim_flags)
    out += struct.pack("<I", len(record.texture_name))
    out += record.texture_name
    out += struct.pack("<I", record.field_524)
    out += bytes([record.field_517])
    out += struct.pack("<I", record.field_1d0)
    out += write_path(record.path)
    return bytes(out)


def write_campaign_list0(records: list[CampaignDino]) -> bytes:
    out = bytearray(struct.pack("<I", len(records)))
    for record in records:
        out += write_campaign_dino(record)
    return bytes(out)


def _find_list0_offset(path: Path) -> tuple[bytes, int, int]:
    save = read_sav(path)
    data = path.read_bytes()
    off = save.state_offset
    roads = _u32(data, off, "road count")
    off += 4 + roads * 16
    return data, off, _u32(data, off, "list0 count")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sav", type=Path)
    parser.add_argument("--json", action="store_true", help="dump records as JSON")
    parser.add_argument("--limit", type=int, default=0, help="only the first N")
    parser.add_argument("--check", action="store_true",
                        help="verify each record re-encodes byte-identically")
    args = parser.parse_args(argv)
    try:
        data, list0_at, count = _find_list0_offset(args.sav)
        records, end = read_campaign_list0(data, list0_at + 4, count)
        print(f"sav campaign: version={read_sav(args.sav).version} "
              f"list0={count} start={list0_at:#x} end={end:#x} "
              f"next_count={_u32(data, end, 'next list count')}")
        shown = records[:args.limit] if args.limit > 0 else records
        for index, record in enumerate(shown):
            print(f"{index:2d} id={record.object_id} name={record.name!r} "
                  f"cell=({record.x},{record.y}) nodes={record.node_count} "
                  f"tail={record.size - 0x19 - len(record.name_bytes):#x} "
                  f"food={record.food} need_states={record.need_states} "
                  f"action={record.action_id}")
        if args.json:
            import json
            print(json.dumps([record.to_dict() for record in shown], indent=2))
        if args.check:
            for index, record in enumerate(records):
                raw = data[record.offset:record.offset + record.size]
                if write_campaign_dino(record) != raw:
                    raise SavCampaignError(
                        f"record {index} ({record.name}) does not re-encode")
            if write_campaign_list0(records) != data[list0_at:end]:
                raise SavCampaignError("list0 re-encode differs from the file")
            print(f"list0 round-trip: {len(records)} records identical "
                  f"({end - list0_at} bytes)")
    except Exception as exc:  # noqa: BLE001 - one clean CLI error
        print(f"sav campaign failed: {exc}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
