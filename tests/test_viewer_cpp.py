import subprocess
from pathlib import Path

import pytest
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "build" / "mingw" / "dino_viewer.exe"
GAME = ROOT / "game" / "%MAINDIR%"
ALLO_MESH = GAME / "Data" / "Graphics" / "Actors" / "Dinos" / "allo" / "allo_lod1.mesh"
ALLO_TEX = GAME / "Data" / "Graphics" / "Textures" / "actors" / "dinos" / "allo.tga"


def test_viewer_window_smoke(tmp_path):
    if not EXE.exists():
        pytest.skip("dino_viewer not built (run scripts/build.ps1)")
    shot = tmp_path / "window.bmp"
    result = subprocess.run(
        [str(EXE), "--frames", "2", "--screenshot", str(shot),
         "--width", "320", "--height", "240", "--no-vsync"],
        capture_output=True, text=True, timeout=60,
    )
    assert result.returncode == 0, result.stderr
    assert "SDL 3." in result.stdout
    assert shot.exists()
    assert Image.open(shot).size == (320, 240)


def test_cpp_mesh_validate():
    if not EXE.exists():
        pytest.skip("dino_viewer not built (run scripts/build.ps1)")
    if not ALLO_MESH.exists() or not ALLO_TEX.exists():
        pytest.skip("retail samples not present")
    result = subprocess.run(
        [str(EXE), "--validate", "--mesh", str(ALLO_MESH),
         "--texture", str(ALLO_TEX)],
        capture_output=True, text=True, timeout=60,
    )
    assert result.returncode == 0, result.stderr
    assert "mesh dino_lod1: 317 vertices" in result.stdout
    assert "382 triangles" in result.stdout
    assert "317 uvs, 317 normals, 317 colors" in result.stdout
    assert "texture" in result.stdout and "256x256 RGBA" in result.stdout


def test_cpp_mesh_window(tmp_path):
    if not EXE.exists():
        pytest.skip("dino_viewer not built (run scripts/build.ps1)")
    if not ALLO_MESH.exists() or not ALLO_TEX.exists():
        pytest.skip("retail samples not present")
    shot = tmp_path / "allo.bmp"
    result = subprocess.run(
        [str(EXE), "--frames", "2", "--screenshot", str(shot),
         "--width", "400", "--height", "300", "--no-vsync",
         "--mesh", str(ALLO_MESH), "--texture", str(ALLO_TEX)],
        capture_output=True, text=True, timeout=60,
    )
    assert result.returncode == 0, result.stderr
    assert shot.exists()
    image = Image.open(shot).convert("RGB")
    assert image.size == (400, 300)
    bg = image.getpixel((0, 0))
    data = image.tobytes()
    foreground = sum(
        1 for i in range(0, len(data), 3)
        if abs(data[i] - bg[0]) + abs(data[i + 1] - bg[1]) + abs(data[i + 2] - bg[2]) > 30
    )
    assert foreground > 200
