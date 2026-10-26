import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from translator_constants import (  # noqa: E402
    STRUCT_OFFSETS,
    read_translator_constants,
)

GAME = ROOT / "game" / "%MAINDIR%"
INI = GAME / "Data" / "Design" / "Genotype" / "TranslatorConstants.ini"


def test_constants_table():
    if not INI.exists():
        pytest.skip("retail sample not present")
    constants = read_translator_constants(INI)
    assert constants.section == "SIMU_CONSTANTS"
    assert len(constants.values) == 41
    assert constants["MaxQuadrupedRange"] == 150
    assert constants["MinHeightForHeavyBiped"] == 400
    assert constants["MinWeightForHeavyBiped"] == 1500
    assert constants["SizeCategory1MaxLength"] == 200
    assert constants["SizeCategory4MaxLength"] == 3200
    assert constants["SizeCategory4Radius"] == 7
    assert constants["FoodConsumptionRate"] == pytest.approx(0.1)
    assert constants["WalkingSpeedCoef"] == pytest.approx(0.1)
    assert constants["RunningSpeedCoef"] == pytest.approx(0.4)
    assert constants["CuteNeedBeautyFactor"] == pytest.approx(0.6)
    assert constants["DangerJawFactor"] == pytest.approx(0.15)
    assert constants["TamingNeedAgressivityFactor"] == pytest.approx(0.15)
    assert constants["MaxDinosaurSize"] == pytest.approx(1346.0)
    assert constants["MotricityPower"] == pytest.approx(0.85)
    assert constants["IntelligencePower"] == pytest.approx(0.22)
    assert constants["MaxPerception"] == 90


def test_struct_offsets_cover_every_key():
    assert len(STRUCT_OFFSETS) == 41
    offsets = sorted(STRUCT_OFFSETS.values())
    assert len(set(offsets)) == 41
    assert offsets[0] == 0x228
    assert offsets[-1] == 0x2DC
    # The object has a deliberate 4-byte gap at +0x2B4.
    assert 0x2B4 not in offsets
    assert 0x2B0 in offsets and 0x2B8 in offsets

def test_lab_attribute_ranges():
    """The lab panel labels and their display maxima."""
    ranges = GAME / "Data" / "Overlay" / "AttributeRange.ini"
    if not ranges.exists():
        pytest.skip("retail sample not present")
    sys.path.insert(0, str(ROOT / "tools" / "formats"))
    from overlay import parse_ini

    sections = {section.name: section for section in parse_ini(ranges)}
    dino = sections["DINO"].values
    assert dino == {
        "PERCEPTION": "100.0",
        "BEAUTY": "100.0",
        "STRANGENESS": "100.0",
        "ATTACK": "120.0",
        "AGGRESSIVENESS": "100.0",
        "ARMOR": "100.0",
        "WALKSPEED": "100.0",
        "AGILITY": "100.0",
        "INTELLIGENCE": "100.0",
        "COURAGE": "100.0",
        "STAMINA": "100.0",
    }
    assert sections["MODIFY"].values == {"RADIATIONRESISTANCE": "100.0"}
