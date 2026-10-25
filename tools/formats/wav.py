#!/usr/bin/env python3
"""Reader for the PCM WAV files shipped with Dino Island.

Retail inventory: 231 files, all RIFF/WAVE PCM (format 1), 16-bit.

- 225 × mono 22,050 Hz
- 4 × stereo 22,050 Hz
- 2 × mono 44,100 Hz
"""
from __future__ import annotations

import argparse
import array
import json
import struct
from dataclasses import dataclass
from pathlib import Path


class WavFormatError(ValueError):
    pass


@dataclass
class Wav:
    path: str
    format_tag: int
    channels: int
    sample_rate: int
    byte_rate: int
    block_align: int
    bits_per_sample: int
    data_size: int
    frames: int
    duration: float
    data: bytes = b""
    samples: array.array | None = None

    def summary(self) -> dict:
        return {
            "path": self.path,
            "format_tag": self.format_tag,
            "channels": self.channels,
            "sample_rate": self.sample_rate,
            "byte_rate": self.byte_rate,
            "block_align": self.block_align,
            "bits_per_sample": self.bits_per_sample,
            "data_size": self.data_size,
            "frames": self.frames,
            "duration": round(self.duration, 6),
            "loaded": bool(self.data),
        }


def _parse(path: Path, *, load_data: bool) -> Wav:
    with path.open("rb") as f:
        header = f.read(12)
        if len(header) < 12 or header[:4] != b"RIFF" or header[8:12] != b"WAVE":
            raise WavFormatError(f"{path}: not a RIFF/WAVE file")
        fmt = None
        data_size = None
        data_offset = None
        while True:
            head = f.read(8)
            if len(head) < 8:
                break
            cid, csz = head[:4], struct.unpack_from("<I", head, 4)[0]
            if cid == b"fmt ":
                body = f.read(csz)
                if len(body) < 16:
                    raise WavFormatError(f"{path}: short fmt chunk")
                fmt = struct.unpack_from("<HHIIHH", body, 0)
            elif cid == b"data":
                data_offset = f.tell()
                data_size = csz
                f.seek(csz + (csz & 1), 1)
            else:
                f.seek(csz + (csz & 1), 1)
        if fmt is None:
            raise WavFormatError(f"{path}: missing fmt chunk")
        if data_offset is None:
            raise WavFormatError(f"{path}: missing data chunk")

        format_tag, channels, sample_rate, byte_rate, block_align, bits = fmt
        if format_tag != 1:
            raise WavFormatError(f"{path}: unsupported format tag {format_tag}")
        if channels not in (1, 2):
            raise WavFormatError(f"{path}: unsupported channel count {channels}")
        if bits not in (8, 16):
            raise WavFormatError(f"{path}: unsupported bit depth {bits}")
        expected_align = channels * (bits // 8)
        if block_align != expected_align:
            raise WavFormatError(
                f"{path}: block_align {block_align} != expected {expected_align}"
            )
        frames = data_size // block_align if block_align else 0
        duration = frames / sample_rate if sample_rate else 0.0

        data = b""
        samples = None
        if load_data:
            f.seek(data_offset)
            data = f.read(data_size)
            if len(data) != data_size:
                raise WavFormatError(f"{path}: truncated data chunk")
            if bits == 16:
                samples = array.array("h")
                samples.frombytes(data)
                if struct.pack("=H", 1) != struct.pack("<H", 1):
                    samples.byteswap()
            else:
                samples = array.array("B", data)

        return Wav(
            path=str(path),
            format_tag=format_tag,
            channels=channels,
            sample_rate=sample_rate,
            byte_rate=byte_rate,
            block_align=block_align,
            bits_per_sample=bits,
            data_size=data_size,
            frames=frames,
            duration=duration,
            data=data,
            samples=samples,
        )


def read_wav(path: Path, *, load_data: bool = True) -> Wav:
    return _parse(path, load_data=load_data)


def scan(root: Path) -> tuple[int, list[tuple[str, str]], dict]:
    total = 0
    bad: list[tuple[str, str]] = []
    sums = {"duration": 0.0, "frames": 0}
    for p in sorted(root.rglob("*.wav")):
        total += 1
        try:
            w = read_wav(p, load_data=False)
        except Exception as e:
            bad.append((str(p), str(e)))
            continue
        sums["duration"] += w.duration
        sums["frames"] += w.frames
    return total, bad, sums


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--dump", type=Path)
    g.add_argument("--scan", type=Path)
    args = ap.parse_args(argv)
    if args.dump:
        print(json.dumps(read_wav(args.dump, load_data=False).summary(), indent=2))
        return 0
    total, bad, sums = scan(args.scan)
    print(f"scanned {total} WAV files, {len(bad)} bad, "
          f"{sums['duration']:.3f}s total, {sums['frames']:,} frames")
    for path, why in bad[:20]:
        print(f"  FAIL {path}: {why}")
    return 0 if not bad else 1


if __name__ == "__main__":
    raise SystemExit(main())
