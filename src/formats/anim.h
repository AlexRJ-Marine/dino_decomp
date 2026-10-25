#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "math.h"

namespace dino {

struct AnimBone {
    std::string name;
    std::vector<std::uint64_t> times;
    std::vector<Mat4> matrices;

    Mat4 sample(double time) const;
};

struct Animation {
    std::uint32_t hash = 0;
    std::uint32_t duration = 0;
    std::uint32_t start = 0;
    std::vector<AnimBone> bones;

    const AnimBone* find(const std::string& name) const;
};

Animation load_anim(const std::string& path);

}  // namespace dino
