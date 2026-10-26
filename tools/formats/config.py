#!/usr/bin/env python3
"""Readers for the ``Data/Config`` startup/input/camera files.

- ``KeyMapper.ini`` — game command -> event -> key name -> modifier rows
  (``CameraRotateLeft KeyRepeat FlècheGauche +Shift``).
- ``key/KeyFrancais.ini`` — the DIK/MIK key table (144 rows, French AZERTY
  names and shift/AltGr ASCII).
- ``camera.ini`` — five identical ``[Config1..5]`` blocks of camera motion
  limits (move/progress/rotation speed, elevation, distance, hardness).
- ``Start.Ini`` — engine startup (window, 3D device, key table path).
- ``Dino.ini`` — user settings (video/sound/language).

See ``docs/formats/config.md``.
"""
from __future__ import annotations

import argparse
import json
import math
from dataclasses import dataclass, field
from pathlib import Path

from overlay import parse_ini

KEY_COLUMNS = (
    "value",
    "mik",
    "dik",
    "name",
    "pad",
    "ctrl",
    "ascii",
    "ascii_plain",
    "ascii_shift",
    "ascii_altgr",
)


class ConfigError(ValueError):
    pass


@dataclass
class KeyBinding:
    command: str
    event: str
    key: str
    modifier: str

    def summary(self) -> dict:
        return {
            "command": self.command,
            "event": self.event,
            "key": self.key,
            "modifier": self.modifier,
        }


@dataclass
class KeyRow:
    value: int
    mik: str
    dik: str
    name: str
    pad: str
    ctrl: str
    ascii: str
    ascii_plain: str
    ascii_shift: str
    ascii_altgr: str

    def summary(self) -> dict:
        return {
            "value": self.value,
            "mik": self.mik,
            "dik": self.dik,
            "name": self.name,
            "pad": self.pad,
            "ctrl": self.ctrl,
            "ascii": self.ascii,
            "ascii_plain": self.ascii_plain,
            "ascii_shift": self.ascii_shift,
            "ascii_altgr": self.ascii_altgr,
        }


@dataclass
class CameraConfig:
    name: str
    values: dict[str, float]


@dataclass
class ConfigBundle:
    path: str
    bindings: list[KeyBinding] = field(default_factory=list)
    keys: list[KeyRow] = field(default_factory=list)
    cameras: list[CameraConfig] = field(default_factory=list)
    start: dict[str, str] = field(default_factory=dict)
    dino: dict[str, dict[str, str]] = field(default_factory=dict)

    def summary(self) -> dict:
        return {
            "path": self.path,
            "bindings": len(self.bindings),
            "keys": len(self.keys),
            "cameras": [camera.name for camera in self.cameras],
            "start": self.start,
            "dino": self.dino,
        }


def read_keymapper(path: Path) -> list[KeyBinding]:
    rows: list[KeyBinding] = []
    for lineno, raw in enumerate(
        path.read_text(encoding="cp1252", errors="replace").splitlines(), 1
    ):
        line = raw.split("//", 1)[0].strip()
        if not line:
            continue
        parts = line.split()
        if len(parts) < 3:
            raise ConfigError(f"{path}:{lineno}: short binding {line!r}")
        command, event, key = parts[:3]
        modifier = parts[3] if len(parts) > 3 else ""
        rows.append(
            KeyBinding(command=command, event=event, key=key, modifier=modifier)
        )
    return rows


def read_keytable(path: Path) -> list[KeyRow]:
    rows: list[KeyRow] = []
    for lineno, raw in enumerate(
        path.read_text(encoding="cp1252", errors="replace").splitlines(), 1
    ):
        if not raw.strip() or raw.lstrip().startswith("//"):
            continue
        cells = [cell.strip() for cell in raw.split("\t")]
        if len(cells) < 7:
            raise ConfigError(f"{path}:{lineno}: {len(cells)} columns")
        cells += [""] * (len(KEY_COLUMNS) - len(cells))
        try:
            value = int(cells[0], 16)
        except ValueError as error:
            raise ConfigError(f"{path}:{lineno}: bad value {cells[0]!r}") from error
        rows.append(
            KeyRow(
                value=value,
                mik=cells[1],
                dik=cells[2],
                name=cells[3],
                pad=cells[4],
                ctrl=cells[5],
                ascii=cells[6],
                ascii_plain=cells[7],
                ascii_shift=cells[8],
                ascii_altgr=cells[9],
            )
        )
    return rows


def read_camera(path: Path) -> list[CameraConfig]:
    out: list[CameraConfig] = []
    for section in parse_ini(path):
        values = {key: float(value) for key, value in section.values.items()}
        out.append(CameraConfig(name=section.name, values=values))
    return out


def read_start_ini(path: Path) -> dict[str, str]:
    out: dict[str, str] = {}
    for raw in path.read_text(encoding="cp1252", errors="replace").splitlines():
        line = raw.split("//", 1)[0].strip()
        if not line:
            continue
        parts = line.split(None, 1)
        key = parts[0]
        value = parts[1].strip() if len(parts) > 1 else ""
        out[key] = value
    return out


def read_dino_ini(path: Path) -> dict[str, dict[str, str]]:
    return {
        section.name: dict(section.values) for section in parse_ini(path)
    }


def read_config_bundle(directory: Path) -> ConfigBundle:
    bundle = ConfigBundle(path=str(directory))
    mapper = directory / "KeyMapper.ini"
    if mapper.exists():
        bundle.bindings = read_keymapper(mapper)
    table = directory / "key" / "KeyFrancais.ini"
    if table.exists():
        bundle.keys = read_keytable(table)
    camera = directory / "camera.ini"
    if camera.exists():
        bundle.cameras = read_camera(camera)
    start = directory / "Start.Ini"
    if start.exists():
        bundle.start = read_start_ini(start)
    dino = directory / "Dino.ini"
    if dino.exists():
        bundle.dino = read_dino_ini(dino)
    return bundle


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("path", type=Path, help="a file or the Config directory")
    args = ap.parse_args(argv)
    if args.path.is_dir():
        print(json.dumps(read_config_bundle(args.path).summary(), indent=2))
    elif args.path.name.lower() == "keymapper.ini":
        print(json.dumps([row.summary() for row in read_keymapper(args.path)], indent=2))
    elif args.path.name.lower() == "camera.ini":
        print(json.dumps([camera.__dict__ for camera in read_camera(args.path)], indent=2))
    else:
        print(json.dumps(read_keytable(args.path), default=lambda o: o.__dict__, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
