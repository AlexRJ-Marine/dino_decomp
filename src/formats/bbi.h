#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace dino {

// `.bbi` building definition (39 files, one per BuildingList.ini entry).
// 121-byte header followed by ten length-prefixed asset paths; the tail
// interleaves numeric profile bytes and UI/sound/texture strings.
struct BuildingDef {
    std::string name;
    std::array<std::uint32_t, 30> header{};
    std::uint8_t flag = 0;
    std::vector<std::string> meshes;  // skin, crash/destroy/construction/lod1/lod2..., skel, anim
    std::vector<std::string> tail_strings;
};

BuildingDef load_bbi(const std::string& path);
std::vector<std::string> load_building_list(const std::string& path);

}  // namespace dino
