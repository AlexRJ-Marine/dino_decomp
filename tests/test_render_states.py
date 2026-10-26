import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from render_states import (  # noqa: E402
    STAGE_STATES,
    read_render_state,
    scan,
)

GAME = ROOT / "game" / "%MAINDIR%"
STATES = GAME / "Data" / "States"


def test_all_profiles_parse():
    if not STATES.exists():
        pytest.skip("retail sample not present")
    profiles = scan(STATES)
    assert len(profiles) == 34
    for profile in profiles:
        assert profile.states, profile.path
        for state in profile.states:
            assert state.name.startswith("mgState")


def test_standard_expands_includes():
    if not STATES.exists():
        pytest.skip("retail sample not present")
    standard = read_render_state(STATES / "Standard.ini")
    assert standard.includes == ["Misc.txt", "TextureParam.txt", "FogParam.txt"]
    assert standard.last("mgStateFillMode").text == "Solid"
    assert standard.last("mgStateCullMode").text == "CCW"
    assert standard.last("mgStateZBufferFunc").text == "LessEqual"
    assert standard.last("mgStateAlphaBlendSrc").text == "SrcAlpha"
    assert standard.last("mgStateAlphaBlendDest").text == "InvSrcAlpha"
    # From Misc.txt / TextureParam.txt / FogParam.txt.
    assert standard.last("mgStateDither").as_bool() is True
    assert standard.last("mgStateTextureOp", stage=0).text == "Modulate"
    assert standard.last("mgStateTextureOp", stage=1).text == "Disable"
    assert standard.last("mgStateFogDistStart").as_float() == 1500.0
    assert standard.last("mgStateFogDistEnd").as_float() == 2000.0
    assert standard.last("mgStateFogDistColor").as_color() == (255, 220, 165)


def test_texture_stages_are_grouped():
    if not STATES.exists():
        pytest.skip("retail sample not present")
    water = read_render_state(STATES / "TextureParam_WaterSurface.txt")
    assert water.last("mgStateTextureOp", stage=0).text == "SelectArg1"
    assert water.last("mgStateTextureOp", stage=1).text == "Modulate2x"
    assert water.last("mgStateTextureOp", stage=2).text == "Disable"
    assert water.last("mgStateTextureAlphaOp", stage=0).text == "SelectArg1"
    assert water.last("mgStateTextureCoordIndex", stage=1).as_int() == 1
    assert water.last("mgStateTextureWrapU", stage=0).text == "Wrap"


def test_stage_states_are_explicit():
    if not STATES.exists():
        pytest.skip("retail sample not present")
    for profile in scan(STATES):
        for state in profile.states:
            if state.name in STAGE_STATES and not state.free:
                assert state.stage is not None, (state.name, state.args)
            else:
                assert state.stage is None, (state.name, state.args)


def test_every_name_is_documented():
    if not STATES.exists():
        pytest.skip("retail sample not present")
    description = (STATES / "Description.txt").read_text(
        encoding="cp1252", errors="replace"
    )
    # Four retail names are missing from (or misspelled in) Description.txt.
    undocumented = {
        "mgStateAlphablendOp",
        "mgStatePointSizeMax",
        "mgStateStencilFunc",
        "mgStateTextureMaxAnisotropy",
    }
    for profile in scan(STATES):
        for state in profile.states:
            if state.name in undocumented:
                continue
            assert state.name in description, state.name
