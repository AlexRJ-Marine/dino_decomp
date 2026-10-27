import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
LIVE = ROOT / "run-test" / "save" / "auto" / "auto.sav"
STABLE = ROOT / "build" / "campaign-autosave.sav"
TOOL = ROOT / "tools" / "formats" / "sav_campaign_scan.py"


def test_campaign_autosave_list0_scan():
    """Walk the populated campaign list0 with the packed 0x19-byte prefix."""
    campaign = LIVE if LIVE.exists() and LIVE.stat().st_size > 20000 else STABLE
    if not campaign.exists() or campaign.stat().st_size < 20000:
        pytest.skip("campaign autosave not present")
    result = subprocess.run(
        [sys.executable, str(TOOL), str(campaign)],
        capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stderr
    assert "world_dinos=20 roads=14 list0=20" in result.stdout
    assert ("0 id=63823 x=517 y=450 states=(3, 3, 1, 2) "
            "name='Ankylosaur1' tail=0x344") in result.stdout
    assert "19 id=63932" in result.stdout
