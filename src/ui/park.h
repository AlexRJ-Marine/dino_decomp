#pragma once

#include <string>

#include "../formats/loc.h"
#include "overlay.h"

namespace dino::ui {

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
    bool load(const std::string& interface_path, const std::string& db_path,
              const std::string& loc_path, std::string& error);
    bool loaded() const { return loaded_; }
    bool hud_hidden() const { return hud_hidden_; }
    bool is_visible(const std::string& root) const;

    // Handles a click at design-space (x, y). `action` receives a short
    // description such as "open BUDGET_PANEL"; returns true when the click
    // belongs to the HUD and must not reach the camera/build tools.
    bool click(int x, int y, std::string& action);

    Overlay overlay;
    Loc loc;

private:
    void open_panel(const std::string& name);
    void close_panel(const std::string& name);
    void hide_panels();
    void show_hud(bool visible);

    bool loaded_ = false;
    bool hud_hidden_ = false;
};

}  // namespace dino::ui
