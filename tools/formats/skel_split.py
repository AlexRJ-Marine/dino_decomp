#!/usr/bin/env python3
"""Parse the split .skel matrix/translation arrays and assemble .skin blocks."""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from skin import read_skin  # noqa: E402

MAGIC = b"\x01\x03\x00\x00\x00"


def read_skel_split(path: Path):
    d = path.read_bytes()
    assert d[:5] == MAGIC
    count = int.from_bytes(d[13:17], "little")
    mats_off = 0x19
    trans_off = mats_off + count * 64
    mats = np.frombuffer(d, dtype="<f4", count=count * 16, offset=mats_off).reshape(count, 4, 4)
    trans = np.frombuffer(d, dtype="<f4", count=count * 3, offset=trans_off).reshape(count, 3)

    off = trans_off + count * 12
    names = []
    for _ in range(count):
        ln = int.from_bytes(d[off : off + 4], "little")
        off += 4
        names.append(d[off : off + ln - 1].decode("latin-1"))
        off += ln
    return names, mats, trans


def orthonormality(m: np.ndarray) -> float:
    r = m[:3, :3]
    return float(np.abs(r @ r.T - np.eye(3)).max())


def main():
    root = Path(sys.argv[1] if len(sys.argv) > 1 else "game/%MAINDIR%")
    skel_path = Path(sys.argv[2]) if len(sys.argv) > 2 else root / "Data/Graphics/Actors/Dinos/allo/allo.skel"
    names, mats, trans = read_skel_split(skel_path)
    print(f"{skel_path.name}: {len(names)} bones, mats {mats.shape}, trans {trans.shape}")
    for name in ["Scene_Root", "Bip01", "Bip01_Pelvis", "Bip01_Spine", "Bip01_Head", "Bip01_L_Thigh", "Bip01_Tail"]:
        i = names.index(name)
        print(f"\n{name}  idx={i}  ortho_err={orthonormality(mats[i]):.4f}")
        print("  matrix:")
        for row in mats[i]:
            print("   ", [round(float(x), 4) for x in row])
        print("  translation:", [round(float(x), 4) for x in trans[i]])
        print("  col-major ortho_err:", f"{orthonormality(mats[i].T):.4f}")


if __name__ == "__main__":
    main()
