#!/usr/bin/env python3
"""Reader for Dino Island ``.bai`` action definitions.

64 files live in ``Data/Design/attractions/actions`` and are loaded in
``ActionList.ini`` order by ``FUN_0053e060`` into a 0xac-byte runtime record.
This module decodes the fixed header shared by every file; the variable tail
(the animation/event arrays) is preserved as raw bytes until it is decoded.

Layout (little-endian, packed -- several fields are not 4-byte aligned)::

    0x00  char name[20]      # truncated to 19 chars + NUL for long names
    0x14  u32  field_14
    0x18  f32  field_18
    0x1c  u32  field_1c
    0x20  u8   flag_20
    0x21  u32  special_action  # 0 normal, 1..6 special groups
    0x25  u8   eat_flag
    0x26  u32  field_26        # 50 in most files, 60 in ReachWaypoint
    0x2a  f32  field_2a        # 30.0 in 63/64 files, 10.0 in ReachWaypoint
    0x2e  f32  field_2e        # 30.0 in all 64 files
    0x32  u32  field_32
    0x36  u32  params[8]
    0x56  ...  variable tail

The packed fields were recovered from the loader's read sequence: it reads a
byte at ``0x20``, a dword at ``0x21``, a byte at ``0x25`` and a dword at
``0x26`` before the two floats.

See ``docs/formats/bai.md``.
"""
from __future__ import annotations

import argparse
import json
import struct
from dataclasses import dataclass
from pathlib import Path

NAME_SIZE = 20
HEADER_SIZE = 0x56
PARAM_COUNT = 8

#: ``special_action`` values; matches the "special action" groups in
#: ActionList.ini (Exploration, RunAway, Sleep, attacks, Victory).
ACTION_GROUPS = {
    0: "normal",
    1: "exploration",
    2: "runaway",
    3: "sleep",
    4: "dino attack",
    5: "obstacle attack",
    6: "victory",
}


class BaiFormatError(ValueError):
    pass


@dataclass
class Bai:
    path: str
    name: str
    field_14: int
    field_18: float
    field_1c: int
    flag_20: int
    special_action: int
    eat_flag: int
    field_26: int
    field_2a: float
    field_2e: float
    field_32: int
    params: tuple[int, ...]
    tail: bytes

    @property
    def group(self) -> str:
        return ACTION_GROUPS.get(self.special_action, "unknown")

    def summary(self) -> dict:
        return {
            "path": self.path,
            "name": self.name,
            "group": self.group,
            "field_14": self.field_14,
            "field_18": self.field_18,
            "field_1c": self.field_1c,
            "flag_20": self.flag_20,
            "special_action": self.special_action,
            "eat_flag": self.eat_flag,
            "field_26": self.field_26,
            "field_2a": self.field_2a,
            "field_2e": self.field_2e,
            "field_32": self.field_32,
            "params": list(self.params),
            "tail_bytes": len(self.tail),
        }


def read_bai(path: Path) -> Bai:
    data = path.read_bytes()
    if len(data) < HEADER_SIZE:
        raise BaiFormatError(f"{path.name}: {len(data)} bytes < {HEADER_SIZE}")
    raw_name = data[:NAME_SIZE].split(b"\x00", 1)[0]
    try:
        name = raw_name.decode("ascii")
    except UnicodeDecodeError as exc:
        raise BaiFormatError(f"{path.name}: non-ASCII action name") from exc
    if not name:
        raise BaiFormatError(f"{path.name}: empty action name")
    field_14, field_18, field_1c = struct.unpack_from("<IfI", data, 0x14)
    flag_20 = data[0x20]
    (special_action,) = struct.unpack_from("<I", data, 0x21)
    eat_flag = data[0x25]
    (field_26,) = struct.unpack_from("<I", data, 0x26)
    field_2a, field_2e = struct.unpack_from("<ff", data, 0x2A)
    (field_32,) = struct.unpack_from("<I", data, 0x32)
    params = struct.unpack_from(f"<{PARAM_COUNT}I", data, 0x36)
    return Bai(
        path=str(path),
        name=name,
        field_14=field_14,
        field_18=field_18,
        field_1c=field_1c,
        flag_20=flag_20,
        special_action=special_action,
        eat_flag=eat_flag,
        field_26=field_26,
        field_2a=field_2a,
        field_2e=field_2e,
        field_32=field_32,
        params=params,
        tail=data[HEADER_SIZE:],
    )


def read_action_list(path: Path) -> list[str]:
    names: list[str] = []
    for line in path.read_text(encoding="latin-1").splitlines():
        line = line.strip()
        if not line or line.startswith("//"):
            continue
        if not names and line.isdigit():
            continue
        names.append(line)
    return names


def scan(root: Path) -> tuple[int, list[Bai], list[tuple[str, str]]]:
    """Return (file_count, parsed, errors) for an actions directory."""
    parsed: list[Bai] = []
    errors: list[tuple[str, str]] = []
    files = sorted(root.glob("*.bai"))
    for path in files:
        try:
            parsed.append(read_bai(path))
        except (BaiFormatError, OSError) as exc:
            errors.append((path.name, str(exc)))
    return len(files), parsed, errors


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path", type=Path, help="one .bai or the actions directory")
    parser.add_argument("--json", action="store_true", help="machine-readable output")
    args = parser.parse_args(argv)

    if args.path.is_dir():
        count, parsed, errors = scan(args.path)
        if args.json:
            print(
                json.dumps(
                    {
                        "files": count,
                        "parsed": len(parsed),
                        "errors": errors,
                        "actions": [b.summary() for b in parsed],
                    },
                    indent=2,
                )
            )
            return 0 if not errors else 1
        print(f"{len(parsed)}/{count} .bai files parsed, {len(errors)} errors")
        for bai in parsed:
            print(
                f"  {bai.name:<20} group={bai.group:<14} "
                f"f14={bai.field_14:<4} f18={bai.field_18:<6g} "
                f"f1c={bai.field_1c} flag={bai.flag_20} "
                f"special={bai.special_action} eat={bai.eat_flag} "
                f"f26={bai.field_26} f32={bai.field_32:<6} "
                f"tail={len(bai.tail):<3} params={bai.params}"
            )
        for name, error in errors:
            print(f"  ERROR {name}: {error}")
        return 0 if not errors else 1

    bai = read_bai(args.path)
    print(json.dumps(bai.summary(), indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
