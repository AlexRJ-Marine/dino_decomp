#!/usr/bin/env python3
"""Reader for Dino Island ``.anim`` clips.

Layout (validated on all 299 retail files)::

    0x00  5   magic          01 03 00 00 00
    0x05  4   hash
    0x09  4   sentinel       0xdef01234
    0x0d  4   duration       time units (4800 for most clips)
    0x11  4   start          usually 0
    0x15  4   bone_count     (35 for the dinosaur rigs)
    0x19  ... one block per animated bone:
              u32 name_len + NUL-terminated name
              u32 key_count
              key_count x u64 time (strictly increasing)
              u32 matrix_count (== key_count)
              matrix_count x 64-byte 4x4 matrix, row-major

The matrices are **local** transforms in the same row-vector convention as
``.skel``: upper 3x3 rotation, translation in row 3. They are pure rotation +
translation (row norms are 1, last column is 0,0,0,1).

Sampling interpolates translation linearly and rotation with quaternion slerp.
"""
from __future__ import annotations

import argparse
import bisect
import json
import math
import struct
from dataclasses import dataclass, field
from pathlib import Path

MAGIC = b"\x01\x03\x00\x00\x00"
SENTINEL = 0xDEF01234
HEADER_SIZE = 0x19


class AnimFormatError(ValueError):
    pass


def _matrix_quat(m: tuple[float, ...]) -> tuple[float, float, float, float]:
    """Quaternion for a row-vector 3x3 rotation (transpose to column form)."""
    # Column-vector rotation is the transpose of the row-vector one.
    r00, r01, r02 = m[0], m[4], m[8]
    r10, r11, r12 = m[1], m[5], m[9]
    r20, r21, r22 = m[2], m[6], m[10]
    t = r00 + r11 + r22
    if t > 0.0:
        s = math.sqrt(t + 1.0) * 2.0
        w = 0.25 * s
        x = (r21 - r12) / s
        y = (r02 - r20) / s
        z = (r10 - r01) / s
    elif r00 > r11 and r00 > r22:
        s = math.sqrt(1.0 + r00 - r11 - r22) * 2.0
        w = (r21 - r12) / s
        x = 0.25 * s
        y = (r01 + r10) / s
        z = (r02 + r20) / s
    elif r11 > r22:
        s = math.sqrt(1.0 + r11 - r00 - r22) * 2.0
        w = (r02 - r20) / s
        x = (r01 + r10) / s
        y = 0.25 * s
        z = (r12 + r21) / s
    else:
        s = math.sqrt(1.0 + r22 - r00 - r11) * 2.0
        w = (r10 - r01) / s
        x = (r02 + r20) / s
        y = (r12 + r21) / s
        z = 0.25 * s
    n = math.sqrt(w * w + x * x + y * y + z * z)
    return (w / n, x / n, y / n, z / n)


def _slerp(q0, q1, a: float):
    dot = q0[0] * q1[0] + q0[1] * q1[1] + q0[2] * q1[2] + q0[3] * q1[3]
    if dot < 0.0:
        q1 = (-q1[0], -q1[1], -q1[2], -q1[3])
        dot = -dot
    if dot > 0.9995:
        q = tuple(q0[i] + a * (q1[i] - q0[i]) for i in range(4))
        n = math.sqrt(sum(v * v for v in q))
        return tuple(v / n for v in q)
    theta = math.acos(max(-1.0, min(1.0, dot)))
    sin_theta = math.sin(theta)
    w0 = math.sin((1.0 - a) * theta) / sin_theta
    w1 = math.sin(a * theta) / sin_theta
    return tuple(w0 * q0[i] + w1 * q1[i] for i in range(4))


def _quat_matrix(q, translation) -> tuple[float, ...]:
    w, x, y, z = q
    # Column-vector matrix, then transpose for the row-vector convention.
    xx, yy, zz = x * x, y * y, z * z
    xy, xz, yz = x * y, x * z, y * z
    wx, wy, wz = w * x, w * y, w * z
    col = (
        (1 - 2 * (yy + zz), 2 * (xy - wz), 2 * (xz + wy)),
        (2 * (xy + wz), 1 - 2 * (xx + zz), 2 * (yz - wx)),
        (2 * (xz - wy), 2 * (yz + wx), 1 - 2 * (xx + yy)),
    )
    tx, ty, tz = translation
    return (
        col[0][0], col[1][0], col[2][0], 0.0,
        col[0][1], col[1][1], col[2][1], 0.0,
        col[0][2], col[1][2], col[2][2], 0.0,
        tx, ty, tz, 1.0,
    )


@dataclass
class AnimBone:
    name: str
    times: tuple[int, ...]
    matrices: tuple[tuple[float, ...], ...]

    def sample(self, time: float) -> tuple[float, ...]:
        if not self.times:
            return (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
                    0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0)
        if time <= self.times[0]:
            return self.matrices[0]
        if time >= self.times[-1]:
            return self.matrices[-1]
        i = bisect.bisect_right(self.times, time) - 1
        t0, t1 = self.times[i], self.times[i + 1]
        if time == t0:
            return self.matrices[i]
        if time == t1:
            return self.matrices[i + 1]
        a = (time - t0) / (t1 - t0) if t1 != t0 else 0.0
        m0, m1 = self.matrices[i], self.matrices[i + 1]
        q = _slerp(_matrix_quat(m0), _matrix_quat(m1), a)
        t = tuple(m0[12 + k] + a * (m1[12 + k] - m0[12 + k]) for k in range(3))
        return _quat_matrix(q, t)

    def to_dict(self) -> dict:
        return {
            "name": self.name,
            "keys": len(self.times),
            "first_time": self.times[0] if self.times else None,
            "last_time": self.times[-1] if self.times else None,
        }


@dataclass
class Animation:
    path: str
    hash: int
    duration: int
    start: int
    bones: list[AnimBone] = field(default_factory=list)

    @property
    def bone_count(self) -> int:
        return len(self.bones)

    @property
    def total_keys(self) -> int:
        return sum(len(b.times) for b in self.bones)

    def by_name(self, name: str) -> AnimBone:
        for bone in self.bones:
            if bone.name == name:
                return bone
        raise KeyError(name)

    def sample(self, time: float) -> dict[str, tuple[float, ...]]:
        return {b.name: b.sample(time) for b in self.bones}

    def summary(self) -> dict:
        return {
            "path": self.path,
            "hash": f"{self.hash:#010x}",
            "duration": self.duration,
            "start": self.start,
            "bone_count": self.bone_count,
            "total_keys": self.total_keys,
            "bones": [b.to_dict() for b in self.bones],
        }


def read_anim(path: Path) -> Animation:
    data = path.read_bytes()
    if len(data) < HEADER_SIZE:
        raise AnimFormatError(f"file too short: 0x{len(data):x} bytes")
    if data[:5] != MAGIC:
        raise AnimFormatError(f"bad magic at 0x00: {data[:5].hex(' ')}")
    hash_, sentinel, duration, start, bone_count = struct.unpack_from("<5I", data, 5)
    if sentinel != SENTINEL:
        raise AnimFormatError(f"bad sentinel at 0x09: {sentinel:#010x}")
    if bone_count > 10_000:
        raise AnimFormatError(f"implausible bone_count {bone_count}")

    off = HEADER_SIZE
    bones: list[AnimBone] = []
    for _ in range(bone_count):
        if off + 4 > len(data):
            raise AnimFormatError(f"name length past EOF at 0x{off:x}")
        n = struct.unpack_from("<I", data, off)[0]
        off += 4
        if not 1 <= n <= 256 or off + n > len(data):
            raise AnimFormatError(f"bad name length {n} at 0x{off-4:x}")
        raw = data[off : off + n]
        if raw[-1] != 0:
            raise AnimFormatError(f"name at 0x{off:x} is not NUL-terminated")
        name = raw[:-1].decode("latin-1")
        off += n

        if off + 4 > len(data):
            raise AnimFormatError(f"{name}: missing key count")
        key_count = struct.unpack_from("<I", data, off)[0]
        off += 4
        if key_count > 100_000 or off + key_count * 8 > len(data):
            raise AnimFormatError(f"{name}: bad key count {key_count}")
        times = struct.unpack_from("<%dQ" % key_count, data, off)
        off += key_count * 8
        if any(times[i] >= times[i + 1] for i in range(len(times) - 1)):
            raise AnimFormatError(f"{name}: times are not strictly increasing")

        if off + 4 > len(data):
            raise AnimFormatError(f"{name}: missing matrix count")
        matrix_count = struct.unpack_from("<I", data, off)[0]
        off += 4
        if matrix_count != key_count:
            raise AnimFormatError(
                f"{name}: matrix count {matrix_count} != key count {key_count}"
            )
        if off + matrix_count * 64 > len(data):
            raise AnimFormatError(f"{name}: matrices past EOF")
        matrices = [
            struct.unpack_from("<16f", data, off + i * 64)
            for i in range(matrix_count)
        ]
        off += matrix_count * 64
        bones.append(AnimBone(name=name, times=tuple(times), matrices=tuple(matrices)))

    if off != len(data):
        raise AnimFormatError(f"trailing {len(data) - off} bytes at 0x{off:x}")
    return Animation(
        path=str(path), hash=hash_, duration=duration, start=start, bones=bones
    )


def scan(root: Path) -> tuple[int, list[tuple[str, str]], dict]:
    total = 0
    bad: list[tuple[str, str]] = []
    sums = {"bones": 0, "keys": 0, "clips": 0}
    for p in sorted(root.rglob("*.anim")):
        total += 1
        try:
            a = read_anim(p)
        except Exception as e:
            bad.append((str(p), str(e)))
            continue
        sums["clips"] += 1
        sums["bones"] += a.bone_count
        sums["keys"] += a.total_keys
    return total, bad, sums


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--dump", type=Path)
    g.add_argument("--scan", type=Path)
    args = ap.parse_args(argv)

    if args.dump:
        print(json.dumps(read_anim(args.dump).summary(), indent=2))
        return 0

    total, bad, sums = scan(args.scan)
    print(f"scanned {total} .anim files, {len(bad)} bad, "
          f"{sums['bones']:,} bone tracks, {sums['keys']:,} keys")
    for path, why in bad[:20]:
        print(f"  FAIL {path}: {why}")
    return 0 if not bad else 1


if __name__ == "__main__":
    raise SystemExit(main())
