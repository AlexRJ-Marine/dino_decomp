"""The retail UI click map matches the shipped overlay data.

The map is the contract testing agents use for virtual clicks, so the points
that existing harness scripts rely on are pinned here. The generated JSON is
also checked for staleness against a fresh build.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from ui_map import (  # noqa: E402
    DEFAULT_DATA_ROOT,
    INTERACTIVE_TYPES,
    MENU_INI,
    PARK_INI,
    RADIAL_INI,
    build_map,
    load_interface,
)

JSON_MAP = ROOT / "docs" / "platform" / "ui-coordinates.json"
MARKDOWN_MAP = ROOT / "docs" / "platform" / "ui-coordinates.md"


def _map():
    if not DEFAULT_DATA_ROOT.exists():
        pytest.skip("retail sample not present")
    return build_map()


def _control(ui_map, screen_id: str, name: str) -> dict:
    for screen in ui_map["screens"]:
        if screen["id"] != screen_id:
            continue
        for control in screen["controls"]:
            if control["name"] == name:
                return control
    raise AssertionError(f"{screen_id}: no control {name}")


def test_map_counts_and_sources():
    ui_map = _map()
    assert ui_map["design"] == {"width": 800, "height": 600}
    assert ui_map["counts"]["screens"] == 46
    assert ui_map["counts"]["controls"] == 499
    assert ui_map["counts"]["menu_controls"] == 37
    assert ui_map["counts"]["park_controls"] == 444
    assert ui_map["counts"]["radial_controls"] == 18
    assert ui_map["sources"]["park"] == "Data/Overlay/ParkInterface.Ini"


def test_menu_click_points():
    ui_map = _map()
    assert _control(ui_map, "menu-main", "FREE_GAME_BUTTON")["click"] == [395, 234]
    assert _control(ui_map, "menu-main", "CAMPAIGN_BUTTON")["click"] == [395, 189]
    assert _control(ui_map, "menu-main", "QUIT_BUTTON")["click"] == [587, 511]
    assert _control(ui_map, "menu-free-game", "FREE_EASY_BUTTON")["click"] == [494, 451]
    assert _control(ui_map, "menu-free-game", "FREE_START_BUTTON")["click"] == [526, 531]
    assert _control(ui_map, "menu-campaign-select", "CAMPAIGN_START_BUTTON")["click"] == [364, 530]
    assert _control(ui_map, "menu-campaign-info", "CAMPAIGN_INFO_START_BUTTON")["click"] == [521, 449]


def test_menu_list_rows():
    ui_map = _map()
    islands = _control(ui_map, "menu-free-game", "ISLAND_LIST")
    assert islands["row_height"] == 34
    assert len(islands["rows"]) == 8
    assert islands["rows"][0]["click"] == [269, 183]
    assert islands["rows"][0]["label"] == "IGUANAO"
    assert islands["rows"][7]["click"] == [269, 421]
    assert islands["rows"][7]["label"] == "SANTO DIPLODO"
    assert islands["scroll"] == {
        "width": 22,
        "up": [352, 177],
        "down": [352, 440],
    }

    campaigns = _control(ui_map, "menu-campaign-select", "CAMPAIGN_LIST")
    assert campaigns["row_height"] == 34
    assert len(campaigns["rows"]) == 8
    assert campaigns["rows"][3]["click"] == [409, 285]
    assert campaigns["rows"][3]["label"] == "PANIC IN THE SAVANNA"


def test_park_hud_and_save_points():
    ui_map = _map()
    assert _control(ui_map, "park-hud", "OPEN_LABORATORY")["click"] == [757, 46]
    assert _control(ui_map, "park-hud", "OPEN_OPTIONS")["click"] == [765, 348]
    assert _control(ui_map, "park-hud", "OPEN_BUDGET_PANEL")["click"] == [97, 584]
    assert _control(ui_map, "park-hud", "OPEN_VISITOR_PANEL")["click"] == [207, 584]
    assert _control(ui_map, "park-hud", "OPEN_SHOW_LIST_PANEL")["click"] == [397, 584]
    assert _control(ui_map, "park-hud", "OPEN_PADDOCK_LIST_PANEL")["click"] == [492, 584]

    assert _control(ui_map, "park-options", "OPTIONS_MENU_SAVE_BUTTON")["click"] == [348, 73]
    assert _control(ui_map, "park-options", "OPTIONS_NAME_SLOT")["click"] == [349, 382]
    assert _control(ui_map, "park-options", "OPTIONS_VALIDATE_BUTTON")["click"] == [334, 436]


def test_open_paths_resolve():
    ui_map = _map()
    for screen in ui_map["screens"]:
        for step in screen["open"]:
            assert step["click"] is not None, (screen["id"], step["control"])
            assert step["screen"] is not None, (screen["id"], step["control"])


def test_every_click_is_inside_its_rect():
    ui_map = _map()
    for screen in ui_map["screens"]:
        names = set()
        for control in screen["controls"]:
            assert control["name"] not in names, (screen["id"], control["name"])
            names.add(control["name"])
            x, y, w, h = control["rect"]
            cx, cy = control["click"]
            assert w > 0 and h > 0
            assert x <= cx < x + w
            assert y <= cy < y + h
            if "rows" in control:
                for row in control["rows"]:
                    assert y <= row["click"][1] < y + h
            if "scroll" in control:
                for point in (control["scroll"]["up"], control["scroll"]["down"]):
                    assert x <= point[0] < x + w
                    assert y <= point[1] < y + h


def test_key_controls_are_active_when_their_screen_opens():
    ui_map = _map()
    assert _control(ui_map, "park-hud", "OPEN_LABORATORY")["active"]
    assert _control(ui_map, "park-options", "OPTIONS_MENU_SAVE_BUTTON")["active"]
    assert not _control(ui_map, "park-options", "OPTIONS_VALIDATE_BUTTON")["active"]
    assert _control(ui_map, "dialog-warning", "WARNING_OUTCOME_ICON")["active"]
    # The texture gene group is the authored default in LAB_BREED_CENTER.
    assert _control(ui_map, "park-lab-breed", "BREED_SKIN_TYPE_VARIATION_1")["active"]
    assert not _control(ui_map, "park-lab-breed", "BREED_HEAD_TYPE_VARIATION_1")["active"]


def test_map_covers_every_interactive_retail_widget():
    ui_map = _map()
    actual = {c["name"] for s in ui_map["screens"] for c in s["controls"]}

    expected = set()
    interfaces = []
    interfaces.append(load_interface(DEFAULT_DATA_ROOT / MENU_INI))
    park = load_interface(DEFAULT_DATA_ROOT / PARK_INI)
    park.apply_resize(800, 600)
    interfaces.append(park)
    interfaces.append(load_interface(DEFAULT_DATA_ROOT / RADIAL_INI))
    for interface in interfaces:
        for widget in interface.widgets.values():
            if widget.type in INTERACTIVE_TYPES and widget.width > 0 and widget.height > 0:
                expected.add(widget.name)

    missing = expected - actual
    extra = actual - expected
    assert not missing, f"missing from map: {sorted(missing)}"
    assert not extra, f"not in retail data: {sorted(extra)}"


def test_generated_json_is_current():
    if not JSON_MAP.exists():
        pytest.skip("generated UI map not present")
    ui_map = _map()
    on_disk = json.loads(JSON_MAP.read_text(encoding="utf-8"))
    assert on_disk == ui_map, "regenerate docs/platform/ui-coordinates.json"


def test_generated_markdown_is_present():
    if not MARKDOWN_MAP.exists():
        pytest.skip("generated UI map not present")
    text = MARKDOWN_MAP.read_text(encoding="utf-8")
    assert text.startswith("# Retail UI click map")
    assert "docs/platform/ui-coordinates.json" in text
    assert "499 interactive controls" in text
