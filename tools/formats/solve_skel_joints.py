#!/usr/bin/env python3
"""Score candidate .skel transform layouts by bone-chain continuity.

Each .skin bone block is local geometry along +X: x=0 is the bone head,
x=max is the bone tail. A correct bind transform should place a child's head
at its parent's tail. We try candidate 4x4/3x3 layouts and scale factors.
"""
from __future__ import annotations

import itertools
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from skel import read_skel  # noqa: E402
from skin import read_skin  # noqa: E402

# Standard 3ds Max Biped chains used by the dinosaur skeletons.
PARENTS = {
    "Bip01": "Scene_Root",
    "Bip01_Pelvis": "Bip01",
    "Bip01_Spine": "Bip01_Pelvis",
    "Bip01_Spine1": "Bip01_Spine",
    "Bip01_Spine2": "Bip01_Spine1",
    "Bip01_Spine3": "Bip01_Spine2",
    "Bip01_Neck": "Bip01_Spine3",
    "Bip01_Neck1": "Bip01_Neck",
    "Bip01_Neck2": "Bip01_Neck1",
    "Bip01_Head": "Bip01_Neck2",
    "Bip01_L_Clavicle": "Bip01_Spine3",
    "Bip01_L_UpperArm": "Bip01_L_Clavicle",
    "Bip01_L_Forearm": "Bip01_L_UpperArm",
    "Bip01_L_Hand": "Bip01_L_Forearm",
    "Bip01_L_Finger0": "Bip01_L_Hand",
    "Bip01_R_Clavicle": "Bip01_Spine3",
    "Bip01_R_UpperArm": "Bip01_R_Clavicle",
    "Bip01_R_Forearm": "Bip01_R_UpperArm",
    "Bip01_R_Hand": "Bip01_R_Forearm",
    "Bip01_R_Finger0": "Bip01_R_Hand",
    "Bip01_L_Thigh": "Bip01_Pelvis",
    "Bip01_L_Calf": "Bip01_L_Thigh",
    "Bip01_L_Foot": "Bip01_L_Calf",
    "Bip01_L_Toe0": "Bip01_L_Foot",
    "Bip01_R_Thigh": "Bip01_Pelvis",
    "Bip01_R_Calf": "Bip01_R_Thigh",
    "Bip01_R_Foot": "Bip01_R_Calf",
    "Bip01_R_Toe0": "Bip01_R_Foot",
    "Bip01_Tail": "Bip01_Pelvis",
    "Bip01_Tail1": "Bip01_Tail",
    "Bip01_Tail2": "Bip01_Tail1",
    "Bip01_Tail3": "Bip01_Tail2",
    "Bip01_Footsteps": "Bip01",
}


def matrix_for(kind: str, v: np.ndarray, scale_r: float, scale_t: float) -> np.ndarray:
    M = np.eye(4)
    if kind == "4x4_row_t16":
        M[:3, :3] = v[0:16].reshape(4, 4)[:3, :3] * scale_r
        M[:3, 3] = v[16:19] * scale_t
    elif kind == "4x4_col_t16":
        M[:3, :3] = v[0:16].reshape(4, 4)[:3, :3] * scale_r
        M[:3, 3] = v[16:19] * scale_t
    elif kind == "4x4_row_native":
        M = v[0:16].reshape(4, 4).copy()
        M[:3, :3] *= scale_r
        M[:3, 3] *= scale_t
    elif kind == "4x4_col_native":
        M = v[0:16].reshape(4, 4).copy()
        M[:3, :3] *= scale_r
        M[:3, 3] *= scale_t
    elif kind == "3x3_1_t16":
        M[:3, :3] = v[1:10].reshape(3, 3) * scale_r
        M[:3, 3] = v[16:19] * scale_t
    elif kind == "3x3_1_t16_col":
        M[:3, :3] = v[1:10].reshape(3, 3).T * scale_r
        M[:3, 3] = v[16:19] * scale_t
    elif kind == "3x3_1_t13":
        M[:3, :3] = v[1:10].reshape(3, 3) * scale_r
        M[:3, 3] = v[13:16] * scale_t
    elif kind == "3x3_1_t13_col":
        M[:3, :3] = v[1:10].reshape(3, 3).T * scale_r
        M[:3, 3] = v[13:16] * scale_t
    elif kind == "3x3_0_t16":
        M[:3, :3] = v[0:9].reshape(3, 3) * scale_r
        M[:3, 3] = v[16:19] * scale_t
    elif kind == "3x3_0_t16_col":
        M[:3, :3] = v[0:9].reshape(3, 3).T * scale_r
        M[:3, 3] = v[16:19] * scale_t
    elif kind == "3x3_0_t9":
        M[:3, :3] = v[0:9].reshape(3, 3) * scale_r
        M[:3, 3] = v[9:12] * scale_t
    elif kind == "3x4_0_t12":
        M[:3, :] = v[0:12].reshape(3, 4)
        M[:3, :3] *= scale_r
        M[:3, 3] *= scale_t
    elif kind == "3x4_1_t13":
        M[:3, :] = v[1:13].reshape(3, 4)
        M[:3, :3] *= scale_r
        M[:3, 3] *= scale_t
    elif kind == "translation_only":
        M[:3, 3] = v[16:19] * scale_t
    else:
        raise ValueError(kind)
    return M


KINDS = [
    "4x4_row_t16", "4x4_col_t16", "4x4_row_native", "4x4_col_native",
    "3x3_1_t16", "3x3_1_t16_col", "3x3_1_t13", "3x3_1_t13_col",
    "3x3_0_t16", "3x3_0_t16_col", "3x3_0_t9",
    "3x4_0_t12", "3x4_1_t13", "translation_only",
]
SCALES = [1.0, 0.01, 100.0, 0.1, 10.0]


def main():
    root = Path(sys.argv[1] if len(sys.argv) > 1 else "game/%MAINDIR%")
    skin_path = Path(sys.argv[2]) if len(sys.argv) > 2 else root / "Data/Graphics/Actors/Dinos/allo/allo_lod1.skin"
    skin = read_skin(skin_path)
    skel = read_skel(skin_path.with_name(skin_path.name.split("_lod")[0] + ".skel"))
    by_name = {b.name: b for b in skel.bones}
    blocks = {b.name: b for b in skin.blocks}

    pairs = [(p, c) for c, p in PARENTS.items() if p in blocks and c in blocks]
    print(f"skin blocks={len(blocks)} skel bones={len(by_name)} scored pairs={len(pairs)}")

    # local bone endpoints: origin and +X extent
    ends = {}
    for name, b in blocks.items():
        if not b.vertices:
            continue
        xs = [v[0] for v in b.vertices]
        ends[name] = (0.0, max(xs))

    rows = []
    for kind, sr, st in itertools.product(KINDS, SCALES, SCALES):
        dists = []
        for parent, child in pairs:
            if parent not in ends or child not in ends:
                continue
            pm = matrix_for(kind, np.asarray(by_name[parent].values), sr, st)
            cm = matrix_for(kind, np.asarray(by_name[child].values), sr, st)
            pp = pm @ np.array([ends[parent][1], 0, 0, 1.0])
            cp = cm @ np.array([0, 0, 0, 1.0])
            dists.append(float(np.linalg.norm(pp[:3] - cp[:3])))
        if not dists:
            continue
        rows.append((float(np.mean(dists)), float(np.median(dists)), kind, sr, st))

    rows.sort()
    print("\nmean_joint  median_joint  kind                    scale_r  scale_t")
    for mean, med, kind, sr, st in rows[:30]:
        print(f"{mean:11.3f} {med:13.3f}  {kind:22} {sr:8.3g} {st:8.3g}")


if __name__ == "__main__":
    main()
