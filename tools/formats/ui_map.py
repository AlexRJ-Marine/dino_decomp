#!/usr/bin/env python3
"""Build the retail Dino Island UI click map.

The map is derived from the shipped overlay INIs (``MenuInterface.Ini``,
``ParkInterface.Ini`` and its includes, ``RadialMenu.Ini``), not from
screenshots. Coordinates are client pixels in the 800x600 design space with a
top-left origin, exactly like the overlay format.

Usage::

    python tools/formats/ui_map.py build
    python tools/formats/ui_map.py find OPEN_LABORATORY
    python tools/formats/ui_map.py find PORT1 --screen park-paddock-list

``build`` writes ``docs/platform/ui-coordinates.json`` and
``docs/platform/ui-coordinates.md``. The JSON is the machine-readable source;
the Markdown is a generated view of it for humans and test plans.
"""
from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from loc import Loc, read_loc
from overlay import Interface, Widget, load_interface, resolve_text

DESIGN_WIDTH = 800
DESIGN_HEIGHT = 600
SCHEMA_VERSION = 1

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_DATA_ROOT = ROOT / "game" / "%MAINDIR%"
DEFAULT_JSON = ROOT / "docs" / "platform" / "ui-coordinates.json"
DEFAULT_MARKDOWN = ROOT / "docs" / "platform" / "ui-coordinates.md"

MENU_INI = "Data/Overlay/MenuInterface.Ini"
PARK_INI = "Data/Overlay/ParkInterface.Ini"
RADIAL_INI = "Data/Overlay/RadialMenu.Ini"
MENU_LOC = "Data/Overlay/MenuEng.loc"
PARK_LOC = "Data/Overlay/ParkEng.loc"

#: Widget types that receive mouse input in the retail overlay layer.
INTERACTIVE_TYPES = frozenset({"BUTTON", "LIST", "EDIT", "HSCROLL", "VSCROLL", "COMBO"})

#: Scrollbar sub-widgets are created at 0x0 and positioned by the container.
#: They are omitted from the map; click the scroll track or use the wheel.
ZERO_SIZE = 0


@dataclass
class ScreenDef:
    id: str
    title: str
    kind: str
    source: str
    roots: list[str]
    open_path: list[str] = field(default_factory=list)
    note: str = ""
    default_open: bool = False


SCREENS: list[ScreenDef] = [
    # Main-menu pages, in MenuInterface.Ini order.
    ScreenDef(
        "menu-main", "Main menu", "menu", "menu", ["MAIN_MENU_PAGE"],
        note="Boot screen. The seven main buttons; QUIT is at the bottom right.",
    ),
    ScreenDef(
        "menu-free-game", "Free Game: island and difficulty", "menu", "menu",
        ["FREE_GAME_PAGE"], ["FREE_GAME_BUTTON"],
        note="Ten islands in an eight-row scrollable list, three difficulty "
             "buttons, Start and Cancel. A row must be selected before Start.",
    ),
    ScreenDef(
        "menu-campaign-select", "Campaign select", "menu", "menu",
        ["CAMPAIGN_SELECT_PAGE"], ["CAMPAIGN_BUTTON"],
        note="Eight campaign rows. Rows 1-4 are unlocked by `save/dino.dat`; "
             "the harness writes 0x0F for them.",
    ),
    ScreenDef(
        "menu-campaign-info", "Campaign briefing / mission list", "menu", "menu",
        ["CAMPAIGN_INFO_PAGE"], ["CAMPAIGN_BUTTON", "CAMPAIGN_START_BUTTON"],
        note="Mission rows are 17 px apart; the harness uses "
             "`142 + row*17 + 8`.",
    ),
    ScreenDef(
        "menu-load-game", "Load game", "menu", "menu",
        ["LOAD_GAME_PAGE"], ["LOAD_GAME_BUTTON"],
        note="The save-name list is the clickable region; row height has not "
             "been measured against a populated list yet.",
    ),
    ScreenDef(
        "menu-options", "Main-menu options", "menu", "menu",
        ["OPTIONS_PAGE"], ["OPTIONS_BUTTON"],
        note="Three horizontal sliders and six value indicators. The port "
             "renders this page but the controls are not wired yet.",
    ),
    ScreenDef(
        "menu-cinematics", "Cinematics", "menu", "menu",
        ["CINEMATICS_PAGE"], ["CINEMATICS_BUTTON"],
        note="Video list with Start and Cancel.",
    ),
    ScreenDef(
        "menu-credits", "Credits", "menu", "menu",
        ["CREDITS_PAGE"], ["CREDITS_BUTTON"],
        note="Single OK button.",
    ),
    ScreenDef(
        "menu-loading", "Loading cover", "menu", "menu",
        ["LOADING_PAGE"], ["FREE_GAME_BUTTON", "FREE_START_BUTTON"],
        note="No interactive controls; it is a timed cover frame.",
    ),

    # Always-on park HUD.
    ScreenDef(
        "park-hud", "Park HUD (always visible)", "hud", "park",
        ["RIGHT_PANEL", "NAVIGATION_PANEL", "GAUGE_PANEL", "MAP_PANEL", "MESSAGE_PANEL"],
        note="Visible for the whole park session. Right panel opens the six "
             "main panels; gauge opens budget/visitors/dino-list/show-list/"
             "paddock-list; nav and map controls move the camera.",
        default_open=True,
    ),

    # Construction family.
    ScreenDef(
        "park-construction", "Construction bar", "park-panel", "park",
        ["CONSTRUCTION_PANEL"], ["OPEN_CONSTRUCTION"],
        note="Three sub-buttons: kit, fence and ready-made show. It is hidden "
             "while one of those sub-panels is open.",
    ),
    ScreenDef(
        "park-construction-kit", "Construction kit (attraction editor)", "park-panel",
        "park", ["CONSTRUCTION_KIT_PANEL"],
        ["OPEN_CONSTRUCTION", "OPEN_CONSTRUCTION_KIT"],
        note="Switches on the left choose seat count, judge type, feature "
             "category and cage size; the feature tiles share one 3x3 area.",
    ),
    ScreenDef(
        "park-construction-fence", "Fence construction", "park-panel", "park",
        ["FENCE_CONSTRUCTION_AND_INFO_PANEL"],
        ["OPEN_CONSTRUCTION", "OPEN_CONSTRUCTION_FENCE"],
        note="Fence name box plus lunch-box and observation-area switches.",
    ),
    ScreenDef(
        "park-construction-show", "Ready-made show list", "park-panel", "park",
        ["PREDEFINED_SHOW_PANEL"],
        ["OPEN_CONSTRUCTION", "OPEN_CONSTRUCTION_ATTRACTION_PREDEF"],
        note="Seven-column list of pre-built attractions; Validate/Cancel at "
             "the bottom.",
    ),
    ScreenDef(
        "park-construction-road", "Road / metro construction", "park-panel", "park",
        ["CONSTRUCTION_ROAD_METRO_PANEL"], ["OPEN_TRANSPORT"],
        note="Road and metro mode switches plus the farm building; close with "
             "PUTBACK_PANEL_BUTTON.",
    ),
    ScreenDef(
        "park-construction-buildings", "Buildings and shops", "park-panel", "park",
        ["CONSTRUCTION_BUILDINGS_PANEL"], ["OPEN_BUILDING"],
        note="Six category tabs. Each category shows five product buttons at "
             "the same five y positions; only the active category is clickable.",
    ),

    # Info-panel family.
    ScreenDef(
        "park-dino-purchase", "Dinosaur purchase / save", "park-panel", "park",
        ["DINO_ACHAT_SAVE_PANEL"], ["OPEN_PURCHASE"],
        note="Dinosaur list, description and price. The SAVE name is "
             "`DINO_ACHAT_SAVE_OUTCOME_SAVE`; it purchases/saves the selected "
             "dinosaur, not a save-game slot.",
    ),
    ScreenDef(
        "park-options", "In-park options / save / load", "park-panel", "park",
        ["OPTIONS_PANEL"], ["OPEN_OPTIONS"],
        note="Top row switches between options, save and load. The save tab "
             "shows `OPTIONS_SAVE_NAMES_LIST` (12-px rows), the name edit box "
             "and Validate/Cancel; those save controls are hidden until the "
             "Save tab is selected.",
    ),
    ScreenDef(
        "park-mission", "Mission / objectives", "park-panel", "park",
        ["MISSION_PANEL"], ["OBJECTIF_PANEL_LIGHTS_GROUP"],
        note="Opened by the campaign objective light, not by a HUD button. "
             "Two scrolling text zones and an OK button.",
    ),
    ScreenDef(
        "park-dino-list", "Dinosaur list", "park-panel", "park",
        ["DINO_LIST_PANEL"], ["OPEN_DINO_LIST_PANEL"],
        note="Five 64x64 snapshot slots per page, 85 px apart, with sort "
             "buttons on the left and page scroll on the right.",
    ),
    ScreenDef(
        "park-show-list", "Show / attraction list", "park-panel", "park",
        ["ATTRACTION_LIST_PANEL"], ["OPEN_SHOW_LIST_PANEL"],
        note="Ten rows, 29 px apart. Each row has selection light, permutation, "
             "name, lock/unlock and iteration buttons.",
    ),
    ScreenDef(
        "park-paddock-list", "Paddock / fence list", "park-panel", "park",
        ["FENCE_LIST_PANEL"], ["OPEN_PADDOCK_LIST_PANEL"],
        note="Five snapshot slots and page scroll. The HUD paddock button opens "
             "this panel, not CAGE_LIST_PANEL.",
    ),
    ScreenDef(
        "park-cage-list", "Cage list (legacy)", "park-panel", "park",
        ["CAGE_LIST_PANEL"],
        note="Never selected by the HUD controller in retail; kept for "
             "completeness. `ATTRACTION_LIST_OUTCOME_CAGE_LIST` is invisible.",
    ),

    # Laboratory.
    ScreenDef(
        "park-lab-main", "Laboratory main", "lab", "park",
        ["DINO_LAB_MAIN_CENTER"], ["OPEN_LABORATORY"],
        note="Four lab centres (breed, modify, clone, fridge) and Exit. The "
             "two patient lights are selection indicators.",
    ),
    ScreenDef(
        "park-lab-breed", "Laboratory: breed", "lab", "park",
        ["LAB_BREED_CENTER"], ["OPEN_LABORATORY", "SELECT_LAB_BREED"],
        note="Four gene tabs (physical, adornment, texture, general). Only one "
             "gene group is visible at a time; the variation buttons in the "
             "other groups are inactive until their tab is selected.",
    ),
    ScreenDef(
        "park-lab-modify", "Laboratory: modify", "lab", "park",
        ["LAB_MODIFY_CENTER"], ["OPEN_LABORATORY", "SELECT_LAB_MODIFY"],
        note="Three beam buttons plus fence/fridge/back/exit.",
    ),
    ScreenDef(
        "park-lab-fridge", "Laboratory: fridge / egg storage", "lab", "park",
        ["LAB_FRIDGE_CENTER"], ["OPEN_LABORATORY", "SELECT_LAB_FRIDGE"],
        note="Eight 45x75 egg slots in two rows, name edit, Sell/Fence/Back/Exit.",
    ),
    ScreenDef(
        "park-lab-clone", "Laboratory: clone", "lab", "park",
        ["LAB_CLONE_CENTER"], ["OPEN_LABORATORY", "SELECT_LAB_CLONE"],
        note="Clone trigger, offspring name, fence/fridge/back/exit.",
    ),

    # Other park context panels.
    ScreenDef(
        "park-food", "Food distribution", "park-panel", "park",
        ["FOOD_DISTRIBUTION_PANEL"],
        note="Opened by selecting a feeder/food dispenser in the park; not a "
             "HUD button and not ported yet. Vegetarian/carnivore choices and "
             "ten 15-px time slots.",
    ),
    ScreenDef(
        "park-attraction-stats", "Attraction stats", "park-panel", "park",
        ["ATTRACTION_STATS_PANEL"],
        ["OPEN_CONSTRUCTION", "OPEN_CONSTRUCTION_KIT", "CONSTRUCTION_KIT_STATS_ICON"],
        note="Also reachable from `ATTRACTION_LIST_OUTCOME_STATS`. Close and "
             "open-show-list buttons on the right.",
    ),
    ScreenDef(
        "park-dino-info", "Dinosaur info / portrait", "park-panel", "park",
        ["DINO_INFO_PANEL", "DINO_INFO_DINO_PIC"],
        note="Selection-driven: click a dinosaur in the park. The portrait "
             "root overlaps the top of the info strip; its teleport and "
             "next-dinosaur buttons are there.",
    ),
    ScreenDef(
        "park-budget", "Budget ledger", "park-panel", "park",
        ["BUDGET_PANEL"], ["OPEN_BUDGET_PANEL"],
        note="Display-only ledger with a single close button.",
    ),
    ScreenDef(
        "park-visitors", "Visitor statistics", "park-panel", "park",
        ["VISITEURS_STATS_PANEL"], ["OPEN_VISITOR_PANEL"],
        note="Display-only statistics with a single close button.",
    ),
    ScreenDef(
        "park-cage-info", "Cage / paddock info", "park-panel", "park",
        ["CAGE_INFO_PANEL"],
        note="Selection-driven: click a cage or paddock. Assign/unassign share "
             "one position; team switches are 32x33.",
    ),
    ScreenDef(
        "park-building-info", "Building info", "park-panel", "park",
        ["BATIMENTS_INFO_PANEL"],
        note="Selection-driven and display-only: no interactive widgets.",
    ),
    ScreenDef(
        "park-staff-security", "Security staff panel", "park-panel", "park",
        ["EFFECTIFS_SECU_PANEL"],
        note="Selection/context panel; not wired to a HUD opener in the port. "
             "Two 13-px up/down pairs.",
    ),
    ScreenDef(
        "park-staff-food", "Food staff panel", "park-panel", "park",
        ["EFFECTIFS_NUTRI_PANEL"],
        note="Selection/context panel; hire/fire feeder up/down buttons.",
    ),
    ScreenDef(
        "park-dino-placement", "Dinosaur placement marker", "park-panel", "park",
        ["DINO_LIST_PARK_PLACEMENT"],
        note="1x1 placement marker; no interactive widgets.",
    ),
    ScreenDef(
        "park-objective-light", "Campaign objective light", "hud", "park",
        ["OBJECTIF_PANEL_LIGHTS_GROUP"],
        note="20x100 campaign indicator at the left edge; visible only while a "
             "mission objective is active. Opens MISSION_PANEL.",
    ),

    # Modal dialogs (children of DIALOG_BACKGROUND, centred at runtime).
    ScreenDef(
        "dialog-warning", "Warning dialog", "dialog", "park",
        ["WARNING_PANEL"],
        note="Modal warning, e.g. an invalid purchase or placement. Centred by "
             "ALIGN_CENTER; the outcome icon closes it.",
    ),
    ScreenDef(
        "dialog-request", "Request dialog", "dialog", "park",
        ["REQUEST_PANEL"],
        note="Modal visitor/request prompt. Validate and Cancel sit side by "
             "side at the bottom.",
    ),
    ScreenDef(
        "dialog-campaign-end", "Campaign-end dialog", "dialog", "park",
        ["CPGEND_PANEL"],
        note="Campaign completion panel; the outcome icon closes it.",
    ),

    # Dead data, kept so nobody tests a menu that does not exist.
    ScreenDef(
        "radial-show", "Show radial menu (unused data)", "radial", "radial",
        ["SHOW_RADIAL_MENU"],
        note="Dead data: no executable string references and the icon names "
             "do not exist in any GraphicDB. Do not test.",
    ),
    ScreenDef(
        "radial-building", "Building radial menu (unused data)", "radial", "radial",
        ["BUILDING_RADIAL_MENU"], note="Dead data; do not test.",
    ),
    ScreenDef(
        "radial-paddock", "Paddock radial menu (unused data)", "radial", "radial",
        ["PADDOCK_RADIAL_MENU"], note="Dead data; do not test.",
    ),
    ScreenDef(
        "radial-security", "Security radial menu (unused data)", "radial", "radial",
        ["SECURITY_RADIAL_MENU"], note="Dead data; do not test.",
    ),
]


def resolve_label(widget: Widget, loc: Loc | None) -> tuple[str, str]:
    """Return (display label, resolved hint) for a widget."""
    text = widget.get("TEXT")
    hint = widget.get("HINT")
    resolved_text = resolve_text(text, [loc]) if loc is not None and text else text
    resolved_hint = resolve_text(hint, [loc]) if loc is not None and hint else hint
    generic = {"", "&Ok", "&NoText", "OK", "NO TEXT"}
    label = resolved_text
    if label in generic:
        label = resolved_hint or ""
    return label, resolved_hint


def ancestor_chain(interface: Interface, widget: Widget, stop: str) -> list[Widget]:
    chain: list[Widget] = []
    current = widget
    seen: set[str] = set()
    while current.parent and current.parent != "ROOT" and current.name != stop:
        if current.name in seen:
            break
        seen.add(current.name)
        current = interface.widgets[current.parent]
        chain.append(current)
        if current.name == stop:
            break
    return chain


def active_within(interface: Interface, widget: Widget, screen_root: str) -> bool:
    """Visible if every ancestor between the widget and the screen root is on.

    The screen root itself is ignored because opening a screen sets it visible.
    """
    if not widget.visible:
        return False
    for ancestor in ancestor_chain(interface, widget, screen_root):
        if ancestor.name == screen_root:
            break
        if not ancestor.visible:
            return False
    return True


def integer_center(widget: Widget) -> tuple[int, int]:
    return (widget.abs_x + widget.width // 2, widget.abs_y + widget.height // 2)


def list_rows(widget: Widget) -> list[dict]:
    """Synthetic row click points for a LIST, when the line height is known."""
    raw = widget.get("LB_LINE_HEIGHT")
    try:
        line = int(raw) if raw else 0
    except ValueError:
        line = 0
    if line <= 0 and widget.name == "ISLAND_LIST":
        # Menu::hit_test uses height / 10 for the ten islands.
        line = widget.height // 10
    if line <= 0 and widget.name == "CAMPAIGN_LIST":
        # Measured by scripts/x32dbg/start-campaign.ps1: 34-px rows.
        line = 34
    if line <= 0 and widget.name == "CAMPAIGN_INFO_DESCRIPTION":
        # Measured by start-campaign.ps1: 142 + row*17 + 8.
        line = 17
    if line <= 0:
        return []
    count = max(0, widget.height // line)
    rows = []
    for index in range(count):
        y = widget.abs_y + index * line + line // 2
        rows.append({
            "index": index,
            "y": y,
            "click": [widget.abs_x + widget.width // 2, y],
        })
    return rows


def list_row_labels(widget: Widget, loc: Loc | None) -> list[str]:
    if loc is None:
        return []
    if widget.name == "ISLAND_LIST":
        labels = []
        for index in range(1, 11):
            key = f"&Isl{index:02d}_Name"
            try:
                labels.append(loc.by_id(key).text)
            except KeyError:
                labels.append("")
        return labels
    if widget.name == "CAMPAIGN_LIST":
        labels = []
        for index in range(1, 9):
            key = f"&Campaign{index}Title"
            try:
                labels.append(loc.by_id(key).text)
            except KeyError:
                labels.append("")
        return labels
    return []


def widget_entry(interface: Interface, widget: Widget, root: str, loc: Loc | None) -> dict:
    label, hint = resolve_label(widget, loc)
    entry = {
        "name": widget.name,
        "type": widget.type,
        "parent": widget.parent,
        "rect": [widget.abs_x, widget.abs_y, widget.width, widget.height],
        "click": list(integer_center(widget)),
        "visible": widget.visible,
        "active": active_within(interface, widget, root),
        "text": widget.get("TEXT"),
        "hint": widget.get("HINT"),
        "label": label,
        "resolved_hint": hint,
    }
    if widget.type == "LIST":
        rows = list_rows(widget)
        if rows:
            labels = list_row_labels(widget, loc)
            for index, row in enumerate(rows):
                if index < len(labels) and labels[index]:
                    row["label"] = labels[index]
            entry["rows"] = rows
            entry["row_height"] = rows[1]["y"] - rows[0]["y"] if len(rows) > 1 else 0
    return entry


def find_control(screens: list[dict], name: str) -> dict | None:
    for screen in screens:
        for control in screen["controls"]:
            if control["name"] == name:
                return {"screen": screen, "control": control}
    return None


def build_screen(interface: Interface, definition: ScreenDef, loc: Loc | None) -> dict:
    controls: list[dict] = []
    seen: set[str] = set()
    for root in definition.roots:
        if root not in interface.widgets:
            raise KeyError(f"{definition.id}: missing root {root}")
        for widget in interface.walk(root):
            if widget.type not in INTERACTIVE_TYPES:
                continue
            if widget.width <= ZERO_SIZE or widget.height <= ZERO_SIZE:
                continue
            if widget.name in seen:
                continue
            seen.add(widget.name)
            controls.append(widget_entry(interface, widget, root, loc))
    controls.sort(key=lambda item: (item["rect"][1], item["rect"][0], item["name"]))
    return {
        "id": definition.id,
        "title": definition.title,
        "kind": definition.kind,
        "roots": definition.roots,
        "note": definition.note,
        "open_path": definition.open_path,
        "default_open": definition.default_open,
        "controls": controls,
    }


def build_map(data_root: Path = DEFAULT_DATA_ROOT) -> dict:
    menu = load_interface(data_root / MENU_INI)
    park = load_interface(data_root / PARK_INI)
    park.apply_resize(DESIGN_WIDTH, DESIGN_HEIGHT)
    radial = load_interface(data_root / RADIAL_INI)
    menu_loc = read_loc(data_root / MENU_LOC) if (data_root / MENU_LOC).exists() else None
    park_loc = read_loc(data_root / PARK_LOC) if (data_root / PARK_LOC).exists() else None

    interfaces = {"menu": menu, "park": park, "radial": radial}
    locs = {"menu": menu_loc, "park": park_loc, "radial": None}

    screens = []
    for definition in SCREENS:
        screen = build_screen(
            interfaces[definition.source], definition, locs[definition.source]
        )
        screens.append(screen)

    # Resolve opener click points after all screens exist.
    for screen in screens:
        path = []
        for name in screen["open_path"]:
            found = find_control(screens, name)
            if found is None:
                path.append({"control": name, "screen": None, "click": None})
                continue
            path.append({
                "control": name,
                "screen": found["screen"]["id"],
                "click": found["control"]["click"],
            })
        screen["open"] = path

    total = sum(len(screen["controls"]) for screen in screens)
    return {
        "schema": SCHEMA_VERSION,
        "design": {"width": DESIGN_WIDTH, "height": DESIGN_HEIGHT},
        "sources": {
            "menu": MENU_INI,
            "park": PARK_INI,
            "radial": RADIAL_INI,
            "menu_loc": MENU_LOC,
            "park_loc": PARK_LOC,
        },
        "counts": {
            "screens": len(screens),
            "controls": total,
            "menu_controls": sum(len(s["controls"]) for s in screens if s["kind"] == "menu"),
            "park_controls": sum(
                len(s["controls"])
                for s in screens
                if s["kind"] in ("hud", "park-panel", "lab", "dialog")
            ),
            "radial_controls": sum(
                len(s["controls"]) for s in screens if s["kind"] == "radial"
            ),
        },
        "screens": screens,
    }


def md_cell(value: object) -> str:
    text = "" if value is None else str(value)
    return text.replace("|", "\\|").replace("\n", " ").strip()


def md_control_table(screen: dict) -> list[str]:
    lines = [
        "| Control | Parent | Type | Rect (x,y,w,h) | Click | On open | Label / hint |",
        "| --- | --- | --- | --- | --- | --- | --- |",
    ]
    for c in screen["controls"]:
        x, y, w, h = c["rect"]
        active = "yes" if c["active"] else "no"
        label = c["label"] or ""
        if c["resolved_hint"] and c["resolved_hint"] != label:
            label = f"{label} / {c['resolved_hint']}" if label else c["resolved_hint"]
        lines.append(
            f"| `{md_cell(c['name'])}` | `{md_cell(c['parent'])}` | {c['type']} | "
            f"{x},{y} {w}x{h} | **{c['click'][0]},{c['click'][1]}** | {active} | "
            f"{md_cell(label)} |"
        )
    return lines


def md_rows_table(widget: dict) -> list[str]:
    rows = widget.get("rows", [])
    if not rows:
        return []
    lines = [
        f"`{widget['name']}` rows (row height {widget.get('row_height', 0)} px):",
        "",
        "| Row | Click | Label |",
        "| --- | --- | --- |",
    ]
    for row in rows:
        lines.append(
            f"| {row['index']} | **{row['click'][0]},{row['click'][1]}** | "
            f"{md_cell(row.get('label', ''))} |"
        )
    return lines


def render_markdown(ui_map: dict) -> str:
    lines = [
        "# Retail UI click map",
        "",
        "Status: **generated from the shipped overlay INIs** for the Dino Island",
        "testing agents. This is the 800x600 client-space map used by the windowed",
        "and virtual-input harnesses.",
        "",
        "Do not hand-edit this file. Change `tools/formats/ui_map.py` and run:",
        "",
        "```powershell",
        ".venv\\Scripts\\python.exe tools\\formats\\ui_map.py build",
        "```",
        "",
        "Machine-readable source: `docs/platform/ui-coordinates.json`.",
        "",
        "## Coordinate system",
        "",
        "- Origin is the **top-left of the game client**, +X right, +Y down.",
        "- Design space is **800x600**; the windowed runtime keeps an 800x600",
        "  client, so these are window client coordinates.",
        "- `Click` is the integer centre of the control rectangle. Any point",
        "  inside `Rect` works; the activating click is also passed to the UI.",
        "- Rectangles come from `MenuInterface.Ini`, `ParkInterface.Ini` and its",
        "  nine includes, after the `ALIGN_CENTER` runtime pass. They are not",
        "  screenshot estimates.",
        "- `On open` means the control is visible when its screen/panel is open.",
        "  `no` means a parent group is hidden by default and must be switched on",
        "  first (for example a lab gene tab or a construction category).",
        "- Zero-size scrollbar sub-widgets are omitted; click the scroll track or",
        "  use the mouse wheel instead.",
        "",
        "This is the **retail** contract. The portable viewer does not implement",
        "every panel yet. One known port discrepancy: `Menu::click` still uses",
        "`height / 10` (28 px) for `ISLAND_LIST` rows, while retail uses the",
        "declared 34-px rows. If a port test selects the wrong island, treat it",
        "as a port bug, not a map error.",
        "",
        "## Querying the map",
        "",
        "```powershell",
        "# exact control name",
        ".venv\\Scripts\\python.exe tools\\formats\\ui_map.py find OPEN_LABORATORY",
        "",
        "# all controls on one screen",
        ".venv\\Scripts\\python.exe tools\\formats\\ui_map.py find --screen park-hud",
        "",
        "# only controls visible when that screen opens",
        ".venv\\Scripts\\python.exe tools\\formats\\ui_map.py find --screen park-lab-breed --active",
        "",
        "# substring search, JSON output",
        ".venv\\Scripts\\python.exe tools\\formats\\ui_map.py find SORT --json",
        "```",
        "",
        "Click through the windowed harness:",
        "",
        "```powershell",
        ".\\scripts\\x32dbg\\Send-VirtualClick.ps1 -X 757 -Y 46 -GameDir run-test",
        "```",
        "",
        "Retail sets DirectInput to `DISCL_FOREGROUND`: if the game is fully",
        "inactive it logs `virtual mouse: state calls=0` and ignores virtual",
        "clicks. Keep the game active for a click test, or check that log line",
        "first. The coordinate map is independent of that input-state issue.",
        "",
        "## Verified quick paths",
        "",
        "These points are verified to fall inside the controls and are pinned by",
        "`tests/test_ui_map.py`; several are the exact points used by the",
        "harness scripts. A table centre can differ by 1-2 px from a harness",
        "point and still be inside the same rectangle.",
        "",
        "| Action | Click | Control |",
        "| --- | --- | --- |",
        "| Main menu -> Free Game | 395,234 | `FREE_GAME_BUTTON` |",
        "| Main menu -> Campaigns | 395,189 | `CAMPAIGN_BUTTON` |",
        "| Free Game -> island row 0 | 269,183 | `ISLAND_LIST` row 0 (IGUANAO) |",
        "| Free Game -> easy | 494,451 | `FREE_EASY_BUTTON` |",
        "| Free Game -> Start | 526,531 | `FREE_START_BUTTON` |",
        "| Campaign row 3 | 409,285 | `CAMPAIGN_LIST` row 3 |",
        "| Campaign -> Start | 365,530 | `CAMPAIGN_START_BUTTON` |",
        "| Campaign info -> Start | 521,449 | `CAMPAIGN_INFO_START_BUTTON` |",
        "| Park -> Laboratory | 757,46 | `OPEN_LABORATORY` |",
        "| Park -> Options | 765,348 | `OPEN_OPTIONS` |",
        "| Options -> Save tab | 348,73 | `OPTIONS_MENU_SAVE_BUTTON` |",
        "| Save tab -> name box | 349,382 | `OPTIONS_NAME_SLOT` |",
        "| Save tab -> Validate | 334,436 | `OPTIONS_VALIDATE_BUTTON` |",
        "| Park -> Budget | 97,584 | `OPEN_BUDGET_PANEL` |",
        "| Park -> Visitors | 207,584 | `OPEN_VISITOR_PANEL` |",
        "| Park -> Dino list | 302,584 | `OPEN_DINO_LIST_PANEL` |",
        "| Park -> Show list | 397,584 | `OPEN_SHOW_LIST_PANEL` |",
        "| Park -> Paddock list | 492,584 | `OPEN_PADDOCK_LIST_PANEL` |",
        "",
        "`ISLAND_LIST` row `n` is at `y = 183 + 34 * n`; only eight rows",
        "(0-7) fit, so islands 9-10 require the list scrollbar. The harness",
        "click at 260,183 is row 0. `CAMPAIGN_LIST` row `n` is at",
        "`y = 183 + 34 * n` (eight campaign rows). `CAMPAIGN_INFO_DESCRIPTION`",
        "rows are 17 px apart: `y = 150 + 17 * n`.",
        "",
        "## Screen index",
        "",
        "| Screen id | Title | Kind | Controls | Roots |",
        "| --- | --- | --- | --- | --- |",
    ]
    for screen in ui_map["screens"]:
        lines.append(
            f"| `{screen['id']}` | {md_cell(screen['title'])} | {screen['kind']} | "
            f"{len(screen['controls'])} | {md_cell(', '.join(screen['roots']))} |"
        )
    lines.append("")

    sections = [
        ("Menu screens", "menu"),
        ("Park HUD", "hud"),
        ("Park panels", "park-panel"),
        ("Laboratory centres", "lab"),
        ("Modal dialogs", "dialog"),
        ("Radial menus (dead data)", "radial"),
    ]
    for title, kind in sections:
        group = [s for s in ui_map["screens"] if s["kind"] == kind]
        if not group:
            continue
        lines.append(f"## {title}")
        lines.append("")
        if kind == "radial":
            lines.append(
                "`RadialMenu.Ini` is dead data. The executable has no string "
                "references to these sections and the icon names are absent from "
                "every GraphicDB. They are listed only so a test plan does not "
                "mistake them for reachable menus."
            )
            lines.append("")
        for screen in group:
            lines.append(f"### {screen['title']} — `{screen['id']}`")
            lines.append("")
            if screen["note"]:
                lines.append(screen["note"])
                lines.append("")
            if screen.get("open"):
                steps = []
                for step in screen["open"]:
                    if step["click"] is None:
                        continue
                    steps.append(f"`{step['control']}` ({step['click'][0]},{step['click'][1]})")
                if steps:
                    lines.append("Open path: " + " -> ".join(steps))
                    lines.append("")
            roots = ", ".join(f"`{r}`" for r in screen["roots"])
            lines.append(f"Roots: {roots}. Controls: {len(screen['controls'])}.")
            lines.append("")
            if not screen["controls"]:
                lines.append("_No interactive controls._")
                lines.append("")
                continue
            lines.extend(md_control_table(screen))
            lines.append("")
            for control in screen["controls"]:
                if control["type"] == "LIST" and control.get("rows"):
                    lines.extend(md_rows_table(control))
                    lines.append("")

    lines.append("## Regenerating and verifying")
    lines.append("")
    lines.append("```powershell")
    lines.append(".venv\\Scripts\\python.exe tools\\formats\\ui_map.py build")
    lines.append(".venv\\Scripts\\python.exe -m pytest -q tests/test_ui_map.py")
    lines.append("```")
    lines.append("")
    lines.append("Counts: "
                 f"{ui_map['counts']['screens']} screens, "
                 f"{ui_map['counts']['controls']} interactive controls "
                 f"({ui_map['counts']['menu_controls']} menu, "
                 f"{ui_map['counts']['park_controls']} park/dialog/lab, "
                 f"{ui_map['counts']['radial_controls']} radial dead data).")
    lines.append("")
    return "\n".join(lines)


def command_build(args: argparse.Namespace) -> int:
    ui_map = build_map(args.data_root)
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(ui_map, indent=2) + "\n", encoding="utf-8")
        print(f"wrote {args.json} ({args.json.stat().st_size} bytes)")
    if args.markdown:
        args.markdown.parent.mkdir(parents=True, exist_ok=True)
        text = render_markdown(ui_map)
        args.markdown.write_text(text, encoding="utf-8")
        print(f"wrote {args.markdown} ({len(text.splitlines())} lines)")
    return 0


def command_find(args: argparse.Namespace) -> int:
    ui_map = build_map(args.data_root)
    matches = []
    for screen in ui_map["screens"]:
        if args.screen and screen["id"] != args.screen:
            continue
        for control in screen["controls"]:
            if args.active and not control["active"]:
                continue
            if args.names:
                haystack = control["name"].upper()
                if not any(name.upper() in haystack for name in args.names):
                    continue
            matches.append({"screen": screen, "control": control})
            continue
    if args.json:
        payload = [
            {
                "screen": m["screen"]["id"],
                "title": m["screen"]["title"],
                **m["control"],
            }
            for m in matches
        ]
        print(json.dumps(payload, indent=2))
        return 0
    for match in matches:
        screen = match["screen"]
        control = match["control"]
        print(
            f"{screen['id']:28} {control['name']:46} {control['type']:7} "
            f"click={control['click'][0]},{control['click'][1]} "
            f"rect={control['rect'][0]},{control['rect'][1]},"
            f"{control['rect'][2]}x{control['rect'][3]} "
            f"active={'yes' if control['active'] else 'no'} "
            f"label={control['label']!r}"
        )
    if not matches:
        print("no matching controls", file=sys.stderr)
        return 1
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--data-root", type=Path, default=DEFAULT_DATA_ROOT,
        help="retail Data root (default: game/%%MAINDIR%%)",
    )
    sub = parser.add_subparsers(dest="command", required=True)

    build = sub.add_parser("build", help="write the JSON and Markdown map")
    build.add_argument("--json", type=Path, default=DEFAULT_JSON)
    build.add_argument("--markdown", type=Path, default=DEFAULT_MARKDOWN)
    build.set_defaults(func=command_build)

    find = sub.add_parser("find", help="find controls by name or screen")
    find.add_argument("names", nargs="*", help="substring(s) of the control name")
    find.add_argument("--screen", help="restrict to one screen id")
    find.add_argument("--active", action="store_true",
                      help="only controls visible when the screen is open")
    find.add_argument("--json", action="store_true", help="emit JSON")
    find.set_defaults(func=command_find)

    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
