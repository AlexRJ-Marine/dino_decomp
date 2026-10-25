import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from seq import read_seq, scan  # noqa: E402

ANIM = ROOT / "game" / "%MAINDIR%" / "Data" / "Design" / "animations"


def test_light_biped_fx():
    if not (ANIM / "LightBipedFX.seq").exists():
        pytest.skip("retail sample not present")
    seq = read_seq(ANIM / "LightBipedFX.seq")
    assert seq.kind == "FX"
    assert seq.lead is None
    assert (seq.sequence_count, seq.event_count, len(seq.strings)) == (41, 96, 47)
    first = seq.sequences[0]
    assert first.id == 2
    assert [(e.frame, e.type, e.string_index) for e in first.events] == [
        (0, 1, 2), (15, 1, 3),
    ]
    assert [e.time for e in first.events] == [0.0, 0.5]
    assert all(e.string_index < len(seq.strings) for s in seq.sequences for e in s.events)


def test_light_biped_sound():
    if not (ANIM / "LightBipedSound.seq").exists():
        pytest.skip("retail sample not present")
    seq = read_seq(ANIM / "LightBipedSound.seq")
    assert seq.kind == "Sound"
    assert seq.lead == pytest.approx(60.0)
    assert (seq.sequence_count, seq.event_count, len(seq.strings)) == (41, 91, 47)
    first = seq.sequences[0]
    assert first.id == 2
    assert [(e.type, e.string_index, e.time, e.value) for e in first.events] == [
        (0, 0, 4.0, 60.0), (15, 0, 4.0, 60.0),
    ]


def test_scan_all_sequences():
    if not ANIM.exists():
        pytest.skip("retail game tree not present")
    total, bad, sums = scan(ANIM)
    assert total == 10
    assert not bad, bad[:5]
    assert sums["fx"] == 5
    assert sums["sound"] == 5
    assert sums["events"] == 1_304


def test_counts_table():
    if not ANIM.exists():
        pytest.skip("retail game tree not present")
    expected = {
        "HeavyBipedFX.seq": (41, 95, 42, None),
        "HeavyBipedSound.seq": (41, 111, 42, 135.0),
        "LightBipedFX.seq": (41, 96, 47, None),
        "LightBipedSound.seq": (41, 91, 47, 60.0),
        "HighQuadrupedFX.seq": (40, 176, 45, None),
        "HighQuadrupedSound.seq": (40, 124, 45, 165.0),
        "LightQuadrupedFX.seq": (41, 180, 45, None),
        "LightQuadrupedSound.seq": (41, 139, 45, 75.0),
        "LowQuadrupedFX.seq": (40, 174, 44, None),
        "LowQuadrupedSound.seq": (40, 118, 44, 82.5),
    }
    for name, (sequences, events, strings, lead) in expected.items():
        seq = read_seq(ANIM / name)
        assert (seq.sequence_count, seq.event_count, len(seq.strings)) == (
            sequences, events, strings
        ), name
        if lead is None:
            assert seq.lead is None
        else:
            assert seq.lead == pytest.approx(lead), name
