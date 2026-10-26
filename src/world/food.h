#pragma once

#include <string>
#include <vector>

namespace dino {

// One placed food source (a `.bsi` definition instance). Coordinates are raw
// world units (0..world_size), like the fence rectangles.
struct FoodSource {
    int def = 0;
    float x = 0.0f;
    float y = 0.0f;
    float remaining = 0.0f;
};

// `.bsi` food-source definition plus the resolved render assets.
struct FoodDef {
    int kind = 0;
    std::string name;     // FoodSourceList.ini entry
    std::string mesh;     // asset[0], forward slashes
    std::string texture;  // resolved TGA (empty when not found)
    float nutrition = 0.0f;
    float quantity = 0.0f;
    int price = 0;
};

// Kind ids 0..4 are vegetarian, 5..9 carnivorous (the `.bsi` UI buttons and
// the `.bni` animal kinds agree).
bool food_is_carnivore(int kind);

// Reads FoodSourceList.ini and every `<name>.bsi` in `foodsource_dir`
// (relative to `asset_root`, or absolute). Texture candidates are resolved
// under `asset_root`.
std::vector<FoodDef> load_food_defs(const std::string& asset_root,
                                   const std::string& foodsource_dir);

}  // namespace dino
