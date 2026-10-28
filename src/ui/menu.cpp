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

bool Menu::load_campaigns(const std::string& scenario_root,
                          std::uint32_t unlock_value) {
    campaigns.clear();
    campaign_error.clear();
    this->unlock_value = unlock_value;
    try {
        campaigns = dino::load_campaigns(scenario_root);
    } catch (const std::exception& e) {
        campaign_error = e.what();
        return false;
    }
    campaign = std::clamp(campaign, 0,
                          campaigns.empty()
                              ? 0
                              : static_cast<int>(campaigns.size()) - 1);
    return true;
}

bool Menu::campaign_unlocked(int index) const {
    if (index < 0 || index >= static_cast<int>(campaigns.size())) {
        return false;
    }
    return dino::campaign_unlocked(campaigns[static_cast<std::size_t>(index)],
                                   unlock_value);
}

bool Menu::cinematic_unlocked(int index) const {
    if (index < 0 || index >= static_cast<int>(campaigns.size())) {
        return false;
    }
    const dino::Campaign& campaign =
        campaigns[static_cast<std::size_t>(index)];
    return dino::campaign_ending_unlocked(campaign, unlock_value) &&
           !campaign.ending_video.empty();
}

std::string Menu::cinematic_video() const {
    if (cinematic < 0 || cinematic >= static_cast<int>(campaigns.size())) {
        return std::string();
    }
    return campaigns[static_cast<std::size_t>(cinematic)].ending_video;
}

const dino::Campaign* Menu::selected_campaign() const {
    if (campaign < 0 || campaign >= static_cast<int>(campaigns.size())) {
        return nullptr;
    }
    return &campaigns[static_cast<std::size_t>(campaign)];
}

const dino::CampaignMission* Menu::selected_mission() const {
    const dino::Campaign* selected = selected_campaign();
    if (selected == nullptr || selected->missions.empty()) {
        return nullptr;
    }
    const auto stored = campaign_progress.find(selected->name);
    if (stored != campaign_progress.end()) {
        for (const dino::CampaignMission& mission : selected->missions) {
            if (mission.name == stored->second) {
                return &mission;
            }
        }
    }
    // No stored progress (or a stale mission id): start at the first mission.
    return &selected->missions.front();
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
        page = 8;
        loading = true;
        loading_timer = loading_time;
    } else if (name == "FREE_EASY_BUTTON") {
        difficulty = 0;
    } else if (name == "FREE_MIDDLE_BUTTON") {
        difficulty = 1;
    } else if (name == "FREE_HARD_BUTTON") {
        difficulty = 2;
    } else if (name == "ISLAND_LIST") {
        // Retail rows are LB_LINE_HEIGHT (34 px), not height/10: the list is
        // an 8-row viewport over 10 islands (MenuInterface.Ini).
        const int line =
            widget.list_line_height > 0 ? widget.list_line_height : 34;
        const int selected = line > 0 ? (y - widget.abs_y) / line : 0;
        island = std::clamp(selected, 0, 9);
    } else if (name == "CAMPAIGN_LIST") {
        const int line =
            widget.list_line_height > 0 ? widget.list_line_height : 34;
        const int selected = line > 0 ? (y - widget.abs_y) / line : 0;
        campaign = campaigns.empty()
                       ? 0
                       : std::clamp(selected, 0,
                                    static_cast<int>(campaigns.size()) - 1);
    } else if (name == "CAMPAIGN_START_BUTTON") {
        if (selected_campaign() != nullptr && campaign_unlocked(campaign)) {
            page = 3;
        }
    } else if (name == "CAMPAIGN_INFO_START_BUTTON") {
        if (selected_mission() != nullptr && campaign_unlocked(campaign)) {
            page = 8;
            loading = true;
            start_campaign = true;
            loading_timer = loading_time;
        }
    } else if (name == "LOAD_GAME_NAMES_LIST") {
        const int line =
            widget.list_line_height > 0 ? widget.list_line_height : 28;
        const int selected = line > 0 ? (y - widget.abs_y) / line : 0;
        save_slot = save_count > 0
                        ? std::clamp(selected, 0, save_count - 1)
                        : 0;
    } else if (name == "LOAD_LOAD_BUTTON") {
        if (save_count > 0 && save_slot >= 0 && save_slot < save_count) {
            load_requested = true;
        }
    } else if (name == "CINEMATICS_LIST") {
        const int line =
            widget.list_line_height > 0 ? widget.list_line_height : 34;
        const int selected = line > 0 ? (y - widget.abs_y) / line : 0;
        cinematic = campaigns.empty()
                        ? 0
                        : std::clamp(selected, 0,
                                     static_cast<int>(campaigns.size()) - 1);
    } else if (name == "CINEMATICS_START_BUTTON") {
        if (cinematic_unlocked(cinematic) && !cinematic_video().empty()) {
            cinematic_requested = true;
        }
    } else if (ends_with(name, "_CANCEL_BUTTON") ||
               ends_with(name, "_BACK_BUTTON")) {
        page = 0;
    }
    hover = -1;
}

std::string Menu::text_for(const Widget& widget) const {
    if (page == 3 && widget.name == "CAMPAIGN_INFO_TITLE") {
        const dino::Campaign* campaign_ptr = selected_campaign();
        if (campaign_ptr != nullptr && !campaign_ptr->title_id.empty()) {
            return resolve_loc_text(campaign_ptr->title_id, loc);
        }
    }
    return resolve_loc_text(widget.text, loc);
}

const char* Menu::background() const {
    return page >= 0 && page < 9 ? kBackgrounds[page] : kBackgrounds[0];
}

}  // namespace dino::ui
