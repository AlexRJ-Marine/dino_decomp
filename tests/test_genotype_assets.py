import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from genotype_assets import (  # noqa: E402
    read_adornment,
    read_skin_carapace,
)
from simu_trans import read_simu_trans  # noqa: E402

GAME = ROOT / "game" / "%MAINDIR%"
GENOTYPE = GAME / "Data" / "Design" / "Genotype"
SKIN = GENOTYPE / "SkinCarapaceData.ini"
ADORNMENT = GENOTYPE / "AdornmentData.ini"
ELEMENTS = GAME / "Data" / "Graphics" / "Actors" / "elements_sup"


def test_skin_carapace_counts():
    if not SKIN.exists():
        pytest.skip("retail sample not present")
    data = read_skin_carapace(SKIN)
    assert data.skin_dir == r"DATA\graphics\TEXTURES\actors\elements_sup\bonus\peau"
    assert len(data.skins) == 22
    assert data.skins[0] == "void"
    assert data.skins[1] == "bonus01.tga"
    assert len(set(data.skins)) == 22
    assert (
        data.carapace_dir
        == r"DATA\graphics\TEXTURES\actors\elements_sup\bonus\armure"
    )
    assert len(data.carapaces) == 16
    assert data.carapaces[0] == ["void", "hurt.tga", "blood.tga"]
    assert data.carapaces[1] == [
        "armor_01_s.tga",
        "armor_01_s_hurt.tga",
        "armor_01_s_blood.tga",
    ]
    assert data.carapaces[-1] == [
        "darkmaul.tga",
        "darkmaul_hurt.tga",
        "darkmaul_blood.tga",
    ]


def test_skin_carapace_textures_exist():
    if not SKIN.exists():
        pytest.skip("retail sample not present")
    data = read_skin_carapace(SKIN)
    for name in data.skins:
        if name.lower() == "void":
            continue
        path = GAME / data.skin_dir.replace("\\", "/") / name
        assert path.exists(), path
    for row in data.carapaces:
        for name in row:
            if name.lower() == "void":
                continue
            path = GAME / data.carapace_dir.replace("\\", "/") / name
            assert path.exists(), path


def test_maps_fit_the_texture_lists():
    """Part 7/8 maps must index the 22 skins and 16 carapaces."""
    if not (SKIN.exists() and (GENOTYPE / "simu_trans.dic").exists()):
        pytest.skip("retail sample not present")
    data = read_skin_carapace(SKIN)
    trans = read_simu_trans(GENOTYPE / "simu_trans.dic")
    for row in trans.skin_map:
        for index in row:
            assert 0 <= index < len(data.skins)
    for row in trans.carapace_map:
        for index in row:
            assert 0 <= index < len(data.carapaces)


def test_adornment_counts():
    if not ADORNMENT.exists():
        pytest.skip("retail sample not present")
    data = read_adornment(ADORNMENT)
    assert len(data.pairs) == 49
    assert len(data.meshes) == 49 * 2 + 4 * 4
    assert (data.claw_types, data.claw_sizes) == (4, 4)
    assert data.claws[0][0] == r"claw1\claw_xl_01.mesh"
    assert data.pairs[0] == (
        r"horn1\horn_l_01_lod1.mesh",
        r"horn1\horn_l_01_lod2.mesh",
    )
    # The three no-LOD adornments repeat the LOD1 path.
    assert (r"feath2\feath_l_02_lod1.mesh", r"feath2\feath_l_02_lod1.mesh") in data.pairs


def test_adornment_meshes_exist():
    if not ADORNMENT.exists():
        pytest.skip("retail sample not present")
    data = read_adornment(ADORNMENT)
    for name in data.meshes:
        assert (ELEMENTS / name.replace("\\", "/")).exists(), name
