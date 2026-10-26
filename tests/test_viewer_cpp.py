import os
import re
import subprocess
from pathlib import Path

import pytest
from PIL import Image, ImageChops, ImageStat

ROOT = Path(__file__).resolve().parents[1]
EXE = Path(os.environ.get("DINO_VIEWER_EXE",
                          ROOT / "build" / "mingw" / "dino_viewer.exe"))
GAME = ROOT / "game" / "%MAINDIR%"
ALLO_MESH = GAME / "Data" / "Graphics" / "Actors" / "Dinos" / "allo" / "allo_lod1.mesh"
ALLO_TEX = GAME / "Data" / "Graphics" / "Textures" / "actors" / "dinos" / "allo.tga"
ALLO_SKEL = GAME / "Data" / "Graphics" / "Actors" / "Dinos" / "allo" / "allo.skel"
ALLO_SKIN = GAME / "Data" / "Graphics" / "Actors" / "Dinos" / "allo" / "allo_lod1.skin"
ALLO_ANIM = (GAME / "Data" / "Graphics" / "Actors" / "Dinos" / "animations"
             / "HeavyBiped" / "HeavyBiped_Armor.anim")
ISL01 = GAME / "Data" / "Design" / "landscape" / "isl01" / "isl01.lnd"


def test_viewer_window_smoke(tmp_path):
    if not EXE.exists():
        pytest.skip("dino_viewer not built (run scripts/build.ps1)")
    shot = tmp_path / "window.bmp"
    result = subprocess.run(
        [str(EXE), "--frames", "2", "--screenshot", str(shot),
         "--width", "320", "--height", "240", "--no-vsync",
         "--no-save-config"],
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
         "--width", "400", "--height", "300", "--no-vsync", "--no-save-config",
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


def _dino_geometry_args():
    return [
        "--mesh", str(ALLO_MESH),
        "--skel", str(ALLO_SKEL), "--skin", str(ALLO_SKIN),
        "--anim", str(ALLO_ANIM),
    ]


def test_cpp_skin_bind_modes():
    if not EXE.exists():
        pytest.skip("dino_viewer not built (run scripts/build.ps1)")
    if not all(p.exists() for p in (ALLO_MESH, ALLO_SKEL, ALLO_SKIN, ALLO_ANIM)):
        pytest.skip("retail samples not present")
    default = subprocess.run(
        [str(EXE), "--validate", *_skinned_args()],
        capture_output=True, text=True, timeout=60,
    )
    assert default.returncode == 0, default.stderr
    assert "(.mesh bind)" in default.stdout
    skin_bind = subprocess.run(
        [str(EXE), "--validate", "--skin-bind", "skin", *_skinned_args()],
        capture_output=True, text=True, timeout=60,
    )
    assert skin_bind.returncode == 0, skin_bind.stderr
    assert "(.skin bind)" in skin_bind.stdout


def test_cpp_island_dino_validate():
    if not EXE.exists():
        pytest.skip("dino_viewer not built (run scripts/build.ps1)")
    if not ISL01.exists() or not all(
            p.exists() for p in (ALLO_MESH, ALLO_SKEL, ALLO_SKIN, ALLO_ANIM)):
        pytest.skip("retail samples not present")
    result = subprocess.run(
        [str(EXE), "--validate", "--lnd", str(ISL01), "--asset-root", str(GAME),
         "--dino-texture", str(ALLO_TEX), "--dino-pos", "512,512",
         *_dino_geometry_args()],
        capture_output=True, text=True, timeout=120,
    )
    assert result.returncode == 0, result.stderr
    assert "vegetation: 9 groups, 1536 instances, 52291 triangles" in result.stdout
    assert "dino texture" in result.stdout
    assert "(.mesh bind)" in result.stdout


def test_cpp_island_dino_window(tmp_path):
    if not EXE.exists():
        pytest.skip("dino_viewer not built (run scripts/build.ps1)")
    if not ISL01.exists() or not all(
            p.exists() for p in (ALLO_MESH, ALLO_SKEL, ALLO_SKIN, ALLO_ANIM)):
        pytest.skip("retail samples not present")
    shot = tmp_path / "island-dino.bmp"
    result = subprocess.run(
        [str(EXE), "--frames", "2", "--screenshot", str(shot),
         "--width", "480", "--height", "320", "--no-vsync", "--no-save-config",
         "--lnd", str(ISL01), "--asset-root", str(GAME),
         "--dino-texture", str(ALLO_TEX), "--dino-pos", "512,512",
         "--distance", "17", "--pitch", "16",
         *_dino_geometry_args()],
        capture_output=True, text=True, timeout=120,
    )
    assert result.returncode == 0, result.stderr
    assert shot.exists()
    assert Image.open(shot).size == (480, 320)
    assert "spawn: x=512.0 y=512.0" in result.stdout
    image = Image.open(shot).convert("RGB")
    bg = image.getpixel((0, 0))
    data = image.tobytes()
    foreground = sum(
        1 for i in range(0, len(data), 3)
        if abs(data[i] - bg[0]) + abs(data[i + 1] - bg[1]) + abs(data[i + 2] - bg[2]) > 20
    )
    assert foreground > 3000


def test_cpp_free_camera_start(tmp_path):
    if not EXE.exists():
        pytest.skip("dino_viewer not built (run scripts/build.ps1)")
    if not ISL01.exists() or not all(
            p.exists() for p in (ALLO_MESH, ALLO_SKEL, ALLO_SKIN, ALLO_ANIM)):
        pytest.skip("retail samples not present")
    shot = tmp_path / "free-cam.bmp"
    result = subprocess.run(
        [str(EXE), "--frames", "2", "--screenshot", str(shot),
         "--width", "400", "--height", "300", "--no-vsync", "--no-save-config",
         "--free-camera", "--lnd", str(ISL01), "--asset-root", str(GAME),
         "--dino-texture", str(ALLO_TEX), "--dino-pos", "512,512",
         "--distance", "17", "--pitch", "16",
         *_dino_geometry_args()],
        capture_output=True, text=True, timeout=120,
    )
    assert result.returncode == 0, result.stderr
    assert shot.exists()
    assert Image.open(shot).size == (400, 300)


def test_cpp_keymap_has_free_camera_actions():
    if not EXE.exists():
        pytest.skip("dino_viewer not built (run scripts/build.ps1)")
    result = subprocess.run(
        [str(EXE), "--list-keys"], capture_output=True, text=True, timeout=60,
    )
    assert result.returncode == 0, result.stderr
    for action in ("move_forward", "move_back", "move_left", "move_right",
                   "move_up", "move_down", "sprint", "toggle_camera"):
        assert action in result.stdout


def test_cpp_build_save_load(tmp_path):
    if not EXE.exists():
        pytest.skip("dino_viewer not built (run scripts/build.ps1)")
    if not ISL01.exists():
        pytest.skip("retail sample not present")
    save = tmp_path / "park.txt"
    result = subprocess.run(
        [str(EXE), "--validate", "--lnd", str(ISL01), "--asset-root", str(GAME),
         "--no-vegetation", "--build", "--place-path", "128,128",
         "--place-path", "129,128", "--place-fence", "128,128,0",
         "--build-save", str(save)],
        capture_output=True, text=True, timeout=120,
    )
    assert result.returncode == 0, result.stderr
    assert "2 path cells, 1 fences" in result.stdout
    assert "mesh 2 tiles, 1 segments" in result.stdout
    text = save.read_text()
    assert "path 128 128" in text and "path 129 128" in text
    assert "fence 128 128 0" in text

    reload = subprocess.run(
        [str(EXE), "--validate", "--lnd", str(ISL01), "--asset-root", str(GAME),
         "--no-vegetation", "--build-load", str(save)],
        capture_output=True, text=True, timeout=120,
    )
    assert reload.returncode == 0, reload.stderr
    assert "2 path cells, 1 fences" in reload.stdout


def test_cpp_pick_terrain(tmp_path):
    if not EXE.exists():
        pytest.skip("dino_viewer not built (run scripts/build.ps1)")
    if not ISL01.exists() or not all(
            p.exists() for p in (ALLO_MESH, ALLO_SKEL, ALLO_SKIN)):
        pytest.skip("retail samples not present")
    shot = tmp_path / "pick.bmp"
    result = subprocess.run(
        [str(EXE), "--frames", "1", "--screenshot", str(shot),
         "--width", "1200", "--height", "760", "--no-vsync", "--no-save-config",
         "--lnd", str(ISL01), "--asset-root", str(GAME),
         "--yaw", "35", "--pitch", "22", "--distance", "24",
         "--pick-test", "600,380", "--dino-pos", "512,512",
         *_dino_geometry_args()],
        capture_output=True, text=True, timeout=120,
    )
    assert result.returncode == 0, result.stderr
    assert "pick: " in result.stdout
    assert "cell 126 126" in result.stdout
    assert shot.exists()


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
         "--width", "400", "--height", "300", "--no-vsync", "--no-save-config",
         "--time", "1600",
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
         "--no-vsync", "--no-save-config", "--distance", "16", *_skinned_args()],
        capture_output=True, text=True, timeout=60,
    )
    assert result.returncode == 0, result.stderr
    frames = sorted(out_dir.glob("test_*.bmp"))
    assert len(frames) == 3
    assert Image.open(frames[0]).size == (200, 150)


def test_cpp_config_persist(tmp_path):
    if not EXE.exists():
        pytest.skip("dino_viewer not built (run scripts/build.ps1)")
    cfg = tmp_path / "viewer.cfg"
    first = tmp_path / "first.bmp"
    result = subprocess.run(
        [str(EXE), "--config", str(cfg), "--width", "400", "--height", "300",
         "--frames", "2", "--screenshot", str(first), "--no-vsync"],
        capture_output=True, text=True, timeout=60,
    )
    assert result.returncode == 0, result.stderr
    text = cfg.read_text()
    assert "width=400" in text and "height=300" in text
    second = tmp_path / "second.bmp"
    result = subprocess.run(
        [str(EXE), "--config", str(cfg), "--frames", "2",
         "--screenshot", str(second), "--no-vsync", "--no-save-config"],
        capture_output=True, text=True, timeout=60,
    )
    assert result.returncode == 0, result.stderr
    assert Image.open(second).size == (400, 300)


def test_cpp_widescreen_and_4x3(tmp_path):
    if not EXE.exists():
        pytest.skip("dino_viewer not built (run scripts/build.ps1)")
    wide = tmp_path / "wide.bmp"
    tall = tmp_path / "tall.bmp"
    for out, width, height in ((wide, 480, 270), (tall, 480, 360)):
        result = subprocess.run(
            [str(EXE), "--frames", "2", "--screenshot", str(out),
             "--width", str(width), "--height", str(height),
             "--no-vsync", "--no-save-config", "--borderless"],
            capture_output=True, text=True, timeout=60,
        )
        assert result.returncode == 0, result.stderr
        assert Image.open(out).size == (width, height)
    assert Image.open(wide).size[0] / Image.open(wide).size[1] > 1.7
    assert Image.open(tall).size[0] / Image.open(tall).size[1] < 1.4


def test_cpp_keymap_persist(tmp_path):
    if not EXE.exists():
        pytest.skip("dino_viewer not built (run scripts/build.ps1)")
    keys = tmp_path / "keys.cfg"
    result = subprocess.run(
        [str(EXE), "--keys", str(keys),
         "--rebind", "orbit_left=J", "--rebind", "reset=H", "--list-keys"],
        capture_output=True, text=True, timeout=60,
    )
    assert result.returncode == 0, result.stderr
    assert "orbit_left   J" in result.stdout
    assert "reset        H" in result.stdout
    text = keys.read_text()
    assert "orbit_left=J" in text and "reset=H" in text
    again = subprocess.run(
        [str(EXE), "--keys", str(keys), "--list-keys"],
        capture_output=True, text=True, timeout=60,
    )
    assert again.returncode == 0, again.stderr
    assert "orbit_left   J" in again.stdout
    assert "reset        H" in again.stdout


def test_cpp_keymap_rejects_unknown_action():
    if not EXE.exists():
        pytest.skip("dino_viewer not built (run scripts/build.ps1)")
    result = subprocess.run(
        [str(EXE), "--rebind", "no_such_action=J", "--list-keys"],
        capture_output=True, text=True, timeout=60,
    )
    assert result.returncode == 2
    assert "unknown action" in result.stderr


def test_cpp_wav_validate():
    wav = GAME / "Data" / "Sound" / "Actors" / "Dinos" / "avoidance.wav"
    if not EXE.exists():
        pytest.skip("dino_viewer not built (run scripts/build.ps1)")
    if not wav.exists():
        pytest.skip("retail sample not present")
    result = subprocess.run(
        [str(EXE), "--validate", "--wav", str(wav)],
        capture_output=True, text=True, timeout=60,
    )
    assert result.returncode == 0, result.stderr
    assert "44100 Hz" in result.stdout
    assert "1 ch" in result.stdout
    assert "16-bit" in result.stdout
    assert "4410 frames" in result.stdout
    assert "0.100s" in result.stdout


def test_cpp_wav_window(tmp_path):
    wav = GAME / "Data" / "Sound" / "Actors" / "Dinos" / "avoidance.wav"
    if not EXE.exists():
        pytest.skip("dino_viewer not built (run scripts/build.ps1)")
    if not wav.exists():
        pytest.skip("retail sample not present")
    shot = tmp_path / "audio.bmp"
    result = subprocess.run(
        [str(EXE), "--frames", "2", "--screenshot", str(shot),
         "--width", "320", "--height", "240", "--no-vsync", "--no-save-config",
         "--mute", "--wav", str(wav)],
        capture_output=True, text=True, timeout=60,
    )
    assert result.returncode == 0, result.stderr
    assert shot.exists()
    assert Image.open(shot).size == (320, 240)


def test_cpp_lnd_validate():
    if not EXE.exists():
        pytest.skip("dino_viewer not built (run scripts/build.ps1)")
    if not ISL01.exists():
        pytest.skip("retail sample not present")
    result = subprocess.run(
        [str(EXE), "--validate", "--lnd", str(ISL01), "--asset-root", str(GAME)],
        capture_output=True, text=True, timeout=120,
    )
    assert result.returncode == 0, result.stderr
    assert "520x376" in result.stdout
    assert "terrain: 512x512" in result.stdout
    assert "262144 vertices" in result.stdout
    assert "522242 triangles" in result.stdout
    assert "vegetation=25" in result.stdout
    assert "vegetation: 9 groups, 1536 instances, 52291 triangles" in result.stdout


def test_cpp_lnd_no_vegetation():
    if not EXE.exists():
        pytest.skip("dino_viewer not built (run scripts/build.ps1)")
    if not ISL01.exists():
        pytest.skip("retail sample not present")
    result = subprocess.run(
        [str(EXE), "--validate", "--lnd", str(ISL01), "--asset-root", str(GAME),
         "--no-vegetation"],
        capture_output=True, text=True, timeout=120,
    )
    assert result.returncode == 0, result.stderr
    assert "vegetation:" not in result.stdout


def test_cpp_terrain_window(tmp_path):
    if not EXE.exists():
        pytest.skip("dino_viewer not built (run scripts/build.ps1)")
    if not ISL01.exists():
        pytest.skip("retail sample not present")
    shot = tmp_path / "isl01.bmp"
    result = subprocess.run(
        [str(EXE), "--frames", "2", "--screenshot", str(shot),
         "--width", "400", "--height", "300", "--no-vsync", "--no-save-config",
         "--lnd", str(ISL01), "--asset-root", str(GAME),
         "--yaw", "35", "--pitch", "35"],
        capture_output=True, text=True, timeout=120,
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
    assert foreground > 1000

def test_cpp_skybox_and_water():
    if not EXE.exists():
        pytest.skip("dino_viewer not built (run scripts/build.ps1)")
    if not ISL01.exists():
        pytest.skip("retail sample not present")
    result = subprocess.run(
        [str(EXE), "--validate", "--lnd", str(ISL01), "--asset-root", str(GAME),
         "--no-vegetation"],
        capture_output=True, text=True, timeout=120,
    )
    assert result.returncode == 0, result.stderr
    assert "environment: skybox 6/6 faces" in result.stdout
    assert "water yes at z=16.0 (256x256)" in result.stdout

def _path_loop_args():
    args = []
    for x in range(126, 133):
        args += ["--place-path", f"{x},126", "--place-path", f"{x},132"]
    for y in range(127, 132):
        args += ["--place-path", f"126,{y}", "--place-path", f"132,{y}"]
    return args


def test_cpp_visitors_validate():
    if not EXE.exists():
        pytest.skip("dino_viewer not built (run scripts/build.ps1)")
    if not ISL01.exists():
        pytest.skip("retail sample not present")
    result = subprocess.run(
        [str(EXE), "--validate", "--lnd", str(ISL01), "--asset-root", str(GAME),
         "--no-vegetation", "--build", "--visitors", "8", *_path_loop_args()],
        capture_output=True, text=True, timeout=120,
    )
    assert result.returncode == 0, result.stderr
    assert "visitors: 8 spawned on 24 path cells" in result.stdout
    assert "build: cell 4.0, grid 256x256, 24 path cells" in result.stdout
    assert "visitors: 8" in result.stdout

def test_cpp_selftest():
    if not EXE.exists():
        pytest.skip("dino_viewer not built (run scripts/build.ps1)")
    result = subprocess.run(
        [str(EXE), "--selftest"], capture_output=True, text=True, timeout=60,
    )
    assert result.returncode == 0, result.stdout + result.stderr
    assert "selftest: PASS (18 checks)" in result.stdout

def test_cpp_hud(tmp_path):
    if not EXE.exists():
        pytest.skip("dino_viewer not built (run scripts/build.ps1)")
    if not ISL01.exists():
        pytest.skip("retail sample not present")
    validate = subprocess.run(
        [str(EXE), "--validate", "--lnd", str(ISL01), "--asset-root", str(GAME),
         "--no-vegetation"],
        capture_output=True, text=True, timeout=120,
    )
    assert validate.returncode == 0, validate.stderr
    assert "hud font" in validate.stdout and "245 glyphs" in validate.stdout
    shot = tmp_path / "hud.bmp"
    result = subprocess.run(
        [str(EXE), "--frames", "2", "--screenshot", str(shot),
         "--width", "600", "--height", "400", "--no-vsync", "--no-save-config",
         "--lnd", str(ISL01), "--asset-root", str(GAME), "--no-vegetation",
         "--yaw", "35", "--pitch", "18", "--distance", "380"],
        capture_output=True, text=True, timeout=120,
    )
    assert result.returncode == 0, result.stderr
    image = Image.open(shot).convert("RGB")
    corner = image.crop((0, 0, 240, 40))
    bright = sum(1 for p in corner.get_flattened_data()
                 if p[0] > 200 and p[1] > 200 and p[2] > 200)
    assert bright > 50

def _ui_shot(tmp_path, extra, size=(600, 400)):
    shot = tmp_path / "ui.bmp"
    result = subprocess.run(
        [str(EXE), "--frames", "2", "--screenshot", str(shot),
         "--width", str(size[0]), "--height", str(size[1]),
         "--no-vsync", "--no-save-config", "--lnd", str(ISL01),
         "--asset-root", str(GAME), "--no-vegetation",
         "--yaw", "35", "--pitch", "18", "--distance", "380", *extra],
        capture_output=True, text=True, timeout=120,
    )
    assert result.returncode == 0, result.stderr
    return Image.open(shot).convert("RGB")


def test_cpp_start_menu(tmp_path):
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    image = _ui_shot(tmp_path, ["--menu"])
    band = image.crop((100, 110, 500, 170))
    bright = sum(1 for p in band.get_flattened_data()
                 if p[0] > 180 and p[1] > 180 and p[2] > 170)
    assert bright > 200


def test_cpp_help_panel(tmp_path):
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    image = _ui_shot(tmp_path, ["--help-panel"])
    panel = image.crop((50, 70, 400, 180))
    bright = sum(1 for p in panel.get_flattened_data()
                 if p[0] > 180 and p[1] > 180 and p[2] > 170)
    assert bright > 150

def test_cpp_retail_menu_load():
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    result = subprocess.run(
        [str(EXE), "--validate", "--lnd", str(ISL01), "--asset-root", str(GAME),
         "--no-vegetation"],
        capture_output=True, text=True, timeout=120,
    )
    assert result.returncode == 0, result.stderr
    assert "menu: loaded=1 widgets=114 pages=9 graphics=53" in result.stdout
    assert "menu page=0 active=1 island=0 difficulty=1" in result.stdout


def test_cpp_retail_menu_pages():
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    for page in (0, 1, 2, 4, 5, 7, 8):
        result = subprocess.run(
            [str(EXE), "--validate", "--lnd", str(ISL01),
             "--asset-root", str(GAME), "--no-vegetation",
             "--menu-page", str(page)],
            capture_output=True, text=True, timeout=120,
        )
        assert result.returncode == 0, result.stderr
        assert f"menu page={page} active=1" in result.stdout


def test_cpp_retail_menu_clicks():
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    free = subprocess.run(
        [str(EXE), "--validate", "--lnd", str(ISL01), "--asset-root", str(GAME),
         "--no-vegetation", "--menu-click", "395,234"],
        capture_output=True, text=True, timeout=120,
    )
    assert free.returncode == 0, free.stderr
    assert "menu page=1 active=1" in free.stdout

    start = subprocess.run(
        [str(EXE), "--validate", "--lnd", str(ISL01), "--asset-root", str(GAME),
         "--no-vegetation", "--menu-click", "395,234",
         "--menu-click", "526,531"],
        capture_output=True, text=True, timeout=120,
    )
    assert start.returncode == 0, start.stderr
    assert "menu page=8 active=1" in start.stdout


def test_cpp_retail_menu_loading_handoff(tmp_path):
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    shot = tmp_path / "park.bmp"
    result = subprocess.run(
        [str(EXE), "--frames", "5", "--screenshot", str(shot),
         "--width", "800", "--height", "600", "--no-vsync", "--no-save-config",
         "--lnd", str(ISL01), "--asset-root", str(GAME), "--no-vegetation",
         "--menu-loading-time", "0", "--menu-click", "395,234",
         "--menu-click", "526,531"],
        capture_output=True, text=True, timeout=120,
    )
    assert result.returncode == 0, result.stderr
    image = Image.open(shot).convert("RGB")
    # Loading page is white-backed art; the park starts with sky/terrain.
    r, g, b = image.getpixel((10, 10))
    assert not (r > 235 and g > 235 and b > 235)


def test_cpp_retail_menu_shot(tmp_path):
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    shot = tmp_path / "menu.bmp"
    result = subprocess.run(
        [str(EXE), "--frames", "2", "--menu", "--screenshot", str(shot),
         "--width", "800", "--height", "600", "--no-vsync", "--no-save-config",
         "--lnd", str(ISL01), "--asset-root", str(GAME), "--no-vegetation"],
        capture_output=True, text=True, timeout=120,
    )
    assert result.returncode == 0, result.stderr
    image = Image.open(shot).convert("RGB")
    assert image.size == (800, 600)
    # Normal buttons have no frame-4 bar; this is the background screen.
    r, g, b = image.getpixel((400, 235))
    assert r < 30 and abs(g - 83) < 30 and abs(b - 117) < 30
    # The menu art is not a flat scene.
    assert len(image.getcolors(maxcolors=1_000_000)) > 500


def test_cpp_retail_menu_hover(tmp_path):
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    shot = tmp_path / "hover.bmp"
    result = subprocess.run(
        [str(EXE), "--frames", "2", "--menu", "--screenshot", str(shot),
         "--width", "800", "--height", "600", "--no-vsync", "--no-save-config",
         "--lnd", str(ISL01), "--asset-root", str(GAME), "--no-vegetation",
         "--menu-hover", "400,235"],
        capture_output=True, text=True, timeout=120,
    )
    assert result.returncode == 0, result.stderr
    r, g, b = Image.open(shot).convert("RGB").getpixel((400, 235))
    # Hover frame 5 of MAIN_MENU_BUTTON.
    assert r < 30 and abs(g - 113) < 30 and abs(b - 149) < 30


def test_cpp_retail_menu_free_rows(tmp_path):
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    shot = tmp_path / "free.bmp"
    result = subprocess.run(
        [str(EXE), "--frames", "2", "--menu-page", "1", "--screenshot", str(shot),
         "--width", "800", "--height", "600", "--no-vsync", "--no-save-config",
         "--lnd", str(ISL01), "--asset-root", str(GAME), "--no-vegetation"],
        capture_output=True, text=True, timeout=120,
    )
    assert result.returncode == 0, result.stderr
    image = Image.open(shot).convert("RGB")
    # ISLAND_LIST normal row frame 5 is a blue strip.
    r, g, b = image.getpixel((210, 220))
    assert r < 40 and g > 50 and b > 90

def test_cpp_park_hud_load():
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    result = subprocess.run(
        [str(EXE), "--validate", "--lnd", str(ISL01), "--asset-root", str(GAME),
         "--no-vegetation"],
        capture_output=True, text=True, timeout=120,
    )
    assert result.returncode == 0, result.stderr
    assert "park hud: loaded=1 widgets=2694 roots=38 visible=53" in result.stdout
    disabled = subprocess.run(
        [str(EXE), "--validate", "--lnd", str(ISL01), "--asset-root", str(GAME),
         "--no-vegetation", "--no-park-hud"],
        capture_output=True, text=True, timeout=120,
    )
    assert disabled.returncode == 0, disabled.stderr
    assert "park hud:" not in disabled.stdout


def test_cpp_park_hud_shot(tmp_path):
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    shot = tmp_path / "park-hud.bmp"
    result = subprocess.run(
        [str(EXE), "--frames", "2", "--no-menu", "--screenshot", str(shot),
         "--width", "800", "--height", "600", "--no-vsync", "--no-save-config",
         "--lnd", str(ISL01), "--asset-root", str(GAME), "--no-vegetation",
         "--distance", "500"],
        capture_output=True, text=True, timeout=120,
    )
    assert result.returncode == 0, result.stderr
    image = Image.open(shot).convert("RGB")
    assert image.size == (800, 600)
    right = image.crop((707, 0, 800, 417))
    gauge = image.crop((69, 569, 687, 600))
    assert len(right.getcolors(maxcolors=1_000_000)) > 200
    assert len(gauge.getcolors(maxcolors=1_000_000)) > 100
    # The budget readout area is a UI panel, not the terrain.
    r, g, b = image.getpixel((120, 585))
    assert not (r < 40 and g > 100 and b < 80)

def test_cpp_park_panels():
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    result = subprocess.run(
        [str(EXE), "--validate", "--lnd", str(ISL01), "--asset-root", str(GAME),
         "--no-vegetation",
         "--park-click", "97,584", "--park-click", "429,482",
         "--park-click", "750,120", "--park-click", "648,145",
         "--park-click", "750,120", "--park-click", "678,145",
         "--park-click", "750,120", "--park-click", "648,145",
         "--park-click", "84,527",
         "--park-click", "492,584", "--park-click", "492,584",
         "--park-click", "302,584", "--park-click", "302,584",
         "--park-click", "765,348", "--park-click", "765,298",
         "--park-click", "757,46", "--park-click", "261,513",
         "--park-click", "489,565"],
        capture_output=True, text=True, timeout=120,
    )
    assert result.returncode == 0, result.stderr
    log = result.stdout
    assert "park click 97,584 -> open BUDGET_PANEL" in log
    assert "park click 429,482 -> close BUDGET_PANEL" in log
    assert "park click 750,120 -> open CONSTRUCTION_PANEL" in log
    assert "park click 648,145 -> open CONSTRUCTION_KIT_PANEL" in log
    assert "park click 678,145 -> open PREDEFINED_SHOW_PANEL" in log
    assert "park click 84,527 -> open ATTRACTION_STATS_PANEL" in log
    assert "park click 492,584 -> open FENCE_LIST_PANEL" in log
    assert "park click 492,584 -> close FENCE_LIST_PANEL" in log
    assert "park click 302,584 -> open DINO_LIST_PANEL" in log
    assert "park click 302,584 -> close DINO_LIST_PANEL" in log
    assert "park click 765,348 -> open OPTIONS_PANEL" in log
    assert "park click 765,298 -> open DINO_ACHAT_SAVE_PANEL" in log
    assert "park click 757,46 -> open DINO_LAB_MAIN_CENTER" in log
    assert "park click 261,513 -> open LAB_BREED_CENTER" in log
    assert "park click 489,565 -> close lab" in log
    # Every panel is closed again; the base park HUD is back.
    assert "park hud: loaded=1 widgets=2694 roots=38 visible=53" in log


def test_cpp_park_panel_shot(tmp_path):
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    base = tmp_path / "base.bmp"
    budget = tmp_path / "budget.bmp"
    closed = tmp_path / "closed.bmp"
    common = [str(EXE), "--frames", "2", "--no-menu", "--width", "800",
              "--height", "600", "--no-vsync", "--no-save-config", "--no-hud",
              "--lnd", str(ISL01), "--asset-root", str(GAME), "--no-vegetation",
              "--distance", "500"]
    for shot, extra in (
        (base, []),
        (budget, ["--park-click", "97,584"]),
        (closed, ["--park-click", "97,584", "--park-click", "429,482"]),
    ):
        result = subprocess.run(
            common + ["--screenshot", str(shot)] + extra,
            capture_output=True, text=True, timeout=120,
        )
        assert result.returncode == 0, result.stderr
    a = Image.open(base).convert("RGB")
    b = Image.open(budget).convert("RGB")
    c = Image.open(closed).convert("RGB")
    # The info panels do not dim the scene (DIALOG_BACKGROUND is only used by
    # warning/request/campaign-end dialogs).
    assert a.getpixel((50, 300)) == b.getpixel((50, 300))
    # The budget ledger covers the centre; closing restores the frame exactly.
    assert b.getpixel((300, 300)) != a.getpixel((300, 300))
    # The category number widgets show a value (a fresh park has zeros).
    box = b.crop((150, 107, 186, 120)).tobytes()
    bright = sum(
        1
        for i in range(0, len(box), 3)
        if box[i] > 200 and box[i + 1] > 200 and box[i + 2] > 200
    )
    assert bright > 5
    assert ImageChops.difference(a, c).getbbox() is None


def test_cpp_park_lab_shot(tmp_path):
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    base = tmp_path / "base.bmp"
    lab = tmp_path / "lab.bmp"
    common = [str(EXE), "--frames", "2", "--no-menu", "--width", "800",
              "--height", "600", "--no-vsync", "--no-save-config",
              "--lnd", str(ISL01), "--asset-root", str(GAME), "--no-vegetation",
              "--distance", "500"]
    for shot, extra in (
        (base, []),
        (lab, ["--park-click", "757,46", "--park-click", "261,513"]),
    ):
        result = subprocess.run(
            common + ["--screenshot", str(shot)] + extra,
            capture_output=True, text=True, timeout=120,
        )
        assert result.returncode == 0, result.stderr
    a = Image.open(base).convert("RGB")
    b = Image.open(lab).convert("RGB")
    # The breed centre has its own 800x600 lab backdrop and hides the HUD.
    assert a.getpixel((400, 100)) != b.getpixel((400, 100))
    assert a.crop((69, 569, 687, 600)).tobytes() != b.crop((69, 569, 687, 600)).tobytes()


def test_cpp_park_hover_shot(tmp_path):
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    base = tmp_path / "base.bmp"
    hover = tmp_path / "hover.bmp"
    common = [str(EXE), "--frames", "2", "--no-menu", "--width", "800",
              "--height", "600", "--no-vsync", "--no-save-config",
              "--lnd", str(ISL01), "--asset-root", str(GAME), "--no-vegetation",
              "--distance", "500"]
    for shot, extra in (
        (base, []),
        (hover, ["--park-hover", "97,584"]),
    ):
        result = subprocess.run(
            common + ["--screenshot", str(shot)] + extra,
            capture_output=True, text=True, timeout=120,
        )
        assert result.returncode == 0, result.stderr
    a = Image.open(base).convert("RGB")
    b = Image.open(hover).convert("RGB")
    box = (80, 567, 116, 602)
    changed = ImageChops.difference(a.crop(box), b.crop(box)).getbbox()
    assert changed is not None
    assert (changed[2] - changed[0]) * (changed[3] - changed[1]) > 100

def test_cpp_island_list_and_free_game_budgets():
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    result = subprocess.run(
        [str(EXE), "--validate", "--asset-root", str(GAME), "--no-vegetation"],
        capture_output=True, text=True, timeout=120,
    )
    assert result.returncode == 0, result.stderr
    assert "islands: 10 isl01..isl10" in result.stdout
    assert "free game budgets: 75000/50000/25000" in result.stdout


def _menu_start(shot, row, difficulty):
    # ISLAND_LIST is 188x285 at 175,166 -> 28-px rows; difficulty buttons are
    # 52 px wide at x = 494/568/642, y = 451.
    island_y = 166 + row * 28 + 14
    difficulty_x = 494 + difficulty * 74
    return subprocess.run(
        [str(EXE), "--frames", "4", "--screenshot", str(shot),
         "--width", "800", "--height", "600", "--no-vsync", "--no-save-config",
         "--no-hud", "--asset-root", str(GAME),
         "--menu-page", "1",
         "--menu-click", f"200,{island_y}",
         "--menu-click", f"{difficulty_x},451",
         "--menu-click", "526,531",
         "--menu-loading-time", "0"],
        capture_output=True, text=True, timeout=180,
    )


def test_cpp_menu_start_loads_selected_island(tmp_path):
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    first = tmp_path / "isl01.bmp"
    result = _menu_start(first, row=0, difficulty=0)
    assert result.returncode == 0, result.stderr
    assert "island: isl01" in result.stdout
    assert "start: island=isl01 difficulty=0 budget=75000" in result.stdout
    image = Image.open(first).convert("RGB")
    assert image.size == (800, 600)
    # The park HUD (not just the terrain) is present after the handoff.
    assert len(image.crop((707, 0, 800, 417)).getcolors(maxcolors=1_000_000)) > 200
    assert len(image.crop((69, 569, 687, 600)).getcolors(maxcolors=1_000_000)) > 100

    last = tmp_path / "isl10.bmp"
    result = _menu_start(last, row=9, difficulty=2)
    assert result.returncode == 0, result.stderr
    assert "island: isl10" in result.stdout
    assert "start: island=isl10 difficulty=2 budget=25000" in result.stdout
    a = Image.open(first).convert("RGB")
    b = Image.open(last).convert("RGB")
    # Different island: the rendered scene must change.
    assert ImageChops.difference(a, b).getbbox() is not None

def test_cpp_minimap_detail_view(tmp_path):
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    base = tmp_path / "map.bmp"
    zoom = tmp_path / "map-zoom.bmp"
    common = [str(EXE), "--frames", "3", "--no-menu", "--width", "800",
              "--height", "600", "--no-vsync", "--no-save-config", "--no-hud",
              "--lnd", str(ISL01), "--asset-root", str(GAME),
              "--yaw", "35", "--pitch", "22", "--distance", "500"]
    result = subprocess.run(common + ["--screenshot", str(base)],
                            capture_output=True, text=True, timeout=180)
    assert result.returncode == 0, result.stderr
    image = Image.open(base).convert("RGB")
    map_area = image.crop((652, 451, 788, 587))
    # The detail map is a rendered island, not the flat clear colour.
    assert len(map_area.getcolors(maxcolors=1_000_000)) > 500
    reds = [
        map_area.getpixel((x, y)) for y in range(136) for x in range(136)
        if map_area.getpixel((x, y))[0] > 180 and map_area.getpixel((x, y))[1] < 80
    ]
    assert len(reds) > 5  # camera marker

    result = subprocess.run(
        common + ["--screenshot", str(zoom), "--park-click", "633,552"],
        capture_output=True, text=True, timeout=180)
    assert result.returncode == 0, result.stderr
    assert "park click 633,552 -> minimap zoom in" in result.stdout
    assert "minimap scale=0.20" in result.stdout
    zoomed = Image.open(zoom).convert("RGB").crop((652, 451, 788, 587))
    diff = ImageChops.difference(map_area, zoomed).getbbox()
    assert diff is not None
    assert (diff[2] - diff[0]) * (diff[3] - diff[1]) > 1000

def test_cpp_park_hud_1024_anchor(tmp_path):
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    shot = tmp_path / "hud-1024.bmp"
    result = subprocess.run(
        [str(EXE), "--frames", "3", "--no-menu", "--screenshot", str(shot),
         "--width", "1024", "--height", "768", "--no-vsync", "--no-save-config",
         "--no-hud", "--lnd", str(ISL01), "--asset-root", str(GAME),
         "--no-vegetation", "--distance", "500"],
        capture_output=True, text=True, timeout=180,
    )
    assert result.returncode == 0, result.stderr
    image = Image.open(shot).convert("RGB")
    assert image.size == (1024, 768)
    # Retail re-anchor: right panel at 931,168; gauge at 293,737; map at
    # 840,584. The top-right corner is empty and the old 800x600 positions
    # are scene pixels.
    right = image.crop((931, 168, 1024, 585))
    assert len(right.getcolors(maxcolors=1_000_000)) > 200
    gauge = image.crop((293, 737, 911, 768))
    assert len(gauge.getcolors(maxcolors=1_000_000)) > 100
    map_area = image.crop((876, 619, 1012, 755))
    assert len(map_area.getcolors(maxcolors=1_000_000)) > 500
    # Top-right at the 800x600 panel position is no longer the panel.
    assert image.getpixel((950, 100)) != image.getpixel((950, 250))
    # Gauge starts at x=293, not x=69.
    assert image.getpixel((280, 750)) != image.getpixel((300, 750))

def test_cpp_park_clock():
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    for time, scale in (("09:44", "60"), ("12:05", "50"), ("18:59", "60")):
        result = subprocess.run(
            [str(EXE), "--validate", "--lnd", str(ISL01), "--asset-root", str(GAME),
             "--no-vegetation", "--park-time", time,
             "--park-time-scale", scale],
            capture_output=True, text=True, timeout=120,
        )
        assert result.returncode == 0, result.stderr
        assert f"park clock: scale={scale}.0 base=9 time={time} day=1" in result.stdout

    # One game hour at scale 60 is 60 real seconds.
    advanced = subprocess.run(
        [str(EXE), "--validate", "--lnd", str(ISL01), "--asset-root", str(GAME),
         "--no-vegetation", "--park-time", "10:00", "--park-advance", "60"],
        capture_output=True, text=True, timeout=120,
    )
    assert advanced.returncode == 0, advanced.stderr
    assert "park clock: scale=60.0 base=9 time=11:00 day=1" in advanced.stdout
    # 19:59 + 2 s crosses the 11-hour day boundary and rolls to day 2, 09:00.
    rolled = subprocess.run(
        [str(EXE), "--validate", "--lnd", str(ISL01), "--asset-root", str(GAME),
         "--no-vegetation", "--park-time", "19:59", "--park-advance", "2"],
        capture_output=True, text=True, timeout=120,
    )
    assert rolled.returncode == 0, rolled.stderr
    assert "park clock: scale=60.0 base=9 time=09:00 day=2" in rolled.stdout

def test_cpp_pachy_lod2_mesh_bind():
    if not EXE.exists() or not (GAME / "Data/Graphics/Actors/Dinos/pachy").exists():
        pytest.skip("viewer or retail data missing")
    pachy = GAME / "Data/Graphics/Actors/Dinos/pachy"
    common = [str(EXE), "--validate", "--asset-root", str(GAME),
              "--mesh", str(pachy / "pachy_lod2.mesh"),
              "--skel", str(pachy / "pachy.skel"),
              "--skin", str(pachy / "pachy_lod2.skin")]
    mesh = subprocess.run(common + ["--skin-bind", "mesh"],
                          capture_output=True, text=True, timeout=120)
    assert mesh.returncode == 0, mesh.stderr
    match = re.search(r"skin rest max error: ([0-9.]+) cm", mesh.stdout)
    assert match is not None
    # The standing .mesh bind must be exact at rest for pachy LOD2.
    assert float(match.group(1)) < 0.01

    skin = subprocess.run(common + ["--skin-bind", "skin"],
                          capture_output=True, text=True, timeout=120)
    assert skin.returncode == 0, skin.stderr
    match = re.search(r"skin rest max error: ([0-9.]+) cm", skin.stdout)
    assert match is not None
    # The .skin full mesh is the crouched/bent-neck bind pose.
    assert float(match.group(1)) > 1.0

def test_cpp_menu_start_with_dino(tmp_path):
    if not EXE.exists() or not (GAME / "Data/Graphics/Actors/Dinos/allo").exists():
        pytest.skip("viewer or retail data missing")
    base = tmp_path / "base.bmp"
    dino_shot = tmp_path / "dino.bmp"
    common = [str(EXE), "--frames", "5", "--width", "800", "--height", "600",
              "--no-vsync", "--no-save-config", "--no-hud",
              "--asset-root", str(GAME), "--menu-page", "1",
              "--menu-click", "200,180", "--menu-click", "494,451",
              "--menu-click", "526,531", "--menu-loading-time", "0"]
    a = subprocess.run(common + ["--screenshot", str(base)],
                       capture_output=True, text=True, timeout=180)
    assert a.returncode == 0, a.stderr
    b = subprocess.run(common + ["--screenshot", str(dino_shot), "--start-dino"],
                       capture_output=True, text=True, timeout=180)
    assert b.returncode == 0, b.stderr
    assert "start: island=isl01 difficulty=0 budget=75000" in b.stdout
    assert "spawn: x=512.0 y=512.0" in b.stdout
    first = Image.open(base).convert("RGB")
    second = Image.open(dino_shot).convert("RGB")
    # The dinosaur fills the centre of the close-up start camera.
    region = (220, 100, 620, 460)
    diff = ImageChops.difference(first.crop(region), second.crop(region))
    stat = ImageStat.Stat(diff)
    assert sum(stat.mean) / 3 > 5.0

def test_cpp_menu_sounds():
    if not EXE.exists() or not GAME.exists():
        pytest.skip("viewer or retail data missing")
    listed = subprocess.run(
        [str(EXE), "--list-sounds", "--asset-root", str(GAME)],
        capture_output=True, text=True, timeout=120,
    )
    assert listed.returncode == 0, listed.stderr
    assert "sound clicDefault: data/Sound/Interface/clic#3.wav" in listed.stdout
    assert "sound exitClic: data/Sound/Interface/clic#9.wav" in listed.stdout

    played = subprocess.run(
        [str(EXE), "--frames", "2", "--no-menu", "--width", "320",
         "--height", "200", "--no-vsync", "--no-save-config",
         "--asset-root", str(GAME), "--lnd", str(ISL01), "--no-vegetation",
         "--play-sound", "clicDefault"],
        capture_output=True, text=True, timeout=120,
    )
    assert played.returncode == 0, played.stderr
    assert "sound: " in played.stdout and "clic#3.wav" in played.stdout
