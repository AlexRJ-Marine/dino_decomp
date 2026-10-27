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


def test_cpp_campaign_objective_selftest():
    result = _run("--selftest")
    assert result.returncode == 0, result.stderr
    assert "selftest: PASS (83 checks)" in result.stdout


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


def test_cpp_campaign_isolated_success():
    # Cpg01_Save.wld starts with no dinosaurs, so `isolateddinos 71` has no
    # aggressive dino to reject (required count 0) and the mission completes
    # with the treasury override. Cpg04's populated park keeps it pending.
    isl01 = GAME / "Data" / "Design" / "landscape" / "isl01" / "isl01.lnd"
    wld = SCENARIO / "Cpg01" / "Cpg01_Save.wld"
    mission = SCENARIO / "Cpg04" / "Cpg04_Mission1.ini"
    if not isl01.exists() or not wld.exists() or not mission.exists():
        pytest.skip("retail Cpg01/Cpg04 mission data missing")
    result = _run(
        "--frames", "2", "--no-menu", "--width", "320", "--height", "240",
        "--no-vsync", "--no-save-config", "--lnd", str(isl01),
        "--asset-root", str(GAME), "--no-vegetation", "--no-water",
        "--wld-park", str(wld), "--park-budget", "25000",
        "--start-mission", str(mission),
    )
    assert result.returncode == 0, result.stderr
    assert "mission objective 0 isolateddinos -> success" in result.stdout
    assert "mission objective 1 treasury -> success" in result.stdout
    assert "mission objective 2 dyingdinos -> success" in result.stdout
    assert "mission Cpg04_Mission1: success" in result.stdout


def test_cpp_campaign_mission_panel():
    isl04 = GAME / "Data" / "Design" / "landscape" / "isl04" / "isl04.lnd"
    wld = SCENARIO / "Cpg04" / "Cpg04_Save.wld"
    mission = SCENARIO / "Cpg04" / "Cpg04_Mission1.ini"
    if not isl04.exists() or not wld.exists() or not mission.exists():
        pytest.skip("retail Cpg04 mission data missing")
    result = _run(
        "--frames", "2", "--no-menu", "--width", "320", "--height", "240",
        "--no-vsync", "--no-save-config", "--lnd", str(isl04),
        "--asset-root", str(GAME), "--no-vegetation", "--no-water",
        "--wld-park", str(wld), "--park-budget", "25000",
        "--start-mission", str(mission),
    )
    assert result.returncode == 0, result.stderr
    assert "mission: Cpg04_Mission1 objectives=3" in result.stdout
    assert "mission title: 1/4" in result.stdout
    assert (
        "mission objective 0 isolateddinos: Isolate the aggressive dinos "
        "(aggressiveness > 70)" in result.stdout
    )
    # Budget 25,000 and zero deaths satisfy two objectives; isolateddinos
    # needs the isolation tags (not ported), so the mission stays pending.
    assert "mission objective 1 treasury -> success" in result.stdout
    assert "mission objective 2 dyingdinos -> success" in result.stdout
    assert "mission Cpg04_Mission1: success" not in result.stdout


def _unlock_file(tmp_path: Path, mask: int = 0x0F) -> Path:
    path = tmp_path / "dino.dat"
    path.write_bytes(mask.to_bytes(4, "little"))
    return path


def test_cpp_campaign_unlock_mask(tmp_path):
    missing = tmp_path / "missing.dat"
    result = _run(
        "--validate", "--asset-root", str(GAME), "--dino-dat", str(missing)
    )
    assert result.returncode == 0, result.stderr
    assert "menu campaigns: 7 unlocked=0x1" in result.stdout

    unlock = _unlock_file(tmp_path)
    result = _run(
        "--validate", "--asset-root", str(GAME), "--dino-dat", str(unlock)
    )
    assert result.returncode == 0, result.stderr
    assert "menu campaigns: 7 unlocked=0xF" in result.stdout


def test_cpp_menu_campaign_select(tmp_path):
    unlock = _unlock_file(tmp_path)
    result = _run(
        "--validate", "--asset-root", str(GAME),
        "--dino-dat", str(unlock),
        "--menu-page", "2",
        "--menu-click", "404,285",   # CAMPAIGN_LIST row 3 (Cpg04)
        "--menu-click", "364,530",   # CAMPAIGN_START_BUTTON
    )
    assert result.returncode == 0, result.stderr
    assert "menu campaigns: 7 unlocked=0xF" in result.stdout
    assert "menu page=3 active=1" in result.stdout


def test_cpp_campaign_progress_resume(tmp_path):
    # The port-owned progress file stores `Cpg04 Cpg04_Mission2`; the menu
    # campaign start must resume that mission instead of Mission1.
    isl04 = GAME / "Data" / "Design" / "landscape" / "isl04" / "isl04.lnd"
    if not isl04.exists():
        pytest.skip("retail Cpg04 data missing")
    unlock = _unlock_file(tmp_path)
    progress = tmp_path / "campaign-progress.txt"
    progress.write_text("Cpg04 Cpg04_Mission2\n", encoding="utf-8")
    result = _run(
        "--frames", "3", "--menu-page", "2",
        "--dino-dat", str(unlock),
        "--campaign-progress", str(progress),
        "--menu-click", "404,285",
        "--menu-click", "364,530",
        "--menu-click", "521,449",
        "--menu-loading-time", "0",
        "--width", "320", "--height", "240", "--no-vsync", "--no-save-config",
        "--asset-root", str(GAME), "--no-vegetation", "--no-water",
    )
    assert result.returncode == 0, result.stderr
    assert (
        "start: campaign=Cpg04 mission=Cpg04_Mission2 island=Isl04 "
        "budget=0" in result.stdout
    )
    assert "mission: Cpg04_Mission2 objectives=2" in result.stdout
    assert "mission: Cpg04_Mission1" not in result.stdout
    assert "Cpg04 Cpg04_Mission2" in progress.read_text(encoding="utf-8")


def test_cpp_menu_campaign_locked(tmp_path):
    # A fresh profile (mask 1) only unlocks Cpg01. Selecting row 3 (Cpg04)
    # must not advance to the briefing page.
    unlock = _unlock_file(tmp_path, 0x01)
    result = _run(
        "--validate", "--asset-root", str(GAME),
        "--dino-dat", str(unlock),
        "--menu-page", "2",
        "--menu-click", "404,285",
        "--menu-click", "364,530",
    )
    assert result.returncode == 0, result.stderr
    assert "menu campaigns: 7 unlocked=0x1" in result.stdout
    assert "menu page=2 active=1" in result.stdout


def test_cpp_menu_campaign_mission_start(tmp_path):
    isl04 = GAME / "Data" / "Design" / "landscape" / "isl04" / "isl04.lnd"
    wld = SCENARIO / "Cpg04" / "Cpg04_Save.wld"
    if not isl04.exists() or not wld.exists():
        pytest.skip("retail Cpg04 data missing")
    unlock = _unlock_file(tmp_path)
    progress = tmp_path / "campaign-progress.txt"
    # Full retail flow: campaign row -> campaign briefing -> Start. The
    # loading page then loads Isl04, imports Cpg04_Save.wld and opens the
    # MISSION_PANEL with the first mission.
    result = _run(
        "--frames", "3", "--menu-page", "2",
        "--dino-dat", str(unlock),
        "--campaign-progress", str(progress),
        "--menu-click", "404,285",   # CAMPAIGN_LIST row 3 (Cpg04)
        "--menu-click", "364,530",   # CAMPAIGN_START_BUTTON
        "--menu-click", "521,449",   # CAMPAIGN_INFO_START_BUTTON
        "--menu-loading-time", "0",
        "--width", "320", "--height", "240", "--no-vsync", "--no-save-config",
        "--asset-root", str(GAME), "--no-vegetation", "--no-water",
    )
    assert result.returncode == 0, result.stderr
    assert "campaign park:" in result.stdout and "dinos=20" in result.stdout
    assert "wld: placed 57/57 buildings" in result.stdout
    assert (
        "dinos: 20/20 spawned allo=7 anky=2 coelo=4 deino=5 trood=2"
        in result.stdout
    )
    assert "paddocks: 9 capacity=492 used=336" in result.stdout
    assert (
        "start: campaign=Cpg04 mission=Cpg04_Mission1 island=Isl04 budget=20000"
        in result.stdout
    )
    assert "mission: Cpg04_Mission1 objectives=3" in result.stdout
    # Treasury (20,000 < 25,000) and isolation stay pending; zero deaths
    # satisfies the dyingdinos limit.
    assert "mission objective 2 dyingdinos -> success" in result.stdout
    assert "mission Cpg04_Mission1: success" not in result.stdout


def test_cpp_campaign_live_employees():
    # Cpg01_Mission6 wants a farm (connectedbuildings, still pending) and two
    # employees. Hired nutritionists are the live `employees all 2` source.
    isl01 = GAME / "Data" / "Design" / "landscape" / "isl01" / "isl01.lnd"
    mission = SCENARIO / "Cpg01" / "Cpg01_Mission6.ini"
    if not isl01.exists() or not mission.exists():
        pytest.skip("retail Cpg01 mission data missing")
    result = _run(
        "--frames", "2", "--no-menu", "--width", "320", "--height", "240",
        "--no-vsync", "--no-save-config", "--lnd", str(isl01),
        "--asset-root", str(GAME), "--no-vegetation", "--no-water",
        "--hire-nutritionist", "2", "--start-mission", str(mission),
    )
    assert result.returncode == 0, result.stderr
    assert "employees: hired=2" in result.stdout
    assert "mission objective 1 employees -> success" in result.stdout
    # connectedbuildings has no port yet, so the mission itself stays pending.
    assert "mission Cpg01_Mission6: success" not in result.stdout


def test_cpp_campaign_connected_buildings():
    # `0x566620`: mode 0 counts every active building of the type, mode 1
    # requires the +0x34 connected flag. The port derives it from a path
    # touching the footprint perimeter. Cpg04's park has connected LunchDoors
    # and farms.
    isl04 = GAME / "Data" / "Design" / "landscape" / "isl04" / "isl04.lnd"
    wld = SCENARIO / "Cpg04" / "Cpg04_Save.wld"
    mission5 = SCENARIO / "Cpg01" / "Cpg01_Mission5.ini"
    mission6 = SCENARIO / "Cpg01" / "Cpg01_Mission6.ini"
    if not all(p.exists() for p in (isl04, wld, mission5, mission6)):
        pytest.skip("retail Cpg04/Cpg01 data missing")
    common = [
        "--frames", "2", "--no-menu", "--width", "320", "--height", "240",
        "--no-vsync", "--no-save-config", "--lnd", str(isl04),
        "--asset-root", str(GAME), "--no-vegetation", "--no-water",
        "--wld-park", str(wld),
    ]
    lunch = _run(*common, "--start-mission", str(mission5))
    assert lunch.returncode == 0, lunch.stderr
    assert "mission objective 0 notconnectedbuildings -> success" in lunch.stdout
    assert "mission objective 1 connectedbuildings -> success" in lunch.stdout
    assert "mission Cpg01_Mission5: success" in lunch.stdout

    farm = _run(
        *common, "--hire-nutritionist", "2", "--start-mission", str(mission6)
    )
    assert farm.returncode == 0, farm.stderr
    assert "mission objective 0 connectedbuildings -> success" in farm.stdout
    assert "mission objective 1 employees -> success" in farm.stdout
    assert "mission Cpg01_Mission6: success" in farm.stdout


def test_cpp_campaign_mission_port_save(tmp_path):
    # The active mission id is part of DINOPSAV; a later --load-port resumes
    # the mission and its live objective evaluation.
    isl04 = GAME / "Data" / "Design" / "landscape" / "isl04" / "isl04.lnd"
    wld = SCENARIO / "Cpg04" / "Cpg04_Save.wld"
    mission = SCENARIO / "Cpg04" / "Cpg04_Mission1.ini"
    if not isl04.exists() or not wld.exists() or not mission.exists():
        pytest.skip("retail Cpg04 mission data missing")
    save = tmp_path / "mission.psav"
    first = _run(
        "--frames", "2", "--no-menu", "--width", "320", "--height", "240",
        "--no-vsync", "--no-save-config", "--lnd", str(isl04),
        "--asset-root", str(GAME), "--no-vegetation", "--no-water",
        "--wld-park", str(wld), "--park-budget", "25000",
        "--start-mission", str(mission), "--save-port", str(save),
    )
    assert first.returncode == 0, first.stderr
    assert "port save:" in first.stdout
    assert f"mission {mission.stem}" in save.read_text(encoding="utf-8")
    second = _run(
        "--frames", "2", "--no-menu", "--width", "320", "--height", "240",
        "--no-vsync", "--no-save-config", "--lnd", str(isl04),
        "--asset-root", str(GAME), "--no-vegetation", "--no-water",
        "--load-port", str(save),
    )
    assert second.returncode == 0, second.stderr
    assert "port load:" in second.stdout
    assert "mission: Cpg04_Mission1 objectives=3" in second.stdout
    assert "mission objective 2 dyingdinos -> success" in second.stdout


def test_cpp_campaign_auto_advance():
    # Closing CPGEND on a successful mission starts the next mission in the
    # same campaign. --auto-continue is the headless test hook.
    isl01 = GAME / "Data" / "Design" / "landscape" / "isl01" / "isl01.lnd"
    mission = SCENARIO / "Cpg01" / "Cpg01_Mission11.ini"
    if not isl01.exists() or not mission.exists():
        pytest.skip("retail Cpg01 mission data missing")
    args = [
        "--frames", "4", "--no-menu", "--width", "320", "--height", "240",
        "--no-vsync", "--no-save-config", "--lnd", str(isl01),
        "--asset-root", str(GAME), "--no-vegetation", "--no-water",
        "--build", "--visitors", "30", "--start-mission", str(mission),
        "--auto-continue",
    ]
    for x in range(126, 133):
        args += ["--place-path", f"{x},126", "--place-path", f"{x},132"]
    for y in range(127, 132):
        args += ["--place-path", f"126,{y}", "--place-path", f"132,{y}"]
    result = _run(*args)
    assert result.returncode == 0, result.stderr
    assert "mission Cpg01_Mission11: success" in result.stdout
    assert "campaign: next mission Cpg01_Mission12 budget=75000" in result.stdout
    assert "mission: Cpg01_Mission12 objectives=2" in result.stdout
    # Mission12's happyvisitors succeeds; connectedbuildings stays pending.
    assert "mission objective 1 happyvisitors -> success" in result.stdout
    assert "mission Cpg01_Mission12: success" not in result.stdout


def test_cpp_campaign_sized_agility():
    # 0x569040 maps arg0 to pattern[8] (+0x1e0 size class) and arg1 to
    # pattern[5] (+0x98 agility); mode 2 compares as minima. Cpg04 has a
    # size class >= 1 dino with agility >= 50, but none with agility >= 80.
    isl04 = GAME / "Data" / "Design" / "landscape" / "isl04" / "isl04.lnd"
    wld = SCENARIO / "Cpg04" / "Cpg04_Save.wld"
    mission = SCENARIO / "Cpg02" / "Cpg02_Mission3.ini"
    if not isl04.exists() or not wld.exists() or not mission.exists():
        pytest.skip("retail Cpg04/Cpg02 mission data missing")
    result = _run(
        "--frames", "2", "--no-menu", "--width", "320", "--height", "240",
        "--no-vsync", "--no-save-config", "--lnd", str(isl04),
        "--asset-root", str(GAME), "--no-vegetation", "--no-water",
        "--wld-park", str(wld), "--start-mission", str(mission),
    )
    assert result.returncode == 0, result.stderr
    assert "mission objective 1 sizedagilitydinos -> success" in result.stdout
    assert "mission objective 2 speeddinos -> success" in result.stdout
    assert "mission objective 0 sizedagilitydinos -> success" not in result.stdout


def test_cpp_campaign_hungry_objective():
    # Cpg01_Mission7 `hungrydinos -1 60`: every dino must have food >= 60.
    isl04 = GAME / "Data" / "Design" / "landscape" / "isl04" / "isl04.lnd"
    wld = SCENARIO / "Cpg04" / "Cpg04_Save.wld"
    mission = SCENARIO / "Cpg01" / "Cpg01_Mission7.ini"
    if not isl04.exists() or not wld.exists() or not mission.exists():
        pytest.skip("retail Cpg04/Cpg01 mission data missing")
    common = [
        "--frames", "2", "--no-menu", "--width", "320", "--height", "240",
        "--no-vsync", "--no-save-config", "--lnd", str(isl04),
        "--asset-root", str(GAME), "--no-vegetation", "--no-water",
        "--wld-park", str(wld), "--start-mission", str(mission),
    ]
    default = _run(*common)
    assert default.returncode == 0, default.stderr
    assert (
        "mission objective 0 hungrydinos: Prevent the dinos from starving."
        in default.stdout
    )
    # Imported dinosaurs default to FOOD=50, below the 60 threshold.
    assert "mission objective 0 hungrydinos -> success" not in default.stdout

    fed = _run(*common, "--dino-needs", "80,99,99,99")
    assert fed.returncode == 0, fed.stderr
    assert "mission objective 0 hungrydinos -> success" in fed.stdout
    assert "mission Cpg01_Mission7: success" in fed.stdout


def test_cpp_campaign_completion_unlock(tmp_path):
    # Cpg02_Mission5 is the last Cpg02 mission. 100 visitors with need0=99
    # satisfy `happyvisitors parent 60`; completing it shows the campaign
    # ending and writes the next campaign bit to dino.dat (0x02 -> 0x06).
    isl04 = GAME / "Data" / "Design" / "landscape" / "isl04" / "isl04.lnd"
    wld = SCENARIO / "Cpg04" / "Cpg04_Save.wld"
    mission = SCENARIO / "Cpg02" / "Cpg02_Mission5.ini"
    if not isl04.exists() or not wld.exists() or not mission.exists():
        pytest.skip("retail Cpg02/Cpg04 data missing")
    unlock = tmp_path / "dino.dat"
    unlock.write_bytes((0x02).to_bytes(4, "little"))
    progress = tmp_path / "campaign-progress.txt"
    result = _run(
        "--frames", "2", "--no-menu", "--width", "320", "--height", "240",
        "--no-vsync", "--no-save-config", "--lnd", str(isl04),
        "--asset-root", str(GAME), "--no-vegetation", "--no-water",
        "--wld-park", str(wld), "--visitors", "100",
        "--visitor-satisfaction", "99", "--dino-dat", str(unlock),
        "--campaign-progress", str(progress),
        "--start-mission", str(mission),
    )
    assert result.returncode == 0, result.stderr
    assert "visitors: 100 spawned on 301 path cells" in result.stdout
    assert "mission objective 0 happyvisitors -> success" in result.stdout
    assert "mission Cpg02_Mission5: success" in result.stdout
    assert "campaign complete: Cpg02 ending=" in result.stdout
    assert "campaign unlock: mask=0x6" in result.stdout
    assert "campaign progress: Cpg02 complete" in result.stdout
    assert unlock.read_bytes() == b"\x06\x00\x00\x00"
    # The finished campaign is removed from the progress file.
    assert "Cpg02" not in progress.read_text(encoding="utf-8")


def test_cpp_campaign_mission_success():
    isl01 = GAME / "Data" / "Design" / "landscape" / "isl01" / "isl01.lnd"
    mission = SCENARIO / "Cpg01" / "Cpg01_Mission11.ini"
    if not isl01.exists() or not mission.exists():
        pytest.skip("retail Cpg01 mission data missing")
    args = [
        "--frames", "2", "--no-menu", "--width", "320", "--height", "240",
        "--no-vsync", "--no-save-config", "--lnd", str(isl01),
        "--asset-root", str(GAME), "--no-vegetation", "--no-water",
        "--build", "--visitors", "30", "--start-mission", str(mission),
    ]
    for x in range(126, 133):
        args += ["--place-path", f"{x},126", "--place-path", f"{x},132"]
    for y in range(127, 132):
        args += ["--place-path", f"126,{y}", "--place-path", f"132,{y}"]
    result = _run(*args)
    assert result.returncode == 0, result.stderr
    assert "visitors: 30 spawned on 24 path cells" in result.stdout
    assert "mission objective 0 visitors -> success" in result.stdout
    assert "mission Cpg01_Mission11: success" in result.stdout


def test_cpp_campaign_live_traits():
    # Cpg04's imported dinosaurs carry phenotype snapshots; Cpg01_Mission9
    # wants one dinosaur with intelligence >= 80. The mission is just a
    # convenient objective carrier for the live-trait path.
    isl04 = GAME / "Data" / "Design" / "landscape" / "isl04" / "isl04.lnd"
    wld = SCENARIO / "Cpg04" / "Cpg04_Save.wld"
    mission = SCENARIO / "Cpg01" / "Cpg01_Mission9.ini"
    if not isl04.exists() or not wld.exists() or not mission.exists():
        pytest.skip("retail Cpg04/Cpg01 mission data missing")
    result = _run(
        "--frames", "2", "--no-menu", "--width", "320", "--height", "240",
        "--no-vsync", "--no-save-config", "--lnd", str(isl04),
        "--asset-root", str(GAME), "--no-vegetation", "--no-water",
        "--wld-park", str(wld), "--start-mission", str(mission),
    )
    assert result.returncode == 0, result.stderr
    assert "mission objective 0 cleverdinos -> success" in result.stdout
    assert "mission Cpg01_Mission9: success" in result.stdout
