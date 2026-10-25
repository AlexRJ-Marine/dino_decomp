#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dino {

struct Blf {
    std::uint32_t version = 0;
    std::uint32_t lod_count = 0;
    std::uint32_t unknown = 0;
    std::uint32_t texture_count = 0;
    std::vector<std::string> lods;  // .mesh paths, lod1 first
    std::string texture_dir;
};

Blf load_blf(const std::string& path);

}  // namespace dino
