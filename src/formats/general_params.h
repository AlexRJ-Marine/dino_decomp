#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace dino {

// `Data/Design/generalparams/GeneralParams.bgd`, read by `0x542f80` into the
// game-data object (`0x53d4d0`). Field names are the runtime offsets; see
// docs/formats/general-params.md. Mirrors tools/formats/general_params.py.
struct GeneralParams {
    std::array<float, 4> field_110{};
    std::array<std::string, 4> sounds;
    std::string whistle;
    float field_154 = 0.0f;
    float field_150 = 0.0f;
    float field_1a0 = 0.0f;
    // Road price per world unit / 4, i.e. per 4-unit cell
    // (ROAD_CONSTRUCTION `0x52e7c2`, notes/road-construction-recon.md).
    std::uint32_t field_1a4 = 0;
    float field_1f8 = 0.0f;
    std::array<float, 16> matrix_1a8{};
    std::array<float, 4> vector_1e8{};

    std::uint32_t road_price() const { return field_1a4; }
};

GeneralParams load_general_params(const std::string& path);

}  // namespace dino
