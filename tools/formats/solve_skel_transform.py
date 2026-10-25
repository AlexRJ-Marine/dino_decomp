#!/usr/bin/env python3
"""Try candidate interpretations of the 19-float .skel record.

Score: transform each bone-local .skin block and measure how close its vertices
land to the full-mesh vertex cloud. The correct layout should produce many
near-exact matches.
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from mesh import read_mesh  # noqa: E402
from skel import read_skel  # noqa: E402
from skin import read_skin  # noqa: E402


def transform(points: np.ndarray, kind: str, v: np.ndarray) -> np.ndarray:
    p = np.asarray(points, dtype=np.float64)
    if kind == "4x4_row_t16":
        M = v[0:16].reshape(4, 4).copy()
        M[3, :3] = v[16:19]
        return (p @ M[:3, :3].T) + M[3, :3]
    if kind == "4x4_col_t16":
        M = v[0:16].reshape(4, 4).copy()
        M[:3, 3] = v[16:19]
        return (p @ M[:3, :3]) + M[:3, 3]
    if kind == "4x4_row_t13":
        M = v[0:16].reshape(4, 4).copy()
        return (p @ M[:3, :3].T) + M[:3, 3]
    if kind == "4x4_col_t13":
        M = v[0:16].reshape(4, 4).copy()
        return (p @ M[:3, :3]) + M[:3, 3]
    if kind == "3x3_1_t16":
        R = v[1:10].reshape(3, 3)
        return (p @ R.T) + v[16:19]
    if kind == "3x3_1_t16_col":
        R = v[1:10].reshape(3, 3)
        return (p @ R) + v[16:19]
    if kind == "3x3_1_t13":
        R = v[1:10].reshape(3, 3)
        return (p @ R.T) + v[13:16]
    if kind == "3x3_1_t13_col":
        R = v[1:10].reshape(3, 3)
        return (p @ R) + v[13:16]
    if kind == "3x3_0_t16":
        R = v[0:9].reshape(3, 3)
        return (p @ R.T) + v[16:19]
    if kind == "3x3_0_t16_col":
        R = v[0:9].reshape(3, 3)
        return (p @ R) + v[16:19]
    if kind == "3x3_0_t9":
        R = v[0:9].reshape(3, 3)
        return (p @ R.T) + v[9:12]
    if kind == "3x4_0_t12":
        M = np.zeros((4, 4))
        M[:3, :] = v[0:12].reshape(3, 4)
        M[3, 3] = 1
        return (p @ M[:3, :3].T) + M[:3, 3]
    if kind == "3x4_1_t13":
        M = np.zeros((4, 4))
        M[:3, :] = v[1:13].reshape(3, 4)
        M[3, 3] = 1
        return (p @ M[:3, :3].T) + M[:3, 3]
    if kind == "translation_only":
        return p + v[16:19]
    raise ValueError(kind)


KINDS = [
    "4x4_row_t16", "4x4_col_t16", "4x4_row_t13", "4x4_col_t13",
    "3x3_1_t16", "3x3_1_t16_col", "3x3_1_t13", "3x3_1_t13_col",
    "3x3_0_t16", "3x3_0_t16_col", "3x3_0_t9",
    "3x4_0_t12", "3x4_1_t13", "translation_only",
]


def nearest_dist(points: np.ndarray, cloud: np.ndarray) -> np.ndarray:
    # small clouds, brute force is fine
    d = ((points[:, None, :] - cloud[None, :, :]) ** 2).sum(axis=2)
    return np.sqrt(d.min(axis=1))


def main():
    root = Path(sys.argv[1] if len(sys.argv) > 1 else "game/%MAINDIR%")
    skin_path = Path(sys.argv[2]) if len(sys.argv) > 2 else root / "Data/Graphics/Actors/Dinos/allo/allo_lod1.skin"
    skin = read_skin(skin_path)
    skel = read_skel(skin_path.with_name(skin_path.name.split("_lod")[0] + ".skel"))
    assert skin.full_mesh is not None
    cloud = np.asarray(skin.full_mesh.vertices, dtype=np.float64)
    print(f"mesh {skin.full_mesh.name}: {len(cloud)} verts")
    print("mesh bbox", cloud.min(axis=0).round(2), cloud.max(axis=0).round(2))

    by_name = {b.name: b for b in skel.bones}
    print(f"blocks {len(skin.blocks)}")
    results = {}
    for kind in KINDS:
        frac = []
        med = []
        for block in skin.blocks:
            bone = by_name.get(block.name)
            if bone is None or not block.vertices:
                continue
            p = transform(np.asarray(block.vertices), kind, np.asarray(bone.values))
            d = nearest_dist(p, cloud)
            frac.append((d < 0.5).mean())
            med.append(np.median(d))
        results[kind] = (float(np.mean(frac)), float(np.mean(med)), float(np.min(med)))
    print("\nkind                     frac<0.5  mean_median  min_median")
    for kind, (f, m, mn) in sorted(results.items(), key=lambda kv: -kv[1][0]):
        print(f"{kind:24} {f:8.3f} {m:12.3f} {mn:11.3f}")


if __name__ == "__main__":
    main()
