#!/usr/bin/env python3
"""Reader for Dino Island ``.skin`` files.

A ``.skin`` file is a scene-like container:

    header:
      5   magic          01 03 00 00 00
      4   hash
      4   sentinel       0x02468ace
      4   block_count    (36 for allo_lod1)
    blocks:
      N x per-bone proxy block:
          u32 name_len + bone name (NUL-terminated)
          u32 vertex_count
          vertex_count * 3f positions
          u32 index_count
          index_count * u32 indices
          UV block, normal block, color block, material list
      1 x full-mesh block:
          u32 name_len + mesh name
          standard mesh body (vertices, indices, UVs, normals)
          full-mesh skin tail:
              color block, material list,
              u32 triangle_count + u16 per-triangle material indices,
              3 x u32, u32 bone_count,
              bone_count x (name, vertex indices, weights, 4x4 matrix)

The full-mesh block carries the render mesh in **centimetres** and the rigid
skin table. Each full-mesh vertex belongs to exactly one bone with weight 1.0.
The 64-byte matrix is the inverse of that bone's raw (centimetre) world matrix
at bind time::

    v_skinned_cm = v_cm @ inverse_bind @ W_raw_anim(bone)
    v_world      = 0.01 * v_skinned_cm

The 35 per-bone blocks are separate proxy meshes; they do not contain bind
matrices.
"""
from __future__ import annotations

import argparse
import json
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from mesh import Mesh, MeshFormatError, read_mesh, read_mesh_body  # noqa: E402

MAGIC = b"\x01\x03\x00\x00\x00"
SENTINEL = 0x02468ACE


@dataclass
class SkinBlock:
    name: str
    offset: int
    geo_end: int
    vertices: list[tuple[float, float, float]]
    indices: list[int]
    tail: bytes = b""

    @property
    def triangle_count(self) -> int:
        return len(self.indices) // 3

    def to_dict(self) -> dict:
        return {
            "name": self.name,
            "offset": f"0x{self.offset:x}",
            "geometry_end": f"0x{self.geo_end:x}",
            "vertices": len(self.vertices),
            "indices": len(self.indices),
            "triangles": self.triangle_count,
            "tail_bytes": len(self.tail),
        }


@dataclass
class SkinMaterial:
    raw: bytes
    texture: str
    name: str

    def to_dict(self) -> dict:
        return {
            "raw": self.raw.hex(" "),
            "texture": self.texture,
            "name": self.name,
        }


@dataclass
class SkinBone:
    """One rigid-skinning record from the full-mesh tail.

    ``vertex_indices`` is a partition of the full mesh's vertices; each vertex
    is assigned to exactly one bone. ``weights`` is all 1.0 in retail data.
    ``inverse_bind`` is the inverse of the bone's raw (centimetre) world
    matrix at bind time, row-major.
    """

    name: str
    vertex_indices: tuple[int, ...]
    weights: tuple[float, ...]
    inverse_bind: tuple[float, ...]

    def to_dict(self) -> dict:
        return {
            "name": self.name,
            "vertices": len(self.vertex_indices),
            "weights_all_one": all(abs(w - 1.0) < 1e-6 for w in self.weights),
            "inverse_bind": [round(v, 6) for v in self.inverse_bind],
        }


@dataclass
class SkinData:
    materials: list[SkinMaterial]
    triangle_materials: tuple[int, ...]
    unknown: tuple[int, int, int]
    bones: list[SkinBone]

    def to_dict(self) -> dict:
        return {
            "materials": [m.to_dict() for m in self.materials],
            "triangle_materials": len(self.triangle_materials),
            "unknown": list(self.unknown),
            "bones": [b.to_dict() for b in self.bones],
            "vertices_assigned": sum(len(b.vertex_indices) for b in self.bones),
        }


@dataclass
class Skin:
    path: str
    hash: int
    block_count: int
    blocks: list[SkinBlock] = field(default_factory=list)
    full_mesh: Mesh | None = None
    skin_data: SkinData | None = None
    trailing: bytes = b""

    def summary(self) -> dict:
        return {
            "path": self.path,
            "hash": f"{self.hash:#010x}",
            "block_count": self.block_count,
            "bone_blocks": len(self.blocks),
            "bone_vertices": sum(len(b.vertices) for b in self.blocks),
            "bone_triangles": sum(b.triangle_count for b in self.blocks),
            "full_mesh": self.full_mesh.summary() if self.full_mesh else None,
            "skin_data": self.skin_data.to_dict() if self.skin_data else None,
            "trailing_bytes": len(self.trailing),
            "blocks": [b.to_dict() for b in self.blocks],
        }


def _read_skel_names(path: Path) -> list[str]:
    d = path.read_bytes()
    if d[:5] != MAGIC:
        raise ValueError(f"{path}: bad skel magic")
    if struct.unpack_from("<I", d, 9)[0] != 0x56789ABC:
        raise ValueError(f"{path}: bad skel sentinel")
    count = struct.unpack_from("<I", d, 13)[0]
    off = 0x19 + count * 76
    names = []
    for _ in range(count):
        ln = struct.unpack_from("<I", d, off)[0]
        off += 4
        names.append(d[off : off + ln - 1].decode("latin-1"))
        off += ln
    return names


def _skeleton_for(skin_path: Path) -> Path | None:
    direct = skin_path.with_suffix(".skel")
    if direct.exists():
        return direct
    stem = skin_path.name.split("_lod")[0]
    alt = skin_path.with_name(stem + ".skel")
    if alt.exists():
        return alt
    # remove trailing _lodN
    parts = skin_path.stem.rsplit("_lod", 1)
    if len(parts) == 2:
        alt = skin_path.with_name(parts[0] + ".skel")
        if alt.exists():
            return alt
    return None


def _read_bone_block(data: bytes, off: int, name: str, next_offset: int) -> SkinBlock:
    start = off
    _ln = struct.unpack_from("<I", data, off)[0]
    off += 4 + _ln
    vc = struct.unpack_from("<I", data, off)[0]
    off += 4
    if vc > 100_000:
        raise ValueError(f"implausible vertex_count {vc} at 0x{off-4:x}")
    vertices = [struct.unpack_from("<3f", data, off + i * 12) for i in range(vc)]
    off += vc * 12
    ic = struct.unpack_from("<I", data, off)[0]
    off += 4
    if ic > 300_000 or ic % 3:
        raise ValueError(f"implausible index_count {ic} at 0x{off-4:x}")
    indices = list(struct.unpack_from("<%dI" % ic, data, off))
    off += ic * 4
    if any(i >= vc for i in indices):
        raise ValueError("index out of range")
    return SkinBlock(
        name=name,
        offset=start,
        geo_end=off,
        vertices=vertices,
        indices=indices,
        tail=data[off:next_offset],
    )


def _find_named_blocks(data: bytes, names: list[str]) -> list[tuple[int, str]]:
    hits = []
    for name in names:
        pat = struct.pack("<I", len(name) + 1) + name.encode("latin-1") + b"\x00"
        start = 0
        while True:
            pos = data.find(pat, start)
            if pos < 0:
                break
            hits.append((pos, name))
            start = pos + 1
    hits.sort()
    return hits


def _find_full_mesh(data: bytes, mesh: Mesh, start: int) -> tuple[int, Mesh] | None:
    """Find the full-mesh block by matching the .mesh vertex count."""
    needle = struct.pack("<I", len(mesh.vertices))
    pos = start
    while True:
        i = data.find(needle, pos)
        if i < 0:
            return None
        # Try every plausible name length immediately before the vertex count.
        for nlen in range(1, 65):
            name_start = i - nlen
            prefix = name_start - 4
            if prefix < 0:
                continue
            if struct.unpack_from("<I", data, prefix)[0] != nlen:
                continue
            raw = data[name_start:i]
            if not raw.endswith(b"\x00"):
                continue
            try:
                name = raw[:-1].decode("latin-1")
            except UnicodeDecodeError:
                continue
            try:
                body = read_mesh_body(data, i, name=name, path="")
            except MeshFormatError:
                continue
            if len(body.vertices) == len(mesh.vertices) and len(body.indices) == len(mesh.indices):
                return prefix, body
        pos = i + 1


def _tail_u32(data: bytes, off: int, what: str) -> tuple[int, int]:
    if off + 4 > len(data):
        raise ValueError(f"{what} past tail EOF at 0x{off:x}")
    return struct.unpack_from("<I", data, off)[0], off + 4


def _tail_string(data: bytes, off: int, what: str) -> tuple[str, int]:
    n, off = _tail_u32(data, off, f"{what} length")
    if not 1 <= n <= 512 or off + n > len(data):
        raise ValueError(f"bad {what} length {n} at 0x{off-4:x}")
    raw = data[off : off + n]
    if raw[-1] != 0:
        raise ValueError(f"{what} is not NUL-terminated")
    return raw[:-1].decode("latin-1"), off + n


def _parse_full_mesh_tail(tail: bytes, vertex_count: int) -> SkinData | None:
    """Parse the full-mesh tail (colors, material, then rigid skin records).

    The full-mesh block in a ``.skin`` stores its vertices in centimetres,
    unlike the matching ``.mesh`` which is in metres. The skin records assign
    every full-mesh vertex to exactly one bone with weight 1.0 and carry the
    inverse of that bone's raw (centimetre) world matrix at bind time.
    """
    try:
        off = 0
        if len(tail) < 5:
            return None
        color_flag = tail[off]
        off += 1
        color_count, off = _tail_u32(tail, off, "color count")
        if color_count > vertex_count:
            return None
        if color_flag:
            if off + color_count * 4 > len(tail):
                return None
            off += color_count * 4
        else:
            # The count is still present but no color payload follows.
            pass

        material_count, off = _tail_u32(tail, off, "material count")
        if material_count > 64:
            return None
        materials: list[SkinMaterial] = []
        for _ in range(material_count):
            if off + 21 > len(tail):
                return None
            raw = tail[off : off + 21]
            off += 21
            texture, off = _tail_string(tail, off, "texture name")
            name, off = _tail_string(tail, off, "material name")
            materials.append(SkinMaterial(raw=raw, texture=texture, name=name))

        triangle_count, off = _tail_u32(tail, off, "triangle count")
        if off + triangle_count * 2 > len(tail):
            return None
        triangle_materials = struct.unpack_from("<%dH" % triangle_count, tail, off)
        off += triangle_count * 2

        unknown = struct.unpack_from("<3I", tail, off)
        off += 12

        bone_count, off = _tail_u32(tail, off, "skin bone count")
        if not 1 <= bone_count <= 10_000:
            return None
        bones: list[SkinBone] = []
        for _ in range(bone_count):
            name, off = _tail_string(tail, off, "skin bone name")
            nidx, off = _tail_u32(tail, off, "skin bone vertex count")
            if off + nidx * 4 > len(tail):
                return None
            indices = tuple(struct.unpack_from("<%dI" % nidx, tail, off))
            off += nidx * 4
            nw, off = _tail_u32(tail, off, "skin bone weight count")
            if nw != nidx or off + nw * 4 > len(tail):
                return None
            weights = tuple(struct.unpack_from("<%df" % nw, tail, off))
            off += nw * 4
            if off + 64 > len(tail):
                return None
            inverse_bind = struct.unpack_from("<16f", tail, off)
            off += 64
            if any(i >= vertex_count for i in indices):
                return None
            bones.append(
                SkinBone(
                    name=name,
                    vertex_indices=indices,
                    weights=weights,
                    inverse_bind=inverse_bind,
                )
            )

        if off != len(tail):
            return None
        assigned = [i for b in bones for i in b.vertex_indices]
        if len(assigned) != vertex_count or len(set(assigned)) != vertex_count:
            return None
        return SkinData(
            materials=materials,
            triangle_materials=tuple(triangle_materials),
            unknown=unknown,
            bones=bones,
        )
    except (ValueError, struct.error):
        return None


def read_skin(skin_path: Path, *, skel_path: Path | None = None, mesh_path: Path | None = None) -> Skin:
    data = skin_path.read_bytes()
    if data[:5] != MAGIC:
        raise ValueError(f"{skin_path}: bad magic")
    hash_, sentinel, block_count = struct.unpack_from("<3I", data, 5)
    if sentinel != SENTINEL:
        raise ValueError(f"{skin_path}: bad sentinel {sentinel:#010x}")
    if block_count > 10_000:
        raise ValueError(f"{skin_path}: implausible block_count {block_count}")

    skel_path = skel_path or _skeleton_for(skin_path)
    if skel_path is None:
        raise ValueError(f"{skin_path}: no matching .skel")
    names = _read_skel_names(skel_path)

    mesh_path = mesh_path or skin_path.with_suffix(".mesh")
    mesh = read_mesh(mesh_path) if mesh_path.exists() else None
    full = None
    full_prefix = None
    if mesh is not None:
        found = _find_full_mesh(data, mesh, 0)
        if found:
            full_prefix, full = found

    # Bone chunks precede the full-mesh block. If there is no .mesh to anchor
    # the full block, the header count still tells us how many bone chunks exist.
    limit = full_prefix if full_prefix is not None else len(data)
    hits = _find_named_blocks(data, names)
    blocks: list[SkinBlock] = []
    for idx, (pos, name) in enumerate(hits):
        if pos >= limit:
            break
        nxt = hits[idx + 1][0] if idx + 1 < len(hits) else limit
        try:
            block = _read_bone_block(data, pos, name, nxt)
        except (ValueError, struct.error):
            continue
        # A real block's geometry must end before the next name hit.
        if block.geo_end > nxt:
            continue
        blocks.append(block)
    if len(blocks) > max(0, block_count - 1):
        blocks = blocks[: block_count - 1]

    trailing_start = 0
    if full is not None:
        # find the end of the full mesh body by re-parsing; read_mesh_body's tail
        # is everything to EOF, so locate the body end from the tail length.
        trailing_start = len(data) - len(full.tail)

    return Skin(
        path=str(skin_path),
        hash=hash_,
        block_count=block_count,
        blocks=blocks,
        full_mesh=full,
        skin_data=_parse_full_mesh_tail(full.tail, len(full.vertices)) if full else None,
        trailing=data[trailing_start:] if trailing_start else b"",
    )


def scan(root: Path) -> tuple[int, list[tuple[str, str]], dict]:
    total = 0
    bad: list[tuple[str, str]] = []
    sums = {"blocks": 0, "bone_vertices": 0, "bone_triangles": 0, "full_meshes": 0, "skin_data": 0}
    for p in sorted(root.rglob("*.skin")):
        total += 1
        try:
            s = read_skin(p)
        except Exception as e:
            bad.append((str(p), str(e)))
            continue
        sums["blocks"] += len(s.blocks)
        sums["bone_vertices"] += sum(len(b.vertices) for b in s.blocks)
        sums["bone_triangles"] += sum(b.triangle_count for b in s.blocks)
        sums["full_meshes"] += 1 if s.full_mesh else 0
        sums["skin_data"] += 1 if s.skin_data else 0
    return total, bad, sums


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--dump", type=Path)
    g.add_argument("--scan", type=Path)
    args = ap.parse_args(argv)

    if args.dump:
        print(json.dumps(read_skin(args.dump).summary(), indent=2))
        return 0

    total, bad, sums = scan(args.scan)
    print(f"scanned {total} .skin files, {len(bad)} bad")
    print(f"  bone blocks={sums['blocks']:,} vertices={sums['bone_vertices']:,} "
          f"triangles={sums['bone_triangles']:,} full_meshes={sums['full_meshes']:,} "
          f"skin_data={sums['skin_data']:,}")
    for path, why in bad[:20]:
        print(f"  FAIL {path}: {why}")
    return 0 if not bad else 1


if __name__ == "__main__":
    raise SystemExit(main())
