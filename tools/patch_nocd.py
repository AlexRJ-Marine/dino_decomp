#!/usr/bin/env python3
"""Apply/revert the DinoIsland.exe no-CD startup patch.

Target build:
  original SHA-256 7ba16f38db4fe9461e700e6005e29a37c0d7a6d9441a233880a7c4405bbd717c
  patched  SHA-256 6ad30c0a801b3b86d4dfb1bab0d87cb411f76dfd1b9bb8c5ae9d8776f0029422

The patch changes the "CD found" branch at file offset 0x1BFC from a
conditional jump to an unconditional jump.
"""
from __future__ import annotations

import argparse
import hashlib
import struct
import sys
from pathlib import Path

OFFSET = 0x1BFC
ORIGINAL = bytes.fromhex("0f854a010000")  # jne 0x401d4c
PATCHED = bytes.fromhex("e94b01000090")   # jmp 0x401d4c ; nop
SHA256_ORIGINAL = "7ba16f38db4fe9461e700e6005e29a37c0d7a6d9441a233880a7c4405bbd717c"
SHA256_PATCHED = "6ad30c0a801b3b86d4dfb1bab0d87cb411f76dfd1b9bb8c5ae9d8776f0029422"


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def state(data: bytes) -> str:
    raw = data[OFFSET : OFFSET + len(ORIGINAL)]
    if raw == ORIGINAL:
        return "original"
    if raw == PATCHED:
        return "patched"
    return f"unknown ({raw.hex(' ')})"


def apply(target: Path, force: bool = False) -> int:
    data = bytearray(target.read_bytes())
    current = state(data)
    if current == "patched":
        print(f"{target}: already patched")
        return 0
    if current != "original" and not force:
        print(f"{target}: refusing to patch: {current}")
        print(f"expected original SHA-256 {SHA256_ORIGINAL}")
        return 1
    actual = sha256(bytes(data))
    if actual != SHA256_ORIGINAL and not force:
        print(f"{target}: SHA-256 mismatch: {actual}")
        print("use --force only if you know this is a compatible build")
        return 1

    backup = target.with_name(target.name + ".orig")
    if not backup.exists():
        backup.write_bytes(bytes(data))
        print(f"backup: {backup}")
    data[OFFSET : OFFSET + len(PATCHED)] = PATCHED
    target.write_bytes(bytes(data))
    print(f"patched: {target} @ 0x{OFFSET:x} -> {PATCHED.hex(' ')}")
    print(f"sha256: {sha256(bytes(data))}")
    return 0


def revert(target: Path) -> int:
    data = bytearray(target.read_bytes())
    current = state(data)
    if current == "original":
        print(f"{target}: already original")
        return 0
    if current != "patched":
        print(f"{target}: refusing to revert: {current}")
        return 1
    data[OFFSET : OFFSET + len(ORIGINAL)] = ORIGINAL
    target.write_bytes(bytes(data))
    print(f"reverted: {target} @ 0x{OFFSET:x} -> {ORIGINAL.hex(' ')}")
    print(f"sha256: {sha256(bytes(data))}")
    return 0


def verify(target: Path) -> int:
    data = target.read_bytes()
    current = state(data)
    digest = sha256(data)
    print(f"{target}: {current}")
    print(f"sha256: {digest}")
    if current == "original":
        print(f"matches original: {digest == SHA256_ORIGINAL}")
    elif current == "patched":
        print(f"matches patched: {digest == SHA256_PATCHED}")
    return 0 if current in ("original", "patched") else 1


def make_ips(out: Path) -> int:
    # IPS: "PATCH", 3-byte offset, 2-byte size, data, "EOF"
    payload = b"PATCH" + struct.pack(">I", OFFSET)[1:] + struct.pack(">H", len(PATCHED)) + PATCHED + b"EOF"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(payload)
    print(f"wrote {out} ({len(payload)} bytes)")
    return 0


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--apply", type=Path, metavar="EXE")
    g.add_argument("--revert", type=Path, metavar="EXE")
    g.add_argument("--verify", type=Path, metavar="EXE")
    g.add_argument("--make-ips", type=Path, metavar="OUT.ips")
    ap.add_argument("--force", action="store_true")
    args = ap.parse_args(argv)

    if args.apply:
        return apply(args.apply, args.force)
    if args.revert:
        return revert(args.revert)
    if args.verify:
        return verify(args.verify)
    return make_ips(args.make_ips)


if __name__ == "__main__":
    raise SystemExit(main())
