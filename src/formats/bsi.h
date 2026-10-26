#pragma once

#include <string>
#include <vector>

namespace dino {

// `.bsi` food-source definition (`Data/Design/foodsource/*.bsi`, one per
// FoodSourceList.ini entry). See docs/formats/bsi.md.
struct FoodSourceDef {
    std::string name;  // file stem
    float nutrition = 0.0f;
    float quantity = 0.0f;
    int price = 0;
    int kind_id = 0;
    int flags = 0;
    int flag28 = 0;
    std::vector<std::string> assets;        // food LOD1, part1, part2, texture dir
    std::vector<std::string> tail_strings;  // feeder mesh + FOOD_* button, interleaved

    // First `*feed*.mesh` in the tail strings (empty when absent).
    std::string feeder_mesh() const;
    // First `FOOD_*` tail string (the FOOD_DISTRIBUTION_*_BUTTON name).
    std::string ui_name() const;
};

FoodSourceDef load_bsi(const std::string& path);

// FoodSourceList.ini is a count/name header followed by one name per line.
std::vector<std::string> load_food_source_list(const std::string& path);

}  // namespace dino
