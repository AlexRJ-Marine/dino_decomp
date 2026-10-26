import sys
from pathlib import Path

import numpy as np
import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from raw import read_heightmap, read_occupation, scan  # noqa: E402

GAME = ROOT / "game" / "%MAINDIR%"
DESIGN = GAME / "Data" / "Design" / "landscape"
GRAPHICS = GAME / "Data" / "Graphics" / "Textures" / "landscape" / "islands"


def test_heightmap_and_occupation_sizes():
    if not (GRAPHICS / "isl01" / "heightmap_01.raw").exists():
        pytest.skip("retail sample not present")
    heightmap = read_heightmap(GRAPHICS / "isl01" / "heightmap_01.raw")
    assert (heightmap.width, heightmap.height) == (512, 512)
    assert len(heightmap.data) == 512 * 512
    assert 0 < heightmap.summary()["max"] <= 255

    occupation = read_occupation(DESIGN / "isl01" / "isl01_occupation.raw")
    assert (occupation.width, occupation.height) == (1024, 1024)
    assert set(occupation.data) == {0, 255}
    assert occupation.buildable == occupation.data.count(0)
    assert occupation.buildable + occupation.summary()["blocked"] == 1024 * 1024
    assert occupation.is_buildable(0, 0) == (occupation.at(0, 0) == 0)


def test_mask_is_land_not_water():
    """0 cells are land (higher, never water); 255 cells include water."""
    if not (GRAPHICS / "isl01" / "heightmap_01.raw").exists():
        pytest.skip("retail sample not present")
    for index in (1, 4, 10):
        heightmap = read_heightmap(
            GRAPHICS / f"isl{index:02d}" / f"heightmap_{index:02d}.raw"
        )
        occupation = read_occupation(
            DESIGN / f"isl{index:02d}" / f"isl{index:02d}_occupation.raw"
        )
        hm = np.frombuffer(heightmap.data, dtype=np.uint8).reshape(512, 512)
        mask = np.frombuffer(occupation.data, dtype=np.uint8).reshape(1024, 1024)
        sampled = hm
        buildable = sampled[mask[::2, ::2] == 0]
        blocked = sampled[mask[::2, ::2] == 255]
        assert buildable.mean() > blocked.mean(), index
        assert not np.any(buildable == 0), index


def test_scan_all_islands():
    if not DESIGN.exists() or not GRAPHICS.exists():
        pytest.skip("retail game tree not present")
    data = scan(DESIGN, GRAPHICS)
    assert len(data["islands"]) == 10
    for island in data["islands"]:
        assert island["heightmap"]["width"] == 512
        assert island["occupation"]["width"] == 1024
        assert island["occupation"]["values"] == [0, 255]
        assert island["buildable_mean_height"] > island["blocked_mean_height"]
