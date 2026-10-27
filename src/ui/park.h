#pragma once

#include <array>
#include <string>
#include <vector>

#include "../formats/loc.h"
#include "overlay.h"

namespace dino::ui {

// One row of the dinosaur store panel (`DINO_ACHAT_SAVE_NAMES_LIST`).
// The retail list shows six rows of `name`/`price`; the viewer feeds the
// entries from Market.ini / the mission DinoStore.
struct StoreRow {
    std::string name;
    int price = 0;
    // Gauge order matches the retail widget row:
    // ORIGINALITY/STAMINA/COURAGE/INTELLIGENCE/AGILITY/SPEED/ARMOR/
    // AGGRESSIVENESS/ATTACK/BEAUTY/PERCEPTION.
    std::array<int, 11> attributes{};
    std::string genotype;  // Market.ini `.gen` file for the 3D preview
};

// One row of `PREDEFINED_SHOW_LIST` (mission `[SHOWS]` catalog). Retail
// renders seven columns from `DUMMY_PREDEFINED_SHOW_LIST`: name, judge icon,
// price and the four cage-size icons. The port draws the same values as text
// because it has no icon atlas for the judge/cage textures.
struct ShowRow {
    std::string title;  // resolved `.shw` Name
    std::string stem;   // `.shw` file stem, the viewer's action key
    int judge_kind = 0;
    int price = 0;
    std::array<int, 4> cage_size{};
};

// In-park HUD controller: owns the ParkInterface.Ini tree and implements the
// panel switching bound by the original FUN_00527d30. Rendering stays in the
// viewer because it needs the GL texture cache.
//
// The original is state-based and strictly one panel at a time:
// - construction states 0xC..0x11 show exactly one construction panel;
// - info-panel controller pages 0..7 share one active panel slot;
// - lab states 0..4 show one lab centre (each has its own 800x600 backdrop);
// - the always-on HUD roots stay visible except inside the lab.
// DIALOG_BACKGROUND belongs to WARNING/REQUEST/CPGEND dialogs and is not used
// by any of these panels (evidence: notes/ui-recon.md).
class ParkHud {
public:
    enum class DialogKind { none, warning, request, campaign_end };

    bool load(const std::string& interface_path, const std::string& db_path,
              const std::string& loc_path, std::string& error);
    bool loaded() const { return loaded_; }
    bool hud_hidden() const { return hud_hidden_; }
    bool is_visible(const std::string& root) const;

    // Modal DIALOG_BACKGROUND family: WARNING_PANEL, REQUEST_PANEL and
    // CPGEND_PANEL. The text is resolved by the caller; the port only renders.
    void show_warning(const std::string& text);
    void show_request(const std::string& text);
    void show_campaign_end(const std::string& text);
    void close_dialog();
    bool dialog_open() const { return dialog_kind_ != DialogKind::none; }
    DialogKind dialog_kind() const { return dialog_kind_; }
    const std::string& dialog_text() const { return dialog_text_; }
    const std::string& warning_text() const { return dialog_text_; }

    // Transient MESSAGE_PANEL toast (`moMessage`: FUN_0052bed0 and the fence
    // insertion message at 0x00536017 call FUN_005e3d70). Retail uses it for
    // purchase/build failures and keeps the active panel (state 0x13 stays
    // open). Text wraps onto LINE1..LINE4_MESSAGE and expires; `dt` is seconds.
    void show_message(const std::string& text);
    void update(double dt);
    const std::string& message_text() const { return message_text_; }

    // Handles a click at design-space (x, y). `action` receives a short
    // description such as "open BUDGET_PANEL"; returns true when the click
    // belongs to the HUD and must not reach the camera/build tools.
    bool click(int x, int y, std::string& action);

    // Options Save/Load tab state: "save" or "load", and the selected list
    // row (-1 when none). The viewer owns the actual save catalog.
    const std::string& options_tab() const { return options_tab_; }
    int save_selected() const { return save_selected_; }

    // Options Save: the editable root name. Clicking OPTIONS_NAME_SLOT starts
    // text entry; the viewer feeds SDL text input into these helpers.
    const std::string& save_name() const { return save_name_; }
    void set_save_name(const std::string& name) { save_name_ = name; }
    bool name_edit_active() const { return name_edit_; }
    void end_name_edit() { name_edit_ = false; }
    void append_name_char(char c);
    void backspace_name();

    // Dinosaur store panel (`DINO_ACHAT_SAVE_PANEL`). The retail state fills
    // six rows from the store vector (0x00535460) and refreshes the detail
    // pane from the selected index (0x00535710).
    void set_store_rows(std::vector<StoreRow> rows);
    const std::vector<StoreRow>& store_rows() const { return store_rows_; }
    void store_select(int index);
    int store_selected() const { return store_selected_; }
    int store_scroll() const { return store_scroll_; }
    void store_scroll_by(int delta);
    // Six visible slots; null past the end.
    const StoreRow* store_visible_row(int slot) const;
    static constexpr int store_visible_rows() { return 6; }
    int store_row_at(int x, int y) const;

    // Mission panel (`MISSION_PANEL`) and the objective ON/OFF lights.
    // `verdicts` uses 0 pending / 1 success / 2 failure (ObjectiveVerdict).
    void show_mission_panel();
    void set_mission_verdicts(const std::vector<int>& verdicts);
    void clear_mission();

    // Show construction (`PREDEFINED_SHOW_PANEL`): the mission `[SHOWS]`
    // catalog fills `PREDEFINED_SHOW_LIST`; VALIDATE arms the viewer's show
    // placement tool with the selected row (`FUN_0052d1e0` computes the
    // footprint and switches the controller state). The list is 365 px tall
    // with 30 px rows (`LB_LINE_HEIGHT`), so twelve rows are visible.
    void set_show_rows(std::vector<ShowRow> rows);
    const std::vector<ShowRow>& show_rows() const { return show_rows_; }
    void show_select(int index);
    int show_selected() const { return show_selected_; }
    int show_scroll() const { return show_scroll_; }
    void show_scroll_by(int delta);
    const ShowRow* show_visible_row(int slot) const;
    static constexpr int show_visible_rows() { return 12; }
    int show_row_at(int x, int y) const;

    Overlay overlay;
    Loc loc;

private:
    void open_panel(const std::string& name);
    void close_panel(const std::string& name);
    void hide_panels();
    void show_hud(bool visible);
    void update_store_gauges();
    void update_message_lines();
    void clear_message();

    bool loaded_ = false;
    bool hud_hidden_ = false;
    DialogKind dialog_kind_ = DialogKind::none;
    std::string dialog_text_;
    std::string message_text_;
    double message_remaining_ = 0.0;
    std::string options_tab_ = "save";
    int save_selected_ = -1;
    std::string save_name_ = "quicksave";
    bool name_edit_ = false;
    std::vector<StoreRow> store_rows_;
    int store_selected_ = -1;
    int store_scroll_ = 0;
    std::vector<ShowRow> show_rows_;
    int show_selected_ = -1;
    int show_scroll_ = 0;
};

}  // namespace dino::ui
