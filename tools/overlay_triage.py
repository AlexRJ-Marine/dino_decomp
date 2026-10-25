#!/usr/bin/env python3
"""Dump overlay header + strings from a PE, to identify installer/archive format."""
import math
import re
import sys
from pathlib import Path

import pefile

p = Path(sys.argv[1])
data = p.read_bytes()
pe = pefile.PE(str(p), fast_load=True)
off = pe.get_overlay_data_start_offset() or 0
ov = data[off:]
print(f"overlay offset={off:,} size={len(ov):,}")
print("first 512 bytes (hex):")
print(" ".join(f"{b:02x}" for b in ov[:512]))
print("first 512 bytes (ascii):")
print("".join(chr(b) if 32 <= b < 127 else "." for b in ov[:512]))

print("\nstrings in stub (len>=5):")
stub = data[:off]
seen = set()
for m in re.finditer(rb"[\x20-\x7e]{5,}", stub):
    s = m.group().decode()
    if s not in seen:
        seen.add(s)
        print(f"  {m.start():#08x}  {s}")

def entropy(b):
    if not b:
        return 0.0
    counts = [0] * 256
    for x in b:
        counts[x] += 1
    return -sum((c / len(b)) * math.log2(c / len(b)) for c in counts if c)

print(f"\noverlay entropy: {entropy(ov[:1 << 20]):.3f} bits/byte (first 1 MiB)")
print("overlay first 64 MiB entropy:", f"{entropy(ov[:64 << 20]):.3f}")

# look for interesting ASCII in the overlay head
print("\nASCII runs >= 8 in first 2 MiB of overlay:")
head = ov[:2 << 20]
for m in re.finditer(rb"[\x20-\x7e]{8,}", head):
    print(f"  {m.start():#08x}  {m.group().decode()[:120]}")
