#pragma once

#include <array>
#include <string>

namespace dino {

// One `.bvi` visitor profile. The retail record is 0xEC bytes; the port only
// needs the personality id, walk speed and the five preference weights used by
// the attraction appeal dot product.
struct VisitorProfile {
    std::string name;
    int personality_id = 0;
    float walk_speed = 1.6f;
    std::array<float, 5> weights{};
    // `.bvi` offset 0x34 (1.0..2.5). Multiplies the Entrance ticket price:
    // settlement `0x58ad95` computes round(entrance+0x1c * profile+0x30).
    float price_scale = 1.0f;
    int spawn_weight = 30;  // free-game easy default; see FreeGame.ini
    int spawn_cap = 100;
};

VisitorProfile load_bvi(const std::string& path);

}  // namespace dino
