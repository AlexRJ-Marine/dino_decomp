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
        if (widget.type != "BUTTON" && widget.type != "LIST" &&
            widget.type != "HSCROLL") {
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

int Menu::list_visible_rows(const std::string& name) const {
    const Widget* widget = overlay.get(name);
    if (widget == nullptr) {
        return 0;
    }
    const int line =
        widget->list_line_height > 0 ? widget->list_line_height : 34;
    return line > 0 ? widget->height / line : 0;
}

int Menu::list_total_rows(const std::string& name) const {
    if (name == "ISLAND_LIST") {
        return 10;
    }
    if (name == "CAMPAIGN_LIST" || name == "CINEMATICS_LIST") {
        return static_cast<int>(campaigns.size());
    }
    if (name == "LOAD_GAME_NAMES_LIST") {
        return save_count;
    }
    return 0;
}

int Menu::list_first_row(const std::string& name) const {
    if (name == "ISLAND_LIST") {
        return island_first;
    }
    if (name == "CAMPAIGN_LIST") {
        return campaign_first;
    }
    if (name == "LOAD_GAME_NAMES_LIST") {
        return save_first;
    }
    if (name == "CINEMATICS_LIST") {
        return cinematic_first;
    }
    return 0;
}

void Menu::list_set_first_row(const std::string& name, int first) {
    const int visible = list_visible_rows(name);
    const int total = list_total_rows(name);
    first = std::clamp(first, 0, std::max(0, total - visible));
    if (name == "ISLAND_LIST") {
        island_first = first;
    } else if (name == "CAMPAIGN_LIST") {
        campaign_first = first;
    } else if (name == "LOAD_GAME_NAMES_LIST") {
        save_first = first;
    } else if (name == "CINEMATICS_LIST") {
        cinematic_first = first;
    }
}

void Menu::list_scroll(const std::string& name, int delta) {
    list_set_first_row(name, list_first_row(name) + delta);
}

void Menu::list_ensure_visible(const std::string& name, int row) {
    const int visible = list_visible_rows(name);
    if (visible <= 0) {
        return;
    }
    const int first = list_first_row(name);
    if (row < first) {
        list_set_first_row(name, row);
    } else if (row >= first + visible) {
        list_set_first_row(name, row - visible + 1);
    }
}

ListScrollGeometry Menu::list_scroll_geometry(const std::string& name) const {
    ListScrollGeometry g;
    const Widget* widget = overlay.get(name);
    if (widget == nullptr || widget->list_scroll_width <= 0) {
        return g;
    }
    g.width = widget->list_scroll_width;
    g.x = widget->abs_x + widget->width - g.width;
    g.y = widget->abs_y;
    g.height = widget->height;
    g.arrow = g.width;
    const int visible = list_visible_rows(name);
    const int total = list_total_rows(name);
    g.range = std::max(1, total - visible + 1);
    if (g.range <= 1) {
        g.thumb_y = g.y + g.arrow;
        g.thumb_h = std::max(0, g.height - 2 * g.arrow);
        g.travel = 0;
    } else {
        // Retail 0x5c4720: max(5, H - (U + D + 5) - 3*range).
        g.thumb_h = std::max(5, g.height - (2 * g.arrow + 5) - 3 * g.range);
        g.travel = std::max(0, g.height - 2 * g.arrow - g.thumb_h);
        g.thumb_y = g.y + g.arrow +
                    list_first_row(name) * g.travel / (g.range - 1);
    }
    return g;
}

void Menu::list_drag_to(const std::string& name, int y) {
    const ListScrollGeometry g = list_scroll_geometry(name);
    if (g.range <= 1 || g.travel <= 0) {
        return;
    }
    int pos = (y - (g.y + g.arrow)) * g.range / g.travel;
    list_set_first_row(name, std::clamp(pos, 0, g.range - 1));
}

void Menu::options_sync_positions() {
    auto set_pos = [&](const char* name, int value) {
        Widget* widget = overlay.get(name);
        if (widget == nullptr) {
            return;
        }
        const int range = widget->hs_range > 0 ? widget->hs_range : 32;
        widget->hs_position = std::clamp(
            (value * range + 50) / 100, 0, range - 1);
    };
    set_pos("OPTIONS_MUSIC_HSCROLL", music_volume);
    set_pos("OPTIONS_SOUNDS_HSCROLL", sfx_volume);
    set_pos("OPTIONS_GAME_SPEED_HSCROLL", game_speed);
}

void Menu::options_cycle(const std::string& name) {
    if (name == "OPTIONS_GRAPH_QUALITY_INDICATION") {
        // Retail 0x519640: v = (v == 0) ? 3 : v - 1.
        lod_quality = lod_quality == 0 ? 3 : lod_quality - 1;
    } else if (name == "OPTIONS_GRAPH_TEXTURE_QUALITY_INDICATION") {
        texture_quality = texture_quality == 0 ? 3 : texture_quality - 1;
    } else if (name == "OPTIONS_SCREEN_SIZE_INDICATION") {
        if (!screen_modes.empty()) {
            screen_mode = (screen_mode + 1) %
                          static_cast<int>(screen_modes.size());
            screen_mode_changed = true;
        }
    } else if (name == "OPTIONS_GRAPH_MIXING_INDICATION") {
        hardware_tnl = hardware_tnl != 0 ? 0 : 1;
    } else if (name == "OPTIONS_MUSIC_MIXING_INDICATION") {
        hardware_sound = hardware_sound != 0 ? 0 : 1;
    } else if (name == "OPTIONS_GRAPH_TEXTURE_DEPTH_INDICATION") {
        texture_depth = texture_depth == 16 ? 32 : 16;
    }
}

std::string Menu::option_text(const std::string& name) const {
    auto quality = [](int value) -> const char* {
        switch (value) {
            case 0: return "High";
            case 1: return "Medium";
            case 2: return "Low";
            default: return "Auto";
        }
    };
    if (name == "OPTIONS_GRAPH_QUALITY_INDICATION") {
        return quality(lod_quality);
    }
    if (name == "OPTIONS_GRAPH_TEXTURE_QUALITY_INDICATION") {
        return quality(texture_quality);
    }
    if (name == "OPTIONS_SCREEN_SIZE_INDICATION") {
        if (!screen_modes.empty()) {
            return screen_modes[static_cast<std::size_t>(std::clamp(
                screen_mode, 0, static_cast<int>(screen_modes.size()) - 1))];
        }
        return "800x600x16";
    }
    if (name == "OPTIONS_GRAPH_MIXING_INDICATION") {
        return hardware_tnl != 0 ? "Hardware" : "Software";
    }
    if (name == "OPTIONS_MUSIC_MIXING_INDICATION") {
        return hardware_sound != 0 ? "Hardware" : "Software";
    }
    if (name == "OPTIONS_GRAPH_TEXTURE_DEPTH_INDICATION") {
        return texture_depth == 32 ? "32 bits" : "16 bits";
    }
    return std::string();
}

void Menu::slider_step(const std::string& name, int delta) {
    Widget* widget = overlay.get(name);
    if (widget == nullptr) {
        return;
    }
    const int range = widget->hs_range > 0 ? widget->hs_range : 32;
    widget->hs_position =
        std::clamp(widget->hs_position + delta, 0, range - 1);
    const int value = (widget->hs_position * 100 + range / 2) / range;
    if (name == "OPTIONS_MUSIC_HSCROLL") {
        music_volume = value;
    } else if (name == "OPTIONS_SOUNDS_HSCROLL") {
        sfx_volume = value;
    } else if (name == "OPTIONS_GAME_SPEED_HSCROLL") {
        game_speed = value;
        game_speed_dirty = true;
    }
}

void Menu::slider_drag_to(const std::string& name, int x) {
    Widget* widget = overlay.get(name);
    if (widget == nullptr) {
        return;
    }
    const int range = widget->hs_range > 0 ? widget->hs_range : 32;
    if (range <= 1 || widget->width <= 0) {
        return;
    }
    const int arrow = widget->height;
    const int thumb_w =
        range <= 1
            ? std::max(0, widget->width - 2 * arrow)
            : std::max(5, widget->width - (2 * arrow + 5) - 3 * range);
    const int travel = std::max(0, widget->width - 2 * arrow - thumb_w);
    if (travel <= 0) {
        return;
    }
    const int pos =
        (x - (widget->abs_x + arrow)) * range / travel;
    widget->hs_position = std::clamp(pos, 0, range - 1);
    const int value = (widget->hs_position * 100 + range / 2) / range;
    if (name == "OPTIONS_MUSIC_HSCROLL") {
        music_volume = value;
    } else if (name == "OPTIONS_SOUNDS_HSCROLL") {
        sfx_volume = value;
    } else if (name == "OPTIONS_GAME_SPEED_HSCROLL") {
        game_speed = value;
        game_speed_dirty = true;
    }
}

void Menu::click(int x, int y) {
    const int hit = hit_test(x, y);
    if (hit < 0) {
        return;
    }
    const Widget& widget = overlay.widgets[static_cast<std::size_t>(hit)];
    const std::string& name = widget.name;

    // Retail HSCROLL (0x5be7e0): left/right arrows -1/+1, track -3/+3,
    // absolute thumb drag, wheel one notch. Options sliders apply live.
    if (widget.type == "HSCROLL") {
        const int range = widget.hs_range > 0 ? widget.hs_range : 32;
        const int arrow = widget.height;
        const int thumb_w =
            range <= 1
                ? std::max(0, widget.width - 2 * arrow)
                : std::max(5, widget.width - (2 * arrow + 5) - 3 * range);
        const int travel =
            std::max(0, widget.width - 2 * arrow - thumb_w);
        const int pos = std::clamp(widget.hs_position, 0, range - 1);
        const int thumb_x =
            widget.abs_x + arrow +
            (range > 1 && travel > 0 ? pos * travel / (range - 1) : 0);
        if (x < widget.abs_x + arrow) {
            slider_step(name, -1);
        } else if (x >= widget.abs_x + widget.width - arrow) {
            slider_step(name, 1);
        } else if (x >= thumb_x && x < thumb_x + thumb_w) {
            slider_drag_active = true;
            slider_drag_name = name;
        } else if (x < thumb_x) {
            slider_step(name, -3);
        } else {
            slider_step(name, 3);
        }
        hover = -1;
        return;
    }

    // Retail VSCROLL (0x5c4650): the list's right strip. Up/down = 1 line,
    // track above/below the thumb = 3, thumb press starts a drag (the viewer
    // feeds motion/up), wheel = 1 line per notch.
    if (widget.type == "LIST" && widget.list_scroll_width > 0) {
        const ListScrollGeometry scroll = list_scroll_geometry(name);
        if (x >= scroll.x && x < scroll.x + scroll.width &&
            y >= scroll.y && y < scroll.y + scroll.height) {
            if (y < scroll.y + scroll.arrow) {
                list_scroll(name, -1);
            } else if (y >= scroll.y + scroll.height - scroll.arrow) {
                list_scroll(name, 1);
            } else if (y >= scroll.thumb_y &&
                       y < scroll.thumb_y + scroll.thumb_h) {
                scroll_drag_active = true;
                scroll_drag_list = name;
            } else if (y < scroll.thumb_y) {
                list_scroll(name, -3);
            } else {
                list_scroll(name, 3);
            }
            hover = -1;
            return;
        }
    }

    // `firstLine + (y - list.y) / lineHeight`, rejected past the visible rows
    // (0x5bdb10).
    auto pick_row = [&](const char* list_name, int fallback_line) {
        const int line = widget.list_line_height > 0
                             ? widget.list_line_height
                             : fallback_line;
        const int first = list_first_row(list_name);
        const int visible = list_visible_rows(list_name);
        const int row =
            line > 0 ? first + (y - widget.abs_y) / line : first;
        if (row < first || row >= first + visible) {
            return -1;
        }
        return row;
    };

    if (name == "FREE_GAME_BUTTON") {
        page = 1;
        island_first = 0;
    } else if (name == "CAMPAIGN_BUTTON") {
        page = 2;
        campaign_first = 0;
    } else if (name == "LOAD_GAME_BUTTON") {
        page = 4;
        save_first = 0;
    } else if (name == "OPTIONS_BUTTON") {
        page = 5;
    } else if (name == "CINEMATICS_BUTTON") {
        page = 6;
        cinematic_first = 0;
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
        const int selected = pick_row("ISLAND_LIST", 34);
        if (selected >= 0) {
            island = std::clamp(selected, 0, 9);
            list_ensure_visible("ISLAND_LIST", island);
        }
    } else if (name == "CAMPAIGN_LIST") {
        const int selected = pick_row("CAMPAIGN_LIST", 34);
        if (selected >= 0) {
            campaign =
                campaigns.empty()
                    ? 0
                    : std::clamp(selected, 0,
                                 static_cast<int>(campaigns.size()) - 1);
            list_ensure_visible("CAMPAIGN_LIST", campaign);
        }
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
        const int selected = pick_row("LOAD_GAME_NAMES_LIST", 20);
        if (selected >= 0 && save_count > 0) {
            save_slot = std::clamp(selected, 0, save_count - 1);
            list_ensure_visible("LOAD_GAME_NAMES_LIST", save_slot);
        }
    } else if (name == "LOAD_LOAD_BUTTON") {
        if (save_count > 0 && save_slot >= 0 && save_slot < save_count) {
            load_requested = true;
        }
    } else if (name == "CINEMATICS_LIST") {
        const int selected = pick_row("CINEMATICS_LIST", 34);
        if (selected >= 0) {
            cinematic =
                campaigns.empty()
                    ? 0
                    : std::clamp(selected, 0,
                                 static_cast<int>(campaigns.size()) - 1);
            list_ensure_visible("CINEMATICS_LIST", cinematic);
        }
    } else if (name == "CINEMATICS_START_BUTTON") {
        if (cinematic_unlocked(cinematic) && !cinematic_video().empty()) {
            cinematic_requested = true;
        }
    } else if (name == "OPTIONS_GRAPH_QUALITY_INDICATION" ||
               name == "OPTIONS_GRAPH_TEXTURE_QUALITY_INDICATION" ||
               name == "OPTIONS_SCREEN_SIZE_INDICATION" ||
               name == "OPTIONS_GRAPH_MIXING_INDICATION" ||
               name == "OPTIONS_MUSIC_MIXING_INDICATION" ||
               name == "OPTIONS_GRAPH_TEXTURE_DEPTH_INDICATION") {
        options_cycle(name);
    } else if (name == "OPTIONS_OK_BUTTON" ||
               name == "OPTIONS_CANCEL_BUTTON") {
        // Retail binds both to the same close pointer: no apply/revert.
        page = 0;
    } else if (ends_with(name, "_CANCEL_BUTTON") ||
               ends_with(name, "_BACK_BUTTON")) {
        page = 0;
    }
    hover = -1;
}

std::string Menu::text_for(const Widget& widget) const {
    const std::string option = option_text(widget.name);
    if (!option.empty()) {
        return option;
    }
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
