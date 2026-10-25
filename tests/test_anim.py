import sys
from pathlib import Path

import numpy as np
import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from anim import read_anim, scan  # noqa: E402
from skel import read_skel  # noqa: E402
from skin import read_skin  # noqa: E402

GAME = ROOT / "game" / "%MAINDIR%"
HEAVY = GAME / "Data" / "Graphics" / "Actors" / "Dinos" / "animations" / "HeavyBiped" / "HeavyBiped_Armor.anim"
ALLO = GAME / "Data" / "Graphics" / "Actors" / "Dinos" / "allo"


def test_heavy_biped_header():
    if not HEAVY.exists():
        pytest.skip("retail sample not present")
    a = read_anim(HEAVY)
    assert a.hash == 0x8A504920
    assert a.duration == 4800
    assert a.start == 0
    assert a.bone_count == 35
    assert a.total_keys == 1085
    first = a.by_name("Bip01")
    assert first.times == tuple(range(0, 4801, 160))
    assert len(first.matrices) == 31
    assert first.matrices[0][12:15] == pytest.approx((0.0, 0.0, 358.18), abs=0.01)
    # Pure rotation + translation, row-vector convention.
    m = first.matrices[0]
    assert m[3] == m[7] == m[11] == 0.0
    assert m[15] == 1.0
    for r in range(3):
        assert np.linalg.norm(m[r * 4:r * 4 + 3]) == pytest.approx(1.0, abs=1e-4)


def test_sampling():
    if not HEAVY.exists():
        pytest.skip("retail sample not present")
    a = read_anim(HEAVY)
    s0 = a.sample(0)
    s1 = a.sample(160)
    assert set(s0) == set(s1)
    assert s0["Bip01"] == pytest.approx(a.by_name("Bip01").matrices[0])
    assert s1["Bip01"] == pytest.approx(a.by_name("Bip01").matrices[1])
    mid = a.sample(80)["Bip01"]
    # A slerp at the halfway point should stay orthonormal.
    for r in range(3):
        assert np.linalg.norm(mid[r * 4:r * 4 + 3]) == pytest.approx(1.0, abs=1e-4)
    # The clip loops: first and last keys are identical.
    assert a.sample(0)["Bip01"] == pytest.approx(a.sample(4800)["Bip01"], abs=1e-5)


def _deform(anim, time):
    skin = read_skin(ALLO / "allo_lod1.skin")
    skel = read_skel(ALLO / "allo.skel")
    data = skin.skin_data
    by_name = {b.name: b.index for b in skel.bones}
    local = [np.array(b.matrix).reshape(4, 4) for b in skel.bones]
    for bone in anim.bones:
        local[by_name[bone.name]] = np.array(bone.sample(time)).reshape(4, 4)
    world = [None] * len(local)

    def get(i):
        if world[i] is None:
            p = skel.bones[i].parent
            world[i] = local[i] if p < 0 else local[i] @ get(p)
        return world[i]

    for i in range(len(local)):
        get(i)
    to_cm = np.diag([100.0, 100.0, 100.0, 1.0])
    raw = [m @ to_cm for m in world]
    v = np.asarray(skin.full_mesh.vertices, dtype=np.float64)
    out = np.zeros_like(v)
    for rec in data.bones:
        if not rec.vertex_indices:
            continue
        idx = np.asarray(rec.vertex_indices, dtype=np.int64)
        R = np.asarray(rec.inverse_bind).reshape(4, 4)
        vh = np.hstack([v[idx], np.ones((len(idx), 1))])
        out[idx] = (vh @ (R @ raw[by_name[rec.name]]))[:, :3] * 0.01
    return out


def test_animation_drives_skin():
    if not HEAVY.exists():
        pytest.skip("retail sample not present")
    a = read_anim(HEAVY)
    p0 = _deform(a, 0)
    p_mid = _deform(a, 1600)
    p_end = _deform(a, 4800)
    assert np.isfinite(p0).all()
    assert np.max(np.abs(p0 - p_end)) < 1e-4
    assert np.max(np.abs(p0 - p_mid)) > 0.5
    assert p0[:, 2].min() > -0.1  # feet stay on the ground plane


def test_scan_all_anims():
    if not GAME.exists():
        pytest.skip("retail game tree not present")
    total, bad, sums = scan(GAME)
    assert total == 299
    assert not bad, bad[:5]
    assert sums["bones"] == 9_129
    assert sums["keys"] == 285_507
