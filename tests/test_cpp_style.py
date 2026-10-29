"""Static guards for the C++ Core Guidelines policy.

See `docs/platform/cpp-guidelines.md`. These read files only, so they run in
the fast gate. The 32-bit retail proxy and the vendored decoders are out of
scope and keep their own style.
"""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
EXCLUDED = {"hook", "third_party"}


def _sources(suffix):
    for path in sorted(SRC.rglob(f"*{suffix}")):
        if path.relative_to(SRC).parts[0] in EXCLUDED:
            continue
        yield path


def _code(path):
    """Source with strings and comments removed, so guards see code only."""
    text = path.read_text(encoding="utf-8").replace("\r\n", "\n")
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = re.sub(r'"(?:\\.|[^"\\])*"', '""', text)
    text = re.sub(r"'(?:\\.|[^'\\])*'", "''", text)
    return re.sub(r"//[^\n]*", "", text)


def _hits(pattern, paths):
    rx = re.compile(pattern)
    return [f"{path.relative_to(ROOT)}: {match.group(0).strip()}"
            for path in paths for match in rx.finditer(_code(path))]


def test_no_naked_new_or_delete():
    """R.11: ownership goes through an RAII handle, not new/delete."""
    paths = list(_sources(".cpp")) + list(_sources(".h"))
    offenders = _hits(
        r"\bnew\s+(?:[A-Za-z_]\w*::)*[A-Za-z_]\w*\s*[({[]", paths)
    offenders += _hits(r"\bdelete\s*(?:\[\s*\])?\s*[A-Za-z_(]", paths)
    assert not offenders, "\n".join(offenders)


def test_no_using_namespace_in_headers():
    """SF.7: a header must not impose a namespace on its includers."""
    offenders = _hits(r"^\s*using\s+namespace\b", list(_sources(".h")))
    assert not offenders, "\n".join(offenders)


def test_no_null_or_std_endl():
    """ES.47/SL.io.50: use nullptr and '\\n'."""
    paths = list(_sources(".cpp")) + list(_sources(".h"))
    offenders = _hits(r"\bNULL\b", paths) + _hits(r"\bstd::endl\b", paths)
    assert not offenders, "\n".join(offenders)


def test_pi_literal_has_one_definition():
    """ES.45: dino::kPi is the only 3.14159... in first-party code."""
    paths = list(_sources(".cpp")) + list(_sources(".h"))
    offenders = []
    for path in paths:
        for number, line in enumerate(_code(path).split("\n"), 1):
            if "3.14159" not in line:
                continue
            if (path.name == "math.h" and
                    "constexpr float kPi = 3.14159265f;" in line):
                continue
            offenders.append(f"{path.relative_to(ROOT)}:{number}: {line.strip()}")
    assert not offenders, "\n".join(offenders)


def test_game_is_neither_copyable_nor_movable():
    """C.21: Game owns SDL/GL resources; a copy would double-destroy them."""
    text = (ROOT / "src" / "app" / "game.h").read_text(encoding="utf-8")
    for deleted in (
        "Game(const Game&) = delete;",
        "Game& operator=(const Game&) = delete;",
        "Game(Game&&) = delete;",
        "Game& operator=(Game&&) = delete;",
    ):
        assert deleted in text, deleted
