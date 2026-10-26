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

    // Retail buildability mask (`<island>_occupation.raw`, 1024x1024 u8):
    // 0 = free/buildable, 255 = blocked (water, cliff or a placed building).
    std::vector<std::uint8_t> occupation;
    int occupation_width = 0;
    int occupation_height = 0;

    // Bilinear height in world units. Coordinates are centred on the island:
    // both span [-world_size/2, +world_size/2].
    float height_at(float world_x, float world_y) const;

    // True when the centred world point maps to a free occupation cell.
    // Missing/invalid masks return true so the port keeps working without one.
    bool buildable(float world_x, float world_y) const;
};

// Slope-aligned placement basis (FUN_0061c510): the normal is the normalized
// sum of three central-difference normals (r = 0.5, 1, 2 world units), the
// right axis is the yaw direction projected onto the slope, and up = n x right.
struct TerrainBasis {
    Vec3 right;
    Vec3 up;
    Vec3 normal;
};
TerrainBasis slope_basis(const Terrain& terrain, float world_x, float world_y,
                         float yaw_radians);

std::vector<std::uint8_t> load_raw_u8(const std::string& path, int& width, int& height);
Terrain build_terrain(const std::vector<std::uint8_t>& heights, int width, int height,
                      float world_size, float height_scale);

}  // namespace dino
