#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../formats/campaign.h"
#include "../formats/loc.h"
#include "overlay.h"

namespace dino::ui {

// Retail VSCROLL geometry, derived per list (the INI `[.._VS]` sections have
// no geometry; LIST::CreateChildren builds `LbVs` from `LB_VSCROLL_WIDTH`).
struct ListScrollGeometry {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int arrow = 0;    // U = D = scrollbar width
    int range = 1;    // number of positions: max(1, total - visible + 1)
    int thumb_y = 0;
    int thumb_h = 0;
    int travel = 0;
};

// Retail menu state machine (page index 0..8) over the parsed overlay data.
struct Menu {
    Overlay overlay;
    dino::Loc loc;
    bool loaded = false;
    bool active = true;   // false once a game is started or quit
    bool quit = false;
    int page = 0;
    int hover = -1;
    int island = 0;       // selected island index 0..9
    int island_first = 0; // first visible ISLAND_LIST row
    int campaign = 0;     // selected campaign row (CampaignList.ini order)
    int campaign_first = 0;
    int save_slot = 0;    // selected LOAD_GAME_NAMES_LIST row
    int save_first = 0;
    int save_count = 0;   // number of save slots (set by the viewer)
    bool load_requested = false;  // LOAD_LOAD_BUTTON pressed
    int cinematic = 0;    // selected CINEMATICS_LIST row (campaign order)
    int cinematic_first = 0;
    bool cinematic_requested = false;  // CINEMATICS_START_BUTTON pressed
    int difficulty = 1;   // 0 easy, 1 middle, 2 hard
    bool loading = false;
    bool start_campaign = false;  // CAMPAIGN_INFO_START_BUTTON was pressed
    float loading_timer = 0.0f;
    float loading_time = 0.8f;
    std::string error;
    std::string campaign_error;
    std::vector<dino::Campaign> campaigns;
    // campaign folder -> current mission name (port-owned progress file).
    std::map<std::string, std::string> campaign_progress;
    // `run/save/dino.dat` unlock value (campaign count; 1 = Cpg01 only).
    // Missing file = 1.
    std::uint32_t unlock_value = 1;

    bool load(const std::string& asset_root);
    bool load_campaigns(const std::string& scenario_root,
                        std::uint32_t unlock_value = 1);
    int hit_test(int x, int y) const;
    void move(int x, int y);
    void click(int x, int y);
    void escape();

    // Retail LIST viewport: rows are `LB_LINE_HEIGHT` tall and the list
    // shows `height / line_height` of them. `*_first` is the first visible
    // row; the scrollbar arrows/track move it.
    int list_visible_rows(const std::string& name) const;
    int list_total_rows(const std::string& name) const;
    int list_first_row(const std::string& name) const;
    void list_set_first_row(const std::string& name, int first);
    void list_scroll(const std::string& name, int delta);
    void list_ensure_visible(const std::string& name, int row);
    ListScrollGeometry list_scroll_geometry(const std::string& name) const;
    // Thumb drag (retail 0x5cbb5d): pos = (y - (up.y+up.h)) * range / travel.
    void list_drag_to(const std::string& name, int y);
    bool scroll_drag_active = false;
    std::string scroll_drag_list;

    const dino::Campaign* selected_campaign() const;
    const dino::CampaignMission* selected_mission() const;
    bool campaign_unlocked(int index) const;
    // Cinematics rows are ending videos: `row < unlock_value - 1` (retail
    // FUN_00518790) and the campaign must have an `EndingVideo`.
    bool cinematic_unlocked(int index) const;
    // `[BRIEFING] EndingVideo` of the selected cinematics row ("" when the
    // campaign has no ending clip, e.g. Cpg06/Cpg08).
    std::string cinematic_video() const;

    std::string text_for(const Widget& widget) const;
    const char* background() const;
    bool widget_visible(int index) const {
        return overlay.visible_on_page(index, page);
    }
};

}  // namespace dino::ui
