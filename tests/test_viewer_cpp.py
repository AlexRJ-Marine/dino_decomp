import os
import re
import shutil
import struct
import subprocess
import sys
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

# Every test in this module launches the viewer; `scripts/test.ps1 -Fast`
# skips them via `-m "not slow"`.
pytestmark = pytest.mark.slow


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


def test_cpp_camera_retail_progress_default():
    """The park camera defaults to the retail ctor state: p=0.1, yaw=0,
    target (0,0,terrain), r=39, h=52.7, pitch=53.5, no roll."""
    if not EXE.exists():
        pytest.skip("dino_viewer not built (run scripts/build.ps1)")
    if not ISL01.exists():
        pytest.skip("retail isl01 missing")
    env = os.environ.copy()
    env["DINO_DEBUG_CAMERA"] = "1"
    result = subprocess.run(
        [str(EXE), "--frames", "31", "--no-menu", "--width", "320",
         "--height", "240", "--no-vsync", "--no-save-config",
         "--lnd", str(ISL01), "--asset-root", str(GAME),
         "--no-vegetation", "--no-water"],
        capture_output=True, text=True, timeout=120, env=env,
    )
    assert result.returncode == 0, result.stderr
    assert ("target=0.0,0.0,32.0 yaw=0.0 pitch=53.5 dist=65.6 "
            "p=0.100 r=39.0 h=52.7") in result.stderr, result.stderr
    assert "up=0,0,1" in result.stderr, result.stderr


def test_cpp_camera_retail_selftest():
    """Exact 0x5e5ff0 equations: per-update pan, dt-scaled yaw/zoom, filters.

    Expected values are derived from the binary and the live retail probe
    (run-test/dino_camera.csv): pan = lerp(1,20,0.1) = 2.9 per update;
    yaw 90 deg/s with a 10/s low-pass (lag a*(1-k)/k = 0.1309 rad);
    progress speed lerp(0.05,0.2,p)."""
    if not EXE.exists():
        pytest.skip("dino_viewer not built (run scripts/build.ps1)")
    if not ISL01.exists():
        pytest.skip("retail isl01 missing")
    result = subprocess.run(
        [str(EXE), "--frames", "10", "--no-menu", "--width", "320",
         "--height", "240", "--no-vsync", "--no-save-config",
         "--lnd", str(ISL01), "--asset-root", str(GAME),
         "--no-vegetation", "--no-water", "--camera-selftest"],
        capture_output=True, text=True, timeout=120,
    )
    assert result.returncode == 0, result.stderr
    out = result.stdout
    assert "camera selftest settle target=0.0000,0.0000,32.0000" in out, out
    assert "camera selftest pan dx=-29.0000" in out, out
    assert "step=2.9000" in out, out
    # Live probe: the pan is rigid. The eye goal moves with the target goal
    # (same delta), the eye-target offset stays at the progress radius, and
    # yaw does not change during a Left/Right pan.
    assert "camera selftest rigid eye_dx=-29.0000" in out, out
    assert "offset=39.0000" in out, out
    assert "camera selftest pan_yaw delta=0.000000" in out, out
    assert "camera selftest yaw goal_delta=1.570795" in out, out
    assert "camera selftest yaw goal_delta=1.570795 cur_delta=1.284372" in out, out
    assert "camera selftest zoom delta=-0.060430 p=0.039570" in out, out


def test_cpp_camera_orbit_option_keeps_legacy_view():
    if not EXE.exists():
        pytest.skip("dino_viewer not built (run scripts/build.ps1)")
    if not ISL01.exists():
        pytest.skip("retail isl01 missing")
    env = os.environ.copy()
    env["DINO_DEBUG_CAMERA"] = "1"
    result = subprocess.run(
        [str(EXE), "--frames", "31", "--no-menu", "--width", "320",
         "--height", "240", "--no-vsync", "--no-save-config",
         "--lnd", str(ISL01), "--asset-root", str(GAME),
         "--no-vegetation", "--no-water", "--camera-orbit"],
        capture_output=True, text=True, timeout=120, env=env,
    )
    assert result.returncode == 0, result.stderr
    assert "yaw=35.0 pitch=20.0" in result.stderr, result.stderr
    assert "p=-1.000" in result.stderr, result.stderr


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


def test_cpp_occupation_mask_blocks_placement():
    if not EXE.exists():
        pytest.skip("dino_viewer not built (run scripts/build.ps1)")
    if not ISL01.exists():
        pytest.skip("retail sample not present")
    # isl01 `_occupation.raw` (1024x1024) is stored bottom-up, like the
    # heightmap: engine cell (x,y) reads file row (1023-y). Cell (130,94)
    # centre maps to mask (522,378) -> file (522,645), still inside the
    # blocked Entrance footprint (255). (128,128) is free in both rows.
    result = subprocess.run(
        [str(EXE), "--validate", "--lnd", str(ISL01), "--asset-root", str(GAME),
         "--no-vegetation", "--build", "--place-path", "130,94",
         "--place-path", "128,128"],
        capture_output=True, text=True, timeout=120,
    )
    assert result.returncode == 0, result.stderr
    assert "place-path: blocked cell 130,94" in result.stderr
    assert "1 path cells, 0 fences" in result.stdout

    # East edge (edge 1) is shared with (x+1,y), not (x-1,y). In engine
    # orientation cell (158,90) has a blocked east neighbour (159,90);
    # (157,90) is free with a free east neighbour.
    fence = subprocess.run(
        [str(EXE), "--validate", "--lnd", str(ISL01), "--asset-root", str(GAME),
         "--no-vegetation", "--build", "--place-fence", "158,90,1",
         "--place-fence", "157,90,1"],
        capture_output=True, text=True, timeout=120,
    )
    assert fence.returncode == 0, fence.stderr
    assert "place-fence: blocked cell 158,90" in fence.stderr
    assert "1 fences" in fence.stdout


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
    assert "water2 256x256" in result.stdout


def test_cpp_skybox_follows_camera(tmp_path):
    """Retail 0x00600df0 centres the sky cube on the eye (no bottom/side seam).

    Looking 60 degrees down only the bottom face is visible, so no corner may
    show the light-blue horizon band; looking up, the sky must not change with
    the orbit distance. The old origin-centred cube failed both
    (notes/skybox-seam-ab.png).
    """
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail isl01 missing")
    common = [str(EXE), "--frames", "2", "--no-menu", "--no-hud",
              "--no-park-hud", "--width", "320", "--height", "240",
              "--no-vsync", "--no-save-config", "--camera-orbit", "--lnd",
              str(ISL01), "--asset-root", str(GAME), "--no-vegetation",
              "--yaw", "45"]

    def shot(name, pitch, distance):
        path = tmp_path / name
        result = subprocess.run(
            common + ["--pitch", pitch, "--distance", distance,
                      "--screenshot", str(path)],
            capture_output=True, text=True, timeout=180)
        assert result.returncode == 0, result.stderr
        return Image.open(path).convert("RGB")

    down = shot("down.bmp", "60", "3000")
    for corner in ((4, 4), (315, 4), (4, 235), (315, 235)):
        r, g, _ = down.getpixel(corner)
        assert r < 30 and g < 30, (corner, down.getpixel(corner))

    near = shot("up-near.bmp", "-60", "1500")
    far = shot("up-far.bmp", "-60", "3000")
    strip = (0, 0, 320, 40)
    diff = ImageStat.Stat(
        ImageChops.difference(near.crop(strip), far.crop(strip))).mean
    assert max(diff) < 2.0, diff


def test_cpp_water_animation(tmp_path):
    """The two water layers scroll at the retail 0.03/0.01 UV/s rates."""
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail isl01 missing")
    common = [str(EXE), "--frames", "2", "--no-menu", "--no-hud",
              "--no-park-hud", "--width", "320", "--height", "240",
              "--no-vsync", "--no-save-config", "--lnd", str(ISL01),
              "--asset-root", str(GAME), "--no-vegetation", "--distance",
              "900"]
    t0 = tmp_path / "water-t0.bmp"
    t8 = tmp_path / "water-t8.bmp"
    a = subprocess.run(common + ["--water-time", "0", "--screenshot", str(t0)],
                       capture_output=True, text=True, timeout=180)
    b = subprocess.run(common + ["--water-time", "8", "--screenshot", str(t8)],
                       capture_output=True, text=True, timeout=180)
    assert a.returncode == 0, a.stderr
    assert b.returncode == 0, b.stderr
    image0 = Image.open(t0).convert("RGB")
    image8 = Image.open(t8).convert("RGB")
    assert ImageChops.difference(
        image0.crop((0, 0, 320, 60)),
        image8.crop((0, 0, 320, 60))).getbbox() is None
    zero = list(image0.crop((40, 150, 120, 220)).getdata())
    later = list(image8.crop((40, 150, 120, 220)).getdata())
    mean_diff = sum(
        abs(p[0] - q[0]) + abs(p[1] - q[1]) + abs(p[2] - q[2])
        for p, q in zip(zero, later)) / (3.0 * len(zero))
    assert mean_diff > 1.0, mean_diff


def test_cpp_water_modulate2x(tmp_path):
    """WaterSurface stage 1 Modulate2x (coordinate set 1, second TGA)."""
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail isl01 missing")
    common = [str(EXE), "--frames", "2", "--no-menu", "--no-hud",
              "--no-park-hud", "--width", "320", "--height", "240",
              "--no-vsync", "--no-save-config", "--lnd", str(ISL01),
              "--asset-root", str(GAME), "--no-vegetation", "--distance",
              "900"]
    on = tmp_path / "water2-on.bmp"
    off = tmp_path / "water2-off.bmp"
    a = subprocess.run(common + ["--screenshot", str(on)],
                       capture_output=True, text=True, timeout=180)
    b = subprocess.run(common + ["--no-water2", "--screenshot", str(off)],
                       capture_output=True, text=True, timeout=180)
    assert a.returncode == 0, a.stderr
    assert b.returncode == 0, b.stderr
    image_on = Image.open(on).convert("RGB")
    image_off = Image.open(off).convert("RGB")
    # Sky/background is untouched by the water stage.
    assert ImageChops.difference(
        image_on.crop((0, 0, 320, 60)),
        image_off.crop((0, 0, 320, 60))).getbbox() is None
    # The sea region changes measurably (the second layer modulates it).
    water_on = list(image_on.crop((40, 150, 120, 220)).getdata())
    water_off = list(image_off.crop((40, 150, 120, 220)).getdata())
    mean_diff = sum(
        abs(p[0] - q[0]) + abs(p[1] - q[1]) + abs(p[2] - q[2])
        for p, q in zip(water_on, water_off)) / (3.0 * len(water_on))
    assert mean_diff > 2.0, mean_diff

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


def test_cpp_daily_settlement():
    if not EXE.exists():
        pytest.skip("dino_viewer not built (run scripts/build.ps1)")
    if not ISL01.exists():
        pytest.skip("retail sample not present")
    result = subprocess.run(
        [str(EXE), "--frames", "2", "--width", "160", "--height", "120",
         "--lnd", str(ISL01),
         "--asset-root", str(GAME), "--no-vegetation", "--no-water",
         "--no-menu", "--no-hud", "--no-vsync", "--no-save-config",
         "--build", "--visitors", "8", "--settle-day",
         *_path_loop_args()],
        capture_output=True, text=True, timeout=120,
    )
    assert result.returncode == 0, result.stderr
    assert "visitors: 8 spawned on 24 path cells" in result.stdout
    assert ("daily settlement: day=0 visitors=8 ticket=50 income=525 "
            "wages=0 budget=75525") in result.stdout
    # 5 easy profiles pay 75 (scale 1.5) and 3 Kids pay 50 (scale 1.0);
    # ticket = round(entrance 50 * .bvi offset 0x34). Fixed spawn seed.


def test_cpp_sim2b_visitor_state_machine(tmp_path):
    if not EXE.exists():
        pytest.skip("dino_viewer not built (run scripts/build.ps1)")
    if not ISL01.exists():
        pytest.skip("retail sample not present")
    bbi = GAME / "Data" / "Design" / "buildings" / "RestCirc.bbi"
    if not bbi.exists():
        pytest.skip("RestCirc.bbi missing")
    build = tmp_path / "p1-build.txt"
    lines = ["cell_size 4", "size 256 256"]
    for x in range(118, 135):
        lines += [f"path {x} 118", f"path {x} 134"]
    for y in range(119, 134):
        lines += [f"path 118 {y}", f"path 134 {y}"]
    build.write_text("\n".join(lines) + "\n", encoding="ascii")
    result = subprocess.run(
        [str(EXE), "--lnd", str(ISL01), "--asset-root", str(GAME),
         "--build", "--build-load", str(build), "--visitors", "8",
         "--building", str(bbi), "--building-pos", "512,512",
         "--building-state", "0", "--frames", "2400", "--no-vegetation",
         "--no-water", "--visitor-stats", "--width", "320", "--height", "240",
         "--screenshot", str(tmp_path / "p1.bmp")],
        capture_output=True, text=True, timeout=180,
    )
    assert result.returncode == 0, result.stderr
    assert "visitors: 8 spawned on 64 path cells" in result.stdout
    assert "building: RestCirc" in result.stdout
    match = re.search(
        r"visitor stats: count=8 states=(\d+)/(\d+)/(\d+)/(\d+) "
        r"needs=([\d.]+)/([\d.]+)/([\d.]+)",
        result.stdout,
    )
    assert match, result.stdout
    states = [int(match.group(i)) for i in range(1, 5)]
    assert sum(states) == 8
    needs = [float(match.group(i)) for i in range(5, 8)]
    assert needs[0] < 99.0 or needs[1] < 99.0 or needs[2] < 99.0

def test_cpp_selftest():
    if not EXE.exists():
        pytest.skip("dino_viewer not built (run scripts/build.ps1)")
    result = subprocess.run(
        [str(EXE), "--selftest"], capture_output=True, text=True, timeout=60,
    )
    assert result.returncode == 0, result.stdout + result.stderr
    assert "selftest: PASS (170 checks)" in result.stdout

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


def test_cpp_retail_menu_load_game_page(tmp_path):
    """LOAD_GAME_NAMES_LIST must list save slots and LOAD_LOAD_BUTTON load one.

    Retail page 4 is the Load Game panel; the port used to render the
    ISLAND_LIST fallback there and ignored the OK button. The first slice
    lists `run/save` slots and loads a port `.psav` (which carries its own
    island). A retail `.sav` needs an island choice and stays out of scope.
    """
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    seed = tmp_path / "menu_seed.psav"
    made = subprocess.run(
        [str(EXE), "--frames", "2", "--no-menu", "--no-hud", "--width", "160",
         "--height", "120", "--no-vsync", "--no-save-config",
         "--lnd", str(ISL01), "--asset-root", str(GAME), "--no-vegetation",
         "--no-water", "--place-path", "128,128", "--save-port", str(seed)],
        capture_output=True, text=True, timeout=120,
    )
    assert made.returncode == 0, made.stderr
    assert "DINOPSAV 1" in seed.read_text()
    assert "island isl01" in seed.read_text()
    assert "path 128 128" in seed.read_text()

    save_dir = ROOT / "run" / "save"
    save_dir.mkdir(parents=True, exist_ok=True)
    target = save_dir / "00_menu_test.psav"
    target.write_bytes(seed.read_bytes())
    shot = tmp_path / "menu-load.bmp"
    try:
        result = subprocess.run(
            [str(EXE), "--frames", "4", "--menu", "--menu-page", "4",
             "--width", "800", "--height", "600", "--no-vsync",
             "--no-save-config", "--lnd", str(ISL01),
             "--asset-root", str(GAME), "--no-vegetation", "--no-water",
             "--menu-loading-time", "0", "--menu-click", "409,154",
             "--menu-click", "354,525", "--screenshot", str(shot)],
            capture_output=True, text=True, timeout=180,
        )
        assert result.returncode == 0, result.stderr
        assert "port load: island=isl01" in result.stdout, result.stdout
        assert "paths=1" in result.stdout, result.stdout
        # The loading page is gone: the shot is the park, not menu art.
        image = Image.open(shot).convert("RGB")
        assert image.getpixel((10, 10)) != (255, 255, 255)
    finally:
        if target.exists():
            target.unlink()


def test_cpp_retail_menu_cinematics_page(tmp_path):
    """CINEMATICS_LIST lists campaign ending videos; START plays the clip.

    Retail fills the list from the campaign database (`FUN_00518400`): one row
    per campaign, row text = `[BRIEFING] EndingVideoName`, and
    `CINEMATICS_START_BUTTON` hands the row to the app, which plays
    `data/video/` + `[BRIEFING] EndingVideo` (`FUN_00518360`, `0x4026fa`).
    The port used to render the island-list fallback on page 6.
    """
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    dino_dat = tmp_path / "dino.dat"
    # `dino.dat` is an unlocked-campaign count; 8 = every listed campaign.
    dino_dat.write_bytes((8).to_bytes(4, "little"))

    def run(extra, dat=None):
        return subprocess.run(
            [str(EXE), "--frames", "4", "--menu", "--menu-page", "6",
             "--dino-dat", str(dat or dino_dat), "--width", "800",
             "--height", "600", "--no-vsync", "--no-save-config",
             "--lnd", str(ISL01), "--asset-root", str(GAME),
             "--no-vegetation", "--no-water", *extra],
            capture_output=True, text=True, timeout=180,
        )

    shot = tmp_path / "cinematics.bmp"
    page = run(["--screenshot", str(shot)])
    assert page.returncode == 0, page.stderr
    image = Image.open(shot).convert("RGB")

    def bright(row):
        top = 167 + 34 * row + 3
        band = image.crop((327, top, 503, top + 28))
        return sum(1 for r, g, b in band.get_flattened_data()
                   if r > 170 and g > 170 and b > 170)

    # Cpg05 ends with mixe.mpg and its loc name is "TCHERNOPIG"; the two
    # campaigns without an ending clip (Cpg06/Cpg08) must render no label.
    assert bright(4) > 200, [bright(i) for i in range(7)]
    assert bright(5) < 100, [bright(i) for i in range(7)]
    assert bright(6) < 100, [bright(i) for i in range(7)]

    cases = [(0, "basket.mpg"), (2, "trampoline.mpg"), (4, "mixe.mpg")]
    for row, clip in cases:
        played = run(["--menu-click", f"400,{184 + 34 * row}",
                      "--menu-click", "369,530", "--video-play-frames", "1"])
        assert played.returncode == 0, played.stderr
        assert f"Data/Video/{clip}" in played.stdout, played.stdout
        assert "video played: 1 frames" in played.stdout, played.stdout

    # Row 5 (Cpg06) has an empty EndingVideo: START must do nothing.
    empty = run(["--menu-click", "400,354", "--menu-click", "369,530",
                 "--video-play-frames", "1"])
    assert empty.returncode == 0, empty.stderr
    assert "menu cinematic:" not in empty.stdout, empty.stdout

    # A fresh profile (unlock value 1) has no watched ending yet: retail
    # enables `row < value-1`, so row 0 START must also do nothing.
    fresh = tmp_path / "fresh.dat"
    fresh.write_bytes((1).to_bytes(4, "little"))
    locked = run(["--menu-click", "400,184", "--menu-click", "369,530",
                  "--video-play-frames", "1"], dat=fresh)
    assert locked.returncode == 0, locked.stderr
    assert "menu cinematic:" not in locked.stdout, locked.stdout


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


def test_cpp_park_window_clicks_1024():
    """Window-space clicks must match the HUD re-anchored to a 1024x768 window.

    The park HUD fills the window, so a click at the drawn control is already
    in park design space. This catches the old letterbox subtraction that made
    the visual buttons appear offset to the right.
    """
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    result = subprocess.run(
        [str(EXE), "--validate", "--width", "1024", "--height", "768",
         "--lnd", str(ISL01), "--asset-root", str(GAME), "--no-vegetation",
         "--park-window-click", "321,752",   # gauge: OPEN_BUDGET_PANEL
         "--park-window-click", "981,214"],  # right panel: OPEN_LABORATORY
        capture_output=True, text=True, timeout=120,
    )
    assert result.returncode == 0, result.stderr
    assert "park window click 321,752 -> open BUDGET_PANEL" in result.stdout
    assert "park window click 981,214 -> open DINO_LAB_MAIN_CENTER" in result.stdout


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
    # Closing restores the scene; tolerate a 1-pixel depth flicker.
    restored = ImageStat.Stat(ImageChops.difference(a, c))
    assert sum(restored.mean) / 3.0 < 0.05


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
    # ISLAND_LIST is an 8-row viewport over 10 islands with 34-px rows
    # (LB_LINE_HEIGHT); rows 8-9 need the VSCROLL down arrow. The arrow click
    # is at 352,440 (22-px strip on the list's right edge).
    first = max(0, row - 7)
    clicks = []
    for _ in range(first):
        clicks += ["--menu-click", "352,440"]
    slot = row - first
    island_y = 166 + slot * 34 + 17
    difficulty_x = 494 + difficulty * 74
    return subprocess.run(
        [str(EXE), "--frames", "4", "--screenshot", str(shot),
         "--width", "800", "--height", "600", "--no-vsync", "--no-save-config",
         "--no-hud", "--asset-root", str(GAME),
         "--menu-page", "1",
         *clicks,
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

    # Middle rows use LB_LINE_HEIGHT (34), not height/10: row 3 is isl04 and
    # row 7 is isl08 (the last row before the viewport scrolls).
    mid = tmp_path / "isl04.bmp"
    result = _menu_start(mid, row=3, difficulty=0)
    assert result.returncode == 0, result.stderr
    assert "island: isl04" in result.stdout
    edge = tmp_path / "isl08.bmp"
    result = _menu_start(edge, row=7, difficulty=0)
    assert result.returncode == 0, result.stderr
    assert "island: isl08" in result.stdout

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

def test_cpp_park_music():
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    result = subprocess.run(
        [str(EXE), "--frames", "2", "--no-menu", "--width", "320",
         "--height", "200", "--no-vsync", "--no-save-config",
         "--asset-root", str(GAME), "--lnd", str(ISL01), "--no-vegetation",
         "--music", "--music-volume", "0"],
        capture_output=True, text=True, timeout=180,
    )
    assert result.returncode == 0, result.stderr
    assert "music: " in result.stdout
    assert "musEquateur.wav" in result.stdout

def test_cpp_options_save_panel(tmp_path):
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    saves = sorted((ROOT / "run" / "save").glob("*.sav"))
    if not saves:
        pytest.skip("retail saves not present")
    header = subprocess.run(
        [str(EXE), "--validate", "--lnd", str(ISL01), "--asset-root", str(GAME),
         "--no-vegetation"],
        capture_output=True, text=True, timeout=120,
    )
    assert header.returncode == 0, header.stderr
    assert "save slot001.sav: name=agenttest version=53 slot=0" in header.stdout

    shot = tmp_path / "options-save.bmp"
    result = subprocess.run(
        [str(EXE), "--frames", "3", "--no-menu", "--screenshot", str(shot),
         "--width", "800", "--height", "600", "--no-vsync", "--no-save-config",
         "--no-hud", "--lnd", str(ISL01), "--asset-root", str(GAME),
         "--no-vegetation", "--distance", "500",
         "--park-click", "765,348", "--park-click", "348,73"],
        capture_output=True, text=True, timeout=120,
    )
    assert result.returncode == 0, result.stderr
    assert "park click 348,73 -> options save tab" in result.stdout
    image = Image.open(shot).convert("RGB")
    listing = image.crop((282, 125, 425, 341)).tobytes()
    # Save names are yellow on the dark-red list background.
    yellow = sum(
        1
        for i in range(0, len(listing), 3)
        if listing[i] > 200 and listing[i + 1] > 200 and listing[i + 2] < 80
    )
    assert yellow > 20

def test_cpp_menu_and_island_music_switch():
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    result = subprocess.run(
        [str(EXE), "--frames", "4", "--width", "320", "--height", "200",
         "--no-vsync", "--no-save-config", "--asset-root", str(GAME),
         "--music", "--menu-page", "1", "--menu-click", "200,180",
         "--menu-click", "494,451", "--menu-click", "526,531",
         "--menu-loading-time", "0"],
        capture_output=True, text=True, timeout=180,
    )
    assert result.returncode == 0, result.stderr
    assert "musLabo.wav" in result.stdout
    assert "musEquateur.wav" in result.stdout

@pytest.mark.parametrize("row", range(10))
def test_cpp_menu_start_all_islands(tmp_path, row):
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    shot = tmp_path / f"isl{row + 1:02d}.bmp"
    # ISLAND_LIST rows are 34 px; rows 8-9 need the VSCROLL down arrow
    # (352,440) because only 8 of 10 fit.
    first = max(0, row - 7)
    clicks = []
    for _ in range(first):
        clicks += ["--menu-click", "352,440"]
    slot = row - first
    island_y = 166 + slot * 34 + 17
    result = subprocess.run(
        [str(EXE), "--frames", "3", "--screenshot", str(shot),
         "--width", "320", "--height", "200", "--no-vsync", "--no-save-config",
         "--no-hud", "--asset-root", str(GAME), "--no-vegetation",
         "--menu-page", "1", *clicks,
         "--menu-click", f"200,{island_y}",
         "--menu-click", "494,451", "--menu-click", "526,531",
         "--menu-loading-time", "0"],
        capture_output=True, text=True, timeout=180,
    )
    assert result.returncode == 0, result.stderr
    name = f"isl{row + 1:02d}"
    assert f"start: island={name}" in result.stdout
    assert f"island: {name} lnd=" in result.stdout
    assert "occupation mask:" not in result.stderr
    # The free-game Entrance is (lnd.width, lnd.height) with orientation
    # lnd.unknown_int (0/1/2); runtime-confirmed on isl01/isl02 and all
    # campaign Entrance records.
    entrance_states = [1, 2, 2, 1, 2, 1, 0, 1, 1, 1]
    entrance = next(
        (ln for ln in result.stdout.splitlines() if "building: Entrance" in ln),
        "",
    )
    assert entrance, result.stdout
    assert "cell=" in entrance
    assert f"state={entrance_states[row]} angle={entrance_states[row] * 90}" in entrance

def test_cpp_warning_dialog(tmp_path):
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    base = tmp_path / "base.bmp"
    open_shot = tmp_path / "warning.bmp"
    closed = tmp_path / "closed.bmp"
    common = [str(EXE), "--frames", "3", "--no-menu", "--width", "800",
              "--height", "600", "--no-vsync", "--no-save-config", "--no-hud",
              "--lnd", str(ISL01), "--asset-root", str(GAME),
              "--no-vegetation", "--distance", "500"]
    for shot, extra in (
        (base, []),
        (open_shot, ["--warning", "Test warning"]),
        (closed, ["--warning", "Test warning", "--park-click", "401,368"]),
    ):
        result = subprocess.run(
            common + ["--screenshot", str(shot)] + extra,
            capture_output=True, text=True, timeout=120,
        )
        assert result.returncode == 0, result.stderr
        if extra:
            assert "park warning: Test warning" in result.stdout
        if shot == closed:
            assert "park click 401,368 -> close warning" in result.stdout
    a = Image.open(base).convert("RGB")
    b = Image.open(open_shot).convert("RGB")
    c = Image.open(closed).convert("RGB")
    # DIALOG_BACKGROUND dims the scene (black, alpha 106).
    assert sum(b.getpixel((50, 300))) < sum(a.getpixel((50, 300)))
    # The warning text is white inside the dark text zone (centred panel:
    # text zone at 283,276 at 800x600).
    area = b.crop((283, 276, 520, 300)).tobytes()
    bright = sum(
        1
        for i in range(0, len(area), 3)
        if area[i] > 220 and area[i + 1] > 220 and area[i + 2] > 220
    )
    assert bright > 20
    # Closing restores the scene.
    assert sum(c.getpixel((50, 300))) == sum(a.getpixel((50, 300)))

def test_cpp_building_bbi_place(tmp_path):
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    bbi = GAME / "Data" / "Design" / "buildings" / "Entrance.bbi"
    if not bbi.exists():
        pytest.skip("Entrance.bbi missing")
    base = tmp_path / "base.bmp"
    placed = tmp_path / "placed.bmp"
    common = [str(EXE), "--frames", "3", "--no-menu", "--width", "800",
              "--height", "600", "--no-vsync", "--no-save-config", "--no-hud",
              "--lnd", str(ISL01), "--asset-root", str(GAME),
              "--no-vegetation", "--distance", "60", "--yaw", "35",
              "--pitch", "20"]
    a = subprocess.run(common + ["--screenshot", str(base)],
                       capture_output=True, text=True, timeout=120)
    assert a.returncode == 0, a.stderr
    b = subprocess.run(common + ["--screenshot", str(placed),
                                 "--building", str(bbi),
                                 "--building-pos", "512,512"],
                       capture_output=True, text=True, timeout=120)
    assert b.returncode == 0, b.stderr
    assert "building: Entrance mesh=" in b.stdout
    assert "verts=296 tris=231" in b.stdout
    first = Image.open(base).convert("RGB")
    second = Image.open(placed).convert("RGB")
    region = (300, 250, 560, 430)
    diff = ImageChops.difference(first.crop(region), second.crop(region))
    assert sum(ImageStat.Stat(diff).mean) / 3 > 3.0

def test_cpp_wld_reader_matches_retail():
    if not EXE.exists():
        pytest.skip("viewer not built (run scripts/build.ps1)")
    scenario = GAME / "Data" / "Design" / "Scenario"
    if not scenario.is_dir():
        pytest.skip("retail scenario data missing")
    expected = {
        "Cpg01_Save.wld": (51, 0, 0, 1, 0, 1),
        "Cpg02_Save.wld": (51, 2, 0, 4, 0, 2),
        "Cpg03_Save.wld": (50, 1, 0, 4, 0, 3),
        "Cpg04_Save.wld": (53, 9, 1, 57, 20, 14),
        "Cpg05_Save.wld": (52, 2, 0, 8, 0, 3),
        "Cpg06_Save.wld": (51, 0, 0, 1, 0, 1),
        "Cpg08_Save.wld": (51, 0, 0, 1, 0, 1),
    }
    for name, (version, fences, shows, buildings, dinos, roads) in expected.items():
        path = scenario / name.split("_")[0] / name
        if not path.exists():
            pytest.skip(f"{name} missing")
        result = subprocess.run(
            [str(EXE), "--wld", str(path), "--wld-info",
             "--asset-root", str(GAME)],
            capture_output=True, text=True, timeout=60,
        )
        assert result.returncode == 0, (name, result.stderr)
        first = result.stdout.splitlines()[0]
        assert f"version={version}" in first
        assert f"fences={fences}" in first
        assert f"shows={shows}" in first
        assert f"buildings={buildings}" in first
        assert f"dinosaurs={dinos}" in first
        assert f"roads={roads}" in first
        assert "camera=1" in first
        lines = [ln for ln in result.stdout.splitlines() if ln.startswith("building:")]
        assert len(lines) == buildings

    # Cpg04's first record is the Entrance at (440,200); its link counts seats.
    cpg04 = scenario / "Cpg04" / "Cpg04_Save.wld"
    text = subprocess.run(
        [str(EXE), "--wld", str(cpg04), "--wld-info",
         "--asset-root", str(GAME)],
        capture_output=True, text=True, timeout=60,
    ).stdout
    assert "shows=1 animals=2 employees=6" in text.splitlines()[0]
    assert ("building: kind=9 name=Entrance x=440 y=200 state=1 "
            "obj=1 linked=2 link=0 class=24") in text
    assert "road: " in text
    assert ("show: obj=194 start=420,416 end=499,451 class=22 "
            "field44=195 field48=63526") in text
    assert "animal: obj=263 field1c=264 field20=0" in text
    assert "employee: obj=12 field1c=13" in text
    assert "sim lists: food_sources=2 employees=6" in text
    assert ("food source: id=264 cell=461,461 pos=461.500,421.500 "
            "dir=-1.000,0.000") in text
    assert ("sim employee: id=13 cell=475,340 pos=474.575,340.001 "
            "dir=1.000,-0.002 state=2") in text


def test_cpp_wld_reads_save_world():
    if not EXE.exists():
        pytest.skip("viewer not built (run scripts/build.ps1)")
    save = ROOT / "run" / "save" / "slot001.sav"
    if not save.exists():
        pytest.skip("retail save missing")
    result = subprocess.run(
        [str(EXE), "--wld", str(save), "--wld-info",
         "--asset-root", str(GAME)],
        capture_output=True, text=True, timeout=60,
    )
    assert result.returncode == 0, result.stderr
    first = result.stdout.splitlines()[0]
    assert "version=53" in first
    assert "buildings=1" in first
    assert "visitors=2" in first
    assert "camera=0" in first
    assert ("building: kind=9 name=Entrance x=520 y=376 state=1 "
            "obj=1 linked=2 link=0 class=24") in result.stdout

def test_cpp_building_placement_transform():
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    bbi = GAME / "Data" / "Design" / "buildings" / "Entrance.bbi"
    if not bbi.exists():
        pytest.skip("Entrance.bbi missing")
    # Position/orientation/Z from FUN_00586b80, FUN_005fecf0 and FUN_0061ffc0.
    result = subprocess.run(
        [str(EXE), "--frames", "2", "--no-menu", "--width", "320",
         "--height", "240", "--no-vsync", "--no-save-config", "--no-hud",
         "--lnd", str(ISL01), "--asset-root", str(GAME),
         "--no-vegetation", "--no-water", "--building", str(bbi),
         "--building-pos", "520,376", "--building-state", "1",
         "--screenshot", str(Path(os.environ["TEMP"]) / "dino_place.bmp")],
        capture_output=True, text=True, timeout=120,
    )
    assert result.returncode == 0, result.stderr
    line = [ln for ln in result.stdout.splitlines() if "building: Entrance" in ln][0]
    assert "cell=520.0,376.0" in line
    assert "centre=525.5,387.5" in line
    # Engine heightmap rows are bottom-up (`0x5c06b0`): the entrance samples
    # file row 323, i.e. the shoreline (18.0), not the volcano (69.4).
    assert "z=18.0" in line
    assert "state=1 angle=90" in line
    # The Entrance is the static LOD1 mesh at index 3 (metres, pivot space);
    # the `.skin` full mesh is the centimetre bind mesh centred on its own
    # origin and puts the boat on the beach without the pier.
    assert "entrance_lod1.mesh" in line


def test_cpp_wld_places_all_buildings():
    if not EXE.exists():
        pytest.skip("viewer not built")
    isl04 = GAME / "Data" / "Design" / "landscape" / "isl04" / "isl04.lnd"
    wld = GAME / "Data" / "Design" / "Scenario" / "Cpg04" / "Cpg04_Save.wld"
    if not isl04.exists() or not wld.exists():
        pytest.skip("retail isl04/Cpg04 missing")
    result = subprocess.run(
        [str(EXE), "--frames", "2", "--no-menu", "--width", "320",
         "--height", "240", "--no-vsync", "--no-save-config", "--no-hud",
         "--lnd", str(isl04), "--asset-root", str(GAME),
         "--no-vegetation", "--no-water", "--wld", str(wld)],
        capture_output=True, text=True, timeout=180,
    )
    assert result.returncode == 0, result.stderr
    assert "wld: placed 57/57 buildings" in result.stdout
    placed = [ln for ln in result.stdout.splitlines()
              if ln.startswith("building: ") and " mesh=" in ln]
    assert len(placed) == 57
    # Every imported building must bind a texture from the retail texture
    # directory; the old code looked inside a mesh/skeleton path and rendered
    # most buildings white.
    assert "building texture: cannot open" not in result.stderr
    assert all("texture=" in ln and not ln.rstrip().endswith("texture=")
               for ln in placed), [ln for ln in placed if "texture=" not in ln]
    assert any("BarakKute" in ln and "texture=standkute.tga" in ln
               for ln in placed)
    # The 12 skinned buildings (farm/souvenir/blackbox/park family) load from
    # their primary .skin; the rest are static meshes.
    assert sum(1 for ln in placed if ".skin" in ln) >= 12
    # Construction meshes have no material and must never be chosen as the
    # normal preview; retail abbreviates them as `construc`.
    assert not any("construc" in ln.lower() or "construction" in ln.lower()
                   for ln in placed), placed
    assert any("MediumSeats" in ln for ln in placed)
    assert any("Entrance" in ln and "centre=445.5,211.5" in ln
               for ln in placed)

def _save_catalog_rows():
    """Replicates the viewer's sorted pair<path, name> save catalog."""
    save_dir = ROOT / "run" / "save"
    paths = []
    if save_dir.is_dir():
        for path in save_dir.iterdir():
            if path.is_file() and path.suffix in (".sav", ".psav"):
                paths.append(str(path))
    return sorted(paths)


def test_cpp_port_save_roundtrip(tmp_path):
    if not EXE.exists():
        pytest.skip("viewer not built")
    isl03 = GAME / "Data" / "Design" / "landscape" / "isl03" / "isl03.lnd"
    isl01 = GAME / "Data" / "Design" / "landscape" / "isl01" / "isl01.lnd"
    if not isl03.exists() or not isl01.exists():
        pytest.skip("retail islands missing")
    target = tmp_path / "roundtrip.psav"
    bbi = GAME / "Data" / "Design" / "buildings" / "Entrance.bbi"
    save = subprocess.run(
        [str(EXE), "--frames", "2", "--no-menu", "--width", "320",
         "--height", "240", "--no-vsync", "--no-save-config", "--no-hud",
         "--lnd", str(isl03), "--asset-root", str(GAME), "--no-vegetation",
         # Engine mask is bottom-up: (100,100) is blocked; (128,128)/(128,129)
         # are free and share a free south neighbour.
         "--place-path", "128,128", "--place-path", "128,129",
         "--place-fence", "128,128,0", "--visitors", "3",
         "--park-time", "10:30", "--park-time-scale", "30",
         "--distance", "250", "--yaw", "40", "--pitch", "25",
         "--building", str(bbi), "--building-pos", "600,300",
         "--building-state", "1",
         "--save-port", str(target)],
        capture_output=True, text=True, timeout=180,
    )
    assert save.returncode == 0, save.stderr
    assert "port save:" in save.stdout
    text = target.read_text()
    assert "DINOPSAV 1" in text
    assert "island isl03" in text
    assert "clock 0 180 30" in text
    assert "grid 4 -512 -512 256 256" in text
    assert "path 128 128" in text and "path 128 129" in text
    assert "fence 128 128 0" in text
    assert "building 9 600 300 1" in text
    visitor_lines = [ln for ln in text.splitlines() if ln.startswith("visitor ")]
    assert len(visitor_lines) == 3
    # profile + three needs are the optional tail; the wander spawn uses the
    # free-game easy profile table (indices 0..5).
    assert all(ln.split()[12:15] == ["39", "99", "99"]
               for ln in visitor_lines)
    assert all(0 <= int(ln.split()[11]) < 6 for ln in visitor_lines)
    # SIM-2b tail: global profile, need0_at_use, state, target, route index,
    # timers, rng, done and the route count.
    assert all(len(ln.split()) >= 27 for ln in visitor_lines)

    load = subprocess.run(
        [str(EXE), "--frames", "2", "--no-menu", "--width", "320",
         "--height", "240", "--no-vsync", "--no-save-config", "--no-hud",
         "--lnd", str(isl01), "--asset-root", str(GAME), "--no-vegetation",
         "--load-port", str(target), "--visitor-stats"],
        capture_output=True, text=True, timeout=180,
    )
    assert load.returncode == 0, load.stderr
    assert "island: isl03" in load.stdout
    assert ("port load: island=isl03 budget=75000 day=0 visitors=3 "
            "paths=2 fences=1 buildings=1 dinos=0") in load.stdout
    assert "build: 2 path cells, 1 fences" in load.stdout
    assert ("building: Entrance mesh=" in load.stdout and
            "cell=600.0,300.0" in load.stdout)
    assert "visitor stats: count=3 " in load.stdout
    assert "needs=39.0/99.0/99.0" in load.stdout


def test_cpp_port_save_dino_state(tmp_path):
    """UI-2 Phase D: imported dinosaurs round-trip with needs/action state."""
    if not EXE.exists():
        pytest.skip("viewer not built")
    isl04 = GAME / "Data" / "Design" / "landscape" / "isl04" / "isl04.lnd"
    wld = GAME / "Data" / "Design" / "Scenario" / "Cpg04" / "Cpg04_Save.wld"
    if not isl04.exists() or not wld.exists():
        pytest.skip("retail isl04/Cpg04 missing")
    target = tmp_path / "dino.psav"
    save = subprocess.run(
        [str(EXE), "--frames", "2", "--no-menu", "--no-hud", "--width", "160",
         "--height", "120", "--no-vsync", "--no-save-config",
         "--lnd", str(isl04), "--asset-root", str(GAME),
         "--no-vegetation", "--no-water", "--wld-park", str(wld),
         "--dino-action-loop", "--dino-needs", "20,90,50,20",
         "--save-port", str(target)],
        capture_output=True, text=True, timeout=300,
    )
    assert save.returncode == 0, save.stderr
    text = target.read_text()
    dino_lines = [ln for ln in text.splitlines() if ln.startswith("dino ")]
    assert len(dino_lines) == 20
    assert all(" 20 90 50 20 " in line for line in dino_lines)
    assert any(" LowQuadruped " in line for line in dino_lines)
    assert any(" HeavyBiped " in line for line in dino_lines)
    # Clip playback phase is field 15; the optional cache tail follows it.
    assert all(len(line.split()) >= 16 for line in dino_lines)
    assert any(float(line.split()[15]) != 0.0 for line in dino_lines)

    load = subprocess.run(
        [str(EXE), "--frames", "4", "--no-menu", "--no-hud", "--width", "160",
         "--height", "120", "--no-vsync", "--no-save-config",
         "--asset-root", str(GAME), "--load-port", str(target),
         "--dino-action-loop"],
        capture_output=True, text=True, timeout=300,
    )
    assert load.returncode == 0, load.stderr
    assert ("port load: island=isl04 budget=75000 day=0 visitors=0 "
            "paths=301 fences=268 buildings=57 dinos=20") in load.stdout
    assert "dino action: " in load.stdout
    # No food source after a save load (paddocks are not part of the save),
    # so the restored 20/90/50/20 state selects Sleep 6360, not Eat.
    assert "Sleep score=6360 needs=20/90/50/20" in load.stdout


def test_cpp_port_save_danger_cache(tmp_path):
    """The danger-grid caches and their phenotype sources survive .psav."""
    if not EXE.exists():
        pytest.skip("viewer not built")
    isl04 = GAME / "Data" / "Design" / "landscape" / "isl04" / "isl04.lnd"
    wld = GAME / "Data" / "Design" / "Scenario" / "Cpg04" / "Cpg04_Save.wld"
    if not isl04.exists() or not wld.exists():
        pytest.skip("retail isl04/Cpg04 missing")
    target = tmp_path / "cache.psav"
    common = [
        str(EXE), "--frames", "2", "--no-menu", "--no-hud", "--width",
        "160", "--height", "120", "--no-vsync", "--no-save-config",
        "--lnd", str(isl04), "--asset-root", str(GAME),
        "--no-vegetation", "--no-water", "--wld-park", str(wld),
    ]
    save = subprocess.run(
        common + ["--dino-threat", "54", "--dino-radius", "7",
                  "--save-port", str(target)],
        capture_output=True, text=True, timeout=300,
    )
    assert save.returncode == 0, save.stderr
    dino_lines = [ln for ln in target.read_text().splitlines()
                  if ln.startswith("dino ")]
    assert len(dino_lines) == 20
    # Tail: threat, threat_radius, combined_attack, obstacle_radius.
    assert all(line.split()[-4] == "54" for line in dino_lines)
    assert all(line.split()[-3] == "7" for line in dino_lines)
    assert all(int(line.split()[-2]) > 0 for line in dino_lines)
    assert all(int(line.split()[-1]) > 0 for line in dino_lines)

    load = subprocess.run(
        common + ["--load-port", str(target), "--secu-probe"],
        capture_output=True, text=True, timeout=300,
    )
    assert load.returncode == 0, load.stderr
    assert "threat=54 radius=7" in load.stdout


def test_cpp_options_save_and_load_buttons(tmp_path):
    if not EXE.exists():
        pytest.skip("viewer not built")
    isl01 = GAME / "Data" / "Design" / "landscape" / "isl01" / "isl01.lnd"
    if not isl01.exists():
        pytest.skip("retail island missing")
    save_dir = ROOT / "run" / "save"
    save_dir.mkdir(parents=True, exist_ok=True)
    name = f"000_pytest_{os.getpid()}"
    target = save_dir / f"{name}.psav"
    if target.exists():
        target.unlink()
    try:
        common = [str(EXE), "--frames", "3", "--no-menu", "--width", "800",
                  "--height", "600", "--no-vsync", "--no-save-config",
                  "--no-hud", "--lnd", str(isl01), "--asset-root", str(GAME),
                  "--no-vegetation"]
        saved = subprocess.run(
            common + ["--place-path", "120,120", "--place-path", "121,120",
                      "--visitors", "2", "--save-name", name,
                      "--park-click", "765,348", "--park-click", "348,73",
                      "--park-click", "334,436",
                      "--screenshot", str(tmp_path / "save.bmp")],
            capture_output=True, text=True, timeout=180,
        )
        assert saved.returncode == 0, saved.stderr
        assert "park click 334,436 -> save port" in saved.stdout
        assert "port save:" in saved.stdout
        assert target.exists()

        rows = _save_catalog_rows()
        row = rows.index(str(target))
        assert row < 18, rows
        loaded = subprocess.run(
            common + ["--park-click", "765,348", "--park-click", "378,73",
                      "--park-click", f"300,{131 + row * 12}",
                      "--park-click", "334,436",
                      "--screenshot", str(tmp_path / "load.bmp")],
            capture_output=True, text=True, timeout=180,
        )
        assert loaded.returncode == 0, loaded.stderr
        assert f"park click 300,{131 + row * 12} -> options select {row}" in loaded.stdout
        assert "park click 334,436 -> load port" in loaded.stdout
        assert "port load: island=isl01 budget=75000 day=0 visitors=2 paths=2 fences=0 buildings=0" in loaded.stdout
        assert "build: 2 path cells, 0 fences" in loaded.stdout
    finally:
        if target.exists():
            target.unlink()

def test_cpp_request_dialog(tmp_path):
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    base = tmp_path / "base.bmp"
    opened = tmp_path / "request.bmp"
    accepted = tmp_path / "accepted.bmp"
    common = [str(EXE), "--frames", "3", "--no-menu", "--width", "800",
              "--height", "600", "--no-vsync", "--no-save-config", "--no-hud",
              "--lnd", str(ISL01), "--asset-root", str(GAME),
              "--no-vegetation", "--distance", "500"]
    for shot, extra in (
        (base, []),
        (opened, ["--request", "A visitor requests a food stand."]),
        (accepted, ["--request", "A visitor requests a food stand.",
                    "--park-click", "384,370"]),
    ):
        result = subprocess.run(
            common + ["--screenshot", str(shot)] + extra,
            capture_output=True, text=True, timeout=120,
        )
        assert result.returncode == 0, result.stderr
        if extra:
            assert "park request: A visitor requests a food stand." in result.stdout
        if shot == accepted:
            assert "park click 384,370 -> accept request" in result.stdout
    a = Image.open(base).convert("RGB")
    b = Image.open(opened).convert("RGB")
    c = Image.open(accepted).convert("RGB")
    assert sum(b.getpixel((50, 300))) < sum(a.getpixel((50, 300)))
    # Request text zone is centred: 283,274 at 800x600.
    area = b.crop((283, 274, 520, 300)).tobytes()
    bright = sum(
        1 for i in range(0, len(area), 3)
        if area[i] > 220 and area[i + 1] > 220 and area[i + 2] > 220
    )
    assert bright > 20
    assert sum(c.getpixel((50, 300))) == sum(a.getpixel((50, 300)))


def test_cpp_campaign_end_dialog(tmp_path):
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    base = tmp_path / "base.bmp"
    opened = tmp_path / "cpgend.bmp"
    closed = tmp_path / "cpgend_closed.bmp"
    common = [str(EXE), "--frames", "3", "--no-menu", "--width", "800",
              "--height", "600", "--no-vsync", "--no-save-config", "--no-hud",
              "--lnd", str(ISL01), "--asset-root", str(GAME),
              "--no-vegetation", "--distance", "500"]
    for shot, extra in (
        (base, []),
        (opened, ["--campaign-end", "Campaign complete."]),
        (closed, ["--campaign-end", "Campaign complete.",
                  "--park-click", "401,447"]),
    ):
        result = subprocess.run(
            common + ["--screenshot", str(shot)] + extra,
            capture_output=True, text=True, timeout=120,
        )
        assert result.returncode == 0, result.stderr
        if extra:
            assert "park campaign end: Campaign complete." in result.stdout
        if shot == closed:
            assert "park click 401,447 -> close campaign end" in result.stdout
    a = Image.open(base).convert("RGB")
    b = Image.open(opened).convert("RGB")
    c = Image.open(closed).convert("RGB")
    assert sum(b.getpixel((50, 300))) < sum(a.getpixel((50, 300)))
    # Campaign text zone is centred: 282,200 at 800x600.
    area = b.crop((282, 200, 520, 225)).tobytes()
    bright = sum(
        1 for i in range(0, len(area), 3)
        if area[i] > 220 and area[i + 1] > 220 and area[i + 2] > 220
    )
    assert bright > 20
    assert sum(c.getpixel((50, 300))) == sum(a.getpixel((50, 300)))

def test_cpp_wld_park_import(tmp_path):
    if not EXE.exists():
        pytest.skip("viewer not built")
    isl04 = GAME / "Data" / "Design" / "landscape" / "isl04" / "isl04.lnd"
    wld = GAME / "Data" / "Design" / "Scenario" / "Cpg04" / "Cpg04_Save.wld"
    if not isl04.exists() or not wld.exists():
        pytest.skip("retail isl04/Cpg04 missing")
    shot = tmp_path / "park.bmp"
    result = subprocess.run(
        [str(EXE), "--frames", "2", "--no-menu", "--no-hud", "--width", "320",
         "--height", "240", "--no-vsync", "--no-save-config",
         "--lnd", str(isl04), "--asset-root", str(GAME),
         "--no-vegetation", "--no-water", "--wld-park", str(wld),
         "--screenshot", str(shot)],
        capture_output=True, text=True, timeout=180,
    )
    assert result.returncode == 0, result.stderr
    assert "wld: placed 57/57 buildings" in result.stdout
    assert ("park import: roads=14 path_cells=301 fences=9 fence_edges=268"
            in result.stdout)
    assert "build: 301 path cells, 268 fences" in result.stdout
    image = Image.open(shot).convert("RGB")
    assert len(image.getcolors(maxcolors=1_000_000)) > 500


def test_cpp_wld_park_dinosaurs(tmp_path):
    if not EXE.exists():
        pytest.skip("viewer not built")
    isl04 = GAME / "Data" / "Design" / "landscape" / "isl04" / "isl04.lnd"
    wld = GAME / "Data" / "Design" / "Scenario" / "Cpg04" / "Cpg04_Save.wld"
    if not isl04.exists() or not wld.exists():
        pytest.skip("retail isl04/Cpg04 missing")
    shot = tmp_path / "dinos.bmp"
    result = subprocess.run(
        [str(EXE), "--frames", "2", "--no-menu", "--no-hud", "--width", "320",
         "--height", "240", "--no-vsync", "--no-save-config",
         "--lnd", str(isl04), "--asset-root", str(GAME),
         "--no-vegetation", "--no-water", "--wld-park", str(wld),
         "--focus-dino", "0", "--screenshot", str(shot)],
        capture_output=True, text=True, timeout=180,
    )
    assert result.returncode == 0, result.stderr
    # 20 dinosaurs, resolved from the dominant head-type gene to the 20 base
    # species (hybrids pick the dominant body plan).
    assert ("dinos: 20/20 spawned allo=7 anky=2 coelo=4 deino=5 trood=2"
            in result.stdout)
    # Each fence rectangle is a paddock: capacity 492 cells total. The cost
    # class comes from the phenotype length (`proportion.dat` blended by the
    # Part 2 merge + Part 3 Body length), matching retail Cpg04 = 336.
    assert "paddocks: 9 capacity=492 used=336" in result.stdout
    image = Image.open(shot).convert("RGB")
    assert len(image.getcolors(maxcolors=1_000_000)) > 500


def test_cpp_cpg04_phenotype_lengths():
    """Exact phenotype lengths from the live Cpg04 run (genotype -> part size).

    The 20 values were read at saDinosaur ctor 0x56FED0 in world-record order
    and reproduce with
    `table * (proportion_avg * body / table)` in float32 (truncated).
    See notes/phenotype-length-runtime.md.
    """
    if not EXE.exists():
        pytest.skip("viewer not built")
    isl04 = GAME / "Data" / "Design" / "landscape" / "isl04" / "isl04.lnd"
    wld = GAME / "Data" / "Design" / "Scenario" / "Cpg04" / "Cpg04_Save.wld"
    if not isl04.exists() or not wld.exists():
        pytest.skip("retail isl04/Cpg04 missing")
    env = dict(os.environ, DINO_DEBUG_PHENO="1")
    result = subprocess.run(
        [str(EXE), "--frames", "2", "--no-menu", "--no-hud", "--width", "160",
         "--height", "120", "--no-vsync", "--no-save-config",
         "--lnd", str(isl04), "--asset-root", str(GAME),
         "--no-vegetation", "--no-water", "--wld-park", str(wld)],
        capture_output=True, text=True, timeout=180, env=env,
    )
    assert result.returncode == 0, result.stderr
    log = result.stdout + result.stderr
    lines = [line for line in log.splitlines() if line.startswith("pheno ")]
    assert len(lines) == 20
    lengths = [int(line.split("len=")[1].split()[0]) for line in lines]
    assert lengths == [
        606, 742, 742, 742, 170, 170, 492, 172, 435, 409,
        702, 175, 240, 266, 266, 266, 266, 266, 175, 175,
    ]
    assert "paddocks: 9 capacity=492 used=336" in result.stdout


def test_cpp_cpg04_phenotype_attributes():
    """Computed phenotype attributes against the live Cpg04 ctor table.

    `notes/phenotype-attributes-runtime.md` records the 20 live rows. The
    The dummy/skin/carapace accumulator (Parts 4-9) is applied, so all 20
    rows match the live ctor values exactly. See
    docs/formats/translator-constants.md and notes/phenotype-attributes-runtime.md.
    """
    if not EXE.exists():
        pytest.skip("viewer not built")
    isl04 = GAME / "Data" / "Design" / "landscape" / "isl04" / "isl04.lnd"
    wld = GAME / "Data" / "Design" / "Scenario" / "Cpg04" / "Cpg04_Save.wld"
    if not isl04.exists() or not wld.exists():
        pytest.skip("retail isl04/Cpg04 missing")
    env = dict(os.environ, DINO_DEBUG_PHENO="1")
    result = subprocess.run(
        [str(EXE), "--frames", "2", "--no-menu", "--no-hud", "--width", "160",
         "--height", "120", "--no-vsync", "--no-save-config",
         "--lnd", str(isl04), "--asset-root", str(GAME),
         "--no-vegetation", "--no-water", "--wld-park", str(wld)],
        capture_output=True, text=True, timeout=180, env=env,
    )
    assert result.returncode == 0, result.stderr
    lines = [line for line in (result.stdout + result.stderr).splitlines()
             if line.startswith("pheno ")]
    assert len(lines) == 20

    def fields(line):
        out = {}
        for token in line.split():
            if "=" in token:
                key, value = token.split("=", 1)
                out[key] = value
        return out

    rows = [fields(line) for line in lines]
    # Live ctor columns (`notes/phenotype-attributes-runtime.md`).
    assert [int(r["wid"]) for r in rows] == [
        139, 260, 260, 260, 69, 69, 187, 71, 165, 165,
        260, 72, 111, 111, 111, 111, 111, 111, 74, 74,
    ]
    assert [int(r["hei"]) for r in rows] == [
        234, 420, 420, 420, 150, 150, 323, 148, 291, 291,
        432, 147, 181, 210, 210, 210, 210, 210, 147, 147,
    ]
    assert [int(r["P"]) for r in rows] == [
        12, 34, 34, 34, 51, 51, 45, 45, 42, 42,
        46, 41, 33, 57, 57, 57, 57, 57, 41, 41,
    ]
    assert [int(r["I"]) for r in rows] == [
        18, 38, 38, 38, 105, 105, 40, 91, 56, 57,
        34, 78, 35, 48, 48, 48, 48, 48, 77, 77,
    ]
    assert [int(r["B"]) for r in rows] == [
        56, 20, 20, 20, 45, 45, 23, 49, 31, 32,
        27, 54, 39, 30, 30, 30, 30, 30, 55, 55,
    ]
    assert [int(r["Arm"]) for r in rows] == [
        78, 25, 25, 25, 30, 30, 33, 28, 25, 27,
        33, 30, 58, 45, 45, 45, 45, 45, 30, 30,
    ]
    assert [int(r["Atk"]) for r in rows] == [
        243, 335, 335, 335, 179, 179, 332, 202, 250, 259,
        345, 225, 305, 325, 325, 325, 325, 325, 225, 225,
    ]
    # Per-part attacks: head / body / arm / leg / tail. The body attack uses
    # the head pair (retail quirk) and the leg attack is zero for quadrupeds;
    # the tail uses the tail pair.
    parts = [(int(r["Ah"]), int(r["Ab"]), int(r["Aa"]), int(r["Al"]),
              int(r["At"])) for r in rows]
    assert parts[0] == (30, 100, 10, 1, 102)   # row 1: accumulator sum 243
    assert sum(parts[0]) == 243
    assert parts[9] == (67, 37, 31, 63, 61)    # row 10: exact live 259
    assert parts[10] == (92, 59, 50, 75, 69)   # row 11: exact live 345
    assert parts[12] == (39, 51, 100, 60, 55)  # row 13: exact live 305
    assert sum(parts[9]) == 259
    assert sum(parts[10]) == 345
    assert sum(parts[12]) == 305
    # Gene expressions: Agre/Stam/Cour with the dummy/skin/carapace
    # accumulator (rows 1/11/13 shift from the bare `matrix[b][a]` value).
    assert [(int(r["Ag"]), int(r["St"]), int(r["Co"])) for r in rows] == [
        (82, 83, 77), (80, 40, 70), (80, 40, 70), (80, 40, 70),
        (70, 20, 40), (70, 20, 40), (90, 50, 75), (60, 40, 30),
        (70, 40, 50), (70, 40, 70), (78, 48, 80), (60, 40, 40),
        (75, 43, 70), (90, 50, 80), (90, 50, 80), (90, 50, 80),
        (90, 50, 80), (90, 50, 80), (60, 30, 50), (60, 30, 50),
    ]
    # `+0x78/+0x7c/+0x80` (obstacle size/square/radius) are computed by the
    # same accumulator; the recorded runtime radii 5/10/3 for rows 1/2/5 are
    # covered in notes/phenotype-attributes-runtime.md. The viewer probe does
    # not print them yet (the cache wiring will expose them through SECU).


def test_cpp_dino_action_loop():
    if not EXE.exists():
        pytest.skip("viewer not built")
    isl04 = GAME / "Data" / "Design" / "landscape" / "isl04" / "isl04.lnd"
    wld = GAME / "Data" / "Design" / "Scenario" / "Cpg04" / "Cpg04_Save.wld"
    if not isl04.exists() or not wld.exists():
        pytest.skip("retail isl04/Cpg04 missing")

    def run(needs):
        args = [str(EXE), "--frames", "2", "--no-menu", "--no-hud",
                "--width", "160", "--height", "120", "--no-vsync",
                "--no-save-config", "--lnd", str(isl04),
                "--asset-root", str(GAME), "--no-vegetation", "--no-water",
                "--wld-park", str(wld), "--dino-action-loop"]
        if needs:
            args += ["--dino-needs", needs]
        result = subprocess.run(
            args,
            capture_output=True, text=True, timeout=180,
        )
        assert result.returncode == 0, result.stderr
        return [line for line in result.stdout.splitlines()
                if line.startswith("dino action:")]

    # Without a food source the `.bai` consume gate hides Eat; the starving
    # dinosaurs pick Exploration instead (score 1580).
    starving = run("20,90,50,90")
    assert len(starving) == 20
    assert all(" Exploration score=1580 " in line for line in starving)
    assert not any(" Eat" in line for line in starving)

    tired = run("70,90,90,20")
    assert len(tired) == 20
    assert any(re.search(r"dino action: \w+ Sleep score=7060 "
                         r"needs=70/90/90/20", line) for line in tired)

    # All-full needs score 0 on the self actions, so the dinosaur stays idle.
    assert run("99,99,99,99") == []

    # Retail steady state: the food gauge starts at 50% and the first 10 s
    # tick normalises UI FOOD to 50; without food the dinosaur explores.
    default = run(None)
    assert len(default) == 20
    assert all(" Exploration score=75 " in line for line in default)


def test_cpp_danger_grid_secu():
    if not EXE.exists():
        pytest.skip("viewer not built")
    isl04 = GAME / "Data" / "Design" / "landscape" / "isl04" / "isl04.lnd"
    wld = GAME / "Data" / "Design" / "Scenario" / "Cpg04" / "Cpg04_Save.wld"
    if not isl04.exists() or not wld.exists():
        pytest.skip("retail isl04/Cpg04 missing")

    line_re = re.compile(
        r"^secu: (\S+) x=([-\d.]+) y=([-\d.]+) threat=(\d+) radius=(\d+) "
        r"cell=(\d+) health=(\d+)/(\d+) secu=(\d+)$"
    )

    def probe(extra):
        result = subprocess.run(
            [str(EXE), "--frames", "2", "--no-menu", "--no-hud",
             "--width", "160", "--height", "120", "--no-vsync",
             "--no-save-config", "--lnd", str(isl04),
             "--asset-root", str(GAME), "--no-vegetation", "--no-water",
             "--wld-park", str(wld), "--dino-action-loop", "--secu-probe"]
            + extra,
            capture_output=True, text=True, timeout=180,
        )
        assert result.returncode == 0, result.stderr
        rows = []
        for line in result.stdout.splitlines():
            match = line_re.match(line)
            if match:
                rows.append(match.groups())
        return rows

    # `FUN_00571440`: SECU from the 8-unit grid cell under each dinosaur.
    rows = probe(["--dino-threat", "50", "--dino-radius", "8"])
    assert len(rows) % 20 == 0 and len(rows) >= 20
    rows = rows[:20]
    entries = []
    for _species, x, y, threat, _radius, cell, health, max_health, secu in rows:
        x, y = float(x), float(y)
        threat, cell = int(threat), int(cell)
        health, max_health, secu = int(health), int(max_health), int(secu)
        # Raw `.wld`/`.raw` units, not the centred render coordinates: the
        # park spans 0..1024 and several Cpg04 dinosaurs sit past x=512.
        assert 0.0 <= x <= 1024.0 and 0.0 <= y <= 1024.0
        # Its own stamp always covers the cell it stands on.
        assert cell >= threat
        health_term = int((health - max_health) * 100 / max_health)
        assert secu == max(0, 99 + health_term - int((cell - threat) / 10))
        entries.append((x, y, secu))
    assert any(float(row[1]) > 512.0 for row in rows)
    assert any(int(row[5]) > int(row[3]) for row in rows)
    assert any(int(row[8]) < 99 for row in rows)

    # The damage term uses the same formula: 50 % health subtracts 50.
    damaged = probe(["--dino-threat", "50", "--dino-radius", "8",
                     "--dino-health", "50,100"])
    assert len(damaged) % 20 == 0 and len(damaged) >= 20
    damaged = damaged[:20]
    healthy = {(x, y): secu for x, y, secu in entries}
    for _species, x, y, _t, _r, _c, health, max_health, secu in damaged:
        assert int(health) == 50 and int(max_health) == 100
        assert int(secu) == max(0, healthy[(float(x), float(y))] - 50)

    # Health alone is enough to make the grid live (no stamps -> cell 0).
    health_only = probe(["--dino-health", "50,100"])
    assert len(health_only) % 20 == 0 and len(health_only) >= 20
    assert all(int(row[5]) == 0 and int(row[8]) == 49
               for row in health_only[:20])

    # A non-exact health ratio (retail 70/169 -> -58) and the 0 clamp.
    damaged_odd = probe(["--dino-threat", "50", "--dino-radius", "8",
                         "--dino-health", "70,169"])
    assert len(damaged_odd) % 20 == 0 and len(damaged_odd) >= 20
    assert all(int(row[6]) == 70 and int(row[7]) == 169
               for row in damaged_odd[:20])
    assert all(int(row[8]) in (41, 36, 31) for row in damaged_odd[:20])
    zeroed = probe(["--dino-threat", "50", "--dino-radius", "8",
                    "--dino-health", "0,100"])
    assert len(zeroed) % 20 == 0 and len(zeroed) >= 20
    assert all(int(row[8]) == 0 for row in zeroed[:20])


def test_cpp_wld_park_no_import_dinos():
    if not EXE.exists():
        pytest.skip("viewer not built")
    isl04 = GAME / "Data" / "Design" / "landscape" / "isl04" / "isl04.lnd"
    wld = GAME / "Data" / "Design" / "Scenario" / "Cpg04" / "Cpg04_Save.wld"
    if not isl04.exists() or not wld.exists():
        pytest.skip("retail isl04/Cpg04 missing")
    result = subprocess.run(
        [str(EXE), "--frames", "1", "--no-menu", "--no-hud", "--width", "160",
         "--height", "120", "--no-vsync", "--no-save-config",
         "--lnd", str(isl04), "--asset-root", str(GAME),
         "--no-vegetation", "--no-water", "--wld-park", str(wld),
         "--no-import-dinos"],
        capture_output=True, text=True, timeout=120,
    )
    assert result.returncode == 0, result.stderr
    assert "dinos:" not in result.stdout


def test_cpp_wld_park_import_single_road():
    if not EXE.exists():
        pytest.skip("viewer not built")
    wld = GAME / "Data" / "Design" / "Scenario" / "Cpg01" / "Cpg01_Save.wld"
    isl01 = GAME / "Data" / "Design" / "landscape" / "isl01" / "isl01.lnd"
    if not wld.exists() or not isl01.exists():
        pytest.skip("retail Cpg01/isl01 missing")
    result = subprocess.run(
        [str(EXE), "--frames", "1", "--no-menu", "--no-hud", "--width", "160",
         "--height", "120", "--no-vsync", "--no-save-config",
         "--lnd", str(isl01), "--asset-root", str(GAME),
         "--no-vegetation", "--no-water", "--wld-park", str(wld)],
        capture_output=True, text=True, timeout=120,
    )
    assert result.returncode == 0, result.stderr
    assert "wld: placed 1/1 buildings" in result.stdout
    assert ("park import: roads=1 path_cells=16 fences=0 fence_edges=0"
            in result.stdout)

def test_cpp_options_name_entry():
    if not EXE.exists():
        pytest.skip("viewer not built")
    if not ISL01.exists():
        pytest.skip("retail isl01 missing")
    target = ROOT / "run" / "save" / "portname.psav"
    target.unlink(missing_ok=True)
    try:
        result = subprocess.run(
            [str(EXE), "--frames", "2", "--no-menu", "--no-hud",
             "--width", "800", "--height", "600", "--no-vsync",
             "--no-save-config", "--lnd", str(ISL01),
             "--asset-root", str(GAME), "--no-vegetation", "--no-water",
             "--type-text", "portname", "--park-click", "765,348",
             "--park-click", "348,73", "--park-click", "349,381",
             "--park-click", "334,436"],
            capture_output=True, text=True, timeout=120,
        )
        assert result.returncode == 0, result.stderr
        assert "park click 349,381 -> options name edit" in result.stdout
        assert "port save: run/save/portname.psav" in result.stdout
        assert target.exists()
    finally:
        target.unlink(missing_ok=True)


def test_cpp_construction_placement():
    if not EXE.exists():
        pytest.skip("viewer not built")
    if not ISL01.exists():
        pytest.skip("retail isl01 missing")
    # Retail panel path: open the building panel, select the restaurant tab,
    # click CONSTRUC_RESTAURANT3_BUTTON, then place on the ground.
    args = [str(EXE), "--frames", "2", "--no-menu", "--no-hud",
            "--width", "800", "--height", "600", "--no-vsync",
            "--no-save-config", "--lnd", str(ISL01),
            "--asset-root", str(GAME), "--no-vegetation", "--no-water",
            "--no-entrance", "--park-click", "748,177",
            "--park-click", "582,28", "--park-click", "577,243",
            "--construct-pos", "500,500", "--construct-pos", "500,500"]
    result = subprocess.run(args, capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stderr
    assert ("park click 577,243 -> "
            "construct CONSTRUC_RESTAURANT3_BUTTON") in result.stdout
    assert "construction: RestCirc selected, cost=2500" in result.stdout
    assert ("construction: RestCirc cost=2500 budget=72500 cell=500,500"
            in result.stdout)
    assert "construction: RestCirc overlaps another building" in result.stdout

    poor = subprocess.run(
        [str(EXE), "--frames", "1", "--no-menu", "--no-hud", "--width", "160",
         "--height", "120", "--no-vsync", "--no-save-config", "--lnd",
         str(ISL01), "--asset-root", str(GAME), "--no-vegetation",
         "--no-water", "--no-entrance", "--park-budget", "100",
         "--construct", "RestCirc", "--construct-pos", "500,500"],
        capture_output=True, text=True, timeout=120,
    )
    assert poor.returncode == 0, poor.stderr
    assert ("construction: RestCirc needs 2500, budget 100 "
            "(WARNING_NOT_ENOUGH_MONEY)") in poor.stdout


def test_cpp_road_tool():
    """Retail ROAD_CONSTRUCTION through the Infrastructure panel on isl01.

    notes/road-construction-recon.md: two-click straight runs, axis lock,
    10 per cell beyond the first (GeneralParams.bgd +0x1a4), CONSTRUCTION
    charge, chaining, and the BUSY/TOO_SHORT/NOT_ENOUGH_MONEY failures.
    """
    if not EXE.exists():
        pytest.skip("viewer not built")
    if not ISL01.exists():
        pytest.skip("retail isl01 missing")
    base = [str(EXE), "--frames", "2", "--no-menu", "--no-hud",
            "--width", "800", "--height", "600", "--no-vsync",
            "--no-save-config", "--lnd", str(ISL01),
            "--asset-root", str(GAME), "--no-vegetation", "--no-water",
            "--no-entrance", "--park-click", "765,248",
            "--park-click", "645,86"]
    clicks = ["120,130", "128,131", "128,135", "128,135", "128,139"]
    args = base + [a for c in clicks for a in ("--road-click", c)]
    result = subprocess.run(args, capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stderr
    out = result.stdout
    assert "park click 645,86 -> construct CONSTRUC_ROAD_BUTTON" in out
    assert "road: tool armed, price=10 per cell" in out
    assert "road: start 120,130" in out
    assert ("road: built 120,130 -> 128,130 cells=9 new=9 cost=80 "
            "budget=74920 construction=1/80") in out
    assert ("road: built 128,130 -> 128,135 cells=6 new=5 cost=50 "
            "budget=74870 construction=2/130") in out
    assert ("road: 128,135 -> 128,135 rejected (WARNING_ROAD_TOO_SHORT)"
            in out)
    assert ("road: 128,135 -> 128,139 rejected (WARNING_BUSY_SPACE)" in out)

    poor = subprocess.run(
        base + ["--park-budget", "60", "--road-click", "120,125",
                "--road-click", "130,125"],
        capture_output=True, text=True, timeout=120,
    )
    assert poor.returncode == 0, poor.stderr
    assert ("road: needs 100, budget 60 (WARNING_NOT_ENOUGH_MONEY)"
            in poor.stdout)
    assert "road: built" not in poor.stdout


def test_cpp_road_tool_menu_started_park():
    """Menu -> free game -> Infrastructure -> Road -> ground clicks, driven
    through the live SDL event loop (--inject-click). Menu-started parks had
    no build grid, so ground clicks were silently dropped."""
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    args = [str(EXE), "--asset-root", str(GAME), "--menu-page", "1",
            "--menu-click", "200,180", "--menu-click", "494,451",
            "--menu-click", "526,531", "--menu-loading-time", "0",
            "--windowed", "--width", "1280", "--height", "800",
            "--no-save-config", "--no-vsync", "--frames", "130",
            "--inject-click", "60,1253,443",   # OPEN_TRANSPORT
            "--inject-click", "70,1121,285",   # CONSTRUC_ROAD_BUTTON
            "--inject-click", "90,600,520",    # start
            "--inject-click", "110,700,520"]   # commit
    result = subprocess.run(args, capture_output=True, text=True, timeout=180)
    assert result.returncode == 0, result.stderr
    out = result.stdout
    assert "start: island=isl01" in out
    assert "construct CONSTRUC_ROAD_BUTTON" in out
    assert "road: start " in out
    assert "road: built " in out


def test_cpp_enclosure_panel_arms_fence_tool():
    """OPEN_CONSTRUCTION_FENCE arms the tool; two ground clicks build a paddock."""
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    result = subprocess.run(
        [str(EXE), "--frames", "15", "--no-menu", "--width", "800",
         "--height", "600", "--no-vsync", "--no-save-config", "--lnd",
         str(ISL01), "--asset-root", str(GAME), "--no-vegetation",
         "--no-water", "--camera-target", "0,0", "--distance", "200",
         "--park-click", "764,135", "--park-click", "618,145",
         "--fence-cursor", "400,300",
         "--inject-click", "5,400,300",
         "--inject-click", "10,450,350"],
        capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stderr
    assert "open FENCE_CONSTRUCTION_AND_INFO_PANEL" in result.stdout
    assert "fence: tool armed (click two corners)" in result.stdout
    assert "fence preview: cell=" in result.stdout
    assert "ok=1" in result.stdout
    assert "fence: corner " in result.stdout
    assert "fence: built rect " in result.stdout
    assert "paddock=1 capacity=" in result.stdout
    match = re.search(r"build: \d+ path cells, (\d+) fences", result.stdout)
    assert match is not None and int(match.group(1)) > 0


def test_cpp_enclosure_preview_inside_paddock_red():
    """A fence cursor inside an existing Cpg04 paddock previews red."""
    if not EXE.exists():
        pytest.skip("viewer not built")
    cpg04 = GAME / "Data" / "Design" / "Scenario" / "Cpg04" / "Cpg04_Save.wld"
    isl04 = GAME / "Data" / "Design" / "landscape" / "isl04" / "isl04.lnd"
    if not cpg04.exists() or not isl04.exists():
        pytest.skip("retail Cpg04/isl04 missing")
    result = subprocess.run(
        [str(EXE), "--frames", "15", "--no-menu", "--width", "800",
         "--height", "600", "--no-vsync", "--no-save-config",
         "--lnd", str(isl04), "--asset-root", str(GAME),
         "--no-vegetation", "--no-water", "--wld-park", str(cpg04),
         "--camera-target", "-44,-36", "--distance", "300", "--pitch", "20",
         "--park-click", "764,135", "--park-click", "618,145",
         "--fence-cursor", "400,300",
         "--inject-click", "5,400,300", "--inject-click", "10,410,310"],
        capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stderr
    assert "fence preview: cell=115,118 ok=0" in result.stdout
    # The red disc means the corner is rejected: no rectangle is built.
    assert "fence: corner " not in result.stdout
    assert "fence: built rect " not in result.stdout


def test_cpp_road_preview_tint(tmp_path):
    """The pending run draws focus/road.tga boxes tinted by gscMaterialBank
    (0x600550): blue when buildable, red when not (here: unaffordable).
    --road-cursor pins the preview end under window pixel 400,300 (cell
    116,121 with this camera), so the run 108..116 is deterministic."""
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    base = [str(EXE), "--frames", "3", "--no-menu", "--no-hud",
            "--width", "800", "--height", "600", "--no-vsync",
            "--no-save-config", "--lnd", str(ISL01),
            "--asset-root", str(GAME), "--no-vegetation", "--no-water",
            "--no-entrance", "--camera-target", "-40,-23", "--distance", "120",
            "--park-click", "765,248", "--park-click", "645,86",
            "--road-click", "108,121"]
    box = (380, 260, 470, 315)

    def shot(name, extra):
        path = tmp_path / f"{name}.bmp"
        result = subprocess.run(base + extra + ["--screenshot", str(path)],
                                capture_output=True, text=True, timeout=120)
        assert result.returncode == 0, result.stderr
        assert "road: start 108,121" in result.stdout
        raw = Image.open(path).convert("RGB").crop(box).tobytes()
        return [tuple(raw[i:i + 3]) for i in range(0, len(raw), 3)]

    plain = shot("plain", [])
    for name, budget, want_blue in (("ok", "75000", True), ("poor", "10", False)):
        tinted = shot(name, ["--park-budget", budget, "--road-cursor", "400,300"])
        changed = [p for p, q in zip(tinted, plain)
                   if sum(abs(a - b) for a, b in zip(p, q)) > 120]
        assert len(changed) > 200, name
        blue = sum(1 for r, g, b in changed if b > r)
        red = sum(1 for r, g, b in changed if r > b + 40)
        if want_blue:
            assert blue > 200 and red == 0, (name, blue, red)
        else:
            assert red > 200 and blue == 0, (name, blue, red)
        # Retail fill is exactly the pulsing material colour:
        # RGB(64,64,255-x) valid, RGB(255-x,64,64) invalid.
        from collections import Counter
        dominant = Counter(changed).most_common(1)[0][0]
        if want_blue:
            assert dominant[:2] == (64, 64) and dominant[2] > 120, dominant
        else:
            assert dominant[1:] == (64, 64) and dominant[0] > 120, dominant


def test_cpp_road_panel_buttons_visible(tmp_path):
    """CONSTRUC_ROAD_BUTTON has no PRIORITY/ZPRIORITY; it must inherit the
    panel's and draw over the P=10 weave panels declared before it."""
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail data missing")
    shot = tmp_path / "road-panel.bmp"
    result = subprocess.run(
        [str(EXE), "--frames", "4", "--no-menu", "--width", "800",
         "--height", "600", "--no-vsync", "--no-save-config", "--lnd",
         str(ISL01), "--asset-root", str(GAME), "--no-water", "--no-entrance",
         "--no-vegetation", "--park-click", "765,248",
         "--screenshot", str(shot)],
        capture_output=True, text=True, timeout=120,
    )
    assert result.returncode == 0, result.stderr
    icon = Image.open(shot).convert("RGB").crop((633, 74, 657, 98)).tobytes()
    pixels = [icon[i:i + 3] for i in range(0, len(icon), 3)]
    # ROAD_ICON: grey metal frame, blue sky, green grass (the weave has none).
    frame = sum(1 for r, g, b in pixels
                if abs(r - g) < 12 and abs(g - b) < 12 and r > 120)
    sky = sum(1 for r, g, b in pixels if b > 150 and b > r + 30)
    assert frame > 40 and sky > 15


def test_cpp_sav_park_import(tmp_path):
    if not EXE.exists():
        pytest.skip("viewer not built")
    isl01 = GAME / "Data" / "Design" / "landscape" / "isl01" / "isl01.lnd"
    save = ROOT / "run" / "save" / "slot001.sav"
    if not isl01.exists() or not save.exists():
        pytest.skip("retail isl01/save missing")
    shot = tmp_path / "sav-park.bmp"
    result = subprocess.run(
        [str(EXE), "--frames", "2", "--no-menu", "--no-hud", "--width", "320",
         "--height", "240", "--no-vsync", "--no-save-config",
         "--lnd", str(isl01), "--asset-root", str(GAME),
         "--no-vegetation", "--no-water", "--sav-park", str(save),
         "--screenshot", str(shot)],
        capture_output=True, text=True, timeout=180,
    )
    assert result.returncode == 0, result.stderr
    assert ("sav park: visitors=2 roads=1 budget=75125 day=0 "
            "elapsed=23.214 fences=0 buildings=1") in result.stdout
    assert ("sav import: budget=75125 day=0 elapsed=23.214 visitors=2 "
            "paths=16 fences=0 buildings=1") in result.stdout
    assert "wld: placed 1/1 buildings" in result.stdout
    # Camera tail: eye[0..2], target[3..5], progress[6].
    assert ("sav camera: eye=551.00,512.00,84.70 target=512.00,512.00,32.00"
            in result.stdout)
    image = Image.open(shot).convert("RGB")
    assert len(image.getcolors(maxcolors=1_000_000)) > 300


def test_cpp_sav_park_auto_camera(tmp_path):
    """The saved camera tail is applied for a non-default view."""
    if not EXE.exists():
        pytest.skip("viewer not built")
    isl01 = GAME / "Data" / "Design" / "landscape" / "isl01" / "isl01.lnd"
    save = ROOT / "run" / "save" / "auto" / "slot001.sav"
    if not isl01.exists() or not save.exists():
        pytest.skip("retail isl01/auto save missing")
    result = subprocess.run(
        [str(EXE), "--frames", "1", "--no-menu", "--no-hud", "--width", "160",
         "--height", "120", "--no-vsync", "--no-save-config",
         "--lnd", str(isl01), "--asset-root", str(GAME),
         "--no-vegetation", "--no-water", "--sav-park", str(save),
         "--no-import-dinos"],
        capture_output=True, text=True, timeout=180)
    assert result.returncode == 0, result.stderr
    assert ("sav camera: eye=128.00,128.00,250.00 target=512.00,512.00,"
            "50.00") in result.stdout


def test_cpp_sav_ticket_value():
    # The manual save's two visitors are drsVisitor profiles 6 (Scientist,
    # 1.5 -> 75) and 9 (Kid, 1.0 -> 50); the stored ticket pair is 125/2.
    if not EXE.exists():
        pytest.skip("viewer not built")
    isl01 = GAME / "Data" / "Design" / "landscape" / "isl01" / "isl01.lnd"
    save = ROOT / "run" / "save" / "slot001.sav"
    if not isl01.exists() or not save.exists():
        pytest.skip("retail isl01/save missing")
    result = subprocess.run(
        [str(EXE), "--frames", "2", "--no-menu", "--no-hud", "--width", "160",
         "--height", "120", "--no-vsync", "--no-save-config",
         "--lnd", str(isl01), "--asset-root", str(GAME),
         "--no-vegetation", "--no-water", "--sav-park", str(save),
         "--settle-day"],
        capture_output=True, text=True, timeout=180,
    )
    assert result.returncode == 0, result.stderr
    assert "sav visitors: 2 restored" in result.stdout
    assert ("daily settlement: day=0 visitors=2 ticket=50 income=125 "
            "wages=0 budget=75250") in result.stdout

def test_cpp_bai_action_table():
    if not EXE.exists():
        pytest.skip("viewer not built")
    result = subprocess.run(
        [str(EXE), "--bai-info", "--asset-root", str(GAME)],
        capture_output=True, text=True, timeout=60,
    )
    assert result.returncode == 0, result.stderr
    assert ("actions: 64 preconditions=46 weights=43 need_deltas=134 "
            "grants=24 elements=204 events=171") in result.stdout
    assert "groups: 0=50 1=1 2=1 3=1 4=2 5=8 6=1" in result.stdout
    assert "classes: 0=44 1=1 2=1 3=1 4=0 5=17" in result.stdout
    assert "first=Exploration last=EatBaitForSmallies" in result.stdout

def test_cpp_dino_action_chooser():
    if not EXE.exists():
        pytest.skip("viewer not built")
    result = subprocess.run(
        [str(EXE), "--dino-actions", "--asset-root", str(GAME)],
        capture_output=True, text=True, timeout=60,
    )
    assert result.returncode == 0, result.stderr
    assert "dino actions: 64 nodes=4" in result.stdout
    assert "dino states starving: 2 3 1 3" in result.stdout
    assert "dino choose starving: Eat 9009" in result.stdout
    assert "dino choose tired: Sleep 7060" in result.stdout
    assert "dino choose both: Eat 9009" in result.stdout
    assert "dino choose full: idle" in result.stdout
    assert "dino choose small: EatForSmallies 9009" in result.stdout
    # `FUN_00571190` completion writers: GetCourage copies +0x44 into the
    # grid threat cache, ObstAttackSize1 copies +0x80 into the stamp radius.
    # Action-target policy (`node+0x3c` vtable +0x14): target cell danger
    # feeds the chooser threat term; a large Eat penalty changes the choice.
    assert "dino target box: -500" in result.stdout
    assert "dino target actor: -40" in result.stdout
    assert "dino target none: 0" in result.stdout
    assert "dino choose threatbox: Eat 8259" in result.stdout
    assert "dino loop threat: Exploration" in result.stdout
    assert "dino cache getcourage: threat=54" in result.stdout
    assert "dino cache obst: radius=7" in result.stdout

def test_cpp_market_info():
    if not EXE.exists():
        pytest.skip("viewer not built")
    result = subprocess.run(
        [str(EXE), "--market-info", "--asset-root", str(GAME)],
        capture_output=True, text=True, timeout=60,
    )
    assert result.returncode == 0, result.stderr
    assert "market: entries=54 stores=6 prices=3200..42000" in result.stdout
    assert "market stores: 0=10 2=14 3=14 4=9 5=5 6=2" in result.stdout
    assert ("first=Allosaurus 9000 allo.gen "
            "last=Zarbosaurus 4900 zarbo.gen") in result.stdout

def test_cpp_buy_dino():
    if not EXE.exists():
        pytest.skip("viewer not built")
    isl04 = GAME / "Data" / "Design" / "landscape" / "isl04" / "isl04.lnd"
    wld = GAME / "Data" / "Design" / "Scenario" / "Cpg04" / "Cpg04_Save.wld"
    if not isl04.exists() or not wld.exists():
        pytest.skip("retail isl04/Cpg04 missing")

    def run(extra):
        return subprocess.run(
            [str(EXE), "--frames", "2", "--no-menu", "--no-hud", "--width",
             "160", "--height", "120", "--no-vsync", "--no-save-config",
             "--lnd", str(isl04), "--asset-root", str(GAME),
             "--no-vegetation", "--no-water", "--wld-park", str(wld)] + extra,
            capture_output=True, text=True, timeout=300,
        )

    success = run(["--buy-dino", "Zarbosaurus", "--park-budget", "10000"])
    assert success.returncode == 0, success.stderr
    assert ("buy: Zarbosaurus price=4900 budget=5100 paddock="
            in success.stdout)
    assert " dinos=21" in success.stdout
    assert "paddocks: 9 capacity=492 used=352" in success.stdout

    no_room = run(["--buy-dino", "Zarbosaurus", "--buy-paddock", "0"])
    assert no_room.returncode == 0, no_room.stderr
    assert ("buy: Zarbosaurus no paddock room "
            "(WARNING_NO_ROOM_IN_FENCE) budget=75000") in no_room.stdout
    assert "paddocks: 9 capacity=492 used=336" in no_room.stdout

    no_money = run(["--buy-dino", "Zarbosaurus", "--park-budget", "100"])
    assert no_money.returncode == 0, no_money.stderr
    assert ("buy: Zarbosaurus price=4900 budget=100 "
            "(WARNING_NOT_ENOUGH_MONEY)") in no_money.stdout
    assert "paddocks: 9 capacity=492 used=336" in no_money.stdout

    unknown = run(["--buy-dino", "Nope"])
    assert unknown.returncode == 0, unknown.stderr
    assert "buy: no store entry named Nope" in unknown.stdout

def test_cpp_freegame_store():
    if not EXE.exists():
        pytest.skip("viewer not built")
    result = subprocess.run(
        [str(EXE), "--store-info", "Data/Design/Scenario/FreeGame.ini",
         "--asset-root", str(GAME)],
        capture_output=True, text=True, timeout=60,
    )
    assert result.returncode == 0, result.stderr
    assert "store: entries=20 stores=6 prices=4000..42000" in result.stdout
    assert "store stores: 0=3 2=4 3=4 4=4 5=4 6=1" in result.stdout
    assert ("first=Allosaurus 9000 allo.gen "
            "last=Velociraptor 15000 velo.gen") in result.stdout

def test_cpp_sav_budget_rows(tmp_path):
    """The budget panel imports the save's yesterday block (0x510430)."""
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail isl01 missing")
    save = ROOT / "run" / "save" / "slot001.sav"
    if not save.exists():
        pytest.skip("retail slot001 save missing")
    sys.path.insert(0, str(ROOT / "tools" / "formats"))
    from sav import read_sav  # noqa: PLC0415
    from sav_graph import read_sav_graph  # noqa: PLC0415

    header = read_sav(save)
    park = read_sav_graph(save).park
    data = bytearray(save.read_bytes())
    e = header.state_offset + park.offset + 0x30

    def put(off, value):
        struct.pack_into("<I", data, e + off, value & 0xFFFFFFFF)

    put(0xB2, 3)                    # yesterday staff bucket 0
    put(0xBA, 11); put(0xBE, 2)     # food total/count
    put(0xC2, 12); put(0xC6, 3)     # secu total/count
    shops = [(10, 1), (20, 2), (30, 3), (40, 4), (50, 5), (60, 6)]
    for i, (revenue, sales) in enumerate(shops, start=1):
        put(0xCA + i * 4, revenue)
        put(0xFE + i * 4, sales)
    totals = [0] * 13
    totals[0], totals[1] = 350, 7    # ENTREES total/count
    totals[5], totals[6] = 4, 5000   # DINOACHAT count/total
    totals[7], totals[8] = 1, 2500   # CONSTRUCTION count/total
    totals[9], totals[10] = 2, 800   # LABOACTION count/total
    totals[11], totals[12] = 3, 900  # VENTES count/total
    for i, value in enumerate(totals):
        put(0x132 + i * 4, value)
    settled = tmp_path / "settled.sav"
    settled.write_bytes(data)

    def run(source, shot):
        return subprocess.run(
            [str(EXE), "--frames", "2", "--no-menu", "--no-hud", "--width",
             "800", "--height", "600", "--no-vsync", "--no-save-config",
             "--lnd", str(ISL01), "--asset-root", str(GAME),
             "--no-vegetation", "--no-water", "--sav-park", str(source),
             "--park-click", "97,584", "--budget-info",
             "--screenshot", str(shot)],
            capture_output=True, text=True, timeout=300,
        )

    plain_shot = tmp_path / "plain.bmp"
    settled_shot = tmp_path / "settled.bmp"
    plain = run(save, plain_shot)
    result = run(settled, settled_shot)
    assert plain.returncode == 0, plain.stderr
    assert result.returncode == 0, result.stderr
    assert "budget yesterday: dinoachat=0/0 construction=0/0 tickets=0/0" \
        in plain.stdout
    log = result.stdout
    assert ("budget yesterday: dinoachat=4/5000 construction=1/2500 "
            "tickets=7/350") in log
    assert "budget totals: expenses=8326 income=1460 net=-6866" in log
    # The row area changes when the imported yesterday block is nonzero.
    a = Image.open(plain_shot).convert("RGB")
    b = Image.open(settled_shot).convert("RGB")
    diff = ImageChops.difference(a.crop((140, 170, 320, 300)),
                                 b.crop((140, 170, 320, 300)))
    assert diff.getbbox() is not None


def test_cpp_budget_panel_ledger(tmp_path):
    if not EXE.exists():
        pytest.skip("viewer not built")
    isl04 = GAME / "Data" / "Design" / "landscape" / "isl04" / "isl04.lnd"
    wld = GAME / "Data" / "Design" / "Scenario" / "Cpg04" / "Cpg04_Save.wld"
    if not isl04.exists() or not wld.exists():
        pytest.skip("retail isl04/Cpg04 missing")
    from PIL import ImageChops

    def run(extra, shot):
        result = subprocess.run(
            [str(EXE), "--frames", "2", "--no-menu", "--no-hud", "--width",
             "800", "--height", "600", "--no-vsync", "--no-save-config",
             "--lnd", str(isl04), "--asset-root", str(GAME),
             "--no-vegetation", "--no-water", "--wld-park", str(wld),
             "--park-click", "97,584", "--settle-day", "--budget-info",
             "--screenshot", str(shot)] + extra,
            capture_output=True, text=True, timeout=300,
        )
        assert result.returncode == 0, result.stderr
        return result

    zero_shot = tmp_path / "zero.bmp"
    spent_shot = tmp_path / "spent.bmp"
    zero = run([], zero_shot)
    assert ("budget yesterday: dinoachat=0/0 construction=0/0 tickets=0/0"
            in zero.stdout)
    assert "budget totals: expenses=0 income=0 net=0" in zero.stdout

    spent = run(["--buy-dino", "Zarbosaurus", "--construct", "RestCirc",
                 # (512,140) is blocked after the engine mask flip; (516,288)
                 # is a free 12x12 footprint clear of the Cpg04 buildings.
                 "--construct-pos", "516,288"], spent_shot)
    assert ("budget ledger: dinoachat=0/0 construction=0/0 tickets=0/0"
            in spent.stdout)
    assert ("budget yesterday: dinoachat=1/4900 construction=1/2500 "
            "tickets=0/0") in spent.stdout
    assert "budget totals: expenses=7400 income=0 net=-7400" in spent.stdout

    a = Image.open(zero_shot).convert("RGB")
    b = Image.open(spent_shot).convert("RGB")
    # CONSTRUCTION and DINOACHAT rows (count + cost) and the total-expenses
    # field must change between a fresh park and one with 4,900 + 2,500 spent.
    for box in ((150, 214, 310, 238), (150, 252, 310, 276),
                (230, 398, 312, 426)):
        diff = ImageChops.difference(a.crop(box), b.crop(box))
        assert diff.getbbox() is not None

def test_cpp_park_fence_and_dino_lists(tmp_path):
    if not EXE.exists():
        pytest.skip("viewer not built")
    isl04 = GAME / "Data" / "Design" / "landscape" / "isl04" / "isl04.lnd"
    wld = GAME / "Data" / "Design" / "Scenario" / "Cpg04" / "Cpg04_Save.wld"
    if not isl04.exists() or not wld.exists():
        pytest.skip("retail Cpg04/isl04 missing")

    def run(click, shot):
        return subprocess.run(
            [str(EXE), "--frames", "2", "--no-menu", "--no-input",
             "--no-vsync", "--no-save-config", "--width", "800",
             "--height", "600", "--lnd", str(isl04), "--asset-root", str(GAME),
             "--no-vegetation", "--no-water", "--wld-park", str(wld),
             "--park-click", click, "--screenshot", str(shot)],
            capture_output=True, text=True, timeout=300,
        )

    fence_shot = tmp_path / "fence-list.bmp"
    fence = run("492,584", fence_shot)
    assert fence.returncode == 0, fence.stderr
    assert "park click 492,584 -> open FENCE_LIST_PANEL" in fence.stdout
    assert "park list: dinos=20 paddocks=9" in fence.stdout
    image = Image.open(fence_shot).convert("RGB")
    # The first paddock name and its counters are drawn from park state.
    assert len(image.crop((175, 541, 239, 557)).getcolors(maxcolors=100000)) > 20
    assert len(image.crop((172, 490, 187, 501)).getcolors(maxcolors=100000)) > 5
    assert len(image.crop((172, 525, 187, 536)).getcolors(maxcolors=100000)) > 5

    dino_shot = tmp_path / "dino-list.bmp"
    dinos = run("302,584", dino_shot)
    assert dinos.returncode == 0, dinos.stderr
    assert "park click 302,584 -> open DINO_LIST_PANEL" in dinos.stdout
    image = Image.open(dino_shot).convert("RGB")
    assert len(image.crop((176, 541, 240, 557)).getcolors(maxcolors=100000)) > 20


def test_cpp_store_panel_purchase():
    """Store row select -> SAVE -> paddock click charges and updates the row."""
    if not EXE.exists():
        pytest.skip("viewer not built")
    isl04 = GAME / "Data" / "Design" / "landscape" / "isl04" / "isl04.lnd"
    wld = GAME / "Data" / "Design" / "Scenario" / "Cpg04" / "Cpg04_Save.wld"
    if not isl04.exists() or not wld.exists():
        pytest.skip("retail Cpg04/isl04 missing")
    result = subprocess.run(
        [str(EXE), "--frames", "3", "--no-menu", "--no-hud", "--width", "800",
         "--height", "600", "--no-vsync", "--no-save-config",
         "--lnd", str(isl04), "--asset-root", str(GAME),
         "--no-vegetation", "--no-water", "--wld-park", str(wld),
         # Pin the legacy spherical camera so the paddock pick is deterministic;
         # the interactive camera now starts at retail progress 0.1.
         "--distance", "300", "--pitch", "20",
         "--park-click", "765,298", "--park-click", "160,88",
         "--park-click", "470,520", "--ground-click", "400,300"],
        capture_output=True, text=True, timeout=300,
    )
    assert result.returncode == 0, result.stderr
    log = result.stdout
    assert "park click 765,298 -> open DINO_ACHAT_SAVE_PANEL" in log
    assert "park click 160,88 -> store select 0" in log
    assert "park click 470,520 -> store buy 0" in log
    assert "store: Allosaurus price=9000 selected, click a paddock" in log
    # 75,000 - 9,000; paddock 7 (456,464)-(479,487); Allosaurus is class 2
    # (32); the other 20 dinosaurs already use 336.
    assert ("store: bought Allosaurus price=9000 budget=66000 paddock=7 "
            "dinos=21 dinoachat=1/9000 used=32 total=368") in log


def test_cpp_store_attribute_gauges(tmp_path):
    """Selecting a store row fills the eleven attribute gauges and numbers."""
    if not EXE.exists():
        pytest.skip("viewer not built")
    isl04 = GAME / "Data" / "Design" / "landscape" / "isl04" / "isl04.lnd"
    wld = GAME / "Data" / "Design" / "Scenario" / "Cpg04" / "Cpg04_Save.wld"
    if not isl04.exists() or not wld.exists():
        pytest.skip("retail isl04/Cpg04 missing")
    shot = tmp_path / "gauges.bmp"
    env = dict(os.environ, DINO_DEBUG_STORE_GAUGES="1")
    result = subprocess.run(
        [str(EXE), "--frames", "4", "--no-menu", "--no-hud", "--width",
         "800", "--height", "600", "--no-vsync", "--no-save-config",
         "--lnd", str(isl04), "--asset-root", str(GAME),
         "--no-vegetation", "--no-water", "--wld-park", str(wld),
         "--park-click", "765,298", "--park-click", "160,88",
         "--park-click", "160,124",
         "--screenshot", str(shot)],
        capture_output=True, text=True, timeout=300, env=env,
    )
    assert result.returncode == 0, result.stderr
    # Allosaurus from Market.ini: the accumulator values are the live Cpg04
    # row (stamina 40, courage 70, intelligence 38, ...). The Attack gauge is
    # the jaw attack (80) because AttributeRange.ini caps it at 120; the
    # five-part total is 335. ORIGINALITY is 100 - conventional appeal
    # (+0x194); Allosaurus is fully conventional (0), the Leoparus breed has
    # distinct dummy alleles (67).
    assert ("store gauges: Allosaurus ORIGINALITY=0/100 STAMINA=40/100 "
            "COURAGE=70/100 INTELLIGENCE=38/100 AGILITY=50/100 SPEED=2/100 "
            "ARMOR=25/100 AGGRESSIVENESS=80/100 ATTACK=80/120 BEAUTY=20/100 "
            "PERCEPTION=34/100") in result.stdout
    assert ("store gauges: Allosaurus Leoparus ORIGINALITY=67/100 "
            "STAMINA=41/100 COURAGE=70/100 INTELLIGENCE=32/100 "
            "AGILITY=46/100 SPEED=2/100 ARMOR=60/100 "
            "AGGRESSIVENESS=50/100 ATTACK=51/120 BEAUTY=38/100 "
            "PERCEPTION=58/100") in result.stdout
    assert shot.exists() and shot.stat().st_size > 0


def test_cpp_store_3d_preview(tmp_path):
    """The store well renders the selected Market.ini dinosaur in 3D."""
    if not EXE.exists():
        pytest.skip("viewer not built")
    isl04 = GAME / "Data" / "Design" / "landscape" / "isl04" / "isl04.lnd"
    wld = GAME / "Data" / "Design" / "Scenario" / "Cpg04" / "Cpg04_Save.wld"
    if not isl04.exists() or not wld.exists():
        pytest.skip("retail isl04/Cpg04 missing")
    shot = tmp_path / "preview.bmp"
    env = dict(os.environ, DINO_DEBUG_STORE_PREVIEW="1")
    result = subprocess.run(
        [str(EXE), "--frames", "4", "--no-menu", "--no-hud", "--width",
         "800", "--height", "600", "--no-vsync", "--no-save-config",
         "--lnd", str(isl04), "--asset-root", str(GAME),
         "--no-vegetation", "--no-water", "--wld-park", str(wld),
         "--park-click", "765,298", "--park-click", "160,88",
         "--screenshot", str(shot)],
        capture_output=True, text=True, timeout=300, env=env,
    )
    assert result.returncode == 0, result.stderr
    log = result.stdout + result.stderr
    assert "store preview: Allosaurus genotype=allo.gen verts=" in log
    assert "ready=1" in log
    assert shot.exists() and shot.stat().st_size > 0
    # The inner well is no longer the flat dark panel: the lit dinosaur
    # supplies green/red values above the purple background.
    image = Image.open(shot).convert("RGB")
    inner = image.crop((267, 93, 267 + 144, 93 + 144))
    bright = sum(1 for r, g, _b in inner.getdata() if g > 70 and r > 50)
    assert bright > 500, bright


def test_cpp_store_panel_no_money():
    if not EXE.exists():
        pytest.skip("viewer not built")
    isl04 = GAME / "Data" / "Design" / "landscape" / "isl04" / "isl04.lnd"
    wld = GAME / "Data" / "Design" / "Scenario" / "Cpg04" / "Cpg04_Save.wld"
    if not isl04.exists() or not wld.exists():
        pytest.skip("retail Cpg04/isl04 missing")
    result = subprocess.run(
        [str(EXE), "--frames", "3", "--no-menu", "--no-hud", "--width", "800",
         "--height", "600", "--no-vsync", "--no-save-config",
         "--lnd", str(isl04), "--asset-root", str(GAME),
         "--no-vegetation", "--no-water", "--wld-park", str(wld),
         "--park-budget", "100",
         "--park-click", "765,298", "--park-click", "160,88",
         "--park-click", "470,520", "--ground-click", "400,300"],
        capture_output=True, text=True, timeout=300,
    )
    assert result.returncode == 0, result.stderr
    assert ("store: Allosaurus price=9000 budget=100 "
            "(WARNING_NOT_ENOUGH_MONEY)") in result.stdout
    assert "store: bought" not in result.stdout


def test_cpp_store_message_keeps_panel(tmp_path):
    """A failed purchase uses the retail MESSAGE_PANEL toast, not a modal
    warning: the text appears in the top-left yellow message panel and the
    store panel stays open (state 0x13 does not change)."""
    if not EXE.exists():
        pytest.skip("viewer not built")
    isl04 = GAME / "Data" / "Design" / "landscape" / "isl04" / "isl04.lnd"
    wld = GAME / "Data" / "Design" / "Scenario" / "Cpg04" / "Cpg04_Save.wld"
    if not isl04.exists() or not wld.exists():
        pytest.skip("retail Cpg04/isl04 missing")
    common = [str(EXE), "--frames", "3", "--no-menu", "--no-hud", "--width",
              "800", "--height", "600", "--no-vsync", "--no-save-config",
              "--lnd", str(isl04), "--asset-root", str(GAME),
              "--no-vegetation", "--no-water", "--wld-park", str(wld),
              "--park-budget", "100", "--park-click", "765,298",
              "--park-click", "160,88", "--camera-orbit"]
    env = dict(os.environ, DINO_DEBUG_MESSAGE="1")
    opened = tmp_path / "opened.bmp"
    refused = tmp_path / "refused.bmp"
    first = subprocess.run(common + ["--screenshot", str(opened)], env=env,
                           capture_output=True, text=True, timeout=300)
    assert first.returncode == 0, first.stderr
    assert "park message:" not in first.stderr + first.stdout
    second = subprocess.run(
        common + ["--park-click", "470,520", "--screenshot", str(refused)],
        env=env, capture_output=True, text=True, timeout=300)
    assert second.returncode == 0, second.stderr
    assert "park click 470,520 -> store buy 0" in second.stdout
    assert "store: Allosaurus price=9000 budget=100 " \
           "(WARNING_NOT_ENOUGH_MONEY)" in second.stdout
    assert "park message: Not enough money!" in second.stderr + second.stdout
    assert "park warning:" not in second.stdout
    # No modal backdrop: the scene pixel and the lower store panel (outside
    # the toast area) are unchanged between "opened" and "refused".
    a = Image.open(opened).convert("RGB")
    b = Image.open(refused).convert("RGB")
    assert a.getpixel((600, 400)) == b.getpixel((600, 400))
    panel = (320, 120, 520, 520)
    diff = ImageChops.difference(a.crop(panel), b.crop(panel))
    assert diff.getbbox() is None, diff.getbbox()
    # The yellow MESSAGE_PANEL line lives at 100,20..302,102.
    area = b.crop((100, 20, 302, 42)).tobytes()
    yellow = sum(1 for i in range(0, len(area), 3)
                 if area[i] > 180 and area[i + 1] > 180 and area[i + 2] < 120)
    assert yellow > 40, yellow


def test_cpp_store_catalog_mission(tmp_path):
    """Campaign missions use their effective [DINOSTORE] block (own or
    inherited from MissionBase.ini), not Market.ini."""
    if not EXE.exists():
        pytest.skip("viewer not built")
    isl04 = GAME / "Data" / "Design" / "landscape" / "isl04" / "isl04.lnd"
    wld = GAME / "Data" / "Design" / "Scenario" / "Cpg04" / "Cpg04_Save.wld"
    mission = GAME / "Data" / "Design" / "Scenario" / "Cpg04" / "Cpg04_Mission2.ini"
    empty = GAME / "Data" / "Design" / "Scenario" / "Cpg04" / "Cpg04_Mission4.ini"
    inherited = GAME / "Data" / "Design" / "Scenario" / "Cpg01" / "Cpg01_Mission1.ini"
    if not (isl04.exists() and wld.exists() and mission.exists() and
            empty.exists() and inherited.exists()):
        pytest.skip("retail Cpg04/isl04/missions missing")
    progress = tmp_path / "campaign-progress.txt"
    base = [str(EXE), "--frames", "3", "--no-menu", "--no-hud", "--width",
            "800", "--height", "600", "--no-vsync", "--no-save-config",
            "--lnd", str(isl04), "--asset-root", str(GAME),
            "--no-vegetation", "--no-water", "--wld-park", str(wld),
            "--campaign-progress", str(progress)]
    env = dict(os.environ, DINO_DEBUG_STORE_GAUGES="1")
    result = subprocess.run(
        base + ["--start-mission", str(mission),
                "--park-click", "765,298", "--park-click", "160,88"],
        env=env, capture_output=True, text=True, timeout=300)
    assert result.returncode == 0, result.stderr
    log = result.stdout
    assert "store catalog: Cpg04_Mission2 entries=4" in log
    assert "store gauges: Kentrosaurus" in log
    assert "store gauges: Allosaurus" not in log.split(
        "store catalog: Cpg04_Mission2")[-1]
    # A mission with an all-empty [DINOSTORE] block has no catalog rows.
    result = subprocess.run(
        base + ["--start-mission", str(empty),
                "--park-click", "765,298", "--park-click", "160,88"],
        env=env, capture_output=True, text=True, timeout=300)
    assert result.returncode == 0, result.stderr
    log = result.stdout
    assert "store catalog: Cpg04_Mission4 entries=0" in log
    assert "store gauges:" not in log.split(
        "store catalog: Cpg04_Mission4")[-1]
    # Cpg01 has no own block: the inherited MissionBase pages are all empty.
    result = subprocess.run(
        base + ["--start-mission", str(inherited),
                "--park-click", "765,298", "--park-click", "160,88"],
        env=env, capture_output=True, text=True, timeout=300)
    assert result.returncode == 0, result.stderr
    log = result.stdout
    assert "store catalog: Cpg01_Mission1 entries=0" in log
    assert "store gauges:" not in log.split(
        "store catalog: Cpg01_Mission1")[-1]
    # The test must not leak the active mission into run/save/.
    assert progress.exists()


def test_cpp_store_catalog_freegame(tmp_path):
    """Free game uses FreeGame.ini's 20-entry [DINOSTORE] subset."""
    if not EXE.exists() or not ISL01.exists():
        pytest.skip("viewer or retail isl01 missing")
    shot = tmp_path / "freegame-store.bmp"
    result = subprocess.run(
        [str(EXE), "--frames", "4", "--screenshot", str(shot),
         "--width", "800", "--height", "600", "--no-vsync", "--no-save-config",
         "--no-hud", "--asset-root", str(GAME),
         "--menu-page", "1", "--menu-click", "200,180",
         "--menu-click", "494,451", "--menu-click", "526,531",
         "--menu-loading-time", "0"],
        capture_output=True, text=True, timeout=180)
    assert result.returncode == 0, result.stderr
    assert "store catalog: FreeGame.ini entries=20" in result.stdout
    assert "start: island=isl01 difficulty=0 budget=75000" in result.stdout


def test_cpp_store_panel_scroll():
    """Scrollbar strip scrolls the six-row viewport before selecting."""
    if not EXE.exists():
        pytest.skip("viewer not built")
    isl04 = GAME / "Data" / "Design" / "landscape" / "isl04" / "isl04.lnd"
    wld = GAME / "Data" / "Design" / "Scenario" / "Cpg04" / "Cpg04_Save.wld"
    if not isl04.exists() or not wld.exists():
        pytest.skip("retail Cpg04/isl04 missing")
    result = subprocess.run(
        [str(EXE), "--frames", "3", "--no-menu", "--no-hud", "--width", "800",
         "--height", "600", "--no-vsync", "--no-save-config",
         "--lnd", str(isl04), "--asset-root", str(GAME),
         "--no-vegetation", "--no-water", "--wld-park", str(wld),
         "--distance", "300", "--pitch", "20",
         "--park-click", "765,298", "--park-click", "232,200",
         "--park-click", "160,88", "--park-click", "470,520",
         "--ground-click", "400,300"],
        capture_output=True, text=True, timeout=300,
    )
    assert result.returncode == 0, result.stderr
    log = result.stdout
    assert "park click 232,200 -> store scroll down" in log
    assert "park click 160,88 -> store select 1" in log
    assert "park click 470,520 -> store buy 1" in log
    assert ("store: Allosaurus_Leoparus price=10000 selected, click a paddock"
            in log)
    assert ("store: bought Allosaurus_Leoparus price=10000 budget=65000 "
            "paddock=7 dinos=21 dinoachat=1/10000 used=32 total=368") in log


def test_cpp_food_info():
    if not EXE.exists():
        pytest.skip("viewer not built")
    food_dir = GAME / "Data" / "Design" / "foodsource"
    if not food_dir.exists():
        pytest.skip("retail foodsource missing")
    result = subprocess.run(
        [str(EXE), "--asset-root", str(GAME),
         "--food-info", "Data/Design/foodsource"],
        capture_output=True, text=True, timeout=120,
    )
    assert result.returncode == 0, result.stderr
    lines = [line for line in result.stdout.splitlines()
             if line.startswith("food: ")]
    assert len(lines) == 10
    assert lines[0] == (
        "food: FlowerFood nutrition=100 quantity=0.25 price=100 kind=0 "
        "flags=256 feeder=DATA\\graphics\\actors\\people\\personnel\\feed\\"
        "feed_flower.mesh ui=FOOD_DISTRIBUTION_VEGETARIAN_FLOWER_BUTTON")
    assert lines[-1] == (
        "food: CowFood nutrition=800 quantity=7 price=1200 kind=9 "
        "flags=256 feeder=DATA\\graphics\\actors\\people\\personnel\\feed\\"
        "feed_cow.mesh ui=FOOD_DISTRIBUTION_CARNIVOROUS_COW_BUTTON")
    assert "food sources: 10" in result.stdout
    for line in lines:
        feeder = line.split("feeder=")[1].split(" ui=")[0].replace("\\", "/")
        assert (GAME / feeder).exists(), feeder


def test_cpp_food_feeding():
    """A placed .bsi source gates Eat by paddock and diet, then depletes."""
    if not EXE.exists():
        pytest.skip("viewer not built")
    isl04 = GAME / "Data" / "Design" / "landscape" / "isl04" / "isl04.lnd"
    wld = GAME / "Data" / "Design" / "Scenario" / "Cpg04" / "Cpg04_Save.wld"
    if not isl04.exists() or not wld.exists():
        pytest.skip("retail Cpg04/isl04 missing")

    def run(frames, speed=None, needs=None):
        args = [
            str(EXE), "--frames", str(frames), "--no-menu", "--no-hud",
            "--width", "160", "--height", "120", "--no-vsync",
            "--no-save-config", "--lnd", str(isl04),
            "--asset-root", str(GAME), "--no-vegetation", "--no-water",
            "--wld-park", str(wld), "--place-food", "PigFood",
            "--food-pos", "118,94", "--dino-action-loop",
        ]
        if speed is not None:
            args += ["--dino-action-speed", str(speed)]
        if needs is not None:
            args += ["--dino-needs", needs]
        result = subprocess.run(args, capture_output=True, text=True,
                                timeout=300)
        assert result.returncode == 0, result.stderr
        return result.stdout

    log = run(3, needs="20,90,50,90")
    assert "food: PigFood kind=8 at 474.0,378.0 portions=5.00" in log
    # Row 1 is the herbivore anky in paddock 0; paddock 1 holds three allos.
    assert ("dino food: anky paddock=0 diet=herbivore source=none"
            in log)
    assert ("dino food: allo paddock=1 diet=carnivore source=PigFood"
            in log)
    assert log.count(" Eat score=9009 ") == 3
    assert "dino eat:" not in log

    # Fast actions: five meals deplete the pig and the source leaves the scene.
    log = run(900, speed=100, needs="20,90,50,90")
    assert log.count("dino eat: allo <- PigFood") == 5
    assert "remaining=0.00" in log
    assert "food: PigFood depleted" in log
    # After depletion the carnivores fall back to Exploration.
    assert "dino action: allo Exploration" in log


def test_cpp_employee_info():
    """The .bei reader returns the two retail employee profiles."""
    if not EXE.exists():
        pytest.skip("viewer not built")
    result = subprocess.run(
        [str(EXE), "--employee-info", "--asset-root", str(GAME)],
        capture_output=True, text=True, timeout=60,
    )
    assert result.returncode == 0, result.stderr
    log = result.stdout
    assert ("employee: Nutritionist kind=0 wage=500 height=1.70 walk=2.00 "
            "action=2.50 work=3.00 meshes=5 sounds=2") in log
    assert ("employee: Guard kind=1 wage=500 height=1.70 walk=1.00 "
            "action=2.50 work=3.00 meshes=4 sounds=2") in log
    assert "employee profiles: 2" in log


def test_cpp_employee_feeding_and_wages():
    """A nutritionist feeds the placed source; wages hit the daily settlement."""
    if not EXE.exists():
        pytest.skip("viewer not built")
    isl04 = GAME / "Data" / "Design" / "landscape" / "isl04" / "isl04.lnd"
    wld = GAME / "Data" / "Design" / "Scenario" / "Cpg04" / "Cpg04_Save.wld"
    if not isl04.exists() or not wld.exists():
        pytest.skip("retail Cpg04/isl04 missing")
    result = subprocess.run(
        [str(EXE), "--frames", "2", "--no-menu", "--no-hud", "--width", "160",
         "--height", "120", "--no-vsync", "--no-save-config",
         "--lnd", str(isl04), "--asset-root", str(GAME),
         "--no-vegetation", "--no-water", "--wld-park", str(wld),
         "--place-food", "PigFood", "--food-pos", "118,94",
         "--hire-nutritionist", "2", "--feed-now", "--settle-day",
         "--park-budget", "75000"],
        capture_output=True, text=True, timeout=300,
    )
    assert result.returncode == 0, result.stderr
    log = result.stdout
    # The `.bei` dword 8 (500) is both the hire cost and the daily wage.
    assert "employees: hired=2 wage=500 budget=74000" in log
    assert "feed: assigned=1 completed=1 serviced=1 sources=1" in log
    assert "feed: source def=8 remaining=5.000 capacity=5.000" in log
    # 75,000 - 2*500 hire - 2*500 wages = 73,000 (no visitors in the .wld).
    assert ("daily settlement: day=0 visitors=0 ticket=50 income=0 "
            "wages=1000 budget=73000") in log


def test_cpp_employee_save_roundtrip(tmp_path):
    """Hired Nutritionists are written to DINOPSAV and restored on load."""
    if not EXE.exists():
        pytest.skip("viewer not built")
    isl04 = GAME / "Data" / "Design" / "landscape" / "isl04" / "isl04.lnd"
    wld = GAME / "Data" / "Design" / "Scenario" / "Cpg04" / "Cpg04_Save.wld"
    if not isl04.exists() or not wld.exists():
        pytest.skip("retail Cpg04/isl04 missing")
    save = tmp_path / "employees.psav"
    base = [str(EXE), "--frames", "2", "--no-menu", "--no-hud", "--width",
            "160", "--height", "120", "--no-vsync", "--no-save-config",
            "--lnd", str(isl04), "--asset-root", str(GAME),
            "--no-vegetation", "--no-water", "--wld-park", str(wld),
            "--park-budget", "75000"]
    first = subprocess.run(
        base + ["--hire-nutritionist", "2", "--save-port", str(save)],
        capture_output=True, text=True, timeout=300,
    )
    assert first.returncode == 0, first.stderr
    assert "employees: hired=2" in first.stdout
    text = save.read_text(encoding="utf-8")
    assert text.count("employee Nutritionist") == 2
    second = subprocess.run(
        base + ["--load-port", str(save)],
        capture_output=True, text=True, timeout=300,
    )
    assert second.returncode == 0, second.stderr
    assert "port employees: 2" in second.stdout


def test_cpp_show_info():
    """The .shw reader matches the retail show catalog."""
    if not EXE.exists():
        pytest.skip("viewer not built")
    show_dir = GAME / "Data" / "Design" / "Scenario" / "Show"
    if not show_dir.exists():
        pytest.skip("retail shows missing")
    result = subprocess.run(
        [str(EXE), "--show-info", str(show_dir), "--asset-root", str(GAME)],
        capture_output=True, text=True, timeout=120,
    )
    assert result.returncode == 0, result.stderr
    assert ("show: Cpg1_Race_1 name=&cpg1_race_1 cost=1800 judge=1 "
            "dims=19x39 wall=1 cages=1,0,0,0 features=14 teams=2") in result.stdout
    assert "bfi features: 62" in result.stdout
    assert "show tiling: Cpg1_Race_1 cells=224 overlaps=0" in result.stdout
    assert sum("overlaps=0" in line for line in result.stdout.splitlines()) == 43
    assert "shows: 43" in result.stdout
    single = subprocess.run(
        [str(EXE), "--show-info", str(show_dir / "Cpg1_Race_1.shw"),
         "--asset-root", str(GAME)],
        capture_output=True, text=True, timeout=120,
    )
    assert single.returncode == 0, single.stderr
    assert "show feature: idx=4 rot=0 x=0 y=18 name=JudgeRace" in single.stdout
    assert "show team: cage=0 team=1" in single.stdout


def test_cpp_port_save_paddocks_food(tmp_path):
    """Paddock capacity/occupancy and placed food survive a .psav round-trip."""
    if not EXE.exists():
        pytest.skip("viewer not built")
    isl04 = GAME / "Data" / "Design" / "landscape" / "isl04" / "isl04.lnd"
    wld = GAME / "Data" / "Design" / "Scenario" / "Cpg04" / "Cpg04_Save.wld"
    if not isl04.exists() or not wld.exists():
        pytest.skip("retail Cpg04/isl04 missing")
    target = tmp_path / "feed.psav"
    save = subprocess.run(
        [str(EXE), "--frames", "2", "--no-menu", "--no-hud", "--width", "160",
         "--height", "120", "--no-vsync", "--no-save-config",
         "--lnd", str(isl04), "--asset-root", str(GAME),
         "--no-vegetation", "--no-water", "--wld-park", str(wld),
         "--place-food", "PigFood", "--food-pos", "118,94",
         "--dino-action-loop", "--dino-needs", "20,90,50,90",
         "--save-port", str(target)],
        capture_output=True, text=True, timeout=300,
    )
    assert save.returncode == 0, save.stderr
    text = target.read_text()
    paddocks = [ln for ln in text.splitlines() if ln.startswith("paddock ")]
    assert len(paddocks) == 9
    assert any(ln.startswith("food PigFood 474 378 5") for ln in text.splitlines())

    load = subprocess.run(
        [str(EXE), "--frames", "4", "--no-menu", "--no-hud", "--width", "160",
         "--height", "120", "--no-vsync", "--no-save-config",
         "--asset-root", str(GAME), "--load-port", str(target),
         "--dino-action-loop"],
        capture_output=True, text=True, timeout=300,
    )
    assert load.returncode == 0, load.stderr
    assert ("port load: island=isl04 budget=75000 day=0 visitors=0 "
            "paths=301 fences=268 buildings=57 dinos=20 paddocks=9 food=1"
            in load.stdout)
    assert "dino food: allo paddock=1 diet=carnivore source=PigFood" in load.stdout
    # The diet gene is saved too: the anky in paddock 0 stays herbivorous.
    assert "dino food: anky paddock=0 diet=herbivore source=none" in load.stdout
    assert "Eat score=9009 needs=20/90/50/90" in load.stdout

def test_cpp_sav_park_roundtrip(tmp_path):
    """The C++ writer reproduces retail saves byte-for-byte."""
    if not EXE.exists():
        pytest.skip("viewer not built")
    saves = sorted((ROOT / "run" / "save").rglob("*.sav"))
    saves += sorted((ROOT / "run-test" / "save").rglob("*.sav"))
    # Stable copy of a populated campaign autosave (20 list0 saDinosaur
    # records); a live run-test autosave may have been replaced by free play.
    campaign = ROOT / "build" / "campaign-autosave.sav"
    if campaign.exists() and campaign.stat().st_size > 20000:
        saves.append(campaign)
    checked = 0
    for index, save in enumerate(saves):
        if not save.exists():
            continue
        result = subprocess.run(
            [str(EXE), "--sav-roundtrip", str(save), "--no-save-config"],
            capture_output=True, text=True, timeout=120)
        assert result.returncode == 0, result.stderr
        assert "sav roundtrip: whole file identical" in result.stdout, result.stdout
        assert "world copied" not in result.stdout, result.stdout

        out_path = tmp_path / f"{index}-{save.name}"
        written = subprocess.run(
            [str(EXE), "--sav-write", str(save), str(out_path),
             "--no-save-config"],
            capture_output=True, text=True, timeout=120)
        assert written.returncode == 0, written.stderr
        assert "sav written:" in written.stdout, written.stdout
        assert out_path.read_bytes() == save.read_bytes(), save
        checked += 1
    if checked == 0:
        pytest.skip("retail saves not present")


def test_cpp_campaign_corrupt_list0(tmp_path):
    """A corrupt list0 name length fails cleanly instead of desynchronizing."""
    if not EXE.exists():
        pytest.skip("viewer not built")
    campaign = ROOT / "build" / "campaign-autosave.sav"
    if not campaign.exists() or campaign.stat().st_size < 20000:
        pytest.skip("campaign autosave not present")
    sys.path.insert(0, str(ROOT / "tools" / "formats"))
    from sav import read_sav

    save = read_sav(campaign)
    data = bytearray(campaign.read_bytes())
    roads = struct.unpack_from("<I", data, save.state_offset)[0]
    list0_at = save.state_offset + 4 + roads * 16
    assert struct.unpack_from("<I", data, list0_at)[0] == 20
    struct.pack_into("<I", data, list0_at + 4 + 0x15, 0xFFFFFFF0)
    bad = tmp_path / "bad-campaign.sav"
    bad.write_bytes(data)
    result = subprocess.run(
        [str(EXE), "--sav-roundtrip", str(bad), "--no-save-config"],
        capture_output=True, text=True, timeout=120)
    assert result.returncode == 2, result.stdout
    assert "bad campaign dino name length" in result.stderr, result.stderr


def test_cpp_sav_write_rename(tmp_path):
    """--sav-name/--sav-slot change only the header; the body is untouched."""
    if not EXE.exists():
        pytest.skip("viewer not built")
    save = ROOT / "run" / "save" / "slot001.sav"
    if not save.exists():
        pytest.skip("retail save not present")
    out_path = tmp_path / "renamed.sav"
    result = subprocess.run(
        [str(EXE), "--sav-write", str(save), str(out_path),
         "--sav-name", "renamed", "--sav-slot", "3", "--no-save-config"],
        capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stderr
    source = save.read_bytes()
    rebuilt = out_path.read_bytes()
    version, slot, name_len = struct.unpack_from("<3I", rebuilt, 0)
    source_name_len = struct.unpack_from("<I", source, 8)[0]
    assert version == 53
    assert slot == 3
    assert rebuilt[0x0c:0x0c + name_len].decode("utf-16-le") == "renamed\x00"
    assert rebuilt[0x18 + name_len:] == source[0x18 + source_name_len:]
    check = subprocess.run(
        [str(EXE), "--sav-roundtrip", str(out_path), "--no-save-config"],
        capture_output=True, text=True, timeout=120)
    assert check.returncode == 0, check.stderr
    assert "whole file identical" in check.stdout, check.stdout


def test_cpp_sav_info():
    """--sav-info prints the header/world/simulation summary."""
    if not EXE.exists():
        pytest.skip("viewer not built")
    save = ROOT / "run" / "save" / "slot001.sav"
    if not save.exists():
        pytest.skip("retail save not present")
    result = subprocess.run(
        [str(EXE), "--sav-info", str(save), "--no-save-config"],
        capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stderr
    assert "sav: version=53 slot=0 name='agenttest'" in result.stdout
    assert "sav catalog: 39" in result.stdout
    assert "sav park: budget=" in result.stdout
    assert "sav objects:" in result.stdout
    assert "sav sim:" in result.stdout


def test_cpp_sav_write_budget(tmp_path):
    """--sav-budget changes the park block field and still round-trips."""
    if not EXE.exists():
        pytest.skip("viewer not built")
    save = ROOT / "run" / "save" / "slot001.sav"
    if not save.exists():
        pytest.skip("retail save not present")
    sys.path.insert(0, str(ROOT / "tools" / "formats"))
    from sav_graph import read_sav_graph

    out_path = tmp_path / "budget.sav"
    result = subprocess.run(
        [str(EXE), "--sav-write", str(save), str(out_path),
         "--sav-budget", "12345", "--no-save-config"],
        capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stderr
    graph = read_sav_graph(out_path)
    assert graph.park.budget == 12345
    check = subprocess.run(
        [str(EXE), "--sav-roundtrip", str(out_path), "--no-save-config"],
        capture_output=True, text=True, timeout=120)
    assert check.returncode == 0, check.stderr
    assert "whole file identical" in check.stdout, check.stdout


def test_cpp_sav_manual_copy_in_auto_dir(tmp_path):
    """The AI-entry split follows the park header, not the directory name."""
    if not EXE.exists():
        pytest.skip("viewer not built")
    save = ROOT / "run" / "save" / "slot001.sav"
    if not save.exists():
        pytest.skip("retail save not present")
    auto_dir = tmp_path / "auto"
    auto_dir.mkdir()
    copy = auto_dir / "slot001.sav"
    shutil.copyfile(save, copy)
    result = subprocess.run(
        [str(EXE), "--sav-roundtrip", str(copy), "--no-save-config"],
        capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stderr
    assert "whole file identical" in result.stdout, result.stdout


def test_cpp_sav_write_refuses_bad_input(tmp_path):
    """A truncated save errors cleanly and no output is written."""
    if not EXE.exists():
        pytest.skip("viewer not built")
    save = ROOT / "run" / "save" / "slot001.sav"
    if not save.exists():
        pytest.skip("retail save not present")
    broken = tmp_path / "truncated.sav"
    broken.write_bytes(save.read_bytes()[:-8])
    out_path = tmp_path / "out.sav"
    result = subprocess.run(
        [str(EXE), "--sav-write", str(broken), str(out_path),
         "--no-save-config"],
        capture_output=True, text=True, timeout=120)
    assert result.returncode == 2, result.stdout
    assert "sav read failed:" in result.stderr, result.stderr
    assert not out_path.exists()
    forced = subprocess.run(
        [str(EXE), "--sav-write", str(broken), str(out_path), "--sav-force",
         "--no-save-config"],
        capture_output=True, text=True, timeout=120)
    assert forced.returncode == 2, forced.stdout
    assert not out_path.exists()


def test_cpp_sav_write_cross_checked_by_python(tmp_path):
    """The C++-written save is accepted by the independent Python reference."""
    if not EXE.exists():
        pytest.skip("viewer not built")
    save = ROOT / "run" / "save" / "slot001.sav"
    if not save.exists():
        pytest.skip("retail save not present")
    sys.path.insert(0, str(ROOT / "tools" / "formats"))
    from sav import read_sav
    from sav_write import rewrite_sav_whole
    from wld import read_sav_world

    out_path = tmp_path / "cpp-renamed.sav"
    result = subprocess.run(
        [str(EXE), "--sav-write", str(save), str(out_path),
         "--sav-name", "renamed", "--sav-slot", "3", "--no-save-config"],
        capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stderr

    parsed = read_sav(out_path)
    assert parsed.version == 53
    assert parsed.slot == 3
    assert parsed.name == "renamed"
    world = read_sav_world(out_path)
    assert world.end == parsed.count_offset
    original, rebuilt, _ = rewrite_sav_whole(out_path)
    assert original == rebuilt
