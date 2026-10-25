import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from mesh import MeshFormatError, read_mesh, scan, write_obj  # noqa: E402

GAME = ROOT / "game" / "%MAINDIR%"
COW = GAME / "Data" / "Graphics" / "Actors" / "animals" / "cow" / "cow_lod1.mesh"
ALLO = GAME / "Data" / "Graphics" / "Actors" / "Dinos" / "allo" / "allo_lod1.mesh"


def test_cow_geometry():
    if not COW.exists():
        pytest.skip("retail sample not present")
    m = read_mesh(COW)
    assert m.name == "cow_lod1"
    assert len(m.vertices) == 93
    assert len(m.indices) == 198
    assert m.triangle_count == 66
    assert len(m.uvs) == 93
    assert len(m.normals) == 93
    assert all(0 <= i < 93 for i in m.indices)
    assert len(m.tail) > 0
    lo, hi = m.bounds()
    assert lo[2] < 0 < hi[2]
    assert hi[2] - lo[2] > 2.0


def test_scan_all_meshes():
    if not GAME.exists():
        pytest.skip("retail game tree not present")
    total, bad, totals = scan(GAME)
    assert total == 536
    assert not bad, bad[:5]
    assert totals["vertices"] == 47_171
    assert totals["triangles"] == 35_190


def test_allo_vertex_colors():
    if not ALLO.exists():
        pytest.skip("retail sample not present")
    m = read_mesh(ALLO)
    assert len(m.colors) == 317
    assert m.colors[0] == (230, 239, 241, 255)
    assert all(c[3] == 255 for c in m.colors)
    assert m.material_offset == 5 + 317 * 4
    # Most meshes do not have a per-vertex color table.
    cow = read_mesh(COW)
    assert cow.colors == []
    assert cow.material_offset == 0


def test_obj_export(tmp_path):
    if not COW.exists():
        pytest.skip("retail sample not present")
    out = tmp_path / "cow.obj"
    write_obj(read_mesh(COW), out)
    lines = out.read_text().splitlines()
    assert sum(1 for line in lines if line.startswith("v ")) == 93
    assert sum(1 for line in lines if line.startswith("vt ")) == 93
    assert sum(1 for line in lines if line.startswith("vn ")) == 93
    assert sum(1 for line in lines if line.startswith("f ")) == 66
    assert any(line.startswith("f 1/1/1 ") for line in lines)


def test_rejects_bad_magic():
    with pytest.raises(MeshFormatError):
        read_mesh(Path(__file__))  # a Python file is not a mesh
