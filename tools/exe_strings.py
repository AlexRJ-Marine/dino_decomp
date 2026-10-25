#!/usr/bin/env python3
"""Pull source-file paths, asset paths, and error strings from the exe."""
import re
import sys
from pathlib import Path

data = Path(sys.argv[1]).read_bytes()

def strings(minlen=4):
    for m in re.finditer(rb"[\x20-\x7e]{%d,}" % minlen, data):
        yield m.start(), m.group().decode("latin-1")

items = list(strings())

print("=== .cpp / source paths ===")
seen = set()
for off, s in items:
    if ".cpp" in s.lower() or ".h" in s.lower() and ("\\" in s or "/" in s):
        s2 = s.strip()
        if s2 not in seen:
            seen.add(s2)
            print(f"  {off:#08x}  {s2}")

print("\n=== Data\\ / asset path strings ===")
seen = set()
for off, s in items:
    if ("Data\\" in s or "Data/" in s or "%s\\" in s) and len(s) < 200:
        if s not in seen:
            seen.add(s)
            print(f"  {off:#08x}  {s}")

print("\n=== interesting engine/audio strings ===")
pat = re.compile(r"(FMOD|DirectSound|DSOUND|dsound|waveOut|MCI|quartz|DirectShow|Bink|Miles|RenderWare|engine|Engine|Monte|Cristo|Dino)", re.I)
seen = set()
for off, s in items:
    if pat.search(s) and len(s) < 160:
        if s not in seen:
            seen.add(s)
            print(f"  {off:#08x}  {s}")
