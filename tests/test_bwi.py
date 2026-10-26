import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from bwi import read_bwi, scan  # noqa: E402

GAME = ROOT / "game" / "%MAINDIR%"
WALLS = GAME / "Data" / "Design" / "attractions" / "walls"
LIST = WALLS / "WallList.ini"


def test_scan_all_walls():
    if not WALLS.exists():
        pytest.skip("retail sample not present")
    walls = scan(WALLS)
    assert len(walls) == 24
    assert len({wall.name for wall in walls}) == 24
    for wall in walls:
        assert wall.version == 4
        assert wall.height in (2.0, 4.0, 6.0, 8.0)
        assert wall.field_08 == 100
        assert wall.field_10 == 30
        assert len(wall.scales) == 7
        assert len(wall.textures) == 4
        assert len(wall.meshes) == 2
        assert wall.field_tail == 4
        assert wall.switch_material.startswith("SWITCH_WALL_")
        assert wall.switch_height.startswith(("SWITCH_WALL_", "SWITCH_FENCE_"))


def test_wall_list_order():
    if not LIST.exists():
        pytest.skip("retail sample not present")
    names = [
        line.strip()
        for line in LIST.read_text(encoding="cp1252").splitlines()
        if line.strip()
    ]
    assert names[0] == "24"
    names = names[1:]
    assert len(names) == 24
    assert names[:4] == ["Fence", "Show", "MediumIron", "LowIron"]
    for name in names:
        assert (WALLS / f"{name}.bwi").exists()


def test_height_and_material_families():
    if not WALLS.exists():
        pytest.skip("retail sample not present")
    walls = {wall.name: wall for wall in scan(WALLS)}
    assert walls["LowWood"].height == 2.0
    assert walls["MediumWood"].height == 4.0
    assert walls["HighWood"].height == 6.0
    assert walls["HugeWood"].height == 8.0
    # Cost/resistance column: wood 10, plates 30, everything else 15.
    assert walls["LowWood"].field_0c == 10
    assert walls["LowPlates"].field_0c == 30
    assert walls["LowIron"].field_0c == 15
    assert [name for name, wall in walls.items() if wall.electrified] == [
        "HighElect",
        "HugeElect",
        "LowElect",
        "MediumElect",
    ]
    for name in ("LowWood", "MediumWood", "HighWood", "HugeWood"):
        # The height scale is height / 8.
        assert walls[name].scales[5] == pytest.approx(walls[name].height / 8)


def test_widget_names_and_assets():
    if not WALLS.exists():
        pytest.skip("retail sample not present")
    for wall in scan(WALLS):
        for relative in (*wall.textures, *wall.meshes):
            if not relative:
                continue
            assert (GAME / relative.replace("\\", "/")).exists(), relative
        if wall.texture_dir:
            assert (GAME / wall.texture_dir.replace("\\", "/")).is_dir()
