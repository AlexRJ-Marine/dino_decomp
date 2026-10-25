#!/usr/bin/env python3
"""Exploratory .skin block walker.

Blocks start with a length-prefixed bone name. Geometry follows:
    u32 vertex_count
    vertex_count * 3f positions
    u32 index_count
    index_count * u32 indices
    ... tail (normals/material/matrix)

We locate blocks by matching the skeleton's length-prefixed names.
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path

MAGIC = b"\x01\x03\x00\x00\x00"
SKIN_SENTINEL = 0x02468ACE


def read_skel_names(path: Path) -> list[str]:
    d = path.read_bytes()
    assert d[:5] == MAGIC
    count = struct.unpack_from("<I", d, 13)[0]
    off = 0x19 + count * 76
    names = []
    for _ in range(count):
        ln = struct.unpack_from("<I", d, off)[0]
        off += 4
        names.append(d[off : off + ln - 1].decode("latin-1"))
        off += ln
    return names


def find_blocks(skin: bytes, names: list[str]):
    hits = []
    for name in names:
        pat = struct.pack("<I", len(name) + 1) + name.encode("latin-1") + b"\x00"
        start = 0
        while True:
            pos = skin.find(pat, start)
            if pos < 0:
                break
            hits.append((pos, name))
            start = pos + 1
    hits.sort()
    # dedupe adjacent identical offsets
    out = []
    for pos, name in hits:
        if not out or out[-1][0] != pos:
            out.append((pos, name))
    return out


def parse_block_geometry(skin: bytes, pos: int, name: str):
    off = pos
    ln = struct.unpack_from("<I", skin, off)[0]
    off += 4
    block_name = skin[off : off + ln - 1].decode("latin-1")
    off += ln
    vc = struct.unpack_from("<I", skin, off)[0]
    off += 4
    positions_end = off + vc * 12
    if positions_end + 4 > len(skin):
        return None
    ic = struct.unpack_from("<I", skin, positions_end)[0]
    idx_start = positions_end + 4
    idx_end = idx_start + ic * 4
    if idx_end > len(skin):
        return None
    indices = struct.unpack_from("<%dI" % ic, skin, idx_start)
    if vc == 0 or ic == 0 or max(indices) >= vc or ic % 3:
        return None
    return {
        "pos": pos,
        "name": block_name,
        "name_match": block_name == name,
        "vc": vc,
        "ic": ic,
        "geo_end": idx_end,
        "indices": indices,
    }


def main():
    root = Path(sys.argv[1] if len(sys.argv) > 1 else "game/%MAINDIR%")
    skin_path = Path(sys.argv[2]) if len(sys.argv) > 2 else root / "Data/Graphics/Actors/Dinos/allo/allo_lod1.skin"
    skin = skin_path.read_bytes()
    print(f"{skin_path}: {len(skin)} bytes")
    print("magic", skin[:5].hex(" "), "sentinel", hex(struct.unpack_from("<I", skin, 9)[0]), "count", struct.unpack_from("<I", skin, 13)[0])

    skel_path = skin_path.with_suffix(".skel")
    if not skel_path.exists():
        # allo_lod1.skin -> allo.skel
        skel_path = skin_path.with_name(skin_path.name.split("_lod")[0] + ".skel")
    names = read_skel_names(skel_path)
    print(f"skeleton {skel_path.name}: {len(names)} names")

    hits = find_blocks(skin, names)
    print(f"name blocks found: {len(hits)}")
    blocks = []
    for i, (pos, name) in enumerate(hits):
        b = parse_block_geometry(skin, pos, name)
        if b is None:
            print(f"  FAIL at 0x{pos:x} {name}")
            continue
        nxt = hits[i + 1][0] if i + 1 < len(hits) else len(skin)
        b["tail"] = nxt - b["geo_end"]
        b["next"] = nxt
        blocks.append(b)
        print(f"  0x{pos:05x} {name:24} vc={b['vc']:5d} ic={b['ic']:6d} tri={b['ic']//3:5d} tail={b['tail']:5d}")

    # 36th block? try parsing right after the last geometry
    if blocks:
        last = blocks[-1]
        extra = parse_block_geometry(skin, last["geo_end"], "?")
        print(f"after last geometry 0x{last['geo_end']:x}: {'extra block ' + str(extra) if extra else 'not a nameless geometry block'}")

    print(f"sum vertices={sum(b['vc'] for b in blocks):,} sum triangles={sum(b['ic']//3 for b in blocks):,}")

    mesh_path = skin_path.with_suffix(".mesh")
    if mesh_path.exists():
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        from mesh import read_mesh
        m = read_mesh(mesh_path)
        print(f"mesh {mesh_path.name}: vertices={len(m.vertices):,} triangles={m.triangle_count:,}")
        print(f"vertex ratio={sum(b['vc'] for b in blocks)/len(m.vertices):.3f}  triangle ratio={sum(b['ic']//3 for b in blocks)/m.triangle_count:.3f}")


if __name__ == "__main__":
    main()
