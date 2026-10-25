#!/usr/bin/env python3
"""List every file in the ISO, with sizes, and dump a machine-readable manifest."""
import json
import os
import sys

import pycdlib

ISO = sys.argv[1] if len(sys.argv) > 1 else "Dino Island.iso"
OUT = sys.argv[2] if len(sys.argv) > 2 else "notes/iso-manifest.json"

entries = []
iso = pycdlib.PyCdlib()
iso.open(ISO)

def walk(path="/"):
    for child in iso.list_children(iso_path=path):
        if child is None:
            continue
        name = child.file_identifier().decode("latin-1")
        if name in (".", ".."):
            continue
        full = path + name + ("/" if child.is_dir() else "")
        if child.is_dir():
            walk(full)
        else:
            entries.append({
                "path": full.lstrip("/"),
                "size": child.get_data_length(),
                "lba": child.extent_location(),
            })

walk()
iso.close()

os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w", encoding="utf-8") as f:
    json.dump(entries, f, indent=1)

total = sum(e["size"] for e in entries)
print(f"{len(entries)} files, {total:,} bytes ({total/1024/1024:.1f} MiB)")
print()
for e in sorted(entries, key=lambda x: -x["size"]):
    print(f'{e["size"]:>12,}  {e["path"]}')
