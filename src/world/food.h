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

// A `.bfi` LunchBox/StaticBait feeder (`saFoodSource` created by the item
// actor). `quantity` is `+0x08`, `max_quantity` the capacity (`+0xc[1]`),
// `nutrition` `+0xc[0]`, `state` `+0x10` (0 full, 1 = 2/3, 2 = 1/3,
// 3 = empty).
struct Feeder {
    std::string name;
    int kind = 0;
    float x = 0.0f;
    float y = 0.0f;
    // `.bfi` footprint half-size (width/2); used for the draw/footprint
    // radius. The Eat proximity gate does **not** use it: the retail
    // object executor scales the `.bai` element radius by the dino's body
    // part length or size radius (`eat_arrival_radius`).
    float radius = 2.0f;
    float quantity = 0.0f;
    float max_quantity = 0.0f;
    float nutrition = 0.0f;
    int state = 0;
};

// `FUN_00576e50`: clamp the amount to the remaining quantity, subtract, set
// the 2/3 and 1/3 state thresholds, and return `amount * nutrition`.
float feeder_drain(Feeder& feeder, float amount);

// Kind ids 0..4 are vegetarian, 5..9 carnivorous (the `.bsi` UI buttons and
// the `.bni` animal kinds agree).
bool food_is_carnivore(int kind);

// Reads FoodSourceList.ini and every `<name>.bsi` in `foodsource_dir`
// (relative to `asset_root`, or absolute). Texture candidates are resolved
// under `asset_root`.
std::vector<FoodDef> load_food_defs(const std::string& asset_root,
                                   const std::string& foodsource_dir);

}  // namespace dino
