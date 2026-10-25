#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dino {

struct FntGlyph {
    int code = 0;
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;

    int width() const { return x1 - x0 + 1; }
    int height() const { return y1 - y0 + 1; }
};

struct Fnt {
    int version = 0;
    int atlas_width = 0;
    int atlas_height = 0;
    int font_size = 0;
    int cell_height = 0;
    int base = 0;
    int line_height = 0;
    int glyph_count = 0;
    std::uint32_t hash = 0;
    std::vector<std::uint16_t> codes;
    std::vector<FntGlyph> glyphs;
    std::vector<std::uint8_t> atlas;  // atlas_width * atlas_height * 2, LE

    const FntGlyph* glyph(int code) const;
    // High byte of the 16-bit texel (retail fonts store the same value twice).
    std::uint8_t alpha_at(int x, int y) const;
};

Fnt load_fnt(const std::string& path);

}  // namespace dino
