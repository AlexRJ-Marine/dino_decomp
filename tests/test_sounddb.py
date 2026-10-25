import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from sounddb import SoundDbError, read_sounddb, scan  # noqa: E402

GAME = ROOT / "game" / "%MAINDIR%"
SOUND = GAME / "Data" / "Sound"
MENU = SOUND / "MenuSoundDB.ini"
PARK = SOUND / "ParkSoundDB.ini"


def test_menu_sound_db():
    if not MENU.exists():
        pytest.skip("retail sound DB missing")
    db = read_sounddb(MENU)
    assert len(db.entries) == 5
    assert [e.name for e in db.entries] == [
        "clicDefault",
        "clic",
        "exitClic",
        "music",
        "mixer",
    ]
    music = db.by_name("music")
    assert music is not None
    assert music.filename == "data/Sound/Musics/musLabo.wav"
    assert music.loop is True
    assert music.use_streaming is True
    assert music.priority == 4
    assert music.use_volume is True


def test_park_sound_db():
    if not PARK.exists():
        pytest.skip("retail sound DB missing")
    db = read_sounddb(PARK)
    assert len(db.entries) == 224
    assert [e.name for e in db.music] == [
        "musTempere",
        "musAtoll",
        "musEquateur",
        "musAfricaine",
        "musLabo",
    ]
    for entry in db.music:
        assert entry.loop is True
        assert entry.use_streaming is True
        assert entry.category == 1
        assert entry.filename.startswith("data/Sound/Musics/")
        assert (db.root / entry.filename).exists()
    for entry in db.entries:
        assert 0 <= entry.priority <= 100
        if entry.filename:
            assert entry.filename.lower().endswith(".wav")


def test_park_sound_db_one_missing_retail_path():
    if not PARK.exists():
        pytest.skip("retail sound DB missing")
    db = read_sounddb(PARK)
    missing = [
        (e.name, e.filename)
        for e in db.entries
        if e.filename and not (db.root / e.filename).exists()
    ]
    assert missing == [("bell", "data/sound/Attractions/Attractions/bell.wav")]
    # The correctly named copy is under Buildings and belongs to [warning].
    warning = db.by_name("warning")
    assert warning is not None
    assert warning.filename == "data/sound/Buildings/bell.wav"
    assert (db.root / warning.filename).exists()


def test_scan_sound_dbs():
    if not SOUND.exists():
        pytest.skip("retail sound DB missing")
    good, bad = scan(SOUND, GAME)
    assert not bad
    assert len(good) == 2
    assert {len(db.entries) for db in good} == {5, 224}


def test_include_and_unknown_key(tmp_path):
    child = tmp_path / "child.ini"
    child.write_text(
        "[child]\nFILENAME = \"data/Sound/x.wav\"\nPRIORITY = 2\n",
        encoding="latin-1",
    )
    parent = tmp_path / "ParentSoundDB.ini"
    parent.write_text(
        "[first]\n"
        "FILENAME = \"data/Sound/y.wav\"\n"
        "USE_3D = 1\n"
        "CUSTOM_KEY = 7\n"
        "#INCLUDE child.ini\n",
        encoding="latin-1",
    )
    db = read_sounddb(parent, tmp_path)
    assert [e.name for e in db.entries] == ["first", "child"]
    assert db.entries[1].priority == 2
    assert db.entries[0].use_3d is True
    assert db.entries[0].raw["CUSTOM_KEY"] == "7"


def test_invalid_bool(tmp_path):
    bad = tmp_path / "BadSoundDB.ini"
    bad.write_text("[x]\nLOOP = yes\n", encoding="latin-1")
    with pytest.raises(SoundDbError, match="LOOP"):
        read_sounddb(bad, tmp_path)
