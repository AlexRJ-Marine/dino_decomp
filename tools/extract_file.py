#!/usr/bin/env python3
"""Extract individual files from the ISO (read-only source)."""
import os
import sys

import pycdlib

iso_path = sys.argv[1]
want = sys.argv[2]
out = sys.argv[3]

iso = pycdlib.PyCdlib()
iso.open(iso_path)
# ISO9660 wants an absolute path and ;1 on files
if not want.startswith("/"):
    want = "/" + want
candidates = [want, want + ";1"]
if want.endswith(".EXE") or want.endswith(".TXT"):
    pass
for c in candidates:
    try:
        iso.get_file_from_iso(local_path=out, iso_path=c)
        print(f"extracted {c} -> {out}")
        break
    except Exception as e:
        last = e
else:
    raise SystemExit(f"could not extract {want}: {last}")
iso.close()
