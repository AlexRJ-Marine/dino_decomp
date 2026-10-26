#pragma once

#include <string>
#include <vector>

#include "../formats/campaign.h"
#include "../formats/loc.h"
#include "overlay.h"

namespace dino::ui {

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
    int campaign = 0;     // selected campaign row (CampaignList.ini order)
    int difficulty = 1;   // 0 easy, 1 middle, 2 hard
    bool loading = false;
    bool start_campaign = false;  // CAMPAIGN_INFO_START_BUTTON was pressed
    float loading_timer = 0.0f;
    float loading_time = 0.8f;
    std::string error;
    std::string campaign_error;
    std::vector<dino::Campaign> campaigns;

    bool load(const std::string& asset_root);
    bool load_campaigns(const std::string& scenario_root);
    int hit_test(int x, int y) const;
    void move(int x, int y);
    void click(int x, int y);
    void escape();

    const dino::Campaign* selected_campaign() const;
    const dino::CampaignMission* selected_mission() const;

    std::string text_for(const Widget& widget) const;
    const char* background() const;
    bool widget_visible(int index) const {
        return overlay.visible_on_page(index, page);
    }
};

}  // namespace dino::ui
