#!/usr/bin/env python3
"""Hexdump representative custom-format files and compare mesh/skin/anim headers."""
import sys
from pathlib import Path

root = Path(sys.argv[1])
samples = [
    "Data/Graphics/Actors/Dinos/allo/allo.mesh",
    "Data/Graphics/Actors/Dinos/allo/allo.skin",
    "Data/Graphics/Actors/Dinos/allo/allo.skel",
    "Data/Graphics/Actors/Dinos/allo/allo.anim",
    "Data/Design/attractions/actions/Eat.bai",
    "Data/Design/attractions/features/CageHigh.bfi",
    "Data/Design/animal/PigAnimal.bni",
    "Data/Design/landscape/isl01/isl01.lnd",
    "Data/Config/GraphicOption.ini",
    "Data/Fonte/font19.fnt",
    "Data/ParkEng.loc",
]

def hexdump(b, n=96):
    for i in range(0, min(len(b), n), 16):
        chunk = b[i:i+16]
        hx = " ".join(f"{x:02x}" for x in chunk)
        asc = "".join(chr(x) if 32 <= x < 127 else "." for x in chunk)
        print(f"  {i:04x}  {hx:<47}  {asc}")

for rel in samples:
    p = root / rel
    if not p.exists():
        # try find by name
        hits = list(root.rglob(p.name))
        if hits:
            p = hits[0]
        else:
            print(f"\n### {rel}: NOT FOUND")
            continue
    b = p.read_bytes()[:160]
    print(f"\n### {p.relative_to(root)}  ({p.stat().st_size:,} bytes)")
    hexdump(b)

print("\n=== .mesh headers compared (first 48 bytes) ===")
meshes = sorted(root.rglob("*.mesh"))[:12]
for p in meshes:
    print(f"{p.name:32} {p.read_bytes()[:48].hex(' ')}")
