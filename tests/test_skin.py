import sys
from pathlib import Path

import numpy as np
import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from skel import read_skel  # noqa: E402
from skin import read_skin, scan  # noqa: E402
from mesh import read_mesh  # noqa: E402

GAME = ROOT / "game" / "%MAINDIR%"
ALLO = GAME / "Data" / "Graphics" / "Actors" / "Dinos" / "allo"
DINOS = GAME / "Data" / "Graphics" / "Actors" / "Dinos"
PACHY = DINOS / "pachy"


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


def test_allo_skin_records():
    if not (ALLO / "allo_lod1.skin").exists():
        pytest.skip("retail sample not present")
    s = read_skin(ALLO / "allo_lod1.skin")
    data = s.skin_data
    assert data is not None
    assert len(data.bones) == 35
    assert data.materials[0].texture == "allo.tga"
    assert data.materials[0].name == "Material #1"
    assert len(data.triangle_materials) == 382
    assert set(data.triangle_materials) == {0}
    assert data.unknown == (1, 0xFFFFFFFF, 35)

    assigned = [i for b in data.bones for i in b.vertex_indices]
    assert len(assigned) == 317
    assert sorted(assigned) == list(range(317))
    assert all(len(b.vertex_indices) == len(b.weights) for b in data.bones)
    assert all(abs(w - 1.0) < 1e-6 for b in data.bones for w in b.weights)
    assert data.bones[0].name == "Bip01"
    assert data.bones[0].vertex_indices == ()
    assert len(data.bones[3].vertex_indices) == 22  # Bip01_Spine


def test_allo_bind_skinning():
    if not (ALLO / "allo_lod1.skin").exists():
        pytest.skip("retail sample not present")
    skin = read_skin(ALLO / "allo_lod1.skin")
    skel = read_skel(ALLO / "allo.skel")
    data = skin.skin_data
    assert data is not None

    # .skin full-mesh vertices are in centimetres (the .mesh is in metres);
    # it is a separate bind mesh with the same vertex/triangle counts.
    skin_bounds = np.array(skin.full_mesh.vertices)
    skin_size = skin_bounds.max(0) - skin_bounds.min(0)
    assert 400 < skin_size[2] < 600

    # Skeleton world_matrices() are metres; convert to raw centimetres.
    scaled = [np.array(m).reshape(4, 4) for m in skel.world_matrices()]
    to_cm = np.diag([100.0, 100.0, 100.0, 1.0])
    raw = [m @ to_cm for m in scaled]
    by_name = {b.name: b.index for b in skel.bones}

    v = skin_bounds
    vh = np.hstack([v, np.ones((len(v), 1))])
    out = np.zeros_like(vh)
    seen = np.zeros(len(v), dtype=bool)
    for bone in data.bones:
        if not bone.vertex_indices:
            continue
        idx = np.asarray(bone.vertex_indices, dtype=np.int64)
        R = np.asarray(bone.inverse_bind).reshape(4, 4)
        M = R @ raw[by_name[bone.name]]
        out[idx] = vh[idx] @ M
        seen[idx] = True

    assert seen.all()
    assert np.max(np.abs(out[:, :3] - v)) < 1e-3


def test_pachy_lod2_bind_pose_retarget():
    """Pachy LOD2/LOD3 store a bent-neck bind pose; the mesh bind fixes it.

    inverse_bind is inverse(skeleton bind). For pachy LOD2/LOD3 only the
    neck/head chain differs from pachy.skel, so skinning the .mesh (standing
    rest pose) with inverse_bind introduces a fixed head offset. Using
    inverse(skeleton rest world) as the mesh bind removes it.
    """
    if not (PACHY / "pachy_lod2.skin").exists():
        pytest.skip("retail samples not present")
    skel = read_skel(PACHY / "pachy.skel")
    to_cm = np.diag([100.0, 100.0, 100.0, 1.0])
    raw = [np.array(m).reshape(4, 4) @ to_cm for m in skel.world_matrices()]
    by_name = {b.name: b.index for b in skel.bones}
    neck_chain = {
        "Bip01_Neck", "Bip01_Neck1", "Bip01_Neck2", "Bip01_Head",
        "Bip01_Ponytail1", "Bip01_Ponytail2",
    }
    for lod in (2, 3):
        skin = read_skin(PACHY / f"pachy_lod{lod}.skin")
        mesh = read_mesh(PACHY / f"pachy_lod{lod}.mesh")
        changed = {}
        for record in skin.skin_data.bones:
            index = by_name[record.name]
            bind = np.linalg.inv(np.asarray(record.inverse_bind).reshape(4, 4))
            dt = float(np.linalg.norm(bind[3, :3] - raw[index][3, :3]))
            dr = float(np.linalg.norm(bind[:3, :3] - raw[index][:3, :3]))
            if dt > 0.01 or dr > 1e-3:
                changed[record.name] = (dt, dr)
        assert set(changed) == neck_chain, lod
        assert changed["Bip01_Neck2"][0] == pytest.approx(7.772, abs=0.01)
        assert changed["Bip01_Neck2"][1] == pytest.approx(1.0938, abs=1e-3)
        assert changed["Bip01_Head"][0] == pytest.approx(4.316, abs=0.01)

        # At skeleton rest the old bind moves the head/neck vertices by up to
        # 4.3 cm (and by 10.9 cm on a walk frame, where the neck rotation acts
        # on the head); the corrected bind is identity.
        v = np.c_[np.array(mesh.vertices) * 100.0, np.ones(len(mesh.vertices))]
        old = np.zeros_like(v)
        new = np.zeros_like(v)
        for record in skin.skin_data.bones:
            index = by_name[record.name]
            idx = list(record.vertex_indices)
            if not idx:
                continue
            R = np.asarray(record.inverse_bind).reshape(4, 4)
            old[idx] = v[idx] @ R @ raw[index]
            new[idx] = v[idx] @ np.linalg.inv(raw[index]) @ raw[index]
        assert np.max(np.linalg.norm(old[:, :3] - v[:, :3], axis=1)) > 4.0
        assert np.max(np.abs(new - v)) < 1e-6


def test_only_pachy_lod23_bind_pose_differs():
    """All other 19 dinosaurs' LOD1-3 inverse binds match the skeleton rest."""
    if not PACHY.exists():
        pytest.skip("retail samples not present")
    mismatched = set()
    for folder in sorted(p for p in DINOS.iterdir() if p.is_dir() and p.name != "animations"):
        skel_path = folder / f"{folder.name}.skel"
        if not skel_path.exists():
            continue
        skel = read_skel(skel_path)
        to_cm = np.diag([100.0, 100.0, 100.0, 1.0])
        raw = [np.array(m).reshape(4, 4) @ to_cm for m in skel.world_matrices()]
        by_name = {b.name: b.index for b in skel.bones}
        for lod in (1, 2, 3):
            skin_path = folder / f"{folder.name}_lod{lod}.skin"
            if not skin_path.exists():
                continue
            skin = read_skin(skin_path)
            assert skin.skin_data is not None, skin_path
            for record in skin.skin_data.bones:
                index = by_name[record.name]
                bind = np.linalg.inv(np.asarray(record.inverse_bind).reshape(4, 4))
                dt = float(np.linalg.norm(bind[3, :3] - raw[index][3, :3]))
                dr = float(np.linalg.norm(bind[:3, :3] - raw[index][:3, :3]))
                if dt > 0.01 or dr > 1e-3:
                    mismatched.add((folder.name, lod))
    assert mismatched == {("pachy", 2), ("pachy", 3)}, sorted(mismatched)


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
    assert sums["skin_data"] == 60
