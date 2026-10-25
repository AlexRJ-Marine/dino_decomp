#!/usr/bin/env python3
"""Extract compiler, version, imports, RTTI, and format/engine clues from DinoIsland.exe."""
import collections
import re
import sys
from pathlib import Path

import pefile

p = Path(sys.argv[1])
data = p.read_bytes()
pe = pefile.PE(str(p), fast_load=False)

print("=== RICH HEADER ===")
try:
    rich = pe.parse_rich_header()
    for k, v in (rich or {}).items():
        if k != "values":
            print(f"  {k}: {v}")
    for prod, build, count in (rich or {}).get("values", []):
        print(f"  prod={prod:#06x} build={build} count={count}")
except Exception as e:
    print("  (none)", e)

print("\n=== VERSION INFO ===")
try:
    for fi in pe.FileInfo:
        for entry in fi:
            if hasattr(entry, "StringTable"):
                for st in entry.StringTable:
                    for k, v in st.entries.items():
                        print(f"  {k.decode(errors='replace')}: {v.decode(errors='replace')}")
except Exception as e:
    print("  (none)", e)

print("\n=== DEBUG DIRECTORY ===")
try:
    for d in pe.DIRECTORY_ENTRY_DEBUG:
        print(f"  type={d.struct.Type} size={d.struct.SizeOfData} addr={d.struct.AddressOfRawData}")
except Exception:
    print("  (none)")

print("\n=== ALL IMPORTS ===")
if hasattr(pe, "DIRECTORY_ENTRY_IMPORT"):
    for e in pe.DIRECTORY_ENTRY_IMPORT:
        names = [i.name.decode(errors="replace") for i in e.imports if i.name]
        print(f"  {e.dll.decode(errors='replace')} ({len(names)}):")
        print("    " + ", ".join(names))

def strings(data, minlen=5):
    for m in re.finditer(rb"[\x20-\x7e]{%d,}" % minlen, data):
        yield m.group().decode("latin-1")

print("\n=== KEYWORD HITS ===")
keywords = [
    "Monte Cristo", "Strategy First", "Dino Island", "DinoIsland",
    "RenderWare", "NetImmerse", "LithTech", "Quake", "Unreal", "Genesis3D",
    "Miles", "Bink", "Smacker", "DSOUND", "dsound.dll", "quartz", "DirectShow",
    "mciSendString", "waveOut", "DirectSound", "OpenAL", "FMOD",
    "d3d8.dll", "ddraw.dll", "dinput8.dll", "dinput.dll",
    "SafeDisc", "SecuROM", "StarForce", "SafeDisc",
    ".mesh", ".skin", ".anim", ".skel", ".tga", ".raw", ".fnt", ".loc",
    ".bai", ".bfi", ".bni", ".bvi", ".bbi", ".bsi", ".bwi", ".blf", ".bgd",
    ".seq", ".lnd", ".shw", ".gen", ".wld", ".imp", ".dic", "Pathmap",
    "Data\\", "Data/", "Config", "assert", "Assert", "ERROR", "Error",
]
lower = data.lower()
for kw in keywords:
    n = lower.count(kw.lower().encode())
    if n:
        print(f"  {kw:24} {n}")

print("\n=== RTTI CLASS NAMES (first 200) ===")
rtti = sorted(set(re.findall(rb"\.\?A[VU][^\x00]{2,120}", data)))
print(f"  total unique: {len(rtti)}")
for r in rtti[:200]:
    print("   ", r.decode("latin-1", errors="replace"))

print("\n=== DLL/LOADLIB STRINGS ===")
for s in set(strings(data)):
    if re.search(r"\.(dll|DLL)$", s) or "LoadLibrary" in s or "GetProcAddress" in s:
        print("   ", s)

print("\n=== FILE EXTENSION STRINGS ===")
exts = collections.Counter()
for s in strings(data):
    for m in re.findall(r"\.[A-Za-z0-9]{2,5}\b", s):
        exts[m.lower()] += 1
for e, n in exts.most_common(60):
    print(f"  {e:8} {n}")
