#!/usr/bin/env python3
"""Dino Island .mesh header reader.

The header is shared by .mesh/.skin/.skel/.anim files:

    00  magic          5 bytes: 01 03 00 00 00
    05  hash           u32 LE, per-file checksum/hash
    09  sentinel       u32 LE, 0x12345678
    0d  chunk_count    u32 LE, usually 1
    11  name_len       u32 LE, includes trailing NUL
    15  name           name_len bytes, ASCII, NUL-terminated
    ..  payload_count  u32 LE
    ..  payload        payload_count * 4 bytes (first values often floats)

This tool only proves the header. Geometry decoding is a separate task.
"""
from __future__ import annotations

import argparse
import json
import struct
import sys
from dataclasses import asdict, dataclass, field
from pathlib import Path

MAGIC = b"\x01\x03\x00\x00\x00"
SENTINEL = 0x12345678


@dataclass
class MeshHeader:
    path: str = ""
    size: int = 0
    hash: int = 0
    sentinel: int = 0
    chunk_count: int = 0
    name: str = ""
    name_len: int = 0
    payload_count: int = 0
    bbox_min: list[float] = field(default_factory=list)
    errors: list[str] = field(default_factory=list)

    @property
    def ok(self) -> bool:
        return not self.errors

    def to_dict(self) -> dict:
        d = asdict(self)
        d["ok"] = self.ok
        return d


def parse_header_bytes(data: bytes, path: str = "<bytes>") -> MeshHeader:
    h = MeshHeader(path=path, size=len(data))

    if len(data) < 21:
        h.errors.append(f"file too short: {len(data)} bytes, need at least 21")
        return h

    if data[:5] != MAGIC:
        h.errors.append(f"bad magic at 0x00: {data[:5].hex(' ')}")
        return h

    (h.hash,) = struct.unpack_from("<I", data, 5)
    (h.sentinel,) = struct.unpack_from("<I", data, 9)
    (h.chunk_count,) = struct.unpack_from("<I", data, 13)
    (h.name_len,) = struct.unpack_from("<I", data, 17)

    if h.sentinel != SENTINEL:
        h.errors.append(f"bad sentinel at 0x09: {h.sentinel:#010x}")
    if not 1 <= h.chunk_count <= 64:
        h.errors.append(f"implausible chunk_count {h.chunk_count}")
    if not 1 <= h.name_len <= 128:
        h.errors.append(f"implausible name_len {h.name_len}")
        return h

    name_start = 21
    name_end = name_start + h.name_len
    if name_end > len(data):
        h.errors.append(f"name runs past EOF at 0x{name_end:x}")
        return h

    raw = data[name_start:name_end]
    if raw[-1] != 0:
        h.errors.append("name is not NUL-terminated")
    try:
        h.name = raw.rstrip(b"\x00").decode("ascii")
    except UnicodeDecodeError:
        h.errors.append("name is not ASCII")
        h.name = raw.rstrip(b"\x00").decode("latin-1")

    if name_end + 4 <= len(data):
        (h.payload_count,) = struct.unpack_from("<I", data, name_end)
        if h.payload_count > 10_000_000:
            h.errors.append(f"implausible payload_count {h.payload_count}")
        bbox_start = name_end + 4
        if bbox_start + 12 <= len(data) and h.payload_count >= 3:
            h.bbox_min = list(struct.unpack_from("<3f", data, bbox_start))
    else:
        h.errors.append("missing payload_count")

    return h


def parse_header(path: Path) -> MeshHeader:
    data = path.read_bytes()
    return parse_header_bytes(data, str(path))


@dataclass
class ScanResult:
    files: list[MeshHeader] = field(default_factory=list)
    errors: list[tuple[str, list[str]]] = field(default_factory=list)


def scan(root: Path) -> ScanResult:
    result = ScanResult()
    for path in sorted(root.rglob("*.mesh")):
        header = parse_header(path)
        result.files.append(header)
        if not header.ok:
            result.errors.append((str(path), header.errors))
    return result


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    group = ap.add_mutually_exclusive_group(required=True)
    group.add_argument("--dump", type=Path, help="dump one file header as JSON")
    group.add_argument("--scan", type=Path, help="scan a directory tree for *.mesh")
    ap.add_argument("--json-out", type=Path, help="write scan results as JSON")
    args = ap.parse_args(argv)

    if args.dump:
        header = parse_header(args.dump)
        print(json.dumps(header.to_dict(), indent=2))
        return 0 if header.ok else 1

    result = scan(args.scan)
    total = len(result.files)
    bad = len(result.errors)
    print(f"scanned {total} .mesh files, {bad} with errors")
    if result.files:
        names = sorted({h.name for h in result.files})
        print(f"unique names: {len(names)}  example: {', '.join(names[:5])}")
    for path, errors in result.errors[:20]:
        print(f"  FAIL {path}: {'; '.join(errors)}")

    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        payload = {
            "root": str(args.scan),
            "count": total,
            "errors": [{"path": p, "errors": e} for p, e in result.errors],
            "files": [h.to_dict() for h in result.files],
        }
        args.json_out.write_text(json.dumps(payload, indent=1), encoding="utf-8")
        print(f"wrote {args.json_out}")

    return 0 if bad == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
