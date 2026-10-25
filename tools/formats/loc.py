#!/usr/bin/env python3
"""Reader for Dino Island ``.loc`` localisation files.

Retail files are UTF-16LE with a BOM and contain records of the form::

    FILE_VERSION="2"
    FIELD_ID="&NoText"
    TEXT="NO TEXT"
    LAST_MODIFIED="16/7/2001 12:27"
    FIELD_ID="&FreeGame"
    TEXT="FREE GAME"
    LAST_MODIFIED="..."

The first record carries ``FILE_VERSION``; every record has ``FIELD_ID``,
``TEXT`` and ``LAST_MODIFIED``. Literal ``\\n`` escapes are kept in the text.
"""
from __future__ import annotations

import argparse
import json
import re
from dataclasses import dataclass, field
from pathlib import Path

LINE_RE = re.compile(r'^([A-Za-z0-9_]+)="(.*)"$')


class LocFormatError(ValueError):
    pass


@dataclass
class LocRecord:
    field_id: str
    text: str
    last_modified: str

    def to_dict(self) -> dict:
        return {
            "field_id": self.field_id,
            "text": self.text,
            "last_modified": self.last_modified,
        }


@dataclass
class Loc:
    path: str
    file_version: str
    records: list[LocRecord] = field(default_factory=list)

    def by_id(self, field_id: str) -> LocRecord:
        for rec in self.records:
            if rec.field_id == field_id:
                return rec
        raise KeyError(field_id)

    def summary(self) -> dict:
        return {
            "path": self.path,
            "file_version": self.file_version,
            "records": len(self.records),
            "first": self.records[0].to_dict() if self.records else None,
            "last": self.records[-1].to_dict() if self.records else None,
        }


def read_loc(path: Path) -> Loc:
    data = path.read_bytes()
    try:
        if data[:2] == b"\xff\xfe":
            text = data.decode("utf-16")
        elif data[:2] == b"\xfe\xff":
            text = data.decode("utf-16")
        else:
            text = data.decode("utf-16-le")
    except UnicodeDecodeError as e:
        raise LocFormatError(f"{path}: invalid UTF-16 data: {e}") from e

    file_version = ""
    records: list[LocRecord] = []
    current: dict[str, str] | None = None

    def finish() -> None:
        nonlocal current
        if current is None:
            return
        missing = [k for k in ("FIELD_ID", "TEXT", "LAST_MODIFIED") if k not in current]
        if missing:
            raise LocFormatError(f"{path}: record missing {missing}")
        records.append(
            LocRecord(
                field_id=current["FIELD_ID"],
                text=current["TEXT"],
                last_modified=current["LAST_MODIFIED"],
            )
        )
        current = None

    for lineno, line in enumerate(text.splitlines(), 1):
        if not line.strip():
            continue
        m = LINE_RE.match(line)
        if not m:
            raise LocFormatError(f"{path}:{lineno}: cannot parse {line[:80]!r}")
        key, value = m.group(1), m.group(2)
        if key == "FILE_VERSION":
            file_version = value
        elif key == "FIELD_ID":
            finish()
            current = {"FIELD_ID": value}
        elif key in ("TEXT", "LAST_MODIFIED"):
            if current is None:
                raise LocFormatError(f"{path}:{lineno}: {key} before FIELD_ID")
            current[key] = value
        else:
            raise LocFormatError(f"{path}:{lineno}: unknown key {key}")
    finish()
    return Loc(path=str(path), file_version=file_version, records=records)


def scan(root: Path) -> tuple[int, list[tuple[str, str]], dict]:
    total = 0
    bad: list[tuple[str, str]] = []
    records = 0
    for p in sorted(root.rglob("*.loc")):
        total += 1
        try:
            loc = read_loc(p)
        except Exception as e:
            bad.append((str(p), str(e)))
            continue
        records += len(loc.records)
    return total, bad, {"records": records}


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--dump", type=Path)
    g.add_argument("--scan", type=Path)
    args = ap.parse_args(argv)
    if args.dump:
        print(json.dumps(read_loc(args.dump).summary(), indent=2))
        return 0
    total, bad, sums = scan(args.scan)
    print(f"scanned {total} .loc files, {len(bad)} bad, {sums['records']:,} records")
    for path, why in bad[:20]:
        print(f"  FAIL {path}: {why}")
    return 0 if not bad else 1


if __name__ == "__main__":
    raise SystemExit(main())
