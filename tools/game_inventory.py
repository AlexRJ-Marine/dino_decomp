#!/usr/bin/env python3
"""Inventory extracted game tree: extension stats + magic bytes per extension."""
import collections
import sys
from pathlib import Path

root = Path(sys.argv[1] if len(sys.argv) > 1 else "game/%MAINDIR%")
files = [p for p in root.rglob("*") if p.is_file()]
by_ext = collections.defaultdict(list)
for p in files:
    by_ext[p.suffix.lower()].append(p)

print(f"{len(files)} files, {sum(p.stat().st_size for p in files)/1048576:.1f} MiB\n")
print(f"{'ext':10} {'count':>6} {'size MiB':>10}  sample / magic")
for ext, ps in sorted(by_ext.items(), key=lambda kv: -sum(p.stat().st_size for p in kv[1])):
    size = sum(p.stat().st_size for p in ps) / 1048576
    sample = max(ps, key=lambda p: p.stat().st_size)
    head = sample.read_bytes()[:16]
    print(f"{ext or '(none)':10} {len(ps):>6} {size:>10.2f}  {sample.name[:40]:40} {head.hex(' ')}")

print("\nDirectories:")
for d in sorted({str(p.parent.relative_to(root)) for p in files}):
    print("  ", d)
