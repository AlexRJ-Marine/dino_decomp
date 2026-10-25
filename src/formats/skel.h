#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "math.h"

namespace dino {

struct SkelBone {
    std::string name;
    Mat4 local;
    int parent = -1;
};

struct Skeleton {
    std::uint32_t hash = 0;
    std::vector<SkelBone> bones;

    int index_of(const std::string& name) const;
    std::vector<Mat4> world_matrices() const;
    std::vector<Mat4> world_matrices(const std::vector<Mat4>& locals) const;
};

Skeleton load_skel(const std::string& path);

}  // namespace dino
