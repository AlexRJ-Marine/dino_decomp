#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dino {

// One retail `.bfi` show feature (`Data/Design/attractions/features`).
// The body is a fixed sequential stream; see docs/formats/bfi.md.
struct BfiFeature {
    std::string name;
    int width = 0;
    int height = 0;
    float vertical = 0.0f;
    int variant = 0;
    int price = 0;
    // `.bfi` `display_type` at 0x21: 0 mesh prop/judge, 1 cage, 3 ground tile.
    int display_type = 0;
    std::string class_tag;
    int judge_kind = 0;
    int food_kind = 0;
    // `field_3c`/`field_40`: the feeder quantity and nutrition (LunchBox
    // 10000/100, StaticBait 10000/70). The runtime saFoodSource uses
    // `+8` = quantity and `+0xc` = {nutrition, capacity}.
    int food_quantity = 0;
    int food_nutrition = 0;
    // The feature's action list (`Eat`/`EatForSmallies` for LunchBox,
    // `EatBait*` for StaticBait); the item behavior's nodes.
    std::vector<std::string> actions;
    int state_count = 0;
    std::vector<std::string> mesh_slots;  // 8
    std::string texture_dir;
    std::vector<std::string> effects;  // 7; effects[5] is the judge arena
    int category = 0;
    std::string switch_key;
};

BfiFeature load_bfi(const std::string& path);

// Reads every `*.bfi` in `dir`, sorted by file name.
std::vector<BfiFeature> load_bfi_dir(const std::string& dir);

// Reads `FeatureList.ini` order (the index used by show waypoints) and loads
// each named `.bfi` from `dir`.
std::vector<BfiFeature> load_bfi_feature_list(const std::string& dir);

}  // namespace dino
