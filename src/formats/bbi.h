#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace dino {

// `.bbi` building definition (39 files, one per BuildingList.ini entry).
// 121-byte packed header (u32 width, u32 height, f32 scale, u8 flag, then 27
// unaligned u32 from byte 13), followed by ten length-prefixed asset paths;
// the tail interleaves numeric profile bytes and UI/sound/texture strings.
struct BuildingDef {
    std::string name;
    std::uint32_t footprint_w = 0;
    std::uint32_t footprint_h = 0;
    float scale = 0.0f;
    std::uint8_t flag = 0;
    std::array<std::uint32_t, 27> header_words{};
    // Named fields used by the simulation (see notes/building-def-record-recon.md).
    std::uint32_t category = 0;
    float duration = 0.0f;
    std::uint32_t price = 0;
    float appeal_scale = 0.0f;
    std::array<float, 5> appeal_weights{};
    std::uint32_t need_delta1 = 0;
    std::uint32_t need_delta2 = 0;
    std::uint32_t radius_squared = 0;
    std::uint32_t model0 = 0;
    float model1 = 0.0f;
    std::uint32_t model_count = 0;
    std::vector<std::string> meshes;  // skin, crash/destroy/construction/lod1/lod2..., skel, anim
    std::vector<std::string> tail_strings;
};

BuildingDef load_bbi(const std::string& path);
std::vector<std::string> load_building_list(const std::string& path);

}  // namespace dino
