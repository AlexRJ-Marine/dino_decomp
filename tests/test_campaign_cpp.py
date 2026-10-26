"""C++ campaign/mission reader against the retail scenario tree."""
import os
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
EXE = Path(
    os.environ.get("DINO_VIEWER_EXE", ROOT / "build" / "mingw" / "dino_viewer.exe")
)
GAME = ROOT / "game" / "%MAINDIR%"
SCENARIO = GAME / "Data" / "Design" / "Scenario"


def _run(*args: str) -> subprocess.CompletedProcess:
    if not EXE.exists():
        pytest.skip("viewer not built")
    if not SCENARIO.exists():
        pytest.skip("retail scenario data missing")
    return subprocess.run(
        [str(EXE), *args], capture_output=True, text=True, timeout=120
    )


def test_cpp_campaign_reader_counts():
    result = _run("--campaign-info", str(SCENARIO))
    assert result.returncode == 0, result.stderr
    assert (
        "campaigns: 7 missions: 39 objectives: 89 unknown: 0 verbs: 33"
        in result.stdout
    )
    # Campaign-level context comes from the child after the base include.
    assert (
        "campaign Cpg04 island=Isl04 budget=20000 sellback=1.00 "
        "missions=4 world=Cpg04_Save.wld title=&Campaign4Title"
    ) in result.stdout
    assert "campaign Cpg08 island=Isl07 budget=20000" in result.stdout
    assert ("campaign Cpg01 island=Isl01 budget=60000 sellback=1.00 "
            "missions=14") in result.stdout
    # Every objective line carries a resolved 0..32 verb id.
    for line in result.stdout.splitlines():
        if line.strip().startswith("objective id="):
            assert "id=-1" not in line, line


def test_cpp_campaign_mission_objectives():
    mission = SCENARIO / "Cpg04" / "Cpg04_Mission1.ini"
    result = _run("--campaign-mission", str(mission))
    assert result.returncode == 0, result.stderr
    assert (
        "mission Cpg04_Mission1 objectives=3 title=&Cpg4_Mission1_Title"
        in result.stdout
    )
    assert (
        "objective id=18 verb=isolateddinos args=71 "
        "desc=&Cpg4_Mission1_Objective1_Description"
    ) in result.stdout
    assert (
        "objective id=0 verb=treasury args=25000 1 "
        "desc=&Cpg4_Mission1_Objective2_Description"
    ) in result.stdout
    assert (
        "objective id=2 verb=dyingdinos args=5 "
        "desc=&Cpg4_Mission1_Objective3_Description"
    ) in result.stdout
