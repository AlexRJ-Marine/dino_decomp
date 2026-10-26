"""The feature status matrix is a maintained index.

Checks that every evidence link resolves, every screen id/control name exists
in the generated retail UI map, and every listed test exists. Also checks the
format/decomp docs are all referenced from at least one feature row.
"""
from __future__ import annotations

import json
import re
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
MATRIX = ROOT / "docs" / "platform" / "feature-status.md"
UI_JSON = ROOT / "docs" / "platform" / "ui-coordinates.json"

STATUSES = {"ported", "partial", "decoded", "not ported"}
LINK_RE = re.compile(r"\[[^\]]*\]\(([^)]+)\)")
TEST_RE = re.compile(r"tests/[\w./*\-]+(?:\.py)?(?:::\w+)?")


def _rows() -> list[dict[str, str]]:
    assert MATRIX.exists(), "missing docs/platform/feature-status.md"
    lines = MATRIX.read_text(encoding="utf-8").splitlines()
    header = None
    rows = []
    for line in lines:
        if not line.startswith("|"):
            continue
        cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
        if not cells:
            continue
        if cells[0] == "Domain":
            header = cells
            continue
        if all(set(cell) <= {"-", ":"} for cell in cells):
            continue
        if header is None or len(cells) != len(header):
            continue
        rows.append(dict(zip(header, cells)))
    assert rows, "feature matrix has no rows"
    assert header == [
        "Domain", "Feature", "Status", "Screen", "Controls", "Evidence",
        "Port / docs", "Tests", "Gaps",
    ], header
    return rows


def _ui_map() -> dict:
    if not UI_JSON.exists():
        pytest.skip("generated UI map not present")
    return json.loads(UI_JSON.read_text(encoding="utf-8"))


def _screen_controls(ui_map: dict) -> dict[str, set[str]]:
    return {
        screen["id"]: {control["name"] for control in screen["controls"]}
        for screen in ui_map["screens"]
    }


def test_matrix_rows_are_well_formed():
    seen = set()
    for row in _rows():
        assert row["Feature"] not in seen, f"duplicate feature: {row['Feature']}"
        seen.add(row["Feature"])
        assert row["Status"] in STATUSES, (row["Feature"], row["Status"])
        for column in ("Evidence", "Port / docs", "Tests"):
            assert row[column], (row["Feature"], column)


def test_matrix_links_resolve():
    for row in _rows():
        for column in ("Evidence", "Port / docs"):
            for link in LINK_RE.findall(row[column]):
                if link.startswith(("http://", "https://", "mailto:")):
                    continue
                target = link.split("#", 1)[0]
                resolved = (MATRIX.parent / target).resolve()
                assert resolved.exists(), (
                    row["Feature"], column, link, str(resolved)
                )


def test_matrix_tests_exist():
    for row in _rows():
        for token in TEST_RE.findall(row["Tests"]):
            file_part, _, function = token.partition("::")
            if "*" in file_part:
                assert list(ROOT.glob(file_part)), (
                    row["Feature"], "no files match", file_part
                )
                continue
            path = ROOT / file_part
            assert path.exists(), (row["Feature"], file_part)
            if function:
                assert re.search(
                    rf"^def {re.escape(function)}\b",
                    path.read_text(encoding="utf-8"),
                    re.M,
                ), (row["Feature"], function, file_part)


def test_matrix_screens_and_controls_exist():
    screens = _screen_controls(_ui_map())
    for row in _rows():
        screen_cell = row["Screen"].replace("`", "").strip()
        if screen_cell == "-":
            continue
        ids = [part.strip() for part in screen_cell.split(",")]
        for screen_id in ids:
            assert screen_id in screens, (row["Feature"], screen_id)
        control_cell = row["Controls"].replace("`", "").strip()
        if control_cell == "-":
            continue
        for control in (part.strip() for part in control_cell.split(",")):
            assert any(control in screens[screen_id] for screen_id in ids), (
                row["Feature"], control, ids
            )


def test_format_and_decomp_docs_are_referenced():
    referenced = set()
    for row in _rows():
        for link in LINK_RE.findall(row["Evidence"]):
            target = link.split("#", 1)[0]
            referenced.add((MATRIX.parent / target).resolve())
    missing = []
    for directory in ("docs/formats", "docs/decomp", "docs/platform"):
        for doc in (ROOT / directory).glob("*.md"):
            if doc.name in {"feature-status.md"}:
                continue
            if doc.resolve() not in referenced:
                missing.append(str(doc.relative_to(ROOT)))
    assert not missing, "docs missing from the feature matrix: " + ", ".join(
        sorted(missing)
    )


def test_roadmap_links_the_matrix_and_task_list():
    roadmap = (ROOT / "docs" / "ROADMAP.md").read_text(encoding="utf-8")
    assert "platform/feature-status.md" in roadmap
    assert "notes/OPEN-TASKS.md" in roadmap


def test_docs_index_links_the_matrix():
    index = (ROOT / "docs" / "README.md").read_text(encoding="utf-8")
    assert "platform/feature-status.md" in index
    assert "notes/OPEN-TASKS.md" in index
