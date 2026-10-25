import sys
import wave
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from wav import read_wav, scan  # noqa: E402

GAME = ROOT / "game" / "%MAINDIR%"
AVOID = GAME / "Data" / "Sound" / "Actors" / "Dinos" / "avoidance.wav"
MUSIC = GAME / "Data" / "Sound" / "Musics" / "musAfricaine.wav"


def _assert_matches_wave(path):
    w = read_wav(path)
    with wave.open(str(path), "rb") as wf:
        assert w.channels == wf.getnchannels()
        assert w.sample_rate == wf.getframerate()
        assert w.bits_per_sample == wf.getsampwidth() * 8
        assert w.frames == wf.getnframes()
        assert w.duration == pytest.approx(wf.getnframes() / wf.getframerate())
        assert len(w.data) == w.data_size
    return w


def test_avoidance_pcm():
    if not AVOID.exists():
        pytest.skip("retail sample not present")
    w = _assert_matches_wave(AVOID)
    assert w.format_tag == 1
    assert (w.channels, w.sample_rate, w.bits_per_sample) == (1, 44100, 16)
    assert w.frames == 4410
    assert w.duration == pytest.approx(0.1)
    assert w.samples is not None
    assert list(w.samples[:5]) == [-31, -12, -7, -5, -2]


def test_stereo_music():
    if not MUSIC.exists():
        pytest.skip("retail sample not present")
    w = _assert_matches_wave(MUSIC)
    assert (w.channels, w.sample_rate, w.bits_per_sample) == (2, 22050, 16)
    assert w.duration > 300.0


def test_header_only():
    if not AVOID.exists():
        pytest.skip("retail sample not present")
    w = read_wav(AVOID, load_data=False)
    assert w.data == b""
    assert w.samples is None
    assert w.frames == 4410


def test_scan_all_wavs():
    if not GAME.exists():
        pytest.skip("retail game tree not present")
    total, bad, sums = scan(GAME)
    assert total == 231
    assert not bad, bad[:5]
    assert sums["frames"] == 38_715_314
    assert sums["duration"] == pytest.approx(1755.246, abs=0.01)
