#!/usr/bin/env python3
"""Reader for Dino Island ``.fnt`` bitmap fonts.

Retail files (``arial12``/``font12``/``font14``/``font19``):

    0x00  9 x u32 header:
            version, atlas_width, atlas_height, font_size,
            cell_height, base, line_height, glyph_count, hash
    0x24  glyph_count x u16 character codes
    ...   glyph_count x 8 bytes metrics (u16 x0, y0, x1, y1)
    ...   atlas: atlas_width * atlas_height * 2 bytes, 16-bit texels

The metrics array has two leading entries that are not addressed by the code
table; retail data maps ``codes[i]`` to ``metrics[i + 2]``. The last two code
words are padding. Glyph rectangles are inclusive: width = x1 - x0 + 1.

The 16-bit atlas stores the same value in both bytes for the retail fonts; the
high byte is the coverage/alpha used for rendering.
"""
from __future__ import annotations

import argparse
import json
import struct
from dataclasses import dataclass, field
from pathlib import Path

HEADER_SIZE = 0x24
METRIC_SHIFT = 2


class FntFormatError(ValueError):
    pass


@dataclass
class FntGlyph:
    code: int
    x0: int
    y0: int
    x1: int
    y1: int

    @property
    def width(self) -> int:
        return self.x1 - self.x0 + 1

    @property
    def height(self) -> int:
        return self.y1 - self.y0 + 1

    def to_dict(self) -> dict:
        return {
            "code": self.code,
            "char": chr(self.code) if 32 <= self.code < 127 else None,
            "rect": [self.x0, self.y0, self.x1, self.y1],
            "width": self.width,
            "height": self.height,
        }


@dataclass
class Fnt:
    path: str
    version: int
    atlas_width: int
    atlas_height: int
    font_size: int
    cell_height: int
    base: int
    line_height: int
    glyph_count: int
    hash: int
    codes: tuple[int, ...]
    metrics: tuple[tuple[int, int, int, int], ...]
    atlas: bytes
    glyphs: dict[int, FntGlyph] = field(default_factory=dict)

    @property
    def padding_codes(self) -> tuple[int, ...]:
        return self.codes[-2:]

    def glyph(self, char: str) -> FntGlyph | None:
        return self.glyphs.get(ord(char))

    def alpha_at(self, x: int, y: int) -> int:
        return self.atlas[(y * self.atlas_width + x) * 2 + 1]

    def summary(self) -> dict:
        return {
            "path": self.path,
            "version": self.version,
            "atlas_width": self.atlas_width,
            "atlas_height": self.atlas_height,
            "font_size": self.font_size,
            "cell_height": self.cell_height,
            "base": self.base,
            "line_height": self.line_height,
            "glyph_count": self.glyph_count,
            "mapped_glyphs": len(self.glyphs),
            "padding_codes": list(self.padding_codes),
            "hash": f"{self.hash:#010x}",
            "atlas_bytes": len(self.atlas),
        }


def read_fnt(path: Path) -> Fnt:
    data = path.read_bytes()
    if len(data) < HEADER_SIZE:
        raise FntFormatError(f"{path}: shorter than header")
    (version, aw, ah, font_size, cell_height, base, line_height,
     glyph_count, hash_) = struct.unpack_from("<9I", data, 0)
    if version != 1:
        raise FntFormatError(f"{path}: unsupported version {version}")
    if not 1 <= aw <= 4096 or not 1 <= ah <= 4096:
        raise FntFormatError(f"{path}: bad atlas size {aw}x{ah}")
    if not 1 <= glyph_count <= 10_000:
        raise FntFormatError(f"{path}: bad glyph count {glyph_count}")

    codes_off = HEADER_SIZE
    metrics_off = codes_off + glyph_count * 2
    atlas_off = metrics_off + glyph_count * 8
    if atlas_off > len(data):
        raise FntFormatError(f"{path}: tables past EOF")
    codes = struct.unpack_from("<%dH" % glyph_count, data, codes_off)
    metrics = tuple(
        struct.unpack_from("<4H", data, metrics_off + i * 8)
        for i in range(glyph_count)
    )
    expected = aw * ah * 2
    atlas = data[atlas_off:]
    if len(atlas) != expected:
        raise FntFormatError(
            f"{path}: atlas {len(atlas)} bytes, expected {expected} ({aw}x{ah}x2)"
        )

    glyphs: dict[int, FntGlyph] = {}
    for i in range(glyph_count - METRIC_SHIFT):
        code = codes[i]
        x0, y0, x1, y1 = metrics[i + METRIC_SHIFT]
        if not (0 <= x0 <= x1 < aw and 0 <= y0 <= y1 < ah):
            raise FntFormatError(f"{path}: glyph {i} rect out of atlas")
        glyphs[code] = FntGlyph(code=code, x0=x0, y0=y0, x1=x1, y1=y1)

    return Fnt(
        path=str(path),
        version=version,
        atlas_width=aw,
        atlas_height=ah,
        font_size=font_size,
        cell_height=cell_height,
        base=base,
        line_height=line_height,
        glyph_count=glyph_count,
        hash=hash_,
        codes=tuple(codes),
        metrics=metrics,
        atlas=atlas,
        glyphs=glyphs,
    )


def scan(root: Path) -> tuple[int, list[tuple[str, str]], dict]:
    total = 0
    bad: list[tuple[str, str]] = []
    sums = {"glyphs": 0}
    for p in sorted(root.rglob("*.fnt")):
        total += 1
        try:
            f = read_fnt(p)
        except Exception as e:
            bad.append((str(p), str(e)))
            continue
        sums["glyphs"] += len(f.glyphs)
    return total, bad, sums


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--dump", type=Path)
    g.add_argument("--scan", type=Path)
    args = ap.parse_args(argv)
    if args.dump:
        print(json.dumps(read_fnt(args.dump).summary(), indent=2))
        return 0
    total, bad, sums = scan(args.scan)
    print(f"scanned {total} .fnt files, {len(bad)} bad, {sums['glyphs']:,} glyphs")
    for path, why in bad[:20]:
        print(f"  FAIL {path}: {why}")
    return 0 if not bad else 1


if __name__ == "__main__":
    raise SystemExit(main())
