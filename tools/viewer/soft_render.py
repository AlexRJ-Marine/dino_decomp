"""Small software renderer used by the Phase 1 asset viewer.

It is deliberately simple: orthographic/perspective camera, z-buffer,
per-vertex texture UVs, optional vertex colors, one key light and one fill.
The goal is to check decoded data by eye, not to be a game renderer.
"""
from __future__ import annotations

import math
import sys
from pathlib import Path

import numpy as np
from PIL import Image

FORMATS = Path(__file__).resolve().parents[1] / "formats"
if str(FORMATS) not in sys.path:
    sys.path.insert(0, str(FORMATS))

from mesh import Mesh  # noqa: E402
from tga import Tga, read_tga  # noqa: E402


def texture_from_tga(tga: Tga) -> np.ndarray:
    """Return an (H, W, 3) float RGB array, top-left origin."""
    if not tga.pixels:
        raise ValueError("TGA was opened header-only; decode it first")
    rgba = np.frombuffer(tga.pixels, dtype=np.uint8).reshape(tga.height, tga.width, 4)
    return rgba[:, :, :3].astype(np.float64) / 255.0


def _normalize(v: np.ndarray) -> np.ndarray:
    n = np.linalg.norm(v)
    return v / n if n else v


def _sample_texture(tex: np.ndarray, tu: np.ndarray, tv: np.ndarray) -> np.ndarray:
    h, w = tex.shape[:2]
    x = (tu % 1.0) * (w - 1)
    y = ((1.0 + tv) % 1.0) * (h - 1)
    x0 = np.floor(x).astype(np.int64)
    y0 = np.floor(y).astype(np.int64)
    x1 = np.minimum(x0 + 1, w - 1)
    y1 = np.minimum(y0 + 1, h - 1)
    fx = x - x0
    fy = y - y0
    c00 = tex[y0, x0]
    c10 = tex[y0, x1]
    c01 = tex[y1, x0]
    c11 = tex[y1, x1]
    return (
        c00 * (1 - fx)[..., None] * (1 - fy)[..., None]
        + c10 * fx[..., None] * (1 - fy)[..., None]
        + c01 * (1 - fx)[..., None] * fy[..., None]
        + c11 * fx[..., None] * fy[..., None]
    )


VIEW_DIRECTIONS = {
    "side": (2.6, 0.0, 0.25),
    "three_quarter": (1.5, -1.9, 0.95),
    "front": (0.05, -2.6, 0.25),
    "top": (0.01, 0.02, 2.6),
}


def render_mesh(
    mesh: Mesh,
    texture: np.ndarray | None,
    out: Path,
    *,
    view: str = "three_quarter",
    width: int = 1400,
    height: int = 900,
    fov_deg: float = 33.0,
    supersample: int = 2,
    background: tuple[int, int, int] = (226, 233, 240),
) -> Path:
    if view not in VIEW_DIRECTIONS:
        raise ValueError(f"unknown view {view!r}; choose {sorted(VIEW_DIRECTIONS)}")
    if not mesh.vertices or not mesh.indices:
        raise ValueError("mesh has no geometry")

    ss = max(1, int(supersample))
    W, H = width * ss, height * ss
    verts = np.asarray(mesh.vertices, dtype=np.float64)
    tris = np.asarray(mesh.indices, dtype=np.int64).reshape(-1, 3)
    normals = (
        np.asarray(mesh.normals, dtype=np.float64)
        if len(mesh.normals) == len(verts)
        else None
    )
    uvs = (
        np.asarray(mesh.uvs, dtype=np.float64)
        if len(mesh.uvs) == len(verts)
        else None
    )
    colors = (
        np.asarray(mesh.colors, dtype=np.float64)[:, :3] / 255.0
        if len(mesh.colors) == len(verts)
        else None
    )

    lo = verts.min(axis=0)
    hi = verts.max(axis=0)
    center = (lo + hi) / 2
    direction = _normalize(np.asarray(VIEW_DIRECTIONS[view], dtype=np.float64))
    up_hint = np.array([0.0, 1.0, 0.0]) if view == "top" else np.array([0.0, 0.0, 1.0])

    fwd = -direction
    right = _normalize(np.cross(fwd, up_hint))
    up = np.cross(right, fwd)

    # Fit the bounding box to the frame using the projection's half-angles.
    f = 0.5 * H / math.tan(math.radians(fov_deg) / 2.0)
    corners = np.array([
        [x, y, z] for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])
    ]) - center
    max_x = float(np.max(np.abs(corners @ right)))
    max_y = float(np.max(np.abs(corners @ up)))
    distance = max(
        f * max_x / (0.45 * W),
        f * max_y / (0.45 * H),
    )
    eye = center + direction * distance
    target = center

    rel = verts - eye
    xc = rel @ right
    yc = rel @ up
    zc = rel @ fwd
    sx = W / 2 + xc / zc * f
    sy = H / 2 - yc / zc * f

    img = np.empty((H, W, 3), dtype=np.float64)
    img[:] = np.asarray(background, dtype=np.float64) / 255.0
    zbuf = np.full((H, W), np.inf)

    light1 = _normalize(np.array([-0.45, -0.85, 0.95]))
    light2 = _normalize(np.array([0.8, 0.25, 0.35]))
    view_dir = _normalize(eye - target)

    for tri in tris:
        p = np.stack([sx[tri], sy[tri]], axis=1)
        z = zc[tri]
        xmin = max(int(np.floor(p[:, 0].min())), 0)
        xmax = min(int(np.ceil(p[:, 0].max())), W - 1)
        ymin = max(int(np.floor(p[:, 1].min())), 0)
        ymax = min(int(np.ceil(p[:, 1].max())), H - 1)
        if xmin > xmax or ymin > ymax:
            continue
        xs = np.arange(xmin, xmax + 1)
        ys = np.arange(ymin, ymax + 1)
        px, py = np.meshgrid(xs + 0.5, ys + 0.5)

        x0, y0 = p[0]
        x1, y1 = p[1]
        x2, y2 = p[2]
        denom = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
        if abs(denom) < 1e-12:
            continue
        w0 = ((y1 - y2) * (px - x2) + (x2 - x1) * (py - y2)) / denom
        w1 = ((y2 - y0) * (px - x2) + (x0 - x2) * (py - y2)) / denom
        w2 = 1.0 - w0 - w1
        inside = (w0 >= -1e-6) & (w1 >= -1e-6) & (w2 >= -1e-6)
        if not inside.any():
            continue
        zz = w0 * z[0] + w1 * z[1] + w2 * z[2]
        mask = inside & (zz < zbuf[ymin:ymax + 1, xmin:xmax + 1])
        if not mask.any():
            continue

        if uvs is not None and texture is not None:
            uu = w0 * uvs[tri[0], 0] + w1 * uvs[tri[1], 0] + w2 * uvs[tri[2], 0]
            vv = w0 * uvs[tri[0], 1] + w1 * uvs[tri[1], 1] + w2 * uvs[tri[2], 1]
            col = _sample_texture(texture, uu, vv)
        else:
            col = np.ones((*px.shape, 3), dtype=np.float64) * 0.72

        if colors is not None:
            vc = (
                w0[..., None] * colors[tri[0]]
                + w1[..., None] * colors[tri[1]]
                + w2[..., None] * colors[tri[2]]
            )
            col = col * np.clip(vc * 1.6, 0.0, 1.0)

        if normals is not None:
            nn = (
                w0[..., None] * normals[tri[0]]
                + w1[..., None] * normals[tri[1]]
                + w2[..., None] * normals[tri[2]]
            )
            nn = nn / np.maximum(np.linalg.norm(nn, axis=-1, keepdims=True), 1e-9)
            facing = nn @ view_dir
            nn[facing < 0] *= -1.0
            light = 0.50 + 0.60 * np.clip(nn @ light1, 0, 1) + 0.20 * np.clip(nn @ light2, 0, 1)
            light = np.clip(light, 0.0, 1.2)
        else:
            light = np.ones(px.shape, dtype=np.float64)

        col = np.clip(col * light[..., None] * 1.15, 0.0, 1.0)
        sub = img[ymin:ymax + 1, xmin:xmax + 1]
        sub[mask] = col[mask]
        zbuf[ymin:ymax + 1, xmin:xmax + 1][mask] = zz[mask]

    image = Image.fromarray((np.clip(img, 0, 1) * 255).astype(np.uint8))
    if ss > 1:
        image = image.resize((width, height), Image.LANCZOS)
    out.parent.mkdir(parents=True, exist_ok=True)
    image.save(out)
    return out


def save_texture_preview(tga: Tga, out: Path) -> Path:
    if not tga.pixels:
        raise ValueError("TGA was opened header-only; decode it first")
    rgba = Image.frombytes("RGBA", (tga.width, tga.height), tga.pixels)
    background = Image.new("RGB", rgba.size, (238, 240, 243))
    background.paste(rgba, mask=rgba.split()[3])
    out.parent.mkdir(parents=True, exist_ok=True)
    background.save(out)
    return out


def load_texture(path: Path) -> np.ndarray:
    return texture_from_tga(read_tga(path))
