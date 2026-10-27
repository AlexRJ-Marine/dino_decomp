import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "tools" / "formats" / "sav_manual.py"
AUTO = ROOT / "run" / "save" / "auto" / "slot001.sav"

sys.path.insert(0, str(ROOT / "tools" / "formats"))


def test_manualize_free_game_autosave(tmp_path):
    if not AUTO.exists():
        pytest.skip("retail autosave not present")
    out = tmp_path / "manual.sav"
    result = subprocess.run(
        [sys.executable, str(TOOL), str(AUTO), str(out)],
        capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stderr
    source = AUTO.read_bytes()
    rebuilt = out.read_bytes()
    assert len(rebuilt) == len(source)
    from sav_graph import read_sav_graph

    park = read_sav_graph(out).park
    assert park.header_a == 0
    assert park.header_c == 10.0
    # Everything outside the two header fields is untouched.
    from sav import read_sav

    base = read_sav(out).state_offset + park.offset
    diff = [i for i, (a, b) in enumerate(zip(source, rebuilt)) if a != b]
    assert diff
    assert all(base + 0x04 <= i < base + 0x08 or base + 0x0C <= i < base + 0x10
               for i in diff)
