import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from skin import read_skin, scan  # noqa: E402

GAME = ROOT / "game" / "%MAINDIR%"
ALLO = GAME / "Data" / "Graphics" / "Actors" / "Dinos" / "allo"


def test_allo_lod1():
    if not (ALLO / "allo_lod1.skin").exists():
        pytest.skip("retail sample not present")
    s = read_skin(ALLO / "allo_lod1.skin")
    assert s.block_count == 36
    assert len(s.blocks) == 35
    assert sum(len(b.vertices) for b in s.blocks) == 1105
    assert sum(b.triangle_count for b in s.blocks) == 614
    assert s.blocks[0].name == "Bip01"
    assert s.blocks[-1].name == "Bip01_Tail3"
    assert s.full_mesh is not None
    assert s.full_mesh.name == "dino_lod1"
    assert len(s.full_mesh.vertices) == 317
    assert s.full_mesh.triangle_count == 382
    assert len(s.full_mesh.uvs) == 317
    assert len(s.full_mesh.normals) == 317
    assert len(s.trailing) > 0


def test_skin_lods_share_bone_mesh():
    if not (ALLO / "allo_lod1.skin").exists():
        pytest.skip("retail samples not present")
    meshes = {}
    for lod in (1, 2, 3):
        s = read_skin(ALLO / f"allo_lod{lod}.skin")
        assert len(s.blocks) == 35
        assert sum(len(b.vertices) for b in s.blocks) == 1105
        assert sum(b.triangle_count for b in s.blocks) == 614
        meshes[lod] = (len(s.full_mesh.vertices), s.full_mesh.triangle_count)
    assert meshes == {1: (317, 382), 2: (225, 240), 3: (97, 75)}


def test_scan_all_skins():
    if not GAME.exists():
        pytest.skip("retail game tree not present")
    total, bad, sums = scan(GAME)
    assert total == 104
    assert not bad, bad[:5]
    assert sums["blocks"] == 2244
    assert sums["bone_vertices"] == 76_923
    assert sums["bone_triangles"] == 42_976
    assert sums["full_meshes"] == 73
