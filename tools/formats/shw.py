#!/usr/bin/env python3
"""Reader for Dino Island ``.shw`` show scenarios.

The files are plain text, line-oriented, with ``// Section`` comments. All 43
retail files share the same sections:

    // Name                      &cpg1_race_1
    // Cost                      1800
    // Judge Kind                1
    // Available cage size       4 ints
    // Dimensions                2 ints (width, height)
    // Wall type                 1 int
    // Number of features        N
    // Features                  N x (4 ints)
    // Team assignments          M
    // Cage number / Team        M pairs

See ``docs/formats/shw.md``.
"""
from __future__ import annotations

import argparse
import json
from dataclasses import dataclass, field
from pathlib import Path

SECTIONS = {
    "Name",
    "Cost",
    "Judge Kind",
    "Available cage size",
    "Dimensions",
    "Wall type",
    "Number of features",
    "Features",
    "Team assignments",
    "Cage number",
    "Team",
}


class ShwFormatError(ValueError):
    pass


@dataclass
class Show:
    path: str
    name: str = ""
    cost: int = 0
    judge_kind: int = 0
    cage_size: tuple[int, int, int, int] = (0, 0, 0, 0)
    dimensions: tuple[int, int] = (0, 0)
    wall_type: int = 0
    feature_count: int = 0
    features: list[tuple[int, int, int, int]] = field(default_factory=list)
    team_count: int = 0
    teams: list[tuple[int, int]] = field(default_factory=list)

    def summary(self) -> dict:
        return {
            "path": self.path,
            "name": self.name,
            "cost": self.cost,
            "judge_kind": self.judge_kind,
            "cage_size": list(self.cage_size),
            "dimensions": list(self.dimensions),
            "wall_type": self.wall_type,
            "features": len(self.features),
            "teams": self.teams,
        }


def _ints(lines: list[str], what: str) -> list[int]:
    out = []
    for line in lines:
        for token in line.split():
            try:
                out.append(int(token))
            except ValueError as e:
                raise ShwFormatError(f"{what}: not an integer: {line!r}") from e
    return out


def read_shw(path: Path) -> Show:
    text = path.read_text(encoding="cp1252", errors="replace")
    blocks: list[tuple[str, list[str]]] = []
    current: tuple[str, list[str]] | None = None
    for raw in text.splitlines():
        line = raw.strip()
        if not line:
            continue
        if line.startswith("//"):
            name = line[2:].strip()
            if name not in SECTIONS:
                raise ShwFormatError(f"{path}: unknown section {name!r}")
            current = (name, [])
            blocks.append(current)
            continue
        if current is None:
            raise ShwFormatError(f"{path}: value before the first section")
        current[1].append(line)

    show = Show(path=str(path))
    cage_lines: list[list[int]] = []
    for name, lines in blocks:
        if name == "Name":
            show.name = lines[0] if lines else ""
        elif name == "Cost":
            show.cost = _ints(lines, "Cost")[0]
        elif name == "Judge Kind":
            show.judge_kind = _ints(lines, "Judge Kind")[0]
        elif name == "Available cage size":
            values = _ints(lines, name)
            if len(values) != 4:
                raise ShwFormatError(f"{path}: cage size has {len(values)} values")
            show.cage_size = tuple(values)  # type: ignore[assignment]
        elif name == "Dimensions":
            values = _ints(lines, name)
            if len(values) != 2:
                raise ShwFormatError(f"{path}: dimensions has {len(values)} values")
            show.dimensions = tuple(values)  # type: ignore[assignment]
        elif name == "Wall type":
            show.wall_type = _ints(lines, name)[0]
        elif name == "Number of features":
            show.feature_count = _ints(lines, name)[0]
        elif name == "Features":
            for line in lines:
                values = _ints([line], name)
                if len(values) != 4:
                    raise ShwFormatError(f"{path}: feature has {len(values)} values: {line!r}")
                show.features.append(tuple(values))  # type: ignore[arg-type]
        elif name == "Team assignments":
            show.team_count = _ints(lines, name)[0]
        elif name == "Cage number":
            cage_lines.append(_ints(lines, name))
        elif name == "Team":
            if not cage_lines:
                raise ShwFormatError(f"{path}: team before cage number")
            cage = cage_lines.pop(0)[0]
            show.teams.append((cage, _ints(lines, name)[0]))

    if len(show.features) != show.feature_count:
        raise ShwFormatError(
            f"{path}: feature count {show.feature_count} != {len(show.features)}"
        )
    if len(show.teams) != show.team_count:
        raise ShwFormatError(f"{path}: team count {show.team_count} != {len(show.teams)}")
    return show


@dataclass
class DemoShow:
    """Older editor show format from ``Data/Edition/Show``.

    ``name / width / height / judge-feature / feature count / features`` —
    no cost, cage flags, wall type or team list.
    """

    path: str
    name: str
    width: int
    height: int
    judge: int
    features: list[tuple[int, int, int, int]]

    def summary(self) -> dict:
        return {
            "path": self.path,
            "name": self.name,
            "width": self.width,
            "height": self.height,
            "judge": self.judge,
            "features": len(self.features),
        }


def read_demo_shw(path: Path) -> DemoShow:
    lines = [
        line.strip()
        for line in path.read_text(encoding="cp1252", errors="replace").splitlines()
        if line.strip()
    ]
    if len(lines) < 6:
        raise ShwFormatError(f"{path}: demo show too short")
    name = lines[0]
    width, height, judge, count = (int(value) for value in lines[1:5])
    rows = lines[5:]
    if len(rows) != count:
        raise ShwFormatError(
            f"{path}: demo feature count {count} != {len(rows)}"
        )
    features = []
    for row in rows:
        values = [int(value) for value in row.split()]
        if len(values) != 4:
            raise ShwFormatError(f"{path}: demo feature row {row!r}")
        features.append(tuple(values))
    return DemoShow(
        path=str(path),
        name=name,
        width=width,
        height=height,
        judge=judge,
        features=features,
    )


def read_feature_names(features_dir: Path) -> list[str]:
    """Return the ``FeatureList.ini`` order used by the .shw feature tuples.

    A feature tuple is ``(feature_index, rotation, x, y)`` where
    ``feature_index`` indexes this list (62 entries).
    """
    list_path = features_dir / "FeatureList.ini"
    lines = [
        line.strip()
        for line in list_path.read_text(encoding="cp1252", errors="replace").splitlines()
        if line.strip()
    ]
    if not lines:
        raise ShwFormatError(f"{list_path}: empty")
    count = int(lines[0])
    names = lines[1:]
    if len(names) != count:
        raise ShwFormatError(
            f"{list_path}: declared {count} features, found {len(names)}"
        )
    return names


def scan(root: Path) -> tuple[int, list[tuple[str, str]], dict]:
    total = 0
    bad: list[tuple[str, str]] = []
    sums = {"features": 0, "teams": 0}
    for path in sorted(root.rglob("*.shw")):
        total += 1
        try:
            show = read_shw(path)
        except Exception as e:
            bad.append((str(path), str(e)))
            continue
        sums["features"] += len(show.features)
        sums["teams"] += len(show.teams)
    return total, bad, sums


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--dump", type=Path)
    ap.add_argument("--scan", type=Path)
    args = ap.parse_args(argv)
    if args.dump:
        print(json.dumps(read_shw(args.dump).summary(), indent=2))
        return 0
    if args.scan:
        total, bad, sums = scan(args.scan)
        print(f"scanned {total} shows, {len(bad)} bad, {sums['features']} features, "
              f"{sums['teams']} team slots")
        for path, why in bad[:20]:
            print(f"  FAIL {path}: {why}")
        return 0 if not bad else 1
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
