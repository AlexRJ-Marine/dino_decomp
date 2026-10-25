#include "menu.h"

#include <algorithm>
#include <cstdlib>

namespace dino::ui {
namespace {

const char* kBackgrounds[9] = {
    "BACKGROUND_MAIN_MENU",     "BACKGROUND_FREE_GAME",
    "BACKGROUND_CAMPAIGN_SELECT", "BACKGROUND_CAMPAIGN_INFO",
    "BACKGROUND_LOAD_GAME",     "BACKGROUND_OPTIONS",
    "BACKGROUND_CINEMATICS",    "BACKGROUND_CREDITS",
    "BACKGROUND_LOADING",
};

bool ends_with(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

}  // namespace

bool Menu::load(const std::string& asset_root) {
    const std::string interface_path = asset_root + "/Data/Overlay/MenuInterface.Ini";
    const std::string db_path = asset_root + "/Data/Overlay/MenuGraphicDB.ini";
    const std::string loc_path = asset_root + "/Data/Overlay/MenuEng.loc";
    if (!overlay.load_menu(interface_path, db_path, error)) {
        return false;
    }
    try {
        loc = dino::load_loc(loc_path);
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
    loaded = true;
    return true;
}

int Menu::hit_test(int x, int y) const {
    // Later widgets are drawn on top; iterate backwards so nested buttons win.
    for (int i = static_cast<int>(overlay.widgets.size()) - 1; i >= 0; --i) {
        const Widget& widget = overlay.widgets[static_cast<std::size_t>(i)];
        if (!widget_visible(i) || !widget.enable) {
            continue;
        }
        if (widget.type != "BUTTON" && widget.type != "LIST") {
            continue;
        }
        if (x >= widget.abs_x && x < widget.abs_x + widget.width &&
            y >= widget.abs_y && y < widget.abs_y + widget.height) {
            return i;
        }
    }
    return -1;
}

void Menu::move(int x, int y) {
    hover = hit_test(x, y);
}

void Menu::escape() {
    if (page == 0) {
        quit = true;
    } else {
        page = 0;
        hover = -1;
    }
}

void Menu::click(int x, int y) {
    const int hit = hit_test(x, y);
    if (hit < 0) {
        return;
    }
    const Widget& widget = overlay.widgets[static_cast<std::size_t>(hit)];
    const std::string& name = widget.name;

    if (name == "FREE_GAME_BUTTON") {
        page = 1;
    } else if (name == "CAMPAIGN_BUTTON") {
        page = 2;
    } else if (name == "LOAD_GAME_BUTTON") {
        page = 4;
    } else if (name == "OPTIONS_BUTTON") {
        page = 5;
    } else if (name == "CINEMATICS_BUTTON") {
        page = 6;
    } else if (name == "CREDITS_BUTTON") {
        page = 7;
    } else if (name == "QUIT_BUTTON") {
        quit = true;
    } else if (name == "FREE_CANCEL_BUTTON") {
        page = 0;
    } else if (name == "FREE_START_BUTTON") {
        active = false;
    } else if (name == "FREE_EASY_BUTTON") {
        difficulty = 0;
    } else if (name == "FREE_MIDDLE_BUTTON") {
        difficulty = 1;
    } else if (name == "FREE_HARD_BUTTON") {
        difficulty = 2;
    } else if (name == "ISLAND_LIST") {
        const int line = widget.height > 0 ? widget.height / 10 : 28;
        const int selected = line > 0 ? (y - widget.abs_y) / line : 0;
        island = std::clamp(selected, 0, 9);
    } else if (ends_with(name, "_CANCEL_BUTTON") ||
               ends_with(name, "_BACK_BUTTON")) {
        page = 0;
    }
    hover = -1;
}

std::string Menu::text_for(const Widget& widget) const {
    return resolve_loc_text(widget.text, loc);
}

const char* Menu::background() const {
    return page >= 0 && page < 9 ? kBackgrounds[page] : kBackgrounds[0];
}

}  // namespace dino::ui
