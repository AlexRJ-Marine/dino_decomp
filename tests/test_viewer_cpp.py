import subprocess
from pathlib import Path

import pytest
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "build" / "mingw" / "dino_viewer.exe"
GAME = ROOT / "game" / "%MAINDIR%"
ALLO_MESH = GAME / "Data" / "Graphics" / "Actors" / "Dinos" / "allo" / "allo_lod1.mesh"
ALLO_TEX = GAME / "Data" / "Graphics" / "Textures" / "actors" / "dinos" / "allo.tga"
ALLO_SKEL = GAME / "Data" / "Graphics" / "Actors" / "Dinos" / "allo" / "allo.skel"
ALLO_SKIN = GAME / "Data" / "Graphics" / "Actors" / "Dinos" / "allo" / "allo_lod1.skin"
ALLO_ANIM = (GAME / "Data" / "Graphics" / "Actors" / "Dinos" / "animations"
             / "HeavyBiped" / "HeavyBiped_Armor.anim")


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


def _skinned_args():
    return [
        "--mesh", str(ALLO_MESH), "--texture", str(ALLO_TEX),
        "--skel", str(ALLO_SKEL), "--skin", str(ALLO_SKIN),
        "--anim", str(ALLO_ANIM),
    ]


def test_cpp_skinned_validate():
    if not EXE.exists():
        pytest.skip("dino_viewer not built (run scripts/build.ps1)")
    if not all(p.exists() for p in (ALLO_MESH, ALLO_SKEL, ALLO_SKIN, ALLO_ANIM)):
        pytest.skip("retail samples not present")
    result = subprocess.run(
        [str(EXE), "--validate", *_skinned_args(), "--time", "0"],
        capture_output=True, text=True, timeout=60,
    )
    assert result.returncode == 0, result.stderr
    assert "skel" in result.stdout and "80 bones" in result.stdout
    assert "skin" in result.stdout and "35 bones" in result.stdout
    assert "317 assignments" in result.stdout
    assert "anim" in result.stdout and "35 bones" in result.stdout
    assert "1085 keys" in result.stdout
    assert "skin bind bounds" in result.stdout
    assert "skin pose bounds" in result.stdout


def test_cpp_skinned_window(tmp_path):
    if not EXE.exists():
        pytest.skip("dino_viewer not built (run scripts/build.ps1)")
    if not all(p.exists() for p in (ALLO_MESH, ALLO_SKEL, ALLO_SKIN, ALLO_ANIM)):
        pytest.skip("retail samples not present")
    shot = tmp_path / "skin.bmp"
    result = subprocess.run(
        [str(EXE), "--frames", "2", "--screenshot", str(shot),
         "--width", "400", "--height", "300", "--no-vsync", "--time", "1600",
         "--distance", "16", *_skinned_args()],
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


def test_cpp_frame_capture(tmp_path):
    if not EXE.exists():
        pytest.skip("dino_viewer not built (run scripts/build.ps1)")
    if not all(p.exists() for p in (ALLO_MESH, ALLO_SKEL, ALLO_SKIN, ALLO_ANIM)):
        pytest.skip("retail samples not present")
    out_dir = tmp_path / "frames"
    result = subprocess.run(
        [str(EXE), "--frames", "3", "--capture-dir", str(out_dir),
         "--capture-prefix", "test", "--width", "200", "--height", "150",
         "--no-vsync", "--distance", "16", *_skinned_args()],
        capture_output=True, text=True, timeout=60,
    )
    assert result.returncode == 0, result.stderr
    frames = sorted(out_dir.glob("test_*.bmp"))
    assert len(frames) == 3
    assert Image.open(frames[0]).size == (200, 150)
