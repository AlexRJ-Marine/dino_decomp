#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dino {

struct Image {
    int width = 0;
    int height = 0;
    // Top-left origin, RGBA8.
    std::vector<std::uint8_t> rgba;
};

// Reads the retail TGA formats (uncompressed/RLE 24/32-bit, 8-bit grayscale).
// Also supports 16-bit true-color. Throws std::runtime_error on bad input.
Image load_tga(const std::string& path);

}  // namespace dino
