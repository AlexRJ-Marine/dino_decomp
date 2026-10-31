#include "food.h"

#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <utility>

#include "../formats/bsi.h"

namespace dino {
namespace {

std::string normalize(std::string path) {
    for (char& c : path) {
        if (c == '\\') {
            c = '/';
        }
    }
    return path;
}

std::string strip_suffix(const std::string& stem, char marker) {
    const std::size_t pos = stem.find_last_of(marker);
    if (pos == std::string::npos) {
        return {};
    }
    return stem.substr(0, pos);
}

}  // namespace

bool food_is_carnivore(int kind) {
    return kind >= 5 && kind <= 9;
}

std::vector<FoodDef> load_food_defs(const std::string& asset_root,
                                   const std::string& foodsource_dir) {
    std::vector<FoodDef> defs;
    const std::vector<std::string> names =
        load_food_source_list(foodsource_dir + "/FoodSourceList.ini");
    for (const std::string& name : names) {
        const FoodSourceDef raw =
            load_bsi(foodsource_dir + "/" + name + ".bsi");
        FoodDef def;
        def.kind = raw.kind_id;
        def.name = name;
        def.nutrition = raw.nutrition;
        def.quantity = raw.quantity;
        def.price = raw.price;
        if (!raw.assets.empty()) {
            def.mesh = normalize(raw.assets.front());
            std::string texture_dir;
            if (raw.assets.size() >= 4) {
                texture_dir = normalize(raw.assets.back());
            }
            std::string stem = def.mesh;
            const std::size_t slash = stem.find_last_of('/');
            if (slash != std::string::npos) {
                stem = stem.substr(slash + 1);
            }
            const std::size_t dot = stem.find_last_of('.');
            if (dot != std::string::npos) {
                stem.resize(dot);
            }
            if (!texture_dir.empty()) {
                const std::string candidates[] = {
                    texture_dir + "/" + stem + ".tga",
                    texture_dir + "/" + strip_suffix(stem, '_') + ".tga",
                };
                for (const std::string& candidate : candidates) {
                    if (!candidate.empty() &&
                        std::filesystem::exists(
                            std::filesystem::path(asset_root) / candidate)) {
                        def.texture = candidate;
                        break;
                    }
                }
            }
        }
        defs.push_back(std::move(def));
    }
    return defs;
}

int set_food_level(FoodSource& source, const FoodDef& def, int level) {
    if (level < 0 || level > 2) {
        return 0;
    }
    const float factor = level == 0 ? 1.0f
                         : level == 1 ? 0.66666f
                                      : 0.33333f;
    source.level = level;
    source.remaining = def.quantity * factor;
    return def.price;
}

float feeder_drain(Feeder& feeder, float amount) {
    if (feeder.quantity < amount) {
        amount = feeder.quantity;
    }
    feeder.quantity -= amount;
    const float capacity = feeder.max_quantity;
    if (capacity * 0.66666f <= feeder.quantity) {
        feeder.state = 0;
    } else if (capacity * 0.33333f <= feeder.quantity) {
        feeder.state = 1;
    } else if (0.0f < feeder.quantity) {
        feeder.state = 2;
    } else {
        feeder.state = 3;
    }
    return amount * feeder.nutrition;
}

}  // namespace dino
