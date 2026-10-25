#!/usr/bin/env python3
"""Recon: compare each .skin inverse-bind pose with the .skel rest pose.

For every LOD, compute W_bind = inv(inverse_bind) in raw centimetres and
compare it with the skeleton world matrix (root 0.01 scale removed). Prints
per-bone translation/rotation deltas and checks whether the mismatch is a
single global transform.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from skel import read_skel  # noqa: E402
from skin import read_skin  # noqa: E402


def mat(values) -> np.ndarray:
    return np.array(values, dtype=float).reshape(4, 4)


def compare(skel_path: Path, skin_path: Path, verbose: bool = False) -> dict:
    skel = read_skel(skel_path)
    world = [mat(m) * 100.0 for m in skel.world_matrices()]
    skin = read_skin(skin_path)
    if skin.skin_data is None:
        raise ValueError(f"{skin_path}: no skin data")
    deltas = []
    g_left = []
    g_right = []
    for record in skin.skin_data.bones:
        index = skel.by_name(record.name).index
        bind = np.linalg.inv(mat(record.inverse_bind))
        rest = world[index]
        dt = float(np.linalg.norm(bind[3, :3] - rest[3, :3]))
        dr = float(np.linalg.norm(bind[:3, :3] - rest[:3, :3]))
        deltas.append((dt, dr, record.name, index))
        g_left.append(np.linalg.inv(rest) @ bind)
        g_right.append(bind @ np.linalg.inv(rest))
    worst = sorted(deltas, reverse=True)[:8]
    gl = np.array(g_left)
    gr = np.array(g_right)
    gl_spread = float(np.max(np.linalg.norm(gl - gl[0], axis=(1, 2))))
    gr_spread = float(np.max(np.linalg.norm(gr - gr[0], axis=(1, 2))))
    return {
        "skin": skin_path.name,
        "bones": len(deltas),
        "max_translation_cm": max(d[0] for d in deltas),
        "mean_translation_cm": sum(d[0] for d in deltas) / len(deltas),
        "max_rotation_frob": max(d[1] for d in deltas),
        "global_left_spread": gl_spread,
        "global_right_spread": gr_spread,
        "worst": [(round(dt, 3), round(dr, 4), name, idx) for dt, dr, name, idx in worst],
        "g_left": gl[0],
        "g_right": gr[0],
        "verbose": deltas if verbose else None,
    }


def main(argv=None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--skel", type=Path, required=True)
    ap.add_argument("--skins", nargs="+", type=Path, required=True)
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args(argv)
    for skin in args.skins:
        result = compare(args.skel, skin, args.verbose)
        print(f"== {result['skin']}")
        print(f"   bones={result['bones']} max_trans={result['max_translation_cm']:.3f} cm "
              f"mean_trans={result['mean_translation_cm']:.3f} cm "
              f"max_rot={result['max_rotation_frob']:.4f}")
        print(f"   left-spread={result['global_left_spread']:.6f} "
              f"right-spread={result['global_right_spread']:.6f}")
        print("   worst:", result["worst"])
        if result["global_left_spread"] < 1e-4:
            print("   W_bind = W_skel @ G, G=")
            print(np.array2string(result["g_left"], precision=4, suppress_small=True))
        if result["global_right_spread"] < 1e-4:
            print("   W_bind = G @ W_skel, G=")
            print(np.array2string(result["g_right"], precision=4, suppress_small=True))
        if args.verbose:
            for dt, dr, name, idx in sorted(result["verbose"], reverse=True)[:20]:
                print(f"     {idx:2} {name:24} dt={dt:8.3f} dr={dr:8.4f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
