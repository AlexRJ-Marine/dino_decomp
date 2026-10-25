import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from overlay import (  # noqa: E402
    OverlayFormatError,
    load_graphic_db,
    load_interface,
    parse_ini,
    resolve_text,
    text_bits,
)
from loc import read_loc  # noqa: E402

GAME = ROOT / "game" / "%MAINDIR%"
OVERLAY = GAME / "Data" / "Overlay"
MENU_INI = OVERLAY / "MenuInterface.Ini"
PARK_INI = OVERLAY / "ParkInterface.Ini"
MENU_DB = OVERLAY / "MenuGraphicDB.ini"
PARK_DB = OVERLAY / "ParkGraphicDB.Ini"
MENU_LOC = OVERLAY / "MenuEng.loc"
PARK_LOC = OVERLAY / "ParkEng.loc"


def test_parse_ini_counts():
    if not MENU_INI.exists():
        pytest.skip("retail sample not present")
    assert len(parse_ini(MENU_INI, expand=False)) == 114
    assert len(parse_ini(PARK_INI, expand=False)) == 137
    # ParkInterface expands nine panel files.
    assert len(parse_ini(PARK_INI)) == 137 + 2558


def test_menu_interface():
    if not MENU_INI.exists():
        pytest.skip("retail sample not present")
    interface = load_interface(MENU_INI)
    assert len(interface.widgets) == 114
    assert len(interface.pages()) == 9
    assert sum(1 for w in interface.widgets.values() if w.type == "BUTTON") == 47
    assert interface.pages()[0].name == "MAIN_MENU_PAGE"
    assert interface.pages()[-1].name == "LOADING_PAGE"

    # Parent-relative geometry: page -> panel -> button.
    free = interface.widgets["FREE_GAME_BUTTON"]
    assert (free.abs_x, free.abs_y, free.width, free.height) == (288, 223, 214, 23)
    quit_button = interface.widgets["QUIT_BUTTON"]
    assert (quit_button.abs_x, quit_button.abs_y, quit_button.width, quit_button.height) == (559, 484, 56, 55)


def test_park_interface():
    if not PARK_INI.exists():
        pytest.skip("retail sample not present")
    interface = load_interface(PARK_INI)
    assert len(interface.widgets) == 2694
    assert len(interface.roots()) == 38
    assert "DINO_ACHAT_SAVE_PANEL" in interface.widgets

    visitor = interface.widgets["VISITOR_AMOUNT"]
    assert (visitor.abs_x, visitor.abs_y) == (228, 578)
    budget = interface.widgets["BUDGET_AMOUNT"]
    assert (budget.abs_x, budget.abs_y) == (118, 578)
    clock = interface.widgets["PARK_CURRENT_TIME"]
    assert (clock.abs_x, clock.abs_y) == (729, 389)
    assert interface.widgets["MAP_PANEL"].abs_x == 616


def test_menu_graphic_db():
    if not MENU_DB.exists():
        pytest.skip("retail sample not present")
    graphics = load_graphic_db(MENU_DB)
    assert len(graphics) == 53
    button = graphics["MAIN_MENU_BUTTON"]
    assert button.objects == 8
    assert button.skip == frozenset({0, 1, 2, 3})
    assert button.used_frames() == [4, 5, 6, 7]
    assert button.orientation(256) == "vertical"
    assert button.frame_rect(4, 256) == (0, 0, 213, 23)
    assert button.frame_rect(5, 256) == (0, 23, 213, 23)
    assert button.frame_rect(0, 256) is None


def test_park_graphic_db_and_textures():
    if not PARK_DB.exists():
        pytest.skip("retail sample not present")
    graphics = load_graphic_db(PARK_DB)
    assert len(graphics) == 426 + 180 - 1  # LAB_NAME_PANEL is repeated identically
    missing = []
    for graphic in graphics.values():
        path = GAME / graphic.filename.replace("./", "")
        if not path.exists():
            missing.append(path.name)
    assert sorted(set(missing)) == ["TEMP_jaugebas.tga"]
    assert len([g for g in graphics.values() if "TEMP_jaugebas" in g.filename]) == 9


def test_frame_strips():
    if not MENU_DB.exists() or not PARK_DB.exists():
        pytest.skip("retail sample not present")
    park = load_graphic_db(PARK_DB)
    budget = park["BUDGET_PANEL_BUTTON"]
    assert budget.used_frames() == [0, 4, 5, 6, 7]
    assert budget.orientation(256) == "horizontal"
    assert [budget.frame_rect(f, 256)[0] for f in budget.used_frames()] == [0, 31, 62, 93, 124]

    zoom = park["MINI_MAP_ZOOMIN_BUTTON"]
    assert [zoom.frame_rect(f, 256)[0] for f in zoom.used_frames()] == [176, 192, 208, 224, 240]

    menu = load_graphic_db(MENU_DB)
    island = menu["ISLAND_LIST_BUTTON"]
    assert island.orientation(256) == "vertical"
    assert [island.frame_rect(f, 256)[1] for f in island.used_frames()] == [55, 89, 123, 157]

    easy = menu["EASY_MODE_BUTTON"]
    assert easy.orientation(256) == "horizontal"
    assert [easy.frame_rect(f, 256)[0] for f in easy.used_frames()] == [0, 52, 104, 156]


def test_text_justify_bits():
    assert text_bits("CENTER") == 15
    assert text_bits("LEFT") == 1
    assert text_bits("RIGHT") == 2
    assert text_bits("X") == 3
    assert text_bits("Y") == 12
    assert text_bits("LEFT|UP") == 5


def test_loc_resolution():
    if not MENU_LOC.exists():
        pytest.skip("retail sample not present")
    loc = read_loc(MENU_LOC)
    assert resolve_text("&FreeGame", [loc]) == "FREE MODE"
    assert resolve_text("&Options", [loc]) == "OPTIONS"
    assert resolve_text("literal", [loc]) == "literal"
    assert resolve_text("&Missing", [loc]) == "&Missing"


def test_resize_anchors():
    if not PARK_INI.exists():
        pytest.skip("retail sample not present")
    interface = load_interface(PARK_INI)
    gauge = interface.widgets["GAUGE_PANEL"]
    deco = interface.widgets["DECO_BOUT_GAUCHE"]
    render = interface.widgets["RENDERVIEW"]
    interface.apply_resize(1024, 768)
    assert (gauge.abs_x, gauge.abs_y) == (293, 737)
    assert (deco.abs_x, deco.abs_y) == (0, 572)
    assert (render.width, render.height) == (1024, 768)


def test_align_center_and_missing_parent(tmp_path):
    center = tmp_path / "center.ini"
    center.write_text(
        "[PAGE]\nTYPE=WIDGET\nPARENT=ROOT\nX=0\nY=0\nWIDTH=800\nHEIGHT=600\n"
        "ALIGN_CENTER=1\n[CHILD]\nTYPE=WIDGET\nPARENT=PAGE\nX=10\nY=20\nWIDTH=30\nHEIGHT=40\n",
        encoding="utf-8",
    )
    interface = load_interface(center)
    interface.apply_resize(1024, 768)
    page = interface.widgets["PAGE"]
    assert (page.abs_x, page.abs_y) == (112, 84)
    assert (interface.widgets["CHILD"].abs_x, interface.widgets["CHILD"].abs_y) == (122, 104)

    bad = tmp_path / "bad.ini"
    bad.write_text("[A]\nTYPE=WIDGET\nPARENT=NOPE\n", encoding="utf-8")
    with pytest.raises(OverlayFormatError):
        load_interface(bad)

    cycle = tmp_path / "cycle.ini"
    cycle.write_text(
        "[A]\nTYPE=WIDGET\nPARENT=B\n[B]\nTYPE=WIDGET\nPARENT=A\n", encoding="utf-8"
    )
    with pytest.raises(OverlayFormatError):
        load_interface(cycle)
