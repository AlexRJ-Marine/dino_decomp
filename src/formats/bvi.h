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
    int spawn_weight = 30;  // free-game easy default; see FreeGame.ini
    int spawn_cap = 100;
};

VisitorProfile load_bvi(const std::string& path);

}  // namespace dino
