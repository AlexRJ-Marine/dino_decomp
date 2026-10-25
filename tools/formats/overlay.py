#!/usr/bin/env python3
"""Reader for the Dino Island overlay interface INIs and graphic databases.

The overlay UI is data-driven. ``MenuInterface.Ini`` and
``ParkInterface.Ini`` describe widgets; the matching ``*GraphicDB.ini`` maps
sprite names to rectangles inside TGA atlases. Coordinates are pixels in an
800x600 top-left space, relative to the parent widget.

See ``docs/formats/overlay-ini.md`` for the format and the Ghidra evidence.
"""
from __future__ import annotations

import argparse
import json
import re
from dataclasses import dataclass, field
from pathlib import Path

SECTION_RE = re.compile(r"^\s*\[\s*([^\]]+?)\s*\]\s*$")
KEY_RE = re.compile(r"^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*?)\s*$")
INCLUDE_FILE_RE = re.compile(r"^FILE(\d+)$")

JUSTIFY = {"LEFT": 1, "RIGHT": 2, "X": 3, "UP": 4, "DOWN": 8, "Y": 12, "CENTER": 15}


class OverlayFormatError(ValueError):
    pass


def strip_comment(line: str) -> str:
    """Cut a ';' comment, ignoring semicolons inside double quotes."""
    quoted = False
    for i, ch in enumerate(line):
        if ch == '"':
            quoted = not quoted
        elif ch == ";" and not quoted:
            return line[:i]
    return line


def unquote(value: str) -> str:
    value = value.strip()
    if len(value) >= 2 and value[0] == '"' and value[-1] == '"':
        return value[1:-1]
    return value


@dataclass
class Section:
    name: str
    values: dict[str, str] = field(default_factory=dict)
    source: str = ""
    line: int = 0

    def get(self, key: str, default: str = "") -> str:
        return self.values.get(key.upper(), default)

    def has(self, key: str) -> bool:
        return key.upper() in self.values

    def as_int(self, key: str, default: int = 0) -> int:
        raw = self.get(key)
        if not raw:
            return default
        try:
            return int(float(raw))
        except ValueError:
            return default

    def as_float(self, key: str, default: float = 0.0) -> float:
        raw = self.get(key)
        if not raw:
            return default
        try:
            return float(raw)
        except ValueError:
            return default


def parse_ini(path: Path, *, expand: bool = True, _seen: set[Path] | None = None) -> list[Section]:
    """Parse an INI, optionally expanding [#INCLUDE] sections."""
    path = path.resolve()
    seen = _seen if _seen is not None else set()
    if expand and path in seen:
        raise OverlayFormatError(f"{path}: recursive include")
    if expand:
        seen.add(path)
    try:
        text = path.read_text(encoding="cp1252", errors="replace")
    except OSError as e:
        raise OverlayFormatError(f"{path}: {e}") from e

    sections: list[Section] = []
    current: Section | None = None
    for lineno, raw in enumerate(text.splitlines(), 1):
        line = strip_comment(raw)
        if not line.strip():
            continue
        m = SECTION_RE.match(line)
        if m:
            current = Section(m.group(1), source=str(path), line=lineno)
            sections.append(current)
            continue
        m = KEY_RE.match(line)
        if not m:
            continue
        if current is None:
            raise OverlayFormatError(f"{path}:{lineno}: value before a section")
        current.values[m.group(1).upper()] = unquote(m.group(2))

    if not expand:
        seen.discard(path)
        return sections

    expanded: list[Section] = []
    for section in sections:
        expanded.append(section)
        if section.name.upper() != "#INCLUDE":
            continue
        entries = []
        for key, value in section.values.items():
            m = INCLUDE_FILE_RE.match(key)
            if m:
                entries.append((int(m.group(1)), value))
        for _, value in sorted(entries):
            inc = (path.parent / value).resolve()
            expanded.extend(parse_ini(inc, _seen=seen))
    seen.discard(path)
    return expanded


@dataclass
class Graphic:
    """One [NAME] entry from a *GraphicDB.ini."""

    name: str
    filename: str
    x: int = 0
    y: int = 0
    width: int = 1
    height: int = 1
    objects: int = 1
    skip: frozenset[int] = frozenset()

    @property
    def skip_mask(self) -> int:
        mask = 0
        for frame in self.skip:
            mask |= 1 << frame
        return mask

    def used_frames(self) -> list[int]:
        return [i for i in range(self.objects) if i not in self.skip]

    def orientation(self, texture_width: int) -> str:
        """'horizontal' when the compact strip fits the atlas width."""
        count = len(self.used_frames())
        if count <= 1:
            return "horizontal"
        return "horizontal" if self.x + count * self.width <= texture_width else "vertical"

    def frame_rect(self, logical_frame: int, texture_width: int) -> tuple[int, int, int, int] | None:
        """Pixel rect for a logical frame (None when the frame is skipped)."""
        used = self.used_frames()
        if logical_frame not in used:
            return None
        slot = used.index(logical_frame)
        if self.orientation(texture_width) == "horizontal":
            return (self.x + slot * self.width, self.y, self.width, self.height)
        return (self.x, self.y + slot * self.height, self.width, self.height)

    def summary(self) -> dict:
        return {
            "name": self.name,
            "filename": self.filename,
            "rect": [self.x, self.y, self.width, self.height],
            "objects": self.objects,
            "skip": sorted(self.skip),
            "used": self.used_frames(),
        }


def load_graphic_db(path: Path) -> dict[str, Graphic]:
    graphics: dict[str, Graphic] = {}
    for section in parse_ini(path):
        if section.name.upper() == "#INCLUDE" or not section.has("FILENAME"):
            continue
        skip = set()
        for token in section.get("SKIP_LIST").replace(",", " ").split():
            try:
                skip.add(int(token))
            except ValueError:
                pass
        graphic = Graphic(
            name=section.name,
            filename=section.get("FILENAME"),
            x=section.as_int("X"),
            y=section.as_int("Y"),
            width=section.as_int("WIDTH", 1),
            height=section.as_int("HEIGHT", 1),
            objects=section.as_int("NB_OBJECTS", 1),
            skip=frozenset(skip),
        )
        graphics[graphic.name] = graphic
    return graphics


@dataclass
class Widget:
    name: str
    type: str
    props: dict[str, str]
    parent: str = ""
    source: str = ""
    order: int = 0
    children: list[str] = field(default_factory=list)
    x: int = 0
    y: int = 0
    width: int = 0
    height: int = 0
    abs_x: int = 0
    abs_y: int = 0

    def get(self, key: str, default: str = "") -> str:
        return self.props.get(key.upper(), default)

    def as_int(self, key: str, default: int = 0) -> int:
        raw = self.get(key)
        if not raw:
            return default
        try:
            return int(float(raw))
        except ValueError:
            return default

    def as_float(self, key: str, default: float = 0.0) -> float:
        raw = self.get(key)
        if not raw:
            return default
        try:
            return float(raw)
        except ValueError:
            return default

    def has(self, key: str) -> bool:
        return key.upper() in self.props

    @property
    def visible(self) -> bool:
        return self.as_int("VISIBLE", 0) != 0

    @property
    def priority(self) -> int:
        return self.as_int("PRIORITY", 0)

    @property
    def zpriority(self) -> float:
        return self.as_float("ZPRIORITY", 0.0)

    def color(self, key: str, default: tuple[int, int, int, int] = (255, 255, 255, 255)) -> tuple[int, int, int, int]:
        parts = self.get(key).split()
        if len(parts) != 4:
            return default
        try:
            return tuple(int(p) for p in parts)  # type: ignore[return-value]
        except ValueError:
            return default

    def to_dict(self) -> dict:
        return {
            "name": self.name,
            "type": self.type,
            "parent": self.parent,
            "rect": [self.x, self.y, self.width, self.height],
            "abs": [self.abs_x, self.abs_y],
            "priority": self.priority,
            "zpriority": self.zpriority,
            "visible": self.visible,
            "text": self.get("TEXT"),
            "texture": self.get("TEXTURE"),
        }


@dataclass
class Interface:
    path: str
    widgets: dict[str, Widget] = field(default_factory=dict)
    order: list[str] = field(default_factory=list)

    def roots(self) -> list[Widget]:
        return [self.widgets[n] for n in self.order if self.widgets[n].parent in ("", "ROOT")]

    def pages(self) -> list[Widget]:
        return [w for w in self.roots() if w.name.upper().endswith("_PAGE")]

    def page(self, name: str) -> Widget:
        page = self.widgets[name]
        if page.parent not in ("", "ROOT"):
            raise KeyError(f"{name} is not a page")
        return page

    def children(self, name: str) -> list[Widget]:
        return [self.widgets[n] for n in self.widgets[name].children]

    def walk(self, root: str):
        stack = [root]
        while stack:
            name = stack.pop()
            yield self.widgets[name]
            stack.extend(reversed(self.widgets[name].children))

    def layout(self) -> None:
        for root in self.roots():
            root.abs_x = root.x
            root.abs_y = root.y
            for widget in self.walk(root.name):
                parent = self.widgets.get(widget.parent)
                widget.abs_x = widget.x + (parent.abs_x if parent else 0)
                widget.abs_y = widget.y + (parent.abs_y if parent else 0)

    def apply_resize(self, new_w: int, new_h: int, old_w: int = 800, old_h: int = 600) -> None:
        """Literal moWidget::OnResize pass (FUN_005bc000)."""

        def walk(widget: Widget) -> None:
            flags = (
                (widget.as_int("ALIGN_LEFT") << 10)
                | (widget.as_int("ALIGN_UP") << 11)
                | (widget.as_int("ALIGN_RIGHT") << 12)
                | (widget.as_int("ALIGN_DOWN") << 13)
                | (widget.as_int("ALIGN_CENTER") << 14)
            )
            if flags & (1 << 14):
                widget.x = (new_w - widget.width) // 2
                widget.y = (new_h - widget.height) // 2
            else:
                if flags & (1 << 10) and flags & (1 << 12):
                    widget.width += new_w - old_w
                elif flags & (1 << 12):
                    widget.x += new_w - old_w
                if flags & (1 << 11) and flags & (1 << 13):
                    widget.height += new_h - old_h
                elif flags & (1 << 13):
                    widget.y += new_h - old_h
            for child in widget.children:
                walk(self.widgets[child])

        for root in self.roots():
            walk(root)
        self.layout()

    def scan(self) -> dict:
        return {
            "path": self.path,
            "widgets": len(self.widgets),
            "roots": len(self.roots()),
            "pages": [p.name for p in self.pages()],
            "types": _count(w.type for w in self.widgets.values()),
        }


def _count(values) -> dict[str, int]:
    out: dict[str, int] = {}
    for value in values:
        out[value] = out.get(value, 0) + 1
    return dict(sorted(out.items()))


def load_interface(path: Path) -> Interface:
    interface = Interface(path=str(path))
    for section in parse_ini(path):
        if section.name.upper() == "#INCLUDE":
            continue
        props = dict(section.values)
        widget = Widget(
            name=section.name,
            type=props.get("TYPE", ""),
            props=props,
            parent=props.get("PARENT", ""),
            source=section.source,
            order=len(interface.order),
            x=section.as_int("X"),
            y=section.as_int("Y"),
            width=section.as_int("WIDTH"),
            height=section.as_int("HEIGHT"),
        )
        if widget.name in interface.widgets:
            raise OverlayFormatError(f"{path}: duplicate section {widget.name}")
        interface.widgets[widget.name] = widget
        interface.order.append(widget.name)

    for widget in interface.widgets.values():
        if widget.parent and widget.parent != "ROOT":
            if widget.parent not in interface.widgets:
                raise OverlayFormatError(
                    f"{widget.source}: {widget.name} has missing parent {widget.parent}"
                )
            interface.widgets[widget.parent].children.append(widget.name)

    for widget in interface.widgets.values():
        chain: set[str] = set()
        current = widget
        while current.parent and current.parent != "ROOT":
            if current.name in chain:
                raise OverlayFormatError(f"{widget.source}: parent cycle at {current.name}")
            chain.add(current.name)
            current = interface.widgets[current.parent]

    interface.layout()
    return interface


def text_bits(value: str) -> int:
    bits = 0
    for token in re.split(r"[|\s]+", value.strip()):
        if token:
            bits |= JUSTIFY.get(token.upper(), 0)
    return bits


def resolve_text(value: str, locs: list) -> str:
    """Resolve a '&Field' id through .loc records; literal text is returned."""
    if not value.startswith("&"):
        return value
    for loc in locs:
        for record in loc.records:
            if record.field_id == value:
                return record.text
    return value


def scan_interface(root: Path) -> dict:
    files = {}
    for path in sorted(root.glob("*.ini")):
        try:
            sections = parse_ini(path, expand=False)
        except OverlayFormatError as e:
            files[path.name] = {"error": str(e)}
            continue
        files[path.name] = {
            "sections": len(sections),
            "includes": [
                s.name for s in sections if s.name.upper() == "#INCLUDE"
            ],
        }
    return files


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    sub = ap.add_subparsers(dest="command", required=True)

    p_dump = sub.add_parser("dump", help="print one widget subtree")
    p_dump.add_argument("ini", type=Path)
    p_dump.add_argument("--root", default="ROOT")

    p_scan = sub.add_parser("scan", help="summarise every INI in a directory")
    p_scan.add_argument("directory", type=Path)

    p_graphic = sub.add_parser("graphic", help="print one GraphicDB entry")
    p_graphic.add_argument("db", type=Path)
    p_graphic.add_argument("name")

    args = ap.parse_args(argv)
    if args.command == "dump":
        interface = load_interface(args.ini)
        if args.root == "ROOT":
            roots = interface.roots()
            print(json.dumps([r.to_dict() for r in roots], indent=2))
            return 0
        if args.root not in interface.widgets:
            print(f"{args.root}: not found")
            return 1
        print(json.dumps([w.to_dict() for w in interface.walk(args.root)], indent=2))
        return 0
    if args.command == "scan":
        print(json.dumps(scan_interface(args.directory), indent=2))
        return 0
    if args.command == "graphic":
        graphics = load_graphic_db(args.db)
        graphic = graphics.get(args.name)
        if graphic is None:
            print(f"{args.name}: not found")
            return 1
        print(json.dumps(graphic.summary(), indent=2))
        return 0
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
