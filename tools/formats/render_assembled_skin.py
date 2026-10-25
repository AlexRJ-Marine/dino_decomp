#!/usr/bin/env python3
"""Assemble .skin bone blocks with a candidate .skel transform and render PNGs."""
from __future__ import annotations

import math
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).resolve().parent))
from skel import read_skel  # noqa: E402
from skin import read_skin  # noqa: E402


def matrix_for(v: np.ndarray, scale: float) -> np.ndarray:
    M = np.eye(4)
    M[:3, :3] = v[0:9].reshape(3, 3) * scale
    M[:3, 3] = v[16:19] * scale
    return M


def color_for(i: int) -> tuple[int, int, int]:
    h = (i * 0.61803398875) % 1.0
    s, v = 0.65, 0.95
    c = v * s
    x = c * (1 - abs((h * 6) % 2 - 1))
    m = v - c
    if h < 1 / 6: r, g, b = c, x, 0
    elif h < 2 / 6: r, g, b = x, c, 0
    elif h < 3 / 6: r, g, b = 0, c, x
    elif h < 4 / 6: r, g, b = 0, x, c
    elif h < 5 / 6: r, g, b = x, 0, c
    else: r, g, b = c, 0, x
    return tuple(int((q + m) * 255) for q in (r, g, b))


def render(points: list[np.ndarray], tris: list[tuple[int, int, int]], colors: list[tuple[int, int, int]],
           origins: list[np.ndarray], out: Path, view: str, size=(1400, 1000)):
    allp = np.vstack(points)
    if view == "side":
        u, v = allp[:, 1], allp[:, 2]
    elif view == "front":
        u, v = allp[:, 0], allp[:, 2]
    else:
        u, v = allp[:, 0], allp[:, 1]
    umin, umax = u.min(), u.max()
    vmin, vmax = v.min(), v.max()
    pad = 60
    scale = min((size[0] - 2 * pad) / max(1e-6, umax - umin), (size[1] - 2 * pad) / max(1e-6, vmax - vmin))
    ox = (size[0] - (umax - umin) * scale) / 2 - umin * scale
    oy = (size[1] - (vmax - vmin) * scale) / 2 - vmin * scale

    def proj(p):
        if view == "side":
            x, y = p[1], p[2]
        elif view == "front":
            x, y = p[0], p[2]
        else:
            x, y = p[0], p[1]
        return (x * scale + ox, size[1] - (y * scale + oy))

    img = Image.new("RGB", size, (250, 250, 250))
    d = ImageDraw.Draw(img)
    # painter's algorithm: far to near along the remaining axis
    depth_axis = 0 if view == "side" else 1
    order = sorted(range(len(tris)), key=lambda i: np.mean([allp[tris[i][k]][depth_axis] for k in range(3)]))
    for i in order:
        a, b, c = tris[i]
        poly = [proj(allp[a]), proj(allp[b]), proj(allp[c])]
        d.polygon(poly, fill=colors[i], outline=(40, 40, 40))
    for p in origins:
        x, y = proj(p)
        d.ellipse((x - 4, y - 4, x + 4, y + 4), fill=(0, 0, 0))
    d.text((10, 10), f"{view}  points={sum(len(p) for p in points)}  tris={len(tris)}", fill=(0, 0, 0))
    img.save(out)
    print(f"wrote {out}")


def main():
    root = Path(sys.argv[1] if len(sys.argv) > 1 else "game/%MAINDIR%")
    skin_path = Path(sys.argv[2]) if len(sys.argv) > 2 else root / "Data/Graphics/Actors/Dinos/allo/allo_lod1.skin"
    scale = float(sys.argv[3]) if len(sys.argv) > 3 else 0.01
    outdir = Path(sys.argv[4]) if len(sys.argv) > 4 else Path("notes/renders")
    outdir.mkdir(parents=True, exist_ok=True)

    skin = read_skin(skin_path)
    skel = read_skel(skin_path.with_name(skin_path.name.split("_lod")[0] + ".skel"))
    by_name = {b.name: b for b in skel.bones}

    points: list[np.ndarray] = []
    tris: list[tuple[int, int, int]] = []
    colors: list[tuple[int, int, int]] = []
    origins: list[np.ndarray] = []
    offset = 0
    for bi, block in enumerate(skin.blocks):
        bone = by_name.get(block.name)
        if bone is None or not block.vertices:
            continue
        M = matrix_for(np.asarray(bone.values), scale)
        p = np.asarray(block.vertices, dtype=np.float64)
        ph = np.hstack([p, np.ones((len(p), 1))])
        w = (ph @ M.T)[:, :3]
        points.append(w)
        for k in range(0, len(block.indices), 3):
            tris.append((offset + block.indices[k], offset + block.indices[k + 1], offset + block.indices[k + 2]))
            colors.append(color_for(bi))
        origins.append(w[0])
        offset += len(p)

    print(f"assembled {len(points)} blocks, {offset} verts, {len(tris)} tris")
    print("bbox", np.vstack(points).min(axis=0).round(2), np.vstack(points).max(axis=0).round(2))
    for view in ("side", "front"):
        render(points, tris, colors, origins, outdir / f"allo_bind_{view}.png", view)


if __name__ == "__main__":
    main()
