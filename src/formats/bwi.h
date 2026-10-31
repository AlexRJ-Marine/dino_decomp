#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace dino {

// One `Data/Design/attractions/walls/*.bwi` wall definition. The stream is
// packed little-endian and unaligned after the electrified byte; see
// `docs/formats/bwi.md`. `Show`/`Fence` carry the only non-empty mesh slots.
struct BwiWall {
    std::string name;
    std::uint32_t version = 0;
    float height = 0.0f;
    std::uint32_t field_08 = 0;  // 100 (hit points?)
    std::uint32_t field_0c = 0;  // construction cost class (10/15/30)
    std::uint32_t field_10 = 0;  // 30 (build time?)
    bool electrified = false;
    std::array<float, 7> scales{};
    std::array<std::string, 4> textures{};  // base, cracked, mud, ground
    std::array<std::string, 2> meshes{};    // wall mesh, door mesh
    std::string texture_dir;
    std::uint32_t field_tail = 0;  // constant 4
    std::string switch_material;   // SWITCH_WALL_*
    std::string switch_height;     // SWITCH_WALL_* / SWITCH_FENCE_*
};

BwiWall load_bwi(const std::string& path);

// Every `*.bwi` in `dir`, sorted by file name (the WallList.ini order).
std::vector<BwiWall> load_bwi_dir(const std::string& dir);

}  // namespace dino
