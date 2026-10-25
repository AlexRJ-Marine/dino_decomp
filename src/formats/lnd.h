#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dino {

struct Lnd {
    std::string name_key;
    std::string basemap;
    std::string detailmap;
    std::string heightmap;
    std::string skybox;
    std::string water;
    std::string water2;
    std::string occupation;
    float unknown_float = 0.0f;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t unknown_int = 0;
    std::string music;
    std::vector<std::string> vegetation;
};

Lnd load_lnd(const std::string& path);

}  // namespace dino
