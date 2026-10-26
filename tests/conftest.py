import shutil
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
#: Free-game autosave reference (39-show catalog, budget 75,000). A live retail
#: campaign run overwrites run/save/auto/auto.sav with a 20-dinosaur save, so
#: the retail-save tests read a private copy of this reference instead.
FREE_GAME_AUTO = ROOT / "run" / "save" / "auto" / "slot001.sav"


@pytest.fixture(scope="session")
def retail_auto_save(tmp_path_factory):
    if not FREE_GAME_AUTO.exists():
        pytest.skip("free-game auto save reference missing")
    target = tmp_path_factory.mktemp("retail-auto") / "auto.sav"
    shutil.copyfile(FREE_GAME_AUTO, target)
    return target
