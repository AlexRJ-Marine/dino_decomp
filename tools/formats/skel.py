#!/usr/bin/env python3
"""Reader for Dino Island ``.skel`` skeleton files.

Layout (validated on all 64 retail skeletons):

    0x00  5   magic           01 03 00 00 00
    0x05  4   hash
    0x09  4   sentinel        0x56789abc
    0x0d  4   bone_count
    0x11  64*bone_count       4x4 bone matrices, row-major float32
    ...   4   child_count     (== bone_count)
    ...       bone_count x (u32 child_count + child_count x u32)
    ...   4   parent_count    (== bone_count)
    ...       bone_count x i32 parent (-1 for a root)
    ...   4   name_count      (== bone_count)
    ...       bone_count x (u32 length + NUL-terminated name)
    ...   4   flag_count      (== bone_count)
    ...       bone_count x u32 flags (all 1 in retail data)

The matrices use the 3ds Max / row-vector convention::

    [ R00 R01 R02  0 ]
    [ R10 R11 R12  0 ]
    [ R20 R21 R22  0 ]
    [ Tx  Ty  Tz   1 ]

so ``world = local @ parent_world`` and the translation is row 3. The root
bone (``Scene_Root``) carries the global 0.01 scale for dinosaur skeletons;
there is no separate scale field in the header.

The parent array is authoritative. The per-bone child lists are correct for
the 20 dinosaur skeletons, but 44 prop skeletons store each child index one
lower than the parent array implies (and include the bone itself). Read
hierarchy from ``parent``.
"""
from __future__ import annotations

import argparse
import json
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path

MAGIC = b"\x01\x03\x00\x00\x00"
SENTINEL = 0x56789ABC
HEADER_SIZE = 0x11
MATRIX_FLOATS = 16
MATRIX_BYTES = MATRIX_FLOATS * 4


class SkelFormatError(ValueError):
    pass


@dataclass
class SkelBone:
    index: int
    name: str
    matrix: tuple[float, ...]
    parent: int
    children: tuple[int, ...]
    flag: int = 1

    @property
    def translation(self) -> tuple[float, float, float]:
        """Local translation (row 3 of the 4x4 matrix)."""
        return (self.matrix[12], self.matrix[13], self.matrix[14])

    @property
    def rows(self) -> tuple[tuple[float, ...], ...]:
        m = self.matrix
        return (m[0:4], m[4:8], m[8:12], m[12:16])

    def to_dict(self) -> dict:
        return {
            "index": self.index,
            "name": self.name,
            "parent": self.parent,
            "children": list(self.children),
            "flag": self.flag,
            "matrix": [round(v, 6) for v in self.matrix],
            "translation": [round(v, 6) for v in self.translation],
        }


@dataclass
class Skeleton:
    path: str
    hash: int
    bone_count: int
    bones: list[SkelBone] = field(default_factory=list)
    trailing: bytes = b""

    @property
    def roots(self) -> tuple[int, ...]:
        return tuple(b.index for b in self.bones if b.parent < 0)

    @property
    def root_scale(self) -> float:
        """Uniform scale encoded in the first root's 3x3 basis."""
        if not self.bones:
            return 0.0
        m = self.bones[self.roots[0] if self.roots else 0].matrix
        return (m[0] ** 2 + m[1] ** 2 + m[2] ** 2) ** 0.5

    # Backwards-compatible name for the old header field. The value is now
    # derived from the root bone matrix, where it actually lives.
    @property
    def global_scale(self) -> float:
        return self.root_scale

    def by_name(self, name: str) -> SkelBone:
        for bone in self.bones:
            if bone.name == name:
                return bone
        raise KeyError(name)

    def world_matrices(self) -> list[tuple[float, ...]]:
        """Compose local matrices root-to-leaf: ``local @ parent_world``."""
        n = self.bone_count
        world: list[tuple[float, ...] | None] = [None] * n
        visiting: set[int] = set()

        def resolve(i: int) -> tuple[float, ...]:
            cached = world[i]
            if cached is not None:
                return cached
            if i in visiting:
                raise SkelFormatError(f"cycle in skeleton hierarchy at bone {i}")
            visiting.add(i)
            bone = self.bones[i]
            local = bone.matrix
            if bone.parent < 0:
                result = local
            else:
                result = _mat_mul(local, resolve(bone.parent))
            visiting.remove(i)
            world[i] = result
            return result

        for i in range(n):
            resolve(i)
        return [m for m in world if m is not None]

    def world_positions(self) -> list[tuple[float, float, float]]:
        return [(m[12], m[13], m[14]) for m in self.world_matrices()]

    def world_bounds(self) -> tuple[tuple[float, float, float], tuple[float, float, float]]:
        positions = self.world_positions()
        if not positions:
            return ((0.0, 0.0, 0.0), (0.0, 0.0, 0.0))
        lo = tuple(min(p[i] for p in positions) for i in range(3))
        hi = tuple(max(p[i] for p in positions) for i in range(3))
        return lo, hi

    def summary(self) -> dict:
        lo, hi = self.world_bounds()
        return {
            "path": self.path,
            "hash": f"{self.hash:#010x}",
            "bone_count": self.bone_count,
            "root_scale": round(self.root_scale, 8),
            "roots": list(self.roots),
            "world_bounds": {
                "min": [round(v, 4) for v in lo],
                "max": [round(v, 4) for v in hi],
                "size": [round(hi[i] - lo[i], 4) for i in range(3)],
            },
            "bones": [b.to_dict() for b in self.bones],
            "trailing_bytes": len(self.trailing),
        }


def _mat_mul(a: tuple[float, ...], b: tuple[float, ...]) -> tuple[float, ...]:
    """Multiply two row-major 4x4 matrices."""
    out = [0.0] * 16
    for r in range(4):
        for c in range(4):
            out[r * 4 + c] = sum(a[r * 4 + k] * b[k * 4 + c] for k in range(4))
    return tuple(out)


def _read_u32(data: bytes, off: int, what: str) -> tuple[int, int]:
    if off + 4 > len(data):
        raise SkelFormatError(f"{what} past EOF at 0x{off:x}")
    return struct.unpack_from("<I", data, off)[0], off + 4


def _expect_count(data: bytes, off: int, expected: int, what: str) -> int:
    count, off = _read_u32(data, off, what)
    if count != expected:
        raise SkelFormatError(f"{what} {count} != bone_count {expected} at 0x{off-4:x}")
    return off


def read_skel(path: Path) -> Skeleton:
    data = path.read_bytes()
    if len(data) < HEADER_SIZE:
        raise SkelFormatError(f"file too short: 0x{len(data):x} bytes")
    if data[:5] != MAGIC:
        raise SkelFormatError(f"bad magic at 0x00: {data[:5].hex(' ')}")
    hash_, sentinel, count = struct.unpack_from("<3I", data, 5)
    if sentinel != SENTINEL:
        raise SkelFormatError(f"bad sentinel at 0x09: {sentinel:#010x}")
    if not 1 <= count <= 10_000:
        raise SkelFormatError(f"implausible bone_count {count}")

    matrices_start = HEADER_SIZE
    matrices_end = matrices_start + count * MATRIX_BYTES
    if matrices_end > len(data):
        raise SkelFormatError(f"bone matrices run past EOF at 0x{matrices_end:x}")
    matrices = [
        struct.unpack_from("<16f", data, matrices_start + i * MATRIX_BYTES)
        for i in range(count)
    ]

    off = _expect_count(data, matrices_end, count, "child count")
    children: list[tuple[int, ...]] = []
    for i in range(count):
        nchild, off = _read_u32(data, off, f"children[{i}] count")
        if nchild > count:
            raise SkelFormatError(f"implausible children[{i}] count {nchild}")
        if off + nchild * 4 > len(data):
            raise SkelFormatError(f"children[{i}] run past EOF at 0x{off:x}")
        idx = struct.unpack_from("<%dI" % nchild, data, off)
        if any(x >= count for x in idx):
            raise SkelFormatError(f"children[{i}] index out of range")
        children.append(tuple(idx))
        off += nchild * 4

    off = _expect_count(data, off, count, "parent count")
    if off + count * 4 > len(data):
        raise SkelFormatError(f"parent table runs past EOF at 0x{off:x}")
    parents = struct.unpack_from("<%di" % count, data, off)
    if any(p < -1 or p >= count for p in parents):
        raise SkelFormatError("parent index out of range")
    off += count * 4

    off = _expect_count(data, off, count, "name count")
    names: list[str] = []
    for i in range(count):
        ln, off = _read_u32(data, off, f"name[{i}] length")
        if not 1 <= ln <= 256 or off + ln > len(data):
            raise SkelFormatError(f"bad name[{i}] length {ln} at 0x{off-4:x}")
        raw = data[off : off + ln]
        if raw[-1] != 0:
            raise SkelFormatError(f"name[{i}] is not NUL-terminated")
        names.append(raw[:-1].decode("latin-1"))
        off += ln

    off = _expect_count(data, off, count, "flag count")
    if off + count * 4 > len(data):
        raise SkelFormatError("flag table runs past EOF")
    flags = struct.unpack_from("<%dI" % count, data, off)
    off += count * 4

    bones = [
        SkelBone(
            index=i,
            name=names[i],
            matrix=matrices[i],
            parent=parents[i],
            children=children[i],
            flag=flags[i],
        )
        for i in range(count)
    ]
    return Skeleton(
        path=str(path),
        hash=hash_,
        bone_count=count,
        bones=bones,
        trailing=data[off:],
    )


def scan(root: Path) -> tuple[int, list[tuple[str, str]], dict]:
    total = 0
    bad: list[tuple[str, str]] = []
    bones = 0
    for p in sorted(root.rglob("*.skel")):
        total += 1
        try:
            s = read_skel(p)
        except Exception as e:
            bad.append((str(p), str(e)))
            continue
        bones += s.bone_count
    return total, bad, {"bones": bones}


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--dump", type=Path)
    g.add_argument("--scan", type=Path)
    args = ap.parse_args(argv)

    if args.dump:
        print(json.dumps(read_skel(args.dump).summary(), indent=2))
        return 0

    total, bad, sums = scan(args.scan)
    print(f"scanned {total} .skel files, {len(bad)} bad, {sums['bones']:,} bones total")
    for path, why in bad[:20]:
        print(f"  FAIL {path}: {why}")
    return 0 if not bad else 1


if __name__ == "__main__":
    raise SystemExit(main())
