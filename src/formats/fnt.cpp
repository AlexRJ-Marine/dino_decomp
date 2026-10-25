#include "fnt.h"

#include <array>
#include <stdexcept>

#include "bytes.h"
#include "mesh.h"  // read_binary_file

namespace dino {

const FntGlyph* Fnt::glyph(int code) const {
    for (const auto& glyph : glyphs) {
        if (glyph.code == code) {
            return &glyph;
        }
    }
    return nullptr;
}

std::uint8_t Fnt::alpha_at(int x, int y) const {
    if (x < 0 || y < 0 || x >= atlas_width || y >= atlas_height) {
        return 0;
    }
    const std::size_t index =
        (static_cast<std::size_t>(y) * atlas_width + x) * 2;
    return index + 1 < atlas.size() ? atlas[index + 1] : 0;
}

Fnt load_fnt(const std::string& path) {
    auto d = read_binary_file(path);
    if (d.size() < 0x24) {
        throw std::runtime_error("fnt: shorter than header in " + path);
    }
    Fnt font;
    font.version = static_cast<int>(read_u32(d, 0, "fnt version"));
    font.atlas_width = static_cast<int>(read_u32(d, 4, "fnt atlas width"));
    font.atlas_height = static_cast<int>(read_u32(d, 8, "fnt atlas height"));
    font.font_size = static_cast<int>(read_u32(d, 12, "fnt font size"));
    font.cell_height = static_cast<int>(read_u32(d, 16, "fnt cell height"));
    font.base = static_cast<int>(read_u32(d, 20, "fnt base"));
    font.line_height = static_cast<int>(read_u32(d, 24, "fnt line height"));
    font.glyph_count = static_cast<int>(read_u32(d, 28, "fnt glyph count"));
    font.hash = read_u32(d, 32, "fnt hash");
    if (font.version != 1) {
        throw std::runtime_error("fnt: unsupported version in " + path);
    }
    if (font.atlas_width <= 0 || font.atlas_height <= 0 ||
        font.atlas_width > 4096 || font.atlas_height > 4096) {
        throw std::runtime_error("fnt: bad atlas size in " + path);
    }
    if (font.glyph_count <= 0 || font.glyph_count > 10000) {
        throw std::runtime_error("fnt: bad glyph count in " + path);
    }
    const std::size_t codes_off = 0x24;
    const std::size_t metrics_off = codes_off + static_cast<std::size_t>(font.glyph_count) * 2;
    const std::size_t atlas_off = metrics_off + static_cast<std::size_t>(font.glyph_count) * 8;
    if (atlas_off > d.size()) {
        throw std::runtime_error("fnt: tables past EOF in " + path);
    }
    font.codes.reserve(font.glyph_count);
    for (int i = 0; i < font.glyph_count; ++i) {
        font.codes.push_back(static_cast<std::uint16_t>(
            d[codes_off + static_cast<std::size_t>(i) * 2] |
            (d[codes_off + static_cast<std::size_t>(i) * 2 + 1] << 8)));
    }
    // Retail data maps codes[i] to metrics[i + 2]; the last two code words are
    // padding. See docs/formats/fnt.md.
    constexpr int kMetricShift = 2;
    font.glyphs.reserve(static_cast<std::size_t>(font.glyph_count - kMetricShift));
    for (int i = 0; i + kMetricShift < font.glyph_count; ++i) {
        const std::size_t off = metrics_off + static_cast<std::size_t>(i + kMetricShift) * 8;
        const int x0 = d[off] | (d[off + 1] << 8);
        const int y0 = d[off + 2] | (d[off + 3] << 8);
        const int x1 = d[off + 4] | (d[off + 5] << 8);
        const int y1 = d[off + 6] | (d[off + 7] << 8);
        if (x0 < 0 || y0 < 0 || x1 < x0 || y1 < y0 ||
            x1 >= font.atlas_width || y1 >= font.atlas_height) {
            throw std::runtime_error("fnt: glyph rect outside atlas in " + path);
        }
        FntGlyph glyph;
        glyph.code = font.codes[static_cast<std::size_t>(i)];
        glyph.x0 = x0;
        glyph.y0 = y0;
        glyph.x1 = x1;
        glyph.y1 = y1;
        font.glyphs.push_back(glyph);
    }
    const std::size_t expected =
        static_cast<std::size_t>(font.atlas_width) * font.atlas_height * 2;
    if (d.size() - atlas_off != expected) {
        throw std::runtime_error("fnt: atlas size mismatch in " + path);
    }
    font.atlas.assign(d.begin() + static_cast<std::ptrdiff_t>(atlas_off), d.end());
    return font;
}

}  // namespace dino
