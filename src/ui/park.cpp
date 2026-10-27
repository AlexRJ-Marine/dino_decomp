#include "park.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
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

// Building-category tabs inside CONSTRUCTION_BUILDINGS_PANEL. Each tab shows
// one CONSTRUC_*_GROUP; the group's buttons come from the `.bbi` tail names.
struct GroupBinding {
    const char* button;
    const char* group;
};

const GroupBinding kConstructionGroups[] = {
    {"CONSTRUCTION_BARAQUES_BUTTON", "CONSTRUC_BARAQUES_GROUP"},
    {"CONSTRUCTION_BUILDINGS_BUTTON", "CONSTRUC_RESTAURANT_GROUP"},
    {"CONSTRUCTION_SOUVENIRS_BUTTON", "CONSTRUC_SOUVENIRS_GROUP"},
    {"CONSTRUCTION_BLACKBOX_BUTTON", "CONSTRUC_BLACKBOX_GROUP"},
    {"CONSTRUCTION_FOUNTAIN_BUTTON", "CONSTRUC_FOUNTAIN_GROUP"},
    {"CONSTRUCTION_TREEDECO_BUTTON", "CONSTRUC_TREEDECO_GROUP"},
};

// Info-panel controller page 8 / state deactivation is reached through the
// panel's own close, cancel, validate or put-back button.
const Binding kClosers[] = {
    {"BUDGET_OUTCOME_OK", "BUDGET_PANEL"},
    {"VISITEURS_STATS_OUTCOME_OK", "VISITEURS_STATS_PANEL"},
    {"DINO_ACHAT_SAVE_OUTCOME_SAVE", "DINO_ACHAT_SAVE_PANEL"},
    {"DINO_ACHAT_SAVE_OUTCOME_CANCEL", "DINO_ACHAT_SAVE_PANEL"},
    {"OPTIONS_OPTIONS_VALIDATE_BUTTON", "OPTIONS_PANEL"},
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

void ParkHud::show_mission_panel() {
    open_panel("MISSION_PANEL");
}

void ParkHud::set_mission_verdicts(const std::vector<int>& verdicts) {
    const int count =
        std::min(5, static_cast<int>(verdicts.size()));
    for (int i = 1; i <= 5; ++i) {
        const std::string prefix = "OBJECTIF" + std::to_string(i) + "_PANEL";
        const bool has = i <= count;
        const bool success = has && verdicts[static_cast<std::size_t>(i - 1)] == 1;
        overlay.set_visible(prefix, has);
        overlay.set_visible(prefix + "_LIGHT_ON", success);
        overlay.set_visible(prefix + "_LIGHT_OFF", has && !success);
        overlay.set_visible(prefix + "_LIGHT2_ON", success);
        overlay.set_visible(prefix + "_LIGHT2_OFF", has && !success);
        overlay.set_visible("OBJECTIF_PANEL_LIGHT" + std::to_string(i) + "_ON",
                            success);
        overlay.set_visible("OBJECTIF_PANEL_LIGHT" + std::to_string(i) + "_OFF",
                            has && !success);
    }
    overlay.set_visible("OBJECTIF_PANEL_LIGHTS_GROUP", count > 0);
}

void ParkHud::clear_mission() {
    set_mission_verdicts({});
    close_panel("MISSION_PANEL");
}

void ParkHud::show_warning(const std::string& text) {
    // The dialog family is an interface-level overlay
    // (DIALOG_BACKGROUND is a ROOT child with ZPRIORITY 0.01, in front of the
    // panel roots). Retail does not deactivate the active panel, so the port
    // keeps it visible behind the dim backdrop.
    dialog_kind_ = DialogKind::warning;
    dialog_text_ = text;
    overlay.set_visible("REQUEST_PANEL", false);
    overlay.set_visible("CPGEND_PANEL", false);
    overlay.set_visible("WARNING_PANEL", true);
    overlay.set_visible("DIALOG_BACKGROUND", true);
}

void ParkHud::show_request(const std::string& text) {
    dialog_kind_ = DialogKind::request;
    dialog_text_ = text;
    overlay.set_visible("WARNING_PANEL", false);
    overlay.set_visible("CPGEND_PANEL", false);
    overlay.set_visible("REQUEST_PANEL", true);
    overlay.set_visible("DIALOG_BACKGROUND", true);
}

void ParkHud::show_campaign_end(const std::string& text) {
    dialog_kind_ = DialogKind::campaign_end;
    dialog_text_ = text;
    overlay.set_visible("WARNING_PANEL", false);
    overlay.set_visible("REQUEST_PANEL", false);
    overlay.set_visible("CPGEND_PANEL", true);
    overlay.set_visible("DIALOG_BACKGROUND", true);
}

void ParkHud::close_dialog() {
    dialog_kind_ = DialogKind::none;
    dialog_text_.clear();
    overlay.set_visible("WARNING_PANEL", false);
    overlay.set_visible("REQUEST_PANEL", false);
    overlay.set_visible("CPGEND_PANEL", false);
    overlay.set_visible("DIALOG_BACKGROUND", false);
}

void ParkHud::show_message(const std::string& text) {
    message_text_ = text;
    // `moMessage` keeps the toast on screen while the state runs; the exact
    // retail lifetime is a `moMessage` constant we have not extracted, so use
    // a fixed 4 s. The texts are short loc strings.
    message_remaining_ = text.empty() ? 0.0 : 4.0;
    update_message_lines();
    if (std::getenv("DINO_DEBUG_MESSAGE") != nullptr) {
        std::fprintf(stderr, "park message: %s\n", text.c_str());
    }
}

void ParkHud::update(double dt) {
    if (message_remaining_ <= 0.0) {
        return;
    }
    message_remaining_ -= dt;
    if (message_remaining_ <= 0.0) {
        clear_message();
    }
}

void ParkHud::clear_message() {
    message_text_.clear();
    message_remaining_ = 0.0;
    for (int line = 1; line <= 4; ++line) {
        if (Widget* widget =
                overlay.get("LINE" + std::to_string(line) + "_MESSAGE")) {
            widget->text.clear();
        }
    }
}

void ParkHud::update_message_lines() {
    // MESSAGE_PANEL is a 200x20 line per widget with FONT19, so wrap at ~26
    // characters. The retail class wraps by glyph width; the exact width is
    // not recovered yet, and all current warning strings fit on one line.
    std::vector<std::string> lines;
    std::string current;
    std::string word;
    auto flush_line = [&]() {
        lines.push_back(current);
        current.clear();
    };
    auto flush_word = [&]() {
        if (word.empty()) {
            return;
        }
        const std::string candidate =
            current.empty() ? word : current + " " + word;
        if (!current.empty() && candidate.size() > 26) {
            flush_line();
            current = word;
        } else {
            current = candidate;
        }
        word.clear();
    };
    for (const char ch : message_text_) {
        if (ch == '\n') {
            flush_word();
            flush_line();
        } else if (ch == ' ') {
            flush_word();
        } else {
            word.push_back(ch);
        }
    }
    flush_word();
    if (!current.empty()) {
        flush_line();
    }
    for (int line = 1; line <= 4; ++line) {
        if (Widget* widget =
                overlay.get("LINE" + std::to_string(line) + "_MESSAGE")) {
            const std::size_t index = static_cast<std::size_t>(line - 1);
            widget->text = index < lines.size() ? lines[index] : std::string();
        }
    }
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
    if (name == "CONSTRUCTION_BUILDINGS_PANEL") {
        for (const GroupBinding& group : kConstructionGroups) {
            overlay.set_visible(
                group.group,
                std::string(group.group) == "CONSTRUC_RESTAURANT_GROUP");
        }
    }
    if (name == "DINO_ACHAT_SAVE_PANEL") {
        const int count = static_cast<int>(store_rows_.size());
        if (count == 0) {
            store_selected_ = -1;
            store_scroll_ = 0;
        } else {
            if (store_selected_ < 0 || store_selected_ >= count) {
                store_selected_ = 0;
            }
            store_scroll_ = std::clamp(
                store_scroll_, 0,
                std::max(0, count - store_visible_rows()));
        }
    }
}

void ParkHud::close_panel(const std::string& name) {
    overlay.set_visible(name, false);
    if (name == "OPTIONS_PANEL") {
        name_edit_ = false;
    }
    if (is_lab_center(name)) {
        show_hud(true);
    }
}

void ParkHud::append_name_char(char c) {
    if (save_name_.size() >= 24) {
        return;
    }
    const unsigned char value = static_cast<unsigned char>(c);
    if (value < 32 || value > 126) {
        return;
    }
    save_name_.push_back(c);
}

void ParkHud::backspace_name() {
    if (!save_name_.empty()) {
        save_name_.pop_back();
    }
}

void ParkHud::set_store_rows(std::vector<StoreRow> rows) {
    store_rows_ = std::move(rows);
    store_scroll_ = 0;
    store_selected_ = store_rows_.empty() ? -1 : 0;
    update_store_gauges();
}

void ParkHud::update_store_gauges() {
    static const char* const kNames[11] = {
        "ORIGINALITY", "STAMINA", "COURAGE", "INTELLIGENCE", "AGILITY",
        "SPEED", "ARMOR", "AGGRESSIVENESS", "ATTACK", "BEAUTY",
        "PERCEPTION"};
    // `AttributeRange.ini`: every attribute is out of 100 except Attack (120).
    static constexpr int kMax[11] = {100, 100, 100, 100, 100, 100,
                                     100, 100, 120, 100, 100};
    const StoreRow* row =
        store_selected_ >= 0 &&
                store_selected_ < static_cast<int>(store_rows_.size())
            ? &store_rows_[static_cast<std::size_t>(store_selected_)]
            : nullptr;
    const bool debug =
        std::getenv("DINO_DEBUG_STORE_GAUGES") != nullptr;
    if (debug && row != nullptr) {
        std::printf("store gauges: %s", row->name.c_str());
    }
    for (int i = 0; i < 11; ++i) {
        const int value =
            row != nullptr
                ? row->attributes[static_cast<std::size_t>(i)]
                : 0;
        const std::string prefix =
            std::string("DINO_ACHAT_ATTRIBUTES_") + kNames[i];
        if (Widget* gauge = overlay.get(prefix + "_GAUGE")) {
            gauge->progress_max = kMax[i];
            gauge->progress_current =
                std::clamp(value, 0, gauge->progress_max);
        }
        if (Widget* indics = overlay.get(prefix + "_INDICS")) {
            indics->text =
                row != nullptr ? std::to_string(value) : std::string();
        }
        if (debug && row != nullptr) {
            std::printf(" %s=%d/%d", kNames[i], value, kMax[i]);
        }
    }
    if (debug && row != nullptr) {
        std::printf("\n");
    }
}

void ParkHud::store_select(int index) {
    if (index < 0 || index >= static_cast<int>(store_rows_.size())) {
        store_selected_ = -1;
        update_store_gauges();
        return;
    }
    store_selected_ = index;
    update_store_gauges();
    if (index < store_scroll_) {
        store_scroll_ = index;
    } else if (index >= store_scroll_ + store_visible_rows()) {
        store_scroll_ = index - store_visible_rows() + 1;
    }
}

void ParkHud::store_scroll_by(int delta) {
    const int max_scroll = std::max(
        0, static_cast<int>(store_rows_.size()) - store_visible_rows());
    store_scroll_ = std::clamp(store_scroll_ + delta, 0, max_scroll);
}

const StoreRow* ParkHud::store_visible_row(int slot) const {
    if (slot < 0 || slot >= store_visible_rows()) {
        return nullptr;
    }
    const int index = store_scroll_ + slot;
    if (index < 0 || index >= static_cast<int>(store_rows_.size())) {
        return nullptr;
    }
    return &store_rows_[static_cast<std::size_t>(index)];
}

int ParkHud::store_row_at(int x, int y) const {
    const int list_index = overlay.find("DINO_ACHAT_SAVE_NAMES_LIST");
    if (list_index < 0 || !overlay.visible_in_tree(list_index)) {
        return -1;
    }
    const Widget& list = overlay.widgets[static_cast<std::size_t>(list_index)];
    if (x < list.abs_x || x >= list.abs_x + list.width || y < list.abs_y ||
        y >= list.abs_y + list.height) {
        return -1;
    }
    const int pitch =
        std::max(1, list.height / store_visible_rows());
    const int slot = (y - list.abs_y) / pitch;
    if (slot < 0 || slot >= store_visible_rows()) {
        return -1;
    }
    const int index = store_scroll_ + slot;
    if (index < 0 || index >= static_cast<int>(store_rows_.size())) {
        return -1;
    }
    return index;
}

bool ParkHud::click(int x, int y, std::string& action) {
    action.clear();
    if (!loaded_) {
        return false;
    }
    const int hit = overlay.hit_test(x, y);
    if (hit < 0) {
        if (dialog_kind_ != DialogKind::none) {
            return true;
        }
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
    if (name != "OPTIONS_NAME_SLOT") {
        name_edit_ = false;
    }

    if (dialog_kind_ != DialogKind::none) {
        if (dialog_kind_ == DialogKind::warning &&
            name == "WARNING_OUTCOME_ICON") {
            close_dialog();
            action = "close warning";
        } else if (dialog_kind_ == DialogKind::request) {
            if (name == "REQUEST_VALIDATE_ICON") {
                close_dialog();
                action = "accept request";
            } else if (name == "REQUEST_CANCEL_ICON") {
                close_dialog();
                action = "cancel request";
            }
        } else if (dialog_kind_ == DialogKind::campaign_end &&
                   name == "CPGEND_OUTCOME_ICON") {
            close_dialog();
            action = "close campaign end";
        }
        return true;
    }

    if (name == "OPTIONS_MENU_SAVE_BUTTON" ||
        name == "OPTIONS_MENU_LOAD_BUTTON") {
        overlay.set_visible("OPTIONS_SAVE_PANEL", true);
        overlay.set_visible("OPTIONS_OPTIONS_PANEL", false);
        options_tab_ =
            name == "OPTIONS_MENU_SAVE_BUTTON" ? "save" : "load";
        save_selected_ = -1;
        action = name == "OPTIONS_MENU_SAVE_BUTTON" ? "options save tab"
                                                    : "options load tab";
        return true;
    }
    if (name == "OPTIONS_MENU_OPTIONS_BUTTON") {
        overlay.set_visible("OPTIONS_SAVE_PANEL", false);
        overlay.set_visible("OPTIONS_OPTIONS_PANEL", true);
        action = "options tab";
        return true;
    }
    if (name == "OPTIONS_NAME_SLOT") {
        if (options_tab_ == "save") {
            name_edit_ = true;
            action = "options name edit";
        }
        return true;
    }
    if (name == "OPTIONS_SAVE_NAMES_LIST") {
        const Widget& list = overlay.widgets[static_cast<std::size_t>(hit)];
        const int line =
            list.list_line_height > 0 ? list.list_line_height : 12;
        const int row = (y - list.abs_y) / line;
        if (row >= 0) {
            save_selected_ = row;
        }
        action = "options select " + std::to_string(save_selected_);
        return true;
    }
    if (name == "OPTIONS_VALIDATE_BUTTON") {
        action = options_tab_ == "load" ? "load port" : "save port";
        return true;
    }
    if (name == "OPTIONS_UNVALIDATE_BUTTON") {
        close_panel("OPTIONS_PANEL");
        action = "close OPTIONS_PANEL";
        return true;
    }

    // Dinosaur store panel: the list is a six-row viewport over the store
    // vector (0x00535460); SAVE/CANCEL follow the retail state 0x13 flow.
    if (name == "DINO_ACHAT_SAVE_NAMES_LIST") {
        const int list_index = overlay.find("DINO_ACHAT_SAVE_NAMES_LIST");
        if (list_index >= 0) {
            const Widget& list =
                overlay.widgets[static_cast<std::size_t>(list_index)];
            const int strip = list.list_scroll_width > 0
                                  ? list.list_scroll_width
                                  : 15;
            if (x >= list.abs_x + list.width - strip) {
                // The retail VSCROLL children have no INI geometry; use the
                // right strip as an up/down control.
                const bool up = y < list.abs_y + list.height / 2;
                store_scroll_by(up ? -1 : 1);
                action = up ? "store scroll up" : "store scroll down";
                return true;
            }
        }
        const int index = store_row_at(x, y);
        if (index >= 0) {
            store_select(index);
            action = "store select " + std::to_string(index);
        }
        return true;
    }
    if (name == "COMBOBOX1_LB_VS_BTN_UP" ||
        name == "DINO_ACHAT_SAVE_SCROLLUP") {
        store_scroll_by(-1);
        action = "store scroll up";
        return true;
    }
    if (name == "COMBOBOX1_LB_VS_BTN_DOWN" ||
        name == "DINO_ACHAT_SAVE_SCROLLDOWN") {
        store_scroll_by(1);
        action = "store scroll down";
        return true;
    }
    if (name == "DINO_ACHAT_SAVE_OUTCOME_SAVE") {
        // The budget gate stays in the panel on failure (0x00535600), so the
        // viewer performs it and closes the panel only on success.
        action = store_selected_ >= 0
                     ? "store buy " + std::to_string(store_selected_)
                     : "store no selection";
        return true;
    }
    if (name == "DINO_ACHAT_SAVE_OUTCOME_CANCEL") {
        store_selected_ = -1;
        close_panel("DINO_ACHAT_SAVE_PANEL");
        action = "close DINO_ACHAT_SAVE_PANEL";
        return true;
    }

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

    // NAVIGATION_PANEL drives the park camera, not the minimap (0x5257a0).
    if (name == "NAVIGATION_GOTOP") {
        action = "camera pan forward";
        return true;
    }
    if (name == "NAVIGATION_GOBOTTOM") {
        action = "camera pan back";
        return true;
    }
    if (name == "NAVIGATION_GOLEFT") {
        action = "camera pan left";
        return true;
    }
    if (name == "NAVIGATION_GORIGHT") {
        action = "camera pan right";
        return true;
    }
    if (name == "NAVIGATION_TURNLEFT") {
        action = "camera turn left";
        return true;
    }
    if (name == "NAVIGATION_TURNRIGHT") {
        action = "camera turn right";
        return true;
    }
    if (name == "NAVIGATION_ZOOM") {
        action = "camera zoom in";
        return true;
    }
    if (name == "NAVIGATION_DEZOOM") {
        action = "camera zoom out";
        return true;
    }

    if (name == "MAP_PANEL_ZOOMIN_BUTTON") {
        action = "minimap zoom in";
        return true;
    }
    if (name == "MAP_PANEL_ZOOMOUT_BUTTON") {
        action = "minimap zoom out";
        return true;
    }

    for (const GroupBinding& group : kConstructionGroups) {
        if (name == group.button) {
            for (const GroupBinding& other : kConstructionGroups) {
                overlay.set_visible(other.group, other.group == group.group);
            }
            action = std::string("construction group ") + group.group;
            return true;
        }
    }

    // Building pick buttons. Each `.bbi` tail lists its own CONSTRUC_*_BUTTON
    // widget; the viewer maps that name back to the BuildingList.ini index.
    if (name.rfind("CONSTRUC_", 0) == 0 && name.size() > 8 &&
        name.compare(name.size() - 7, 7, "_BUTTON") == 0) {
        action = "construct " + name;
        return true;
    }

    // Any other HUD button (panel internals, lists) is consumed but has no
    // port behaviour yet.
    return true;
}

}  // namespace dino::ui
