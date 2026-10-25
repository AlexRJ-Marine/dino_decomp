#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dino {

struct Wav {
    std::uint16_t format_tag = 0;
    std::uint16_t channels = 0;
    std::uint32_t sample_rate = 0;
    std::uint32_t byte_rate = 0;
    std::uint16_t block_align = 0;
    std::uint16_t bits_per_sample = 0;
    std::uint32_t data_size = 0;
    std::uint32_t frames = 0;
    double duration = 0.0;
    std::vector<std::uint8_t> data;
};

Wav load_wav(const std::string& path);

}  // namespace dino
