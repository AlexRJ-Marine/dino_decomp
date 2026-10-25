#!/usr/bin/env python3
"""Render an overlay page from retail data (reference implementation).

This is the Python oracle for the C++ ``src/ui/`` layer: it parses the same
INI/GraphicDB data and composes the page with the retail TGA atlases, the
``.fnt`` fonts and the ``.loc`` strings. No layout is hard-coded here.

Examples::

    python tools/viewer/overlay_preview.py --page MAIN_MENU_PAGE \
      --out notes/menu-preview.png
    python tools/viewer/overlay_preview.py --page FREE_GAME_PAGE \
      --island 1 --difficulty easy --out notes/menu-freegame-preview.png
"""
from __future__ import annotations

import argparse
import copy
import sys
from dataclasses import dataclass, field
from pathlib import Path

from PIL import Image

FORMATS = Path(__file__).resolve().parents[1] / "formats"
if str(FORMATS) not in sys.path:
    sys.path.insert(0, str(FORMATS))

from fnt import Fnt, read_fnt  # noqa: E402
from loc import Loc, read_loc  # noqa: E402
from overlay import (  # noqa: E402
    Graphic,
    Interface,
    Widget,
    load_graphic_db,
    load_interface,
    resolve_text,
    text_bits,
)
from tga import read_tga  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_GAME = ROOT / "game" / "%MAINDIR%"


class Assets:
    def __init__(self, game: Path) -> None:
        self.game = game
        self._images: dict[str, Image.Image] = {}
        self._fonts: dict[str, Fnt] = {}

    def path(self, name: str) -> Path:
        return self.game / name.replace("./", "")

    def image(self, name: str) -> Image.Image:
        if name not in self._images:
            tga = read_tga(self.path(name))
            self._images[name] = Image.frombytes("RGBA", (tga.width, tga.height), tga.pixels)
        return self._images[name]

    def font(self, name: str) -> Fnt:
        key = name.lower()
        if key not in self._fonts:
            self._fonts[key] = read_fnt(self.path(name))
        return self._fonts[key]


@dataclass
class RenderOptions:
    items: dict[str, list[str]] = field(default_factory=dict)
    states: dict[str, str] = field(default_factory=dict)
    force_visible: set[str] = field(default_factory=set)
    runtime_text: dict[str, str] = field(default_factory=dict)
    selected_row: dict[str, int] = field(default_factory=dict)
    normal_frames: dict[str, int] = field(default_factory=dict)


STATE_FRAMES = {
    "disabled": 0,
    "normal": 4,
    "hover": 5,
    "pressed": 6,
    "selected": 7,
}


def state_frame(
    graphic: Graphic,
    image: Image.Image,
    state: str,
    normal_frame: int | None = None,
) -> tuple[int, int, int, int] | None:
    """Frame for a widget state.

    Frame 4 is normal, 0 disabled, 5 hover, 6 pressed, 7 selected. Frame 4
    may be transparent (menu/list bars), in which case the page art shows
    through. The skip mask removes animation frames, not states.
    """
    frames = graphic.used_frames()
    if not frames:
        return None
    frame = STATE_FRAMES.get(state, 4)
    if state == "normal" and normal_frame is not None:
        frame = normal_frame
    if frame not in frames:
        if state == "selected":
            frame = frames[-1]
        elif state == "disabled":
            frame = frames[0]
        else:
            frame = 4 if 4 in frames else frames[0]
    return graphic.frame_rect(frame, image.width)


def blend_text(
    canvas: Image.Image,
    font: Fnt,
    text: str,
    x: float,
    y: float,
    scale: float,
    color: tuple[int, int, int, int],
) -> None:
    pen_x = x
    for ch in text:
        glyph = font.glyph(ch)
        if glyph is None:
            pen_x += max(1, font.font_size // 2) * scale
            continue
        tile = Image.new("RGBA", (glyph.width, glyph.height), (0, 0, 0, 0))
        tile_px = tile.load()
        for yy in range(glyph.height):
            for xx in range(glyph.width):
                alpha = font.alpha_at(glyph.x0 + xx, glyph.y0 + yy)
                if alpha:
                    tile_px[xx, yy] = (color[0], color[1], color[2], alpha)
        if scale != 1.0:
            tile = tile.resize(
                (max(1, int(round(glyph.width * scale))), max(1, int(round(glyph.height * scale)))),
                Image.NEAREST,
            )
        canvas.alpha_composite(tile, (int(round(pen_x)), int(round(y))))
        pen_x += glyph.width * scale


def draw_text_block(
    canvas: Image.Image,
    text: str,
    font: Fnt,
    height: int,
    bits: int,
    color: tuple[int, int, int, int],
    rect: tuple[int, int, int, int],
) -> None:
    scale = height / float(font.font_size) if font.font_size else 1.0
    lines = text.split("\n")
    line_step = font.line_height * scale
    widths = [
        sum((font.glyph(ch).width if font.glyph(ch) else max(1, font.font_size // 2)) for ch in line)
        * scale
        for line in lines
    ]
    total_h = line_step * len(lines)
    x0, y0, w, h = rect
    horizontal = bits & 3
    vertical = bits & 12
    for index, line in enumerate(lines):
        width = widths[index]
        if horizontal == 1:
            x = x0
        elif horizontal == 2:
            x = x0 + w - width
        else:
            x = x0 + (w - width) / 2.0
        if vertical == 4:
            y = y0 + index * line_step
        elif vertical == 8:
            y = y0 + h - total_h + index * line_step
        else:
            y = y0 + (h - total_h) / 2.0 + index * line_step
        blend_text(canvas, font, line, x, y, scale, color)


def draw_text(
    canvas: Image.Image,
    widget: Widget,
    assets: Assets,
    locs: list[Loc],
    options: RenderOptions,
) -> None:
    raw = widget.get("TEXT")
    text = options.runtime_text.get(widget.name)
    if text is None:
        if not raw:
            return
        text = options.runtime_text.get(raw, resolve_text(raw, locs))
    if widget.width <= 0 or widget.height <= 0:
        return
    text = text.replace("\\n", "\n")
    font_name = widget.get("TEXT_FONT")
    if not font_name:
        return
    font = assets.font(font_name)
    draw_text_block(
        canvas,
        text,
        font,
        widget.as_int("TEXT_HEIGHT", font.font_size),
        text_bits(widget.get("TEXT_JUSTIFY")),
        widget.color("TEXT_COLOR"),
        (widget.abs_x, widget.abs_y, widget.width, widget.height),
    )


def draw_sprite(
    canvas: Image.Image,
    widget: Widget,
    assets: Assets,
    graphics: dict[str, Graphic],
    options: RenderOptions,
) -> None:
    name = widget.get("TEXTURE")
    if not name or name not in graphics:
        return
    if widget.width <= 0 or widget.height <= 0:
        return
    graphic = graphics[name]
    image = assets.image(graphic.filename)
    rect = state_frame(graphic, image, options.states.get(widget.name, "normal"),
                       options.normal_frames.get(name))
    if rect is None:
        return
    sprite = image.crop((rect[0], rect[1], rect[0] + rect[2], rect[1] + rect[3]))
    if sprite.size != (widget.width, widget.height):
        sprite = sprite.resize((widget.width, widget.height), Image.NEAREST)
    if widget.get("TEXTURE_MOSAIC") == "1":
        for y in range(widget.abs_y, widget.abs_y + widget.height, sprite.height):
            for x in range(widget.abs_x, widget.abs_x + widget.width, sprite.width):
                canvas.alpha_composite(sprite, (x, y))
    else:
        canvas.alpha_composite(sprite, (widget.abs_x, widget.abs_y))


def draw_list_scrollbar(
    canvas: Image.Image,
    widget: Widget,
    interface: Interface,
    graphics: dict[str, Graphic],
    assets: Assets,
    options: RenderOptions,
    row_width: int,
) -> None:
    scroll = next((interface.widgets[n] for n in widget.children if interface.widgets[n].type == "VSCROLL"), None)
    if scroll is None:
        return
    width = widget.as_int("LB_VSCROLL_WIDTH", 0)
    if width <= 0:
        return
    x = widget.abs_x + row_width
    y = widget.abs_y
    height = widget.height

    track = graphics.get("CAMPAIGN_TEXT_LIFT")
    if track:
        image = assets.image(track.filename)
        sprite = image.crop((track.x, track.y, track.x + track.width, track.y + track.height))
        canvas.alpha_composite(sprite.resize((width, height), Image.NEAREST), (x, y))

    for name, top in (
        ("CAMPAIGN_TEXT_SCROLLUP", y),
        ("CAMPAIGN_TEXT_SCROLLDOWN", y + height - 22),
    ):
        graphic = graphics.get(name)
        if not graphic:
            continue
        image = assets.image(graphic.filename)
        rect = state_frame(graphic, image, "normal", options.normal_frames.get(name, 4))
        if rect is None:
            continue
        sprite = image.crop((rect[0], rect[1], rect[0] + rect[2], rect[1] + rect[3]))
        canvas.alpha_composite(sprite.resize((width, 22), Image.NEAREST), (x, top))

    thumb = graphics.get("CAMPAIGN_TEXT_THUMB")
    if thumb:
        image = assets.image(thumb.filename)
        rect = state_frame(thumb, image, "normal", options.normal_frames.get("CAMPAIGN_TEXT_THUMB", 4))
        if rect is not None:
            sprite = image.crop((rect[0], rect[1], rect[0] + rect[2], rect[1] + rect[3]))
            items = len(options.items.get(widget.name, []))
            selected = options.selected_row.get(widget.name, 0)
            span = max(1, items - 1)
            free = max(0, height - 44 - 32)
            top = y + 22 + int(free * selected / span)
            canvas.alpha_composite(sprite.resize((width, 32), Image.NEAREST), (x, top))


def draw_list(
    canvas: Image.Image,
    widget: Widget,
    interface: Interface,
    graphics: dict[str, Graphic],
    assets: Assets,
    locs: list[Loc],
    options: RenderOptions,
) -> None:
    items = options.items.get(widget.name, [])
    font_name = widget.get("TEXT_FONT")
    if not font_name:
        return
    font = assets.font(font_name)
    line_height = widget.as_int("LB_LINE_HEIGHT", font.line_height)
    scroll_width = widget.as_int("LB_VSCROLL_WIDTH", 0)
    row_width = widget.as_int("LB_COLUMN0_WIDTH", widget.width - scroll_width)
    back = widget.color("BACK_COLOR", (0, 0, 0, 0))
    if back[3]:
        overlay = Image.new("RGBA", (widget.width, widget.height), back)
        canvas.alpha_composite(overlay, (widget.abs_x, widget.abs_y))

    row_graphic = graphics.get("ISLAND_LIST_BUTTON")
    row_image = assets.image(row_graphic.filename) if row_graphic else None
    selected = options.selected_row.get(widget.name, 0)
    visible_rows = max(0, widget.height // line_height)
    for index, item in enumerate(items[:visible_rows]):
        row_y = widget.abs_y + index * line_height
        row_rect = (widget.abs_x, row_y, row_width, line_height)
        if row_graphic and row_image:
            state = "selected" if index == selected else "normal"
            rect = state_frame(row_graphic, row_image, state,
                               options.normal_frames.get("ISLAND_LIST_BUTTON"))
            if rect is not None:
                sprite = row_image.crop((rect[0], rect[1], rect[0] + rect[2], rect[1] + rect[3]))
                if sprite.size != (row_width, line_height):
                    sprite = sprite.resize((row_width, line_height), Image.NEAREST)
                canvas.alpha_composite(sprite, (row_rect[0], row_rect[1]))
        draw_text_block(
            canvas,
            item,
            font,
            widget.as_int("TEXT_HEIGHT", font.font_size),
            text_bits(widget.get("TEXT_JUSTIFY")) or 15,
            widget.color("TEXT_COLOR"),
            row_rect,
        )
    draw_list_scrollbar(canvas, widget, interface, graphics, assets, options, row_width)


def draw_widget(
    canvas: Image.Image,
    widget: Widget,
    interface: Interface,
    graphics: dict[str, Graphic],
    assets: Assets,
    locs: list[Loc],
    options: RenderOptions,
    *,
    force_visible: bool = False,
) -> None:
    if not (widget.visible or force_visible or widget.name in options.force_visible):
        return
    if widget.type == "LIST":
        draw_list(canvas, widget, interface, graphics, assets, locs, options)
    else:
        draw_sprite(canvas, widget, assets, graphics, options)
        draw_text(canvas, widget, assets, locs, options)
    children = sorted(
        (interface.widgets[n] for n in widget.children),
        key=lambda w: (w.priority, w.order),
    )
    for child in children:
        draw_widget(canvas, child, interface, graphics, assets, locs, options)


def render_page(
    interface: Interface,
    graphics: dict[str, Graphic],
    assets: Assets,
    locs: list[Loc],
    page_name: str,
    size: tuple[int, int] = (800, 600),
    options: RenderOptions | None = None,
) -> Image.Image:
    options = options or RenderOptions()
    page = interface.page(page_name)
    canvas = Image.new("RGBA", size, (0, 0, 0, 255))

    background = "BACKGROUND_" + page_name.removesuffix("_PAGE")
    if background in graphics:
        graphic = graphics[background]
        image = assets.image(graphic.filename)
        sprite = image.crop((graphic.x, graphic.y, graphic.x + graphic.width, graphic.y + graphic.height))
        if sprite.size != size:
            sprite = sprite.resize(size, Image.NEAREST)
        canvas.alpha_composite(sprite, (0, 0))

    children = sorted(
        (interface.widgets[n] for n in page.children),
        key=lambda w: (w.priority, w.order),
    )
    for child in children:
        draw_widget(canvas, child, interface, graphics, assets, locs, options)
    return canvas


def render_hud(
    interface: Interface,
    graphics: dict[str, Graphic],
    assets: Assets,
    locs: list[Loc],
    size: tuple[int, int] = (800, 600),
    options: RenderOptions | None = None,
    background: Image.Image | None = None,
) -> Image.Image:
    options = options or RenderOptions()
    if background is None:
        canvas = Image.new("RGBA", size, (28, 48, 32, 255))
    else:
        canvas = background.convert("RGBA")
        if canvas.size != size:
            canvas = canvas.resize(size, Image.LANCZOS)

    roots = sorted(
        (w for w in interface.roots() if (w.visible or w.name in options.force_visible) and w.type != "RENDER"),
        key=lambda w: (w.priority, w.order),
    )
    for root in roots:
        draw_widget(canvas, root, interface, graphics, assets, locs, options)
    return canvas


def hud_options() -> RenderOptions:
    return RenderOptions(
        runtime_text={
            "BUDGET_AMOUNT": "75050",
            "VISITOR_AMOUNT": "1",
            "DINO_AMOUNT": "0",
            "SHOW_AMOUNT": "0",
            "PADDOCK_AMOUNT": "0",
            "PARK_CURRENT_TIME": "09:44",
            "PARK_CURRENT_DAY": "1",
        },
    )


def free_game_options(interface: Interface, graphics: dict[str, Graphic], locs: list[Loc], island: int,
                      difficulty: str) -> RenderOptions:
    items = [resolve_text(f"&Isl{i:02d}_Name", locs) for i in range(1, 11)]
    options = RenderOptions(items={"ISLAND_LIST": items}, selected_row={"ISLAND_LIST": island - 1})
    for index in range(1, 11):
        options.force_visible.add(f"FREE_GAME_ISLAND_PIC_isl{index:02d}")
    # Only the selected island picture is visible.
    selected_pic = f"FREE_GAME_ISLAND_PIC_isl{island:02d}"
    options.force_visible = {name for name in options.force_visible if name == selected_pic}
    options.states = {
        "FREE_EASY_BUTTON": "selected" if difficulty == "easy" else "normal",
        "FREE_MIDDLE_BUTTON": "selected" if difficulty == "middle" else "normal",
        "FREE_HARD_BUTTON": "selected" if difficulty == "hard" else "normal",
    }
    return options


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--game", type=Path, default=DEFAULT_GAME)
    ap.add_argument("--page", default="MAIN_MENU_PAGE")
    ap.add_argument("--out", type=Path, default=Path("notes/menu-preview.png"))
    ap.add_argument("--width", type=int, default=800)
    ap.add_argument("--height", type=int, default=600)
    ap.add_argument("--island", type=int, default=1)
    ap.add_argument("--difficulty", choices=("easy", "middle", "hard"), default="easy")
    ap.add_argument("--hud", action="store_true", help="render ParkInterface HUD instead of a menu page")
    ap.add_argument("--background", type=Path, help="optional background image for the HUD")
    ap.add_argument("--resize-anchors", action="store_true",
                    help="apply moWidget::OnResize to the HUD for the requested size")
    args = ap.parse_args(argv)

    overlay = args.game / "Data" / "Overlay"
    locs = [read_loc(overlay / "MenuEng.loc"), read_loc(overlay / "ParkEng.loc")]
    assets = Assets(args.game)

    if args.hud:
        interface = load_interface(overlay / "ParkInterface.Ini")
        graphics = load_graphic_db(overlay / "ParkGraphicDB.ini")
        if args.resize_anchors:
            interface.apply_resize(args.width, args.height)
        background = None
        if args.background:
            background = Image.open(args.background)
        image = render_hud(
            interface, graphics, assets, locs, (args.width, args.height),
            hud_options(), background,
        )
    else:
        interface = load_interface(overlay / "MenuInterface.Ini")
        graphics = load_graphic_db(overlay / "MenuGraphicDB.ini")
        options = RenderOptions()
        if args.page == "FREE_GAME_PAGE":
            options = free_game_options(interface, graphics, locs, args.island, args.difficulty)
        image = render_page(interface, graphics, assets, locs, args.page, (args.width, args.height), options)

    args.out.parent.mkdir(parents=True, exist_ok=True)
    image.convert("RGB").save(args.out)
    print(f"{'HUD' if args.hud else args.page}: {args.out} ({image.width}x{image.height})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
