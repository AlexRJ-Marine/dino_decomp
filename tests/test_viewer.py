import subprocess
import sys
from pathlib import Path

import pytest
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))
sys.path.insert(0, str(ROOT / "tools" / "viewer"))

from mesh import read_mesh  # noqa: E402
from soft_render import load_texture, render_mesh, save_texture_preview  # noqa: E402
from tga import read_tga  # noqa: E402

GAME = ROOT / "game" / "%MAINDIR%"
ALLO_MESH = GAME / "Data" / "Graphics" / "Actors" / "Dinos" / "allo" / "allo_lod1.mesh"
ALLO_TEX = GAME / "Data" / "Graphics" / "Textures" / "actors" / "dinos" / "allo.tga"
CURSOR = GAME / "Data" / "Graphics" / "Cursors" / "CursorMain.tga"
AVOID = GAME / "Data" / "Sound" / "Actors" / "Dinos" / "avoidance.wav"


def test_render_allo_mesh(tmp_path):
    if not ALLO_MESH.exists() or not ALLO_TEX.exists():
        pytest.skip("retail samples not present")
    out = tmp_path / "allo.png"
    render_mesh(
        read_mesh(ALLO_MESH),
        load_texture(ALLO_TEX),
        out,
        view="three_quarter",
        width=500,
        height=360,
        supersample=1,
    )
    assert out.exists()
    im = Image.open(out).convert("RGB")
    assert im.size == (500, 360)
    colors = im.getcolors(maxcolors=1_000_000)
    assert colors is not None
    # The background is a flat light colour; the model must occupy real pixels.
    foreground = sum(count for count, color in colors if color != (226, 233, 240))
    assert foreground > 1000


def test_texture_preview(tmp_path):
    if not CURSOR.exists():
        pytest.skip("retail sample not present")
    out = tmp_path / "cursor.png"
    save_texture_preview(read_tga(CURSOR), out)
    im = Image.open(out)
    assert im.size == (32, 32)


def test_cli_wav():
    if not AVOID.exists():
        pytest.skip("retail sample not present")
    result = subprocess.run(
        [sys.executable, str(ROOT / "tools" / "viewer" / "asset_viewer.py"),
         "--wav", str(AVOID)],
        capture_output=True, text=True,
    )
    assert result.returncode == 0, result.stderr
    assert "44100 Hz" in result.stdout
    assert "0.100s" in result.stdout


def test_cli_mesh(tmp_path):
    if not ALLO_MESH.exists() or not ALLO_TEX.exists():
        pytest.skip("retail samples not present")
    out = tmp_path / "cli-allo.png"
    result = subprocess.run(
        [sys.executable, str(ROOT / "tools" / "viewer" / "asset_viewer.py"),
         "--mesh", str(ALLO_MESH), "--texture", str(ALLO_TEX),
         "--out", str(out), "--width", "320", "--height", "240",
         "--no-supersample"],
        capture_output=True, text=True,
    )
    assert result.returncode == 0, result.stderr
    assert out.exists()
    assert Image.open(out).size == (320, 240)


def test_cli_font(tmp_path):
    font = GAME / "Data" / "Fonte" / "arial12.fnt"
    if not font.exists():
        pytest.skip("retail sample not present")
    out = tmp_path / "text.png"
    result = subprocess.run(
        [sys.executable, str(ROOT / "tools" / "viewer" / "asset_viewer.py"),
         "--font", str(font), "--text", "Hello, Dino Island!",
         "--scale", "2", "--out", str(out)],
        capture_output=True, text=True,
    )
    assert result.returncode == 0, result.stderr
    assert out.exists()
    assert Image.open(out).size[0] > 100
