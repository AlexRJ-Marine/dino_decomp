#!/usr/bin/env python3
"""Exploratory: infer mesh payload layout from counts and file sizes."""
from __future__ import annotations

import collections
import struct
import sys
from pathlib import Path

ROOT = Path(sys.argv[1] if len(sys.argv) > 1 else "game/%MAINDIR%")
MAGIC = b"\x01\x03\x00\x00\x00"


def header(data: bytes):
    assert data[:5] == MAGIC, data[:8].hex()
    name_len = struct.unpack_from("<I", data, 17)[0]
    name = data[21 : 21 + name_len].rstrip(b"\0").decode("ascii", "replace")
    payload_start = 21 + name_len
    count = struct.unpack_from("<I", data, payload_start)[0]
    return name, count, payload_start + 4


ratios = collections.Counter()
rows = []
for p in sorted(ROOT.rglob("*.mesh")):
    data = p.read_bytes()
    try:
        name, count, start = header(data)
    except Exception as e:
        print("bad", p, e)
        continue
    body = len(data) - start
    ratio = body / count if count else 0
    ratios[round(ratio, 2)] += 1
    rows.append((p.name, len(data), count, body, ratio))

print("ratio (body bytes / payload_count) distribution:")
for r, n in ratios.most_common(15):
    print(f"  {r:8.2f}  {n}")

print("\nfirst 20 meshes:")
for row in rows[:20]:
    print(f"  {row[0]:32} size={row[1]:7} count={row[2]:6} body={row[3]:7} ratio={row[4]:7.2f}")

# detailed dump of one mesh
name = "cow_lod1.mesh"
p = next((p for p in ROOT.rglob(name) if p.name == name), None)
if p:
    data = p.read_bytes()
    n, count, start = header(data)
    print(f"\n{name}: size={len(data)} count={count} start=0x{start:x} body={len(data)-start}")
    print("first 64 bytes of body:")
    body = data[start:]
    print(body[:64].hex(" "))
    print("as u32:", [hex(x) for x in struct.unpack_from("<16I", body, 0)])
    print("as f32:", [round(x, 5) for x in struct.unpack_from("<16f", body, 0)])
    print("last 64 bytes:")
    print(data[-64:].hex(" "))
    print("last as u16:", list(struct.unpack_from("<32H", data, len(data) - 64)))
