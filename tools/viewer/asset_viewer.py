#!/usr/bin/env python3
"""Phase 1 asset viewer.

Examples::

    python tools/viewer/asset_viewer.py \
        --mesh "game/%MAINDIR%/Data/Graphics/Actors/Dinos/allo/allo_lod1.mesh" \
        --texture "game/%MAINDIR%/Data/Graphics/Textures/actors/dinos/allo.tga" \
        --view three_quarter --out notes/view-allo.png

    python tools/viewer/asset_viewer.py --texture <file.tga> --out notes/view-tga.png
    python tools/viewer/asset_viewer.py --wav <file.wav> [--play]
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

FORMATS = Path(__file__).resolve().parents[1] / "formats"
if str(FORMATS) not in sys.path:
    sys.path.insert(0, str(FORMATS))

from mesh import read_mesh  # noqa: E402
from tga import read_tga  # noqa: E402
from wav import read_wav  # noqa: E402

from soft_render import load_texture, render_mesh, save_texture_preview  # noqa: E402


def _wav_command(path: Path, play: bool) -> int:
    w = read_wav(path, load_data=False)
    print(f"{path}")
    print(f"  {w.channels} ch, {w.sample_rate} Hz, {w.bits_per_sample}-bit PCM")
    print(f"  {w.frames:,} frames, {w.duration:.3f}s")
    if play:
        if sys.platform == "win32":
            import winsound

            winsound.PlaySound(str(path), winsound.SND_FILENAME)
        else:
            print("  --play is only wired up on Windows (winsound)")
    return 0


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--mesh", type=Path, help=".mesh to render")
    ap.add_argument("--texture", type=Path, help=".tga texture or texture-only preview")
    ap.add_argument("--wav", type=Path, help=".wav info (and optional --play)")
    ap.add_argument("--out", type=Path, help="output PNG")
    ap.add_argument("--view", default="three_quarter",
                    choices=("side", "three_quarter", "front", "top"))
    ap.add_argument("--width", type=int, default=1400)
    ap.add_argument("--height", type=int, default=900)
    ap.add_argument("--no-supersample", action="store_true")
    ap.add_argument("--play", action="store_true")
    args = ap.parse_args(argv)

    if args.wav:
        return _wav_command(args.wav, args.play)

    if args.mesh:
        if args.out is None:
            ap.error("--mesh requires --out")
        mesh = read_mesh(args.mesh)
        texture = load_texture(args.texture) if args.texture else None
        out = render_mesh(
            mesh,
            texture,
            args.out,
            view=args.view,
            width=args.width,
            height=args.height,
            supersample=1 if args.no_supersample else 2,
        )
        print(f"wrote {out}: {len(mesh.vertices)} vertices, "
              f"{mesh.triangle_count} triangles, view={args.view}")
        return 0

    if args.texture:
        if args.out is None:
            ap.error("--texture requires --out")
        tga = read_tga(args.texture)
        out = save_texture_preview(tga, args.out)
        print(f"wrote {out}: {tga.width}x{tga.height}, {tga.pixel_depth}-bit")
        return 0

    ap.error("choose --mesh, --texture, or --wav")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
