import math
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from config import (  # noqa: E402
    read_camera,
    read_config_bundle,
    read_dino_ini,
    read_keymapper,
    read_keytable,
    read_start_ini,
)

GAME = ROOT / "game" / "%MAINDIR%"
CONFIG = GAME / "Data" / "Config"


def test_keymapper():
    if not CONFIG.exists():
        pytest.skip("retail sample not present")
    bindings = read_keymapper(CONFIG / "KeyMapper.ini")
    assert len(bindings) == 17
    assert bindings[0].command == "CameraRotateLeft"
    assert bindings[0].key == "FlècheGauche"
    assert bindings[0].modifier == "+Shift"
    assert bindings[1].modifier == ""
    commands = {binding.command for binding in bindings}
    assert {
        "CameraRotateLeft",
        "CameraRotateRight",
        "CameraRotateUp",
        "CameraRotateDown",
        "CameraKeyUp",
        "CameraKeyDown",
        "CameraKeyLeft",
        "CameraKeyRight",
        "RotateFeatureLeft",
        "RotateFeatureRight",
        "Escape",
        "ShowRasters",
        "PauseGame",
    } <= commands


def test_keytable():
    if not CONFIG.exists():
        pytest.skip("retail sample not present")
    rows = read_keytable(CONFIG / "key" / "KeyFrancais.ini")
    assert len(rows) == 144
    escape = next(row for row in rows if row.dik == "DIK_ESCAPE")
    assert escape.value == 0x01
    assert escape.name == "Esc"
    q = next(row for row in rows if row.dik == "DIK_Q")
    assert q.name == "a"  # French AZERTY layout
    assert q.ascii_plain == "a" and q.ascii_shift == "A"
    assert all(row.mik.startswith("MIK_") for row in rows)
    assert [row.value for row in rows] == sorted(row.value for row in rows)


def test_camera():
    if not CONFIG.exists():
        pytest.skip("retail sample not present")
    cameras = read_camera(CONFIG / "camera.ini")
    assert [camera.name for camera in cameras] == [
        "Config1", "Config2", "Config3", "Config4", "Config5",
    ]
    values = cameras[0].values
    assert values["MOVESPEEDMIN"] == 1.0
    assert values["MOVESPEEDMAX"] == 20.0
    assert values["PROGRESSSPEEDMIN"] == 0.05
    assert values["PROGRESSSPEEDMAX"] == 0.2
    assert values["ROTATIONSPEEDMIN"] == pytest.approx(math.pi / 2)
    assert values["ROTATIONSPEEDMAX"] == pytest.approx(math.pi / 2)
    assert values["ELEVATIONMIN"] == 3.0
    assert values["ELEVATIONMAX"] == 500.0
    assert values["DISTANCEMIN"] == 10.0
    assert values["DISTANCEMAX"] == 300.0
    for camera in cameras[1:]:
        assert camera.values == values


def test_start_and_dino():
    if not CONFIG.exists():
        pytest.skip("retail sample not present")
    start = read_start_ini(CONFIG / "Start.Ini")
    assert start["mgWindowName"] == "mcWorld"
    assert start["mgFullScreen"] == "1"
    assert start["mgFullScreenSize"] == "800,600"
    assert start["miKeyLoad"] == r"data\config\Key\KeyFrancais.ini"
    dino = read_dino_ini(CONFIG / "Dino.ini")
    assert dino["DINO"]["GAMESPEED"] == "50"
    assert dino["GRAPHIC"]["RESOLUTION"] == "800x600x16"
    assert dino["SOUND"]["USESOUND"] == "1"


def test_bundle():
    if not CONFIG.exists():
        pytest.skip("retail sample not present")
    bundle = read_config_bundle(CONFIG)
    assert len(bundle.bindings) == 17
    assert len(bundle.keys) == 144
    assert len(bundle.cameras) == 5
    assert bundle.start["mgWindowName"] == "mcWorld"
