#!/usr/bin/env python3
"""Reader for Dino Island ``MenuSoundDB.ini`` / ``ParkSoundDB.ini``.

The sound database is a plain INI keyed by sound name.  The file header
documents the keys; the engine is ``msSoundDatabase`` (``0x005b7d00``) and the
DirectSound wrapper is loaded dynamically from ``dsound.dll``.

Layout::

    ; comments
    [section]                 # sound name, used by the game code
    FILENAME      = "path"    # relative to the game root
    USER_PARAM    = n
    PRIORITY      = 0..100
    CATEGORY      = n
    USE_VOLUME    = 0/1
    USE_FREQUENCY = 0/1
    USE_STEREO    = 0/1
    USE_ALL_FX    = 0/1
    USE_STREAMING = 0/1
    USE_3D        = 0/1
    LOOP          = 0/1
    KEEP_IN_MEMORY= 0/1
    LOCK_HARD     = 0/1       # documented but unused
    LOCK_SOFT     = 0/1       # documented but unused
    START_DELAY   = n
    3D_MIN_DISTANCE = f
    3D_MAX_DISTANCE = f

``[#INCLUDE]`` / ``#INCLUDE`` sections are supported recursively.  The retail
files do not use includes.

Music selection is separate: the island ``.lnd`` stores a music name such as
``musEquateur``; that name is a section in ``ParkSoundDB.ini``.  All five
retail music sections use ``USE_STREAMING=1`` and ``LOOP=1``.

See ``docs/formats/sounddb.md`` and ``notes/media-recon.md``.
"""
from __future__ import annotations

import argparse
import json
from dataclasses import dataclass, field
from pathlib import Path


class SoundDbError(ValueError):
    pass


BOOL_KEYS = {
    "USE_VOLUME",
    "USE_FREQUENCY",
    "USE_STEREO",
    "USE_ALL_FX",
    "USE_STREAMING",
    "USE_3D",
    "LOOP",
    "KEEP_IN_MEMORY",
    "LOCK_HARD",
    "LOCK_SOFT",
}
INT_KEYS = {"USER_PARAM", "PRIORITY", "CATEGORY", "START_DELAY"}
FLOAT_KEYS = {"3D_MIN_DISTANCE", "3D_MAX_DISTANCE"}
KNOWN_KEYS = BOOL_KEYS | INT_KEYS | FLOAT_KEYS | {"FILENAME"}


@dataclass
class SoundEntry:
    name: str
    filename: str = ""
    user_param: int = 0
    priority: int = 0
    category: int = -1
    use_volume: bool = False
    use_frequency: bool = False
    use_stereo: bool = False
    use_all_fx: bool = False
    use_streaming: bool = False
    use_3d: bool = False
    loop: bool = False
    keep_in_memory: bool = False
    lock_hard: bool = False
    lock_soft: bool = False
    start_delay: int = 0
    min_distance: float = 1.0
    max_distance: float = 1_000_000_000.0
    raw: dict[str, str] = field(default_factory=dict)

    def to_dict(self) -> dict:
        return {
            "name": self.name,
            "filename": self.filename,
            "priority": self.priority,
            "category": self.category,
            "use_volume": self.use_volume,
            "use_frequency": self.use_frequency,
            "use_stereo": self.use_stereo,
            "use_all_fx": self.use_all_fx,
            "use_streaming": self.use_streaming,
            "use_3d": self.use_3d,
            "loop": self.loop,
            "keep_in_memory": self.keep_in_memory,
            "start_delay": self.start_delay,
            "min_distance": self.min_distance,
            "max_distance": self.max_distance,
        }


@dataclass
class SoundDb:
    path: str
    root: Path
    entries: list[SoundEntry]
    includes: list[str] = field(default_factory=list)

    def by_name(self, name: str) -> SoundEntry | None:
        lowered = name.lower()
        for entry in self.entries:
            if entry.name.lower() == lowered:
                return entry
        return None

    @property
    def music(self) -> list[SoundEntry]:
        return [e for e in self.entries if e.category == 1 and e.use_streaming]

    def summary(self) -> dict:
        missing = [
            e for e in self.entries if e.filename and not (self.root / e.filename).exists()
        ]
        return {
            "path": self.path,
            "entries": len(self.entries),
            "music": [e.name for e in self.music],
            "streaming": sum(1 for e in self.entries if e.use_streaming),
            "looping": sum(1 for e in self.entries if e.loop),
            "missing_files": len(missing),
        }


def _unquote(value: str) -> str:
    value = value.strip()
    if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
        return value[1:-1]
    return value


def _parse_bool(value: str, key: str, section: str) -> bool:
    value = value.strip()
    if value in ("0", "1"):
        return value == "1"
    raise SoundDbError(f"{section}: {key} expects 0/1, got {value!r}")


def _read_ini(path: Path, root: Path, seen: set[Path]) -> list[SoundEntry]:
    path = path.resolve()
    if path in seen:
        raise SoundDbError(f"include cycle at {path}")
    seen.add(path)
    try:
        text = path.read_text(encoding="latin-1")
    except OSError as exc:
        raise SoundDbError(f"cannot read {path}: {exc}") from exc

    entries: list[SoundEntry] = []
    current: SoundEntry | None = None
    for lineno, raw in enumerate(text.splitlines(), 1):
        line = raw.split(";", 1)[0].strip()
        if not line:
            continue
        if line.startswith("["):
            if not line.endswith("]"):
                raise SoundDbError(f"{path}:{lineno}: malformed section {line!r}")
            name = line[1:-1].strip()
            if not name:
                raise SoundDbError(f"{path}:{lineno}: empty section")
            current = SoundEntry(name=name)
            entries.append(current)
            continue
        if line.upper().startswith("#INCLUDE"):
            target = line[8:].lstrip("= \t")
            target = _unquote(target)
            if not target:
                raise SoundDbError(f"{path}:{lineno}: empty include")
            inc = root / target
            if not inc.exists():
                inc = path.parent / target
            entries.extend(_read_ini(inc, root, seen))
            current = None
            continue
        if "=" not in line:
            raise SoundDbError(f"{path}:{lineno}: expected key=value, got {line!r}")
        key, value = (part.strip() for part in line.split("=", 1))
        if current is None:
            raise SoundDbError(f"{path}:{lineno}: key {key} outside a section")
        key = key.upper()
        if key not in KNOWN_KEYS:
            # The retail files have no unknown keys; keep the raw value so a
            # future extension does not make the whole DB unreadable.
            current.raw[key] = value
            continue
        if key == "FILENAME":
            current.filename = _unquote(value).replace("\\", "/")
            continue
        current.raw[key] = value
        if key in BOOL_KEYS:
            setattr(current, key.lower(), _parse_bool(value, key, current.name))
        elif key in INT_KEYS:
            try:
                setattr(current, key.lower(), int(value, 0))
            except ValueError as exc:
                raise SoundDbError(f"{current.name}: {key} expects int, got {value!r}") from exc
        elif key in FLOAT_KEYS:
            try:
                setattr(current, key.lower(), float(value))
            except ValueError as exc:
                raise SoundDbError(f"{current.name}: {key} expects float, got {value!r}") from exc
    return entries


def read_sounddb(path: Path, root: Path | None = None) -> SoundDb:
    if root is None:
        # .../%MAINDIR%/Data/Sound/file.ini -> .../%MAINDIR%
        root = path.resolve().parents[2]
    entries = _read_ini(path, root, set())
    return SoundDb(str(path), root, entries)


def scan(directory: Path, root: Path | None = None) -> tuple[list[SoundDb], list[tuple[str, str]]]:
    good: list[SoundDb] = []
    bad: list[tuple[str, str]] = []
    for path in sorted(directory.rglob("*SoundDB.ini")):
        try:
            good.append(read_sounddb(path, root))
        except Exception as exc:
            bad.append((str(path), str(exc)))
    return good, bad


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--dump", type=Path)
    ap.add_argument("--scan", type=Path)
    ap.add_argument("--root", type=Path, help="game root used to resolve FILENAME")
    ap.add_argument("--missing", action="store_true", help="list missing WAV files")
    args = ap.parse_args(argv)
    if args.dump:
        db = read_sounddb(args.dump, args.root)
        print(json.dumps([e.to_dict() for e in db.entries], indent=2))
        if args.missing:
            for e in db.entries:
                target = db.root / e.filename
                if e.filename and not target.exists():
                    print(f"MISSING {e.name}: {e.filename}")
        return 0
    if args.scan:
        good, bad = scan(args.scan, args.root)
        print(f"scanned {len(good) + len(bad)} sound DBs, {len(bad)} bad")
        for db in good:
            s = db.summary()
            print(
                f"{db.path}: {s['entries']} entries, {s['streaming']} streaming, "
                f"{s['looping']} looping, music {s['music']}"
            )
        for path, why in bad:
            print(f"FAIL {path}: {why}")
        return 0 if not bad else 1
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
