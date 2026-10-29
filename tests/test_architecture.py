"""Architecture guards: layer direction and the Game size ratchet.

See `docs/platform/architecture.md`. These tests read the source, so they run
in the fast gate and fail on an injected wrong-way include or a Game that grew
past its current budget. Lower the budgets as ARCH-2..6 land; never raise one
without recording why in the architecture page.
"""

import re
from pathlib import Path

from test_app_shell import _game_members

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"

LAYERS = ("app", "ui", "world", "render", "audio", "formats")

# One-way include graph. `formats` may use the vendored decoder; the 32-bit
# hook proxy and third_party are outside the port layers.
ALLOWED = {
    "app": {"app", "ui", "render", "audio", "world", "formats"},
    "ui": {"ui", "formats"},
    "world": {"world", "formats"},
    "render": {"render", "formats"},
    "audio": {"audio", "formats"},
    "formats": {"formats", "third_party"},
}

GAME_MEMBERS_MAX = 266
GAME_METHODS_MAX = 171
GAME_HEADER_LINES_MAX = 665


def _include_edges():
    """(from layer, to layer, relative file) for explicit project includes."""
    edges = []
    for layer in LAYERS:
        for path in sorted((SRC / layer).rglob("*")):
            if path.suffix not in (".h", ".cpp"):
                continue
            text = path.read_text(encoding="utf-8", errors="replace")
            for match in re.finditer(r'#include\s+"([^"]+)"', text):
                target = match.group(1)
                if target.startswith("../"):
                    parts = target.split("/")
                    if len(parts) > 1 and parts[1] in LAYERS + ("third_party",):
                        edges.append((layer, parts[1],
                                      str(path.relative_to(ROOT))))
                elif "/" in target:
                    head = target.split("/")[0]
                    if head in LAYERS + ("third_party",):
                        edges.append((layer, head,
                                      str(path.relative_to(ROOT))))
    return edges


def test_layer_includes_point_downward():
    violations = [f"{source} -> {target} ({path})"
                  for source, target, path in _include_edges()
                  if target not in ALLOWED[source]]
    assert not violations, (
        "wrong-way layer includes (see docs/platform/architecture.md):\n" +
        "\n".join(sorted(set(violations))))


def test_lower_layers_do_not_include_the_app():
    offenders = [f"{path}: {source} -> {target}"
                 for source, target, path in _include_edges()
                 if target == "app" and source != "app"]
    assert not offenders, "\n".join(offenders)


def test_game_size_ratchet():
    members = _game_members()
    assert len(members) <= GAME_MEMBERS_MAX, (
        f"Game grew to {len(members)} data members "
        f"(budget {GAME_MEMBERS_MAX}); a phase must own the new state")
    methods = set()
    for path in (SRC / "app").glob("game*.cpp"):
        text = re.sub(r"//[^\n]*", "", path.read_text(encoding="utf-8"))
        methods.update(re.findall(r"\bGame::(\w+)\s*\(", text))
    assert len(methods) <= GAME_METHODS_MAX, (
        f"Game grew to {len(methods)} methods (budget {GAME_METHODS_MAX})")
    lines = len((SRC / "app" / "game.h").read_text(
        encoding="utf-8").split("\n"))
    assert lines <= GAME_HEADER_LINES_MAX, (
        f"game.h grew to {lines} lines (budget {GAME_HEADER_LINES_MAX}); "
        "put domain state in its own header")
