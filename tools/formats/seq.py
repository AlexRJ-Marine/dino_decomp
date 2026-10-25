#!/usr/bin/env python3
"""Reader for ``.seq`` animation event sequences.

Ten files in ``Data/Design/animations``: five ``*FX.seq`` (particles/bones)
and five ``*Sound.seq`` (sounds), one per animation family
(HeavyBiped, LightBiped, HighQuadruped, LightQuadruped, LowQuadruped).

Common layout::

    <optional f32 lead>     Sound files only (60, 75, 82.5, 135, 165)
    u32 sequence_count      (40/41)
    u32 event_count         (91..180)
    u32 string_count        (42..47)
    string_count x (u32 length + ASCII/NUL)   sounds, particle .ini, bones

    FX:    sequence_count x (u32 id, u32 count)
           then count x 6-byte events: u16 frame, u16 type, u16 string index
           time = frame / 30

    Sound: sequence_count x (u32 id, u32 count)
           then count x 16-byte events:
             u32 string/type, u32 flags, f32 time, f32 duration/value

The sizes match exactly (e.g. LightBipedFX: 41*8 + 96*6 = 904 bytes;
LightBipedSound: 41*8 + 91*16 = 1784 bytes).

See ``docs/formats/seq.md``.
"""
from __future__ import annotations

import argparse
import json
import struct
from dataclasses import dataclass, field
from pathlib import Path


class SeqFormatError(ValueError):
    pass


@dataclass
class SeqEvent:
    frame: int = 0
    type: int = 0
    string_index: int = 0
    flags: int = 0
    time: float = 0.0
    value: float = 0.0

    def to_dict(self) -> dict:
        return {
            "frame": self.frame,
            "type": self.type,
            "string_index": self.string_index,
            "flags": self.flags,
            "time": self.time,
            "value": self.value,
        }


@dataclass
class SeqSequence:
    id: int
    events: list[SeqEvent] = field(default_factory=list)

    def to_dict(self) -> dict:
        return {"id": self.id, "events": [e.to_dict() for e in self.events]}


@dataclass
class SeqFile:
    path: str
    kind: str  # "FX" or "Sound"
    lead: float | None
    sequence_count: int
    event_count: int
    strings: list[str]
    sequences: list[SeqSequence]

    def summary(self) -> dict:
        return {
            "path": self.path,
            "kind": self.kind,
            "lead": self.lead,
            "sequences": self.sequence_count,
            "events": self.event_count,
            "strings": len(self.strings),
            "parsed_events": sum(len(s.events) for s in self.sequences),
            "first_sequence": self.sequences[0].to_dict() if self.sequences else None,
        }


def _read_u32(data: bytes, off: int, what: str) -> tuple[int, int]:
    if off + 4 > len(data):
        raise SeqFormatError(f"{what}: u32 past EOF at {off:#x}")
    return struct.unpack_from("<I", data, off)[0], off + 4


def _read_string(data: bytes, off: int, what: str) -> tuple[str, int]:
    length, off = _read_u32(data, off, what)
    if length < 2 or off + length > len(data):
        raise SeqFormatError(f"{what}: bad string length {length} at {off:#x}")
    raw = data[off:off + length]
    if not raw.endswith(b"\x00"):
        raise SeqFormatError(f"{what}: string at {off:#x} is not NUL-terminated")
    return raw.rstrip(b"\x00").decode("ascii", errors="replace"), off + length


def read_seq(path: Path) -> SeqFile:
    data = path.read_bytes()
    kind = "Sound" if path.stem.endswith("Sound") else "FX"
    lead = None
    off = 0
    if kind == "Sound":
        if len(data) < 4:
            raise SeqFormatError(f"{path}: shorter than lead float")
        lead = struct.unpack_from("<f", data, 0)[0]
        off = 4
    sequence_count, off = _read_u32(data, off, "sequence count")
    event_count, off = _read_u32(data, off, "event count")
    string_count, off = _read_u32(data, off, "string count")
    if not 1 <= sequence_count <= 1000 or not 0 <= event_count <= 100_000:
        raise SeqFormatError(f"{path}: bad counts {sequence_count}/{event_count}")
    strings = []
    for index in range(string_count):
        text, off = _read_string(data, off, f"string {index}")
        strings.append(text)

    sequences = []
    parsed_events = 0
    for _ in range(sequence_count):
        seq_id, off = _read_u32(data, off, "sequence id")
        count, off = _read_u32(data, off, "sequence event count")
        sequence = SeqSequence(seq_id)
        for _ in range(count):
            if kind == "FX":
                if off + 6 > len(data):
                    raise SeqFormatError(f"{path}: FX event past EOF at {off:#x}")
                frame, etype, string_index = struct.unpack_from("<3H", data, off)
                off += 6
                sequence.events.append(
                    SeqEvent(frame=frame, type=etype, string_index=string_index,
                             time=frame / 30.0)
                )
            else:
                if off + 16 > len(data):
                    raise SeqFormatError(f"{path}: Sound event past EOF at {off:#x}")
                first, flags, time, value = struct.unpack_from("<IIff", data, off)
                off += 16
                sequence.events.append(
                    SeqEvent(type=first & 0xFFFF, string_index=first >> 16,
                             flags=flags, time=time, value=value)
                )
            parsed_events += 1
        sequences.append(sequence)
    if off != len(data):
        raise SeqFormatError(f"{path}: {len(data) - off} trailing bytes")
    if parsed_events != event_count:
        raise SeqFormatError(f"{path}: {parsed_events} events, expected {event_count}")
    return SeqFile(
        path=str(path),
        kind=kind,
        lead=lead,
        sequence_count=sequence_count,
        event_count=event_count,
        strings=strings,
        sequences=sequences,
    )


def scan(root: Path) -> tuple[int, list[tuple[str, str]], dict]:
    total = 0
    bad: list[tuple[str, str]] = []
    sums = {"fx": 0, "sound": 0, "events": 0}
    for path in sorted(root.rglob("*.seq")):
        total += 1
        try:
            seq = read_seq(path)
        except Exception as e:
            bad.append((str(path), str(e)))
            continue
        sums["fx" if seq.kind == "FX" else "sound"] += 1
        sums["events"] += seq.event_count
    return total, bad, sums


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--dump", type=Path)
    ap.add_argument("--scan", type=Path)
    args = ap.parse_args(argv)
    if args.dump:
        print(json.dumps(read_seq(args.dump).summary(), indent=2))
        return 0
    if args.scan:
        total, bad, sums = scan(args.scan)
        print(f"scanned {total} sequences, {len(bad)} bad, "
              f"{sums['fx']} FX, {sums['sound']} Sound, {sums['events']} events")
        for path, why in bad[:20]:
            print(f"  FAIL {path}: {why}")
        return 0 if not bad else 1
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
