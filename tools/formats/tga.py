#!/usr/bin/env python3
"""Reader for the TGA files shipped with Dino Island.

Retail inventory: 649 files, all uncompressed, no color map.

- 405 × true-color 32-bit (BGR(A))
- 240 × true-color 24-bit (BGR)
- 4 × grayscale 8-bit

Header origin is bottom-left for every retail file. The reader also supports
16-bit true-color and the RLE variants (types 10/11) for completeness, and
returns top-left RGBA8 pixels.
"""
from __future__ import annotations

import argparse
import json
import struct
from dataclasses import dataclass
from pathlib import Path

HEADER_SIZE = 18


class TgaFormatError(ValueError):
    pass


@dataclass
class Tga:
    path: str
    width: int
    height: int
    pixel_depth: int
    image_type: int
    descriptor: int
    pixels: bytes = b""  # RGBA8, top-left origin; empty when header-only

    @property
    def channels(self) -> int:
        return self.pixel_depth // 8

    @property
    def top_origin(self) -> bool:
        return bool(self.descriptor & 0x20)

    @property
    def right_origin(self) -> bool:
        return bool(self.descriptor & 0x10)

    def summary(self) -> dict:
        return {
            "path": self.path,
            "width": self.width,
            "height": self.height,
            "pixel_depth": self.pixel_depth,
            "image_type": self.image_type,
            "descriptor": self.descriptor,
            "top_origin": self.top_origin,
            "right_origin": self.right_origin,
            "decoded": bool(self.pixels),
        }


def _read_header(data: bytes, path: str) -> tuple[Tga, int, int, int]:
    if len(data) < HEADER_SIZE:
        raise TgaFormatError(f"{path}: file shorter than TGA header")
    id_length, cmap_type, image_type = data[0], data[1], data[2]
    cmap_origin, cmap_length, cmap_depth = struct.unpack_from("<HHB", data, 3)
    width, height = struct.unpack_from("<HH", data, 12)
    pixel_depth, descriptor = data[16], data[17]
    if image_type not in (2, 3, 10, 11):
        raise TgaFormatError(f"{path}: unsupported image_type {image_type}")
    if cmap_type != 0:
        raise TgaFormatError(f"{path}: unsupported color map (type {cmap_type})")
    if width == 0 or height == 0:
        raise TgaFormatError(f"{path}: zero dimensions {width}x{height}")
    if pixel_depth not in (8, 16, 24, 32):
        raise TgaFormatError(f"{path}: unsupported pixel depth {pixel_depth}")
    tga = Tga(
        path=path,
        width=width,
        height=height,
        pixel_depth=pixel_depth,
        image_type=image_type,
        descriptor=descriptor,
    )
    return tga, id_length, cmap_depth, cmap_length


def read_tga(path: Path, *, decode: bool = True) -> Tga:
    data = path.read_bytes()
    tga, id_length, _cmap_depth, _cmap_length = _read_header(data, str(path))
    if not decode:
        return tga

    off = HEADER_SIZE + id_length
    if off > len(data):
        raise TgaFormatError(f"{path}: image ID past EOF")

    bpp = tga.channels
    total = tga.width * tga.height
    raw = bytearray(total * bpp)
    rle = tga.image_type in (10, 11)

    def decode_pixel(chunk: bytes, pos: int) -> bytes:
        if tga.image_type in (2, 10):
            if bpp == 3:
                b, g, r = chunk[pos], chunk[pos + 1], chunk[pos + 2]
                return bytes((r, g, b, 255))
            if bpp == 4:
                b, g, r, a = chunk[pos], chunk[pos + 1], chunk[pos + 2], chunk[pos + 3]
                return bytes((r, g, b, a))
            if bpp == 2:
                v = chunk[pos] | (chunk[pos + 1] << 8)
                r = (v >> 10) & 0x1F
                g = (v >> 5) & 0x1F
                b = v & 0x1F
                return bytes(((r << 3) | (r >> 2), (g << 3) | (g >> 2),
                              (b << 3) | (b >> 2), 255 if v & 0x8000 else 0))
        else:  # grayscale 8-bit
            v = chunk[pos]
            return bytes((v, v, v, 255))
        raise TgaFormatError(f"{path}: unsupported true-color depth {tga.pixel_depth}")

    if not rle:
        needed = total * bpp
        if off + needed > len(data):
            raise TgaFormatError(f"{path}: pixel data past EOF")
        for i in range(total):
            p = decode_pixel(data, off + i * bpp)
            raw[i * 4 : i * 4 + 4] = p
    else:
        written = 0
        while written < total:
            if off >= len(data):
                raise TgaFormatError(f"{path}: RLE data past EOF")
            packet = data[off]
            off += 1
            count = (packet & 0x7F) + 1
            if packet & 0x80:
                p = decode_pixel(data, off)
                off += bpp
                for _ in range(count):
                    if written >= total:
                        break
                    raw[written * 4 : written * 4 + 4] = p
                    written += 1
            else:
                if off + count * bpp > len(data):
                    raise TgaFormatError(f"{path}: raw RLE packet past EOF")
                for _ in range(count):
                    if written >= total:
                        break
                    p = decode_pixel(data, off)
                    off += bpp
                    raw[written * 4 : written * 4 + 4] = p
                    written += 1
        if written != total:
            raise TgaFormatError(f"{path}: RLE decoded {written}/{total} pixels")

    # Convert file order to top-left origin.
    flip_v = not tga.top_origin
    flip_h = tga.right_origin
    out = bytearray(total * 4)
    src_row = tga.width * 4
    for y in range(tga.height):
        src_y = (tga.height - 1 - y) if flip_v else y
        row = raw[src_y * src_row : (src_y + 1) * src_row]
        if flip_h:
            row = b"".join(row[x : x + 4] for x in range(len(row) - 4, -1, -4))
        out[y * src_row : (y + 1) * src_row] = row
    tga.pixels = bytes(out)
    return tga


def scan(root: Path) -> tuple[int, list[tuple[str, str]], dict]:
    total = 0
    bad: list[tuple[str, str]] = []
    sums = {"pixels": 0}
    for p in sorted(root.rglob("*.tga")):
        total += 1
        try:
            t = read_tga(p, decode=False)
        except Exception as e:
            bad.append((str(p), str(e)))
            continue
        sums["pixels"] += t.width * t.height
    return total, bad, sums


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--dump", type=Path)
    g.add_argument("--scan", type=Path)
    args = ap.parse_args(argv)
    if args.dump:
        print(json.dumps(read_tga(args.dump, decode=False).summary(), indent=2))
        return 0
    total, bad, sums = scan(args.scan)
    print(f"scanned {total} TGA files, {len(bad)} bad, {sums['pixels']:,} pixels")
    for path, why in bad[:20]:
        print(f"  FAIL {path}: {why}")
    return 0 if not bad else 1


if __name__ == "__main__":
    raise SystemExit(main())
