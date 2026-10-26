#pragma once

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

    Overlay overlay;
    Loc loc;

private:
    void open_panel(const std::string& name);
    void close_panel(const std::string& name);
    void hide_panels();
    void show_hud(bool visible);

    bool loaded_ = false;
    bool hud_hidden_ = false;
    DialogKind dialog_kind_ = DialogKind::none;
    std::string dialog_text_;
    std::string options_tab_ = "save";
    int save_selected_ = -1;
    std::string save_name_ = "quicksave";
    bool name_edit_ = false;
    std::vector<StoreRow> store_rows_;
    int store_selected_ = -1;
    int store_scroll_ = 0;
};

}  // namespace dino::ui
