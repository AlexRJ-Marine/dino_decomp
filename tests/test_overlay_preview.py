import sys
from pathlib import Path

import pytest
from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))
sys.path.insert(0, str(ROOT / "tools" / "viewer"))

from loc import read_loc  # noqa: E402
from overlay import load_graphic_db, load_interface  # noqa: E402
from overlay_preview import (  # noqa: E402
    Assets,
    free_game_options,
    hud_options,
    render_hud,
    render_page,
)

GAME = ROOT / "game" / "%MAINDIR%"
OVERLAY = GAME / "Data" / "Overlay"
MENU_INI = OVERLAY / "MenuInterface.Ini"
PARK_INI = OVERLAY / "ParkInterface.Ini"
MENU_DB = OVERLAY / "MenuGraphicDB.ini"
PARK_DB = OVERLAY / "ParkGraphicDB.ini"
RETAIL_PARK = ROOT / "notes" / "park-after2.png"


def _bright(image: Image.Image, box: tuple[int, int, int, int], threshold: int = 200) -> int:
    crop = image.crop(box).convert("RGB")
    return sum(1 for p in crop.get_flattened_data() if p[0] > threshold and p[1] > threshold and p[2] > threshold)


def _mean_luma(image: Image.Image, box: tuple[int, int, int, int]) -> float:
    crop = image.crop(box).convert("L")
    pixels = list(crop.get_flattened_data())
    return sum(pixels) / len(pixels)


def _mad(a: Image.Image, b: Image.Image, box: tuple[int, int, int, int]) -> float:
    diff = ImageChops.difference(a.crop(box).convert("RGB"), b.crop(box).convert("RGB")).convert("L")
    hist = diff.histogram()
    return sum(i * count for i, count in enumerate(hist)) / ((box[2] - box[0]) * (box[3] - box[1]))


@pytest.fixture(scope="module")
def menu_assets():
    if not MENU_INI.exists() or not MENU_DB.exists():
        pytest.skip("retail sample not present")
    interface = load_interface(MENU_INI)
    graphics = load_graphic_db(MENU_DB)
    locs = [read_loc(OVERLAY / "MenuEng.loc"), read_loc(OVERLAY / "ParkEng.loc")]
    return interface, graphics, locs, Assets(GAME)


def test_render_main_menu(tmp_path, menu_assets):
    interface, graphics, locs, assets = menu_assets
    image = render_page(interface, graphics, assets, locs, "MAIN_MENU_PAGE")
    assert image.size == (800, 600)
    assert image.convert("RGB").getpixel((5, 5))[0] > 200  # retail white art

    for name in (
        "FREE_GAME_BUTTON",
        "CAMPAIGN_BUTTON",
        "LOAD_GAME_BUTTON",
        "OPTIONS_BUTTON",
        "CINEMATICS_BUTTON",
        "CREDITS_BUTTON",
    ):
        widget = interface.widgets[name]
        box = (widget.abs_x, widget.abs_y, widget.abs_x + widget.width, widget.abs_y + widget.height)
        assert _bright(image, box) > 20, name
    out = tmp_path / "menu.png"
    image.save(out)
    assert out.exists()


def test_render_free_game(menu_assets):
    interface, graphics, locs, assets = menu_assets
    options = free_game_options(interface, graphics, locs, island=1, difficulty="easy")
    image = render_page(interface, graphics, assets, locs, "FREE_GAME_PAGE", options=options)
    assert image.size == (800, 600)

    first_row = (175, 166, 341, 200)
    second_row = (175, 200, 341, 234)
    assert _mean_luma(image, first_row) > _mean_luma(image, second_row) + 5
    assert _bright(image, first_row) > 20

    picture = (448, 177, 673, 367)
    assert _mean_luma(image, picture) > 20
    assert _bright(image, (502, 506, 551, 556), 150) > 10  # start icon


def test_render_hud():
    if not PARK_INI.exists() or not PARK_DB.exists():
        pytest.skip("retail sample not present")
    interface = load_interface(PARK_INI)
    graphics = load_graphic_db(PARK_DB)
    locs = [read_loc(OVERLAY / "MenuEng.loc"), read_loc(OVERLAY / "ParkEng.loc")]
    assets = Assets(GAME)
    image = render_hud(interface, graphics, assets, locs, options=hud_options())
    assert image.size == (800, 600)

    budget = interface.widgets["BUDGET_AMOUNT"]
    assert _bright(image, (budget.abs_x, budget.abs_y, budget.abs_x + budget.width,
                           budget.abs_y + budget.height)) > 20
    clock = interface.widgets["PARK_CURRENT_TIME"]
    assert _bright(image, (clock.abs_x, clock.abs_y, clock.abs_x + clock.width,
                           clock.abs_y + clock.height)) > 10

    # The right-panel icons must use the coloured frame, not the gray disabled one.
    lab = interface.widgets["OPEN_LABORATORY"]
    crop = image.crop((lab.abs_x, lab.abs_y, lab.abs_x + lab.width, lab.abs_y + lab.height)).convert("RGB")
    assert any(b > r + 10 for r, g, b in crop.get_flattened_data())

    if RETAIL_PARK.exists():
        retail = Image.open(RETAIL_PARK).convert("RGB")
        assert _mad(image, retail, (707, 0, 800, 417)) < 15
        assert _mad(image, retail, (69, 569, 687, 600)) < 20
