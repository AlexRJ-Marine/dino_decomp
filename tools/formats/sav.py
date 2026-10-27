#!/usr/bin/env python3
"""Partial reader for Dino Island ``.sav`` files.

The retail save is a serialized world. The header and the show catalog are
solved; the park-state block after it is still an opaque object graph.

Layout (see ``docs/formats/sav.md``)::

    0x00  u32  unknown (53 in the retail saves; maybe version/flags)
    0x04  u32  slot/index
    0x08  u32  name byte length
    0x0c  ...  UTF-16LE save name (length includes the NUL)
    ...        variable header bytes (bitsets, zeros, 0xffffffff sentinels)
    ...   u32  show count
    ...        show_count x record:
                 u32 path length, ASCII path (NUL included)
                 u32 name byte length, UTF-16LE name (NUL included)
                 6 x u32 parameters
    ...        park-state object graph (variable length)
"""
from __future__ import annotations

import argparse
import json
import struct
from dataclasses import dataclass, field
from pathlib import Path

SHOW_PREFIX = b"DATA\\design\\scenario\\show\\"


class SavFormatError(ValueError):
    pass


@dataclass
class SaveShow:
    path: str
    name: str
    params: tuple[int, ...] = ()

    def to_dict(self) -> dict:
        return {"path": self.path, "name": self.name, "params": list(self.params)}


@dataclass
class SaveFile:
    path: str
    version: int
    slot: int
    name: str
    count_offset: int
    shows: list[SaveShow] = field(default_factory=list)
    state_offset: int = 0
    state: bytes = b""
    data: bytes = b""
    name_len: int = 0
    header_tail: bytes = b""

    @property
    def state_bytes(self) -> int:
        return len(self.state)

    def summary(self) -> dict:
        return {
            "path": self.path,
            "version": self.version,
            "slot": self.slot,
            "name": self.name,
            "count_offset": self.count_offset,
            "shows": len(self.shows),
            "first_show": self.shows[0].to_dict() if self.shows else None,
            "last_show": self.shows[-1].to_dict() if self.shows else None,
            "state_offset": self.state_offset,
            "state_bytes": len(self.state),
        }


def _u32(data: bytes, off: int, what: str) -> int:
    if off + 4 > len(data):
        raise SavFormatError(f"{what}: u32 past EOF at {off:#x}")
    return struct.unpack_from("<I", data, off)[0]


def read_sav(path: Path) -> SaveFile:
    data = path.read_bytes()
    if len(data) < 16:
        raise SavFormatError(f"{path}: shorter than header")
    version = _u32(data, 0, "version")
    slot = _u32(data, 4, "slot")
    name_len = _u32(data, 8, "name length")
    if name_len > 1024 or 12 + name_len > len(data):
        raise SavFormatError(f"{path}: bad save name length {name_len}")
    name = data[12:12 + name_len].decode("utf-16-le", errors="replace").rstrip("\x00")
    header_tail = data[12 + name_len:24 + name_len]

    first = data.find(SHOW_PREFIX)
    if first < 8:
        raise SavFormatError(f"{path}: no show catalog")
    count_offset = first - 8  # u32 count, u32 path length, then the path
    count = _u32(data, count_offset, "show count")
    if not 0 <= count <= 10_000:
        raise SavFormatError(f"{path}: bad show count {count}")

    off = count_offset + 4
    shows: list[SaveShow] = []
    for index in range(count):
        plen = _u32(data, off, f"show {index} path length")
        off += 4
        if plen < 2 or off + plen > len(data):
            raise SavFormatError(f"{path}: show {index} bad path length {plen}")
        show_path = data[off:off + plen].rstrip(b"\x00").decode("ascii", errors="replace")
        off += plen
        nlen = _u32(data, off, f"show {index} name length")
        off += 4
        if nlen < 2 or nlen % 2 or off + nlen > len(data):
            raise SavFormatError(f"{path}: show {index} bad name length {nlen}")
        show_name = data[off:off + nlen].decode("utf-16-le", errors="replace").rstrip("\x00")
        off += nlen
        if off + 24 > len(data):
            raise SavFormatError(f"{path}: show {index} params past EOF")
        params = struct.unpack_from("<6I", data, off)
        off += 24
        shows.append(SaveShow(show_path, show_name, params))

    return SaveFile(
        path=str(path),
        version=version,
        slot=slot,
        name=name,
        count_offset=count_offset,
        shows=shows,
        state_offset=off,
        state=data[off:],
        data=data,
        name_len=name_len,
        header_tail=header_tail,
    )


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("save", type=Path)
    ap.add_argument("--shows", action="store_true", help="list every show record")
    args = ap.parse_args(argv)
    save = read_sav(args.save)
    if args.shows:
        for show in save.shows:
            print(f"{show.path}  {show.name!r}  {show.params}")
    print(json.dumps(save.summary(), indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
