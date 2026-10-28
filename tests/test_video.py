"""MPEG-1 ending-video decoding against the retail `Data/Video` files."""
import os
import subprocess
from pathlib import Path

import pytest
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
EXE = Path(
    os.environ.get("DINO_EXE")
    or os.environ.get("DINO_VIEWER_EXE")
    or ROOT / "build" / "mingw" / "dino_port.exe"
)
GAME = ROOT / "game" / "%MAINDIR%"
VIDEO = GAME / "Data" / "Video" / "tennis.mpg"


def _run(*args: str) -> subprocess.CompletedProcess:
    if not EXE.exists():
        pytest.skip("viewer not built")
    if not VIDEO.exists():
        pytest.skip("retail ending video missing")
    return subprocess.run(
        [str(EXE), *args], capture_output=True, text=True, timeout=120
    )


def test_cpp_video_info():
    result = _run("--video-info", str(VIDEO))
    assert result.returncode == 0, result.stderr
    assert "width=640 height=480" in result.stdout
    assert "fps=24.000" in result.stdout
    line = next(
        line for line in result.stdout.splitlines() if line.startswith("video:")
    )
    duration = float(line.split("duration=", 1)[1].split()[0])
    assert duration > 30.0


def test_cpp_video_frame(tmp_path):
    out = tmp_path / "frame.bmp"
    result = _run(
        "--video-frame", str(VIDEO), "--video-frame-index", "300",
        "--video-out", str(out),
    )
    assert result.returncode == 0, result.stderr
    assert "640x480" in result.stdout
    image = Image.open(out).convert("RGB")
    assert image.size == (640, 480)
    # Frame 300 (12.5 s) is a real rendered scene, not a blank/black frame.
    assert len(image.getcolors(maxcolors=1_000_000)) > 1000


def test_cpp_video_play(tmp_path):
    out = tmp_path / "play.bmp"
    result = _run(
        "--play-video", str(VIDEO), "--video-play-start", "300",
        "--video-play-frames", "1", "--width", "320", "--height", "240",
        "--no-vsync", "--screenshot", str(out),
    )
    assert result.returncode == 0, result.stderr
    assert "video played: 1 frames" in result.stdout
    image = Image.open(out).convert("RGB")
    assert image.size == (640, 480)
    assert len(image.getcolors(maxcolors=1_000_000)) > 1000
