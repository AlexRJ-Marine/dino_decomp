#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../formats/mesh.h"

namespace dino {

struct Terrain {
    Mesh mesh;
    int width = 0;
    int height = 0;
    std::uint8_t min_height = 0;
    std::uint8_t max_height = 0;
    float world_size = 0.0f;
    float height_scale = 1.0f;
    std::vector<std::uint8_t> heights;

    // Bilinear height in world units. Coordinates are centred on the island:
    // both span [-world_size/2, +world_size/2].
    float height_at(float world_x, float world_y) const;
};

std::vector<std::uint8_t> load_raw_u8(const std::string& path, int& width, int& height);
Terrain build_terrain(const std::vector<std::uint8_t>& heights, int width, int height,
                      float world_size, float height_scale);

}  // namespace dino
