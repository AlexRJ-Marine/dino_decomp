"""Static guards for the app shell (`src/app`).

These tests read the source instead of launching the binary, so they run in
the fast gate. They exist because the park screenshot tests only sample HUD
pixels: a dropped frame phase or a local shadowing a `Game` member leaves the
3D scene broken while the window tests still pass (see
`notes/refactor-camera-shadowing.md`).

The phase lists are deliberately explicit. Adding, removing or reordering a
phase is a design change; it must update `docs/platform/app-shell.md` and this
test, which is cheaper than silently dropping a call.
"""

import ast
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "src" / "app"

FRAME_PHASES = [
    "handle_input",
    "begin_frame",
    "advance_time",
    "update_session",
    "update_audio",
    "update_camera",
    "update_diagnostics",
    "set_background",
    "update_simulation",
    "update_camera_view",
    "update_placement",
    "update_preview",
    "draw_frame",
    "present_frame",
]

BOOT_PHASES = [
    "boot_command_line",
    "boot_budgets_and_clock",
    "boot_campaign_and_saves",
    "boot_menu_state",
    "boot_menu_overlay",
    "boot_demo_defaults",
    "boot_load_assets",
    "boot_build_textures",
    "boot_print_modes",
    "boot_platform",
    "boot_scene_resources",
    "boot_park_objects",
    "boot_minimap_and_visitors",
    "boot_employee_profiles",
    "boot_import_dinos",
    "boot_food_definitions",
    "boot_camera_setup",
    "boot_park_session",
    "boot_employee_session",
    "boot_build_tools",
    "boot_finalize",
]

DECLARATION = re.compile(
    r"^\s+(?:const\s+)?(?:static\s+)?(?:volatile\s+)?"
    r"(?:unsigned\s+\w+|signed\s+\w+|std::[\w:]+|dino::[\w:]+|[\w:]+)"
    r"(?:\s*<[^;{}]*?>)?"
    r"[\s\*&]+(\w+)\s*(?:[=;{(\[]|$)"
)

# Lines that look like a declaration but are statements with two tokens.
NOT_A_DECLARATION = (
    "return ", "else ", "case ", "if ", "for ", "while ", "do ",
    "switch ", "goto ", "break", "continue", "delete ", "throw ",
)


def _read(name):
    return (APP / name).read_text(encoding="utf-8").replace("\r\n", "\n")


def _method_body(name):
    """The brace-balanced body of the single Game method called `name`."""
    pattern = re.compile(
        r"^(?:[\w:<>,\*&\s]+?\s)?Game::" + re.escape(name) + r"\s*\(",
        re.MULTILINE,
    )
    for path in sorted(APP.glob("game*.cpp")):
        text = path.read_text(encoding="utf-8").replace("\r\n", "\n")
        match = pattern.search(text)
        if match is None:
            continue
        start = text.find("{", match.start())
        assert start >= 0, f"{path.name}: {name} has no body"
        depth = 0
        for index in range(start, len(text)):
            if text[index] == "{":
                depth += 1
            elif text[index] == "}":
                depth -= 1
                if depth == 0:
                    return text[start:index + 1]
        raise AssertionError(f"{path.name}: unbalanced body in {name}")
    raise AssertionError(f"Game::{name} not found in src/app/game*.cpp")


def _game_members():
    """Direct data members of `class Game`, ignoring nested types."""
    members = set()
    depth = 0
    paren = 0
    in_class = False
    for line in _read("game.h").split("\n"):
        code = line.split("//", 1)[0].strip()
        if not in_class:
            if re.match(r"class Game\b", code):
                in_class = True
                depth = code.count("{") - code.count("}")
            continue
        if (depth == 1 and paren == 0 and code.endswith(";")):
            declaration = code[:-1].split("=", 1)[0].strip()
            if not declaration.startswith(
                    ("using ", "struct ", "class ", "enum ", "typedef ",
                     "friend ")):
                # Drop template arguments and brace initializers so members
                # with `std::function<int(const std::string&)>` or `{0}` parse.
                flat = re.sub(r"<[^<>]*(?:<[^<>]*>[^<>]*)*>", "",
                              declaration)
                flat = re.sub(r"\{[^{}]*\}\s*$", "", flat).strip()
                if not any(ch in flat for ch in "(){}"):
                    name = re.search(r"(\w+)\s*(?:\[[^\]]*\])?\s*$", flat)
                    if name:
                        members.add(name.group(1))
        paren += code.count("(") - code.count(")")
        depth += code.count("{") - code.count("}")
        if depth == 0:
            break
    return members


def test_window_and_headless_test_markers_are_correct():
    """`-Fast` must be headless; window launches stay opt-in (MAINT-4/7).

    Every test module that launches the port is scanned, not just the app
    tests. `--frames` and `--play-video` need a swap chain and are window
    modes; report flags (`--selftest`, `--validate`, `--sav-*`, `--*-info`)
    return from `boot()` before SDL starts and must stay unmarked. A
    module-wide `slow` marker is banned because it would deselect the headless
    cases too. Module-level helpers are followed (`_ui_shot`,
    `_vegetation_frame`, `_run`).
    """
    modules = []
    legacy_env = "DINO_" + "VIEWER_EXE"  # keep the naming guard happy
    for path in sorted((ROOT / "tests").glob("test_*.py")):
        if path.name == "test_app_shell.py":
            continue  # this guard reads the other modules; it launches nothing
        text = path.read_text(encoding="utf-8")
        if "DINO_EXE" in text or legacy_env in text:
            modules.append((path, text))

    window_flags = {
        "--frames", "--play-video", "--screenshot", "--capture-dir",
        "--park-window-click", "--camera-selftest",
    }
    report_flags = {
        "--selftest", "--validate", "--sav-info", "--sav-roundtrip",
        "--sav-write", "--wld-info", "--bai-info", "--market-info",
        "--store-info", "--food-info", "--show-info", "--dino-actions",
        "--employee-info", "--campaign-info", "--campaign-coverage",
        "--campaign-mission", "--video-info", "--video-frame",
        "--list-keys", "--list-sounds", "--budget-info",
    }

    def flags(node):
        return {child.value for child in ast.walk(node)
                if isinstance(child, ast.Constant)
                and isinstance(child.value, str)
                and child.value.startswith("--")}

    unmarked_window = []
    marked_headless = []
    fast = 0
    for path, text in modules:
        assert "pytestmark = pytest.mark.slow" not in text, (
            f"{path.name}: a module-wide slow marker at module scope")
        tree = ast.parse(text)
        functions = {node.name: node for node in tree.body
                     if isinstance(node, ast.FunctionDef)}
        tests = [node for node in tree.body
                 if isinstance(node, ast.FunctionDef)
                 and node.name.startswith("test_")]
        helpers = set(functions) - {node.name for node in tests}
        for test in tests:
            seen, pending, used = set(), [test.name], set()
            while pending:
                name = pending.pop()
                if name in seen:
                    continue
                seen.add(name)
                body = functions[name]
                used |= flags(body)
                for child in ast.walk(body):
                    if (isinstance(child, ast.Call)
                            and isinstance(child.func, ast.Name)
                            and child.func.id in helpers):
                        pending.append(child.func.id)
            marked = any(
                isinstance(decorator, ast.Attribute)
                and decorator.attr == "slow"
                for decorator in test.decorator_list)
            where = f"{path.name}::{test.name}"
            if used & window_flags:
                if not marked:
                    unmarked_window.append(where)
            elif used & report_flags:
                fast += 1
                if marked:
                    marked_headless.append(where)
    assert not unmarked_window, (
        "window tests missing @pytest.mark.slow:\n" +
        "\n".join(unmarked_window))
    assert not marked_headless, (
        "headless tests deselected by -Fast:\n" + "\n".join(marked_headless))
    assert fast >= 45, f"only {fast} headless exe tests run in the fast gate"


def test_main_is_only_the_entry_point():
    text = _read("main.cpp")
    assert "Game(argc, argv).run()" in text
    assert len(text.splitlines()) <= 30


def test_run_calls_the_frame_phases_in_order():
    body = _method_body("run")
    calls = re.findall(r"^        (\w+)\(\);$", body, re.MULTILINE)
    assert calls == FRAME_PHASES


def test_boot_calls_the_boot_phases_in_order():
    body = _method_body("boot")
    calls = re.findall(
        r"if \(std::optional<int> exit_code = (\w+)\(\)\)", body)
    assert calls == BOOT_PHASES


def test_lower_layers_do_not_include_sdl_or_gl():
    """`src/formats`, `src/world` and `src/ui` must stay headless.

    The rule keeps the simulation testable without a window and reusable
    outside the port (docs/platform/naming.md). Only includes are checked:
    a comment may name SDL.
    """
    offenders = []
    for folder in ("formats", "world", "ui"):
        for path in sorted((ROOT / "src" / folder).glob("*")):
            if path.suffix not in (".h", ".cpp"):
                continue
            text = path.read_text(encoding="utf-8").replace("\r\n", "\n")
            for number, line in enumerate(text.split("\n"), 1):
                stripped = line.strip()
                if not stripped.startswith("#include"):
                    continue
                if ("SDL" in stripped or "GL/" in stripped or
                        "gl.h" in stripped):
                    offenders.append(
                        f"{path.relative_to(ROOT)}:{number}: {stripped}")
    assert not offenders, "\n".join(offenders)


def test_no_function_scope_local_shadows_a_game_member():
    """The 2026-10-29 camera bug: a local hid a Game member for a whole phase.

    Only declarations at the function body's own scope (and parameters) are
    checked. A local inside a nested block is allowed: it cannot hide the
    member for the rest of the method, and several drawing helpers build
    their own camera that way on purpose.
    """
    members = _game_members()
    assert len(members) > 200, f"member scan found only {len(members)}"

    hits = []
    for path in sorted(APP.glob("game*.cpp")):
        lines = path.read_text(encoding="utf-8").replace("\r\n", "\n").split("\n")
        for index, line in enumerate(lines):
            match = re.match(
                r"^(?:[\w:<>,\*&\s]+?\s)?Game::(\w+)\s*\(", line)
            if match is None:
                continue
            signature = line
            cursor = index
            while "{" not in signature and cursor + 1 < len(lines):
                cursor += 1
                signature += " " + lines[cursor].strip()
            params = re.search(r"\((.*)\)\s*(?:const\s*)?\{", signature)
            if params is not None:
                for part in re.split(r",(?![^<]*>)", params.group(1)):
                    part = part.split("=", 1)[0].strip()
                    name = re.search(r"(\w+)\s*(?:\[\s*\])?\s*$", part)
                    if name and name.group(1) in members:
                        hits.append(
                            f"{path.name}:{index + 1}: parameter "
                            f"{name.group(1)} shadows a Game member")
            depth = 0
            started = False
            for number in range(index, len(lines)):
                text = lines[number]
                if not started:
                    if "{" in text:
                        started = True
                        depth = text.count("{") - text.count("}")
                    continue
                if depth == 1:
                    stripped = text.strip()
                    if (not stripped.startswith(NOT_A_DECLARATION) and
                            not stripped.startswith(":")):
                        declaration = DECLARATION.match(text)
                        if declaration and declaration.group(1) in members:
                            hits.append(
                                f"{path.name}:{number + 1}: local "
                                f"{declaration.group(1)} shadows a Game member")
                depth += text.count("{") - text.count("}")
                if depth <= 0:
                    break
    assert not hits, "\n".join(hits)
