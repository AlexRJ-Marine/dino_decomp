#!/usr/bin/env python3
"""Reader/exporter for Dino Island .mesh files.

Proven layout (validated against all 536 retail .mesh files):

    header:
      5   magic          01 03 00 00 00
      4   hash
      4   sentinel       0x12345678
      4   chunk_count
      4   name_len
      n   name           ASCII, NUL-terminated
      4   vertex_count
      12*vertex_count    positions (3 x f32)
      4   index_count
      4*index_count      indices (u32)
      1   uv_flag
      4   uv_count       (if uv_flag == 1)
      8*uv_count         UVs (2 x f32)
      1   normal_flag
      4   normal_count   (if normal_flag == 1)
      12*normal_count    normals (3 x f32)
      ...                material/color data (not decoded yet)

All counts match vertex_count in the retail files. Indices are 32-bit and
reference the vertex array directly.
"""
from __future__ import annotations

import argparse
import json
import struct
from dataclasses import dataclass, field
from pathlib import Path

MAGIC = b"\x01\x03\x00\x00\x00"
SENTINEL = 0x12345678


class MeshFormatError(ValueError):
    pass


@dataclass
class Mesh:
    path: str
    name: str
    hash: int
    chunk_count: int
    vertices: list[tuple[float, float, float]] = field(default_factory=list)
    indices: list[int] = field(default_factory=list)
    uvs: list[tuple[float, float]] = field(default_factory=list)
    normals: list[tuple[float, float, float]] = field(default_factory=list)
    tail: bytes = b""

    @property
    def triangle_count(self) -> int:
        return len(self.indices) // 3

    def bounds(self):
        if not self.vertices:
            return None
        xs, ys, zs = zip(*self.vertices)
        return (min(xs), min(ys), min(zs)), (max(xs), max(ys), max(zs))

    def summary(self) -> dict:
        return {
            "path": self.path,
            "name": self.name,
            "hash": f"{self.hash:#010x}",
            "chunk_count": self.chunk_count,
            "vertices": len(self.vertices),
            "indices": len(self.indices),
            "triangles": self.triangle_count,
            "uvs": len(self.uvs),
            "normals": len(self.normals),
            "tail_bytes": len(self.tail),
            "bounds": self.bounds(),
        }


def _need(data: bytes, off: int, size: int, what: str) -> None:
    if off + size > len(data):
        raise MeshFormatError(f"{what}: need {size} bytes at 0x{off:x}, file is 0x{len(data):x}")


def read_mesh_body(
    data: bytes,
    off: int,
    *,
    name: str = "",
    hash_: int = 0,
    chunk_count: int = 1,
    path: str = "",
) -> Mesh:
    """Parse a mesh body starting at a vertex_count u32."""
    _need(data, off, 4, "vertex_count")
    vertex_count = struct.unpack_from("<I", data, off)[0]
    off += 4
    if vertex_count > 5_000_000:
        raise MeshFormatError(f"implausible vertex_count {vertex_count}")

    _need(data, off, vertex_count * 12, "positions")
    vertices = [struct.unpack_from("<3f", data, off + i * 12) for i in range(vertex_count)]
    off += vertex_count * 12

    _need(data, off, 4, "index_count")
    index_count = struct.unpack_from("<I", data, off)[0]
    off += 4
    if index_count > 30_000_000:
        raise MeshFormatError(f"implausible index_count {index_count}")
    _need(data, off, index_count * 4, "indices")
    indices = list(struct.unpack_from("<%dI" % index_count, data, off)) if index_count else []
    off += index_count * 4
    for i, idx in enumerate(indices):
        if idx >= vertex_count:
            raise MeshFormatError(f"index[{i}] = {idx} >= vertex_count {vertex_count}")
    if index_count % 3:
        raise MeshFormatError(f"index_count {index_count} is not a multiple of 3")

    uvs: list[tuple[float, float]] = []
    normals: list[tuple[float, float, float]] = []

    _need(data, off, 1, "uv_flag")
    uv_flag = data[off]
    off += 1
    if uv_flag:
        _need(data, off, 4, "uv_count")
        uv_count = struct.unpack_from("<I", data, off)[0]
        off += 4
        _need(data, off, uv_count * 8, "uvs")
        uvs = [struct.unpack_from("<2f", data, off + i * 8) for i in range(uv_count)]
        off += uv_count * 8

    _need(data, off, 1, "normal_flag")
    normal_flag = data[off]
    off += 1
    if normal_flag:
        _need(data, off, 4, "normal_count")
        normal_count = struct.unpack_from("<I", data, off)[0]
        off += 4
        _need(data, off, normal_count * 12, "normals")
        normals = [struct.unpack_from("<3f", data, off + i * 12) for i in range(normal_count)]
        off += normal_count * 12

    return Mesh(
        path=path,
        name=name,
        hash=hash_,
        chunk_count=chunk_count,
        vertices=vertices,
        indices=indices,
        uvs=uvs,
        normals=normals,
        tail=data[off:],
    )


def read_mesh(path: Path) -> Mesh:
    data = path.read_bytes()
    if data[:5] != MAGIC:
        raise MeshFormatError(f"bad magic at 0x00: {data[:5].hex(' ')}")

    hash_, sentinel, chunk_count, name_len = struct.unpack_from("<4I", data, 5)
    if sentinel != SENTINEL:
        raise MeshFormatError(f"bad sentinel at 0x09: {sentinel:#010x}")
    if not 1 <= name_len <= 128:
        raise MeshFormatError(f"implausible name_len {name_len}")

    name_start = 21
    _need(data, name_start, name_len, "name")
    raw_name = data[name_start : name_start + name_len]
    if raw_name[-1] != 0:
        raise MeshFormatError("name is not NUL-terminated")
    name = raw_name[:-1].decode("latin-1")

    return read_mesh_body(
        data,
        name_start + name_len,
        name=name,
        hash_=hash_,
        chunk_count=chunk_count,
        path=str(path),
    )


def write_obj(mesh: Mesh, out: Path) -> None:
    with out.open("w", encoding="utf-8", newline="\n") as f:
        f.write(f"# {mesh.name} from {mesh.path}\n")
        f.write(f"# vertices={len(mesh.vertices)} triangles={mesh.triangle_count}\n")
        for x, y, z in mesh.vertices:
            f.write(f"v {x:.6f} {y:.6f} {z:.6f}\n")
        for u, v in mesh.uvs:
            f.write(f"vt {u:.6f} {v:.6f}\n")
        for x, y, z in mesh.normals:
            f.write(f"vn {x:.6f} {y:.6f} {z:.6f}\n")

        has_uv = len(mesh.uvs) == len(mesh.vertices)
        has_n = len(mesh.normals) == len(mesh.vertices)
        for tri in range(mesh.triangle_count):
            a, b, c = (mesh.indices[tri * 3 + k] + 1 for k in range(3))
            if has_uv and has_n:
                f.write(f"f {a}/{a}/{a} {b}/{b}/{b} {c}/{c}/{c}\n")
            elif has_uv:
                f.write(f"f {a}/{a} {b}/{b} {c}/{c}\n")
            elif has_n:
                f.write(f"f {a}//{a} {b}//{b} {c}//{c}\n")
            else:
                f.write(f"f {a} {b} {c}\n")


def scan(root: Path) -> tuple[int, list[tuple[str, str]], dict]:
    total = 0
    bad: list[tuple[str, str]] = []
    tri = 0
    verts = 0
    for p in sorted(root.rglob("*.mesh")):
        total += 1
        try:
            m = read_mesh(p)
        except Exception as e:
            bad.append((str(p), str(e)))
            continue
        tri += m.triangle_count
        verts += len(m.vertices)
    return total, bad, {"vertices": verts, "triangles": tri}


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--dump", type=Path)
    g.add_argument("--obj", nargs=2, metavar=("MESH", "OUT"))
    g.add_argument("--scan", type=Path)
    args = ap.parse_args(argv)

    if args.dump:
        print(json.dumps(read_mesh(args.dump).summary(), indent=2))
        return 0
    if args.obj:
        mesh = read_mesh(Path(args.obj[0]))
        write_obj(mesh, Path(args.obj[1]))
        print(f"wrote {args.obj[1]}: {len(mesh.vertices)} vertices, {mesh.triangle_count} triangles")
        return 0
    total, bad, totals = scan(args.scan)
    print(f"scanned {total} meshes, {len(bad)} bad, {totals['vertices']:,} vertices, {totals['triangles']:,} triangles")
    for p, why in bad[:20]:
        print(f"  FAIL {p}: {why}")
    return 0 if not bad else 1


if __name__ == "__main__":
    raise SystemExit(main())
