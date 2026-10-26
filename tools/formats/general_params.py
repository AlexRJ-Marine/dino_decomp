#!/usr/bin/env python3
"""Reader for ``Data/Design/GeneralParams/GeneralParams.bgd``.

A small packed binary record (170 bytes), read by ``0x542f80``:

```text
0x00  f32 x4      3000.0, 1800.0, 3.0, 1.5
0x10  string x4   "", "rocket", "trumpet", "startBlast"
      string      "whitle"
      f32         8.0
      f32         120.0
      f32         40.0
      u32         10
      f32         60.0
      f32 x16     3.0 x5, 2.0 x3, 3.0, 2.0 x3, 3.0, 2.0 x3, 6.0,
                  10.5, 14.0, 20.0
      f32 x4
```

Each string is a ``u32`` byte count (including the NUL) followed by that many
bytes, so the stream is unaligned.

The runtime offsets are known from the loader; the values are consumed by the
park general-parameters object (park app ``+0x110..+0x1f8``). See
``docs/formats/general-params.md``.
"""
from __future__ import annotations

import argparse
import json
import struct
from dataclasses import dataclass, field
from pathlib import Path


class GeneralParamsError(ValueError):
    pass


@dataclass
class GeneralParams:
    path: str
    field_110: float
    field_114: float
    field_118: float
    field_11c: float
    sounds: list[str]
    whistle: str
    field_154: float
    field_150: float
    field_1a0: float
    field_1a4: int
    field_1f8: float
    matrix_1a8: list[float] = field(default_factory=list)
    vector_1e8: list[float] = field(default_factory=list)

    def summary(self) -> dict:
        return {
            "path": self.path,
            "field_110": self.field_110,
            "field_114": self.field_114,
            "field_118": self.field_118,
            "field_11c": self.field_11c,
            "sounds": self.sounds,
            "whistle": self.whistle,
            "field_154": self.field_154,
            "field_150": self.field_150,
            "field_1a0": self.field_1a0,
            "field_1a4": self.field_1a4,
            "field_1f8": self.field_1f8,
            "matrix_1a8": self.matrix_1a8,
            "vector_1e8": self.vector_1e8,
        }


def _string(data: bytes, off: int) -> tuple[str, int]:
    if off + 4 > len(data):
        raise GeneralParamsError("truncated string length")
    size = struct.unpack_from("<I", data, off)[0]
    if size == 0 or off + 4 + size > len(data):
        raise GeneralParamsError(f"bad string length {size} at 0x{off:x}")
    raw = data[off + 4: off + 4 + size]
    if raw[-1] != 0:
        raise GeneralParamsError(f"string at 0x{off:x} is not NUL-terminated")
    return raw[:-1].decode("latin-1"), off + 4 + size


def read_general_params(path: Path) -> GeneralParams:
    data = path.read_bytes()
    if len(data) < 0x50:
        raise GeneralParamsError(f"{path}: too short")
    a, b, c, d = struct.unpack_from("<4f", data, 0)
    off = 0x10
    sounds = []
    for _ in range(4):
        text, off = _string(data, off)
        sounds.append(text)
    whistle, off = _string(data, off)
    field_154, field_150, field_1a0, field_1a4, field_1f8 = struct.unpack_from(
        "<ff f I f", data, off
    )
    off += 20
    matrix = list(struct.unpack_from("<16f", data, off))
    off += 64
    vector = list(struct.unpack_from("<4f", data, off))
    off += 16
    if off != len(data):
        raise GeneralParamsError(f"{path}: {len(data) - off} trailing bytes")
    return GeneralParams(
        path=str(path),
        field_110=a,
        field_114=b,
        field_118=c,
        field_11c=d,
        sounds=sounds,
        whistle=whistle,
        field_154=field_154,
        field_150=field_150,
        field_1a0=field_1a0,
        field_1a4=field_1a4,
        field_1f8=field_1f8,
        matrix_1a8=matrix,
        vector_1e8=vector,
    )


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("path", type=Path)
    args = ap.parse_args(argv)
    print(json.dumps(read_general_params(args.path).summary(), indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
