#include "park.h"

#include <cstring>
#include <iterator>

namespace dino::ui {
namespace {

const char* const kHudRoots[] = {
    "RIGHT_PANEL", "NAVIGATION_PANEL", "GAUGE_PANEL", "MAP_PANEL", "MESSAGE_PANEL",
};

// Every panel root except the always-on HUD, RENDERVIEW and the dialog
// backdrop. The original state manager shows at most one of these at a time.
const char* const kPanelRoots[] = {
    "CONSTRUCTION_PANEL",
    "CONSTRUCTION_KIT_PANEL",
    "FENCE_CONSTRUCTION_AND_INFO_PANEL",
    "PREDEFINED_SHOW_PANEL",
    "CONSTRUCTION_ROAD_METRO_PANEL",
    "CONSTRUCTION_BUILDINGS_PANEL",
    "DINO_ACHAT_SAVE_PANEL",
    "OPTIONS_PANEL",
    "MISSION_PANEL",
    "OBJECTIF_PANEL_LIGHTS_GROUP",
    "DINO_LIST_PARK_PLACEMENT",
    "DINO_LIST_PANEL",
    "CAGE_LIST_PANEL",
    "ATTRACTION_LIST_PANEL",
    "FENCE_LIST_PANEL",
    "DINO_LAB_MAIN_CENTER",
    "LAB_BREED_CENTER",
    "LAB_MODIFY_CENTER",
    "LAB_FRIDGE_CENTER",
    "LAB_CLONE_CENTER",
    "FOOD_DISTRIBUTION_PANEL",
    "ATTRACTION_STATS_PANEL",
    "DINO_INFO_PANEL",
    "DINO_INFO_DINO_PIC",
    "EFFECTIFS_SECU_PANEL",
    "EFFECTIFS_NUTRI_PANEL",
    "BUDGET_PANEL",
    "VISITEURS_STATS_PANEL",
    "CAGE_INFO_PANEL",
    "BATIMENTS_INFO_PANEL",
};

const char* const kLabCenters[] = {
    "DINO_LAB_MAIN_CENTER", "LAB_BREED_CENTER", "LAB_MODIFY_CENTER",
    "LAB_FRIDGE_CENTER", "LAB_CLONE_CENTER",
};

struct Binding {
    const char* button;
    const char* panel;
};

// Right-panel/gauge buttons and sub-panel buttons. The HUD buttons come from
// FUN_00527d30; the construction sub-buttons from FUN_00532590; the kit list
// and stats icons share the info-panel handlers (FUN_00528ba0/0x00528c70).
const Binding kOpeners[] = {
    {"OPEN_LABORATORY", "DINO_LAB_MAIN_CENTER"},
    {"OPEN_CONSTRUCTION", "CONSTRUCTION_PANEL"},
    {"OPEN_BUILDING", "CONSTRUCTION_BUILDINGS_PANEL"},
    {"OPEN_TRANSPORT", "CONSTRUCTION_ROAD_METRO_PANEL"},
    {"OPEN_PURCHASE", "DINO_ACHAT_SAVE_PANEL"},
    {"OPEN_OPTIONS", "OPTIONS_PANEL"},
    {"OPEN_BUDGET_PANEL", "BUDGET_PANEL"},
    {"OPEN_VISITOR_PANEL", "VISITEURS_STATS_PANEL"},
    {"OPEN_DINO_LIST_PANEL", "DINO_LIST_PANEL"},
    {"OPEN_SHOW_LIST_PANEL", "ATTRACTION_LIST_PANEL"},
    {"OPEN_PADDOCK_LIST_PANEL", "FENCE_LIST_PANEL"},
    {"OPEN_CONSTRUCTION_KIT", "CONSTRUCTION_KIT_PANEL"},
    {"OPEN_CONSTRUCTION_FENCE", "FENCE_CONSTRUCTION_AND_INFO_PANEL"},
    {"OPEN_CONSTRUCTION_ATTRACTION_PREDEF", "PREDEFINED_SHOW_PANEL"},
    {"CONSTRUCTION_KIT_STATS_ICON", "ATTRACTION_STATS_PANEL"},
    {"CONSTRUCTION_KIT_LIST_ICON", "ATTRACTION_LIST_PANEL"},
    {"ATTRACTION_LIST_OUTCOME_STATS", "ATTRACTION_STATS_PANEL"},
    {"OBJECTIF_PANEL_LIGHTS_GROUP", "MISSION_PANEL"},
};

// Info-panel controller page 8 / state deactivation is reached through the
// panel's own close, cancel, validate or put-back button.
const Binding kClosers[] = {
    {"BUDGET_OUTCOME_OK", "BUDGET_PANEL"},
    {"VISITEURS_STATS_OUTCOME_OK", "VISITEURS_STATS_PANEL"},
    {"DINO_ACHAT_SAVE_OUTCOME_SAVE", "DINO_ACHAT_SAVE_PANEL"},
    {"DINO_ACHAT_SAVE_OUTCOME_CANCEL", "DINO_ACHAT_SAVE_PANEL"},
    {"OPTIONS_OPTIONS_VALIDATE_BUTTON", "OPTIONS_PANEL"},
    {"OPTIONS_VALIDATE_BUTTON", "OPTIONS_PANEL"},
    {"OPTIONS_UNVALIDATE_BUTTON", "OPTIONS_PANEL"},
    {"PREDEFINED_SHOW_VALIDATE", "PREDEFINED_SHOW_PANEL"},
    {"PREDEFINED_SHOW_CANCEL", "PREDEFINED_SHOW_PANEL"},
    {"ATTRACTION_LIST_OUTCOME_CANCEL", "ATTRACTION_LIST_PANEL"},
    {"ATTRACTION_STATS_OUTCOME_CANCEL", "ATTRACTION_STATS_PANEL"},
    {"MISSION_PANEL_OUTCOME_OK", "MISSION_PANEL"},
    {"CONSTRUCTION_BUILDINGS_PUTBACK_BUTTON", "CONSTRUCTION_BUILDINGS_PANEL"},
    {"PUTBACK_PANEL_BUTTON", "CONSTRUCTION_ROAD_METRO_PANEL"},
};

// Lab states 0..4: SELECT_LAB_* switches centre; *_BACK returns to the main
// menu. Each centre carries its own full-screen background.
const Binding kLabTabs[] = {
    {"SELECT_LAB_BREED", "LAB_BREED_CENTER"},
    {"SELECT_LAB_MODIFY", "LAB_MODIFY_CENTER"},
    {"SELECT_LAB_CLONE", "LAB_CLONE_CENTER"},
    {"SELECT_LAB_FRIDGE", "LAB_FRIDGE_CENTER"},
    {"BREED_BACK", "DINO_LAB_MAIN_CENTER"},
    {"MODIFY_BACK", "DINO_LAB_MAIN_CENTER"},
    {"CLONE_BACK", "DINO_LAB_MAIN_CENTER"},
    {"FRIDGE_BACK", "DINO_LAB_MAIN_CENTER"},
};

const char* const kLabExits[] = {
    "CANCEL_LAB_MENU", "BREED_EXIT", "MODIFY_EXIT", "CLONE_EXIT", "FRIDGE_EXIT",
};

// Each lab centre carries its own 800x600 backdrop, stored VISIBLE=0 and
// shown by the corresponding lab state.
struct LabBackground {
    const char* center;
    const char* background;
};

const LabBackground kLabBackgrounds[] = {
    {"DINO_LAB_MAIN_CENTER", "LAB_MAIN_BACKGROUND"},
    {"LAB_BREED_CENTER", "LAB_BREED_BACKGROUND"},
    {"LAB_MODIFY_CENTER", "LAB_MODIFY_BACKGROUND"},
    {"LAB_FRIDGE_CENTER", "LAB_FRIDGE_BACKGROUND"},
    {"LAB_CLONE_CENTER", "LAB_CLONE_BACKGROUND"},
};

bool contains(const char* const* list, std::size_t count, const std::string& value) {
    for (std::size_t i = 0; i < count; ++i) {
        if (value == list[i]) {
            return true;
        }
    }
    return false;
}

bool is_lab_center(const std::string& name) {
    return contains(kLabCenters, std::size(kLabCenters), name);
}

const char* lab_background(const std::string& center) {
    for (const LabBackground& entry : kLabBackgrounds) {
        if (center == entry.center) {
            return entry.background;
        }
    }
    return nullptr;
}

}  // namespace

bool ParkHud::load(const std::string& interface_path, const std::string& db_path,
                   const std::string& loc_path, std::string& error) {
    if (!overlay.load_park(interface_path, db_path, error)) {
        return false;
    }
    try {
        loc = load_loc(loc_path);
    } catch (const std::exception& e) {
        // A missing ParkEng.loc only loses text; the panels still work.
        error = e.what();
    }
    loaded_ = true;
    hud_hidden_ = false;
    return true;
}

bool ParkHud::is_visible(const std::string& root) const {
    const Widget* widget = overlay.get(root);
    return widget != nullptr && widget->visible;
}

void ParkHud::hide_panels() {
    for (const char* root : kPanelRoots) {
        overlay.set_visible(root, false);
    }
    for (const LabBackground& entry : kLabBackgrounds) {
        overlay.set_visible(entry.background, false);
    }
}

void ParkHud::show_hud(bool visible) {
    for (const char* root : kHudRoots) {
        overlay.set_visible(root, visible);
    }
    hud_hidden_ = !visible;
}

void ParkHud::open_panel(const std::string& name) {
    hide_panels();
    overlay.set_visible(name, true);
    if (is_lab_center(name)) {
        if (const char* background = lab_background(name)) {
            overlay.set_visible(background, true);
        }
        show_hud(false);
    } else {
        show_hud(true);
    }
}

void ParkHud::close_panel(const std::string& name) {
    overlay.set_visible(name, false);
    if (is_lab_center(name)) {
        show_hud(true);
    }
}

bool ParkHud::click(int x, int y, std::string& action) {
    action.clear();
    if (!loaded_) {
        return false;
    }
    const int hit = overlay.hit_test(x, y);
    if (hit < 0) {
        // Clicks on a visible panel or on the always-on HUD are consumed so
        // they cannot reach the camera or the build tools.
        for (const Widget& widget : overlay.widgets) {
            if (widget.parent_index >= 0 || !widget.visible ||
                widget.name == "RENDERVIEW" || widget.name == "DIALOG_BACKGROUND") {
                continue;
            }
            if (x >= widget.abs_x && x < widget.abs_x + widget.width &&
                y >= widget.abs_y && y < widget.abs_y + widget.height) {
                return true;
            }
        }
        return false;
    }
    const std::string name = overlay.widgets[static_cast<std::size_t>(hit)].name;

    for (const char* exit_button : kLabExits) {
        if (name == exit_button) {
            hide_panels();
            show_hud(true);
            action = "close lab";
            return true;
        }
    }

    for (const Binding& tab : kLabTabs) {
        if (name == tab.button) {
            open_panel(tab.panel);
            action = std::string("open ") + tab.panel;
            return true;
        }
    }

    for (const Binding& closer : kClosers) {
        if (name == closer.button) {
            close_panel(closer.panel);
            action = std::string("close ") + closer.panel;
            return true;
        }
    }

    for (const Binding& opener : kOpeners) {
        if (name == opener.button) {
            const bool visible = is_visible(opener.panel);
            if (visible) {
                close_panel(opener.panel);
            } else {
                open_panel(opener.panel);
            }
            action = std::string(visible ? "close " : "open ") + opener.panel;
            return true;
        }
    }

    if (name == "OPEN_SELLING") {
        action = "sell mode";
        return true;
    }

    // Any other HUD button (panel internals, lists) is consumed but has no
    // port behaviour yet.
    return true;
}

}  // namespace dino::ui
