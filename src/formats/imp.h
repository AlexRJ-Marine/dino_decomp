#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dino {

struct ImpInstance {
    float x = 0.0f;
    float y = 0.0f;
    float scale = 1.0f;
    float angle = 0.0f;  // radians around the Z axis
};

struct Imp {
    std::string model;  // .blf path from the file
    std::vector<ImpInstance> instances;
};

Imp load_imp(const std::string& path);

}  // namespace dino
