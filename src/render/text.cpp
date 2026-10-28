// Bitmap-font text meshes and word wrap (see text.h).

#include "text.h"

#include "mesh_gpu.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

GLuint upload_font_atlas(const dino::Fnt& font) {
    std::vector<std::uint8_t> alpha(
        static_cast<std::size_t>(font.atlas_width) * font.atlas_height);
    for (std::size_t i = 0; i < alpha.size(); ++i) {
        alpha[i] = font.atlas[i * 2 + 1];
    }
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, font.atlas_width, font.atlas_height, 0,
                 GL_RED, GL_UNSIGNED_BYTE, alpha.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    return id;
}

dino::Mesh build_text_mesh(const dino::Fnt& font, const std::string& text,
                           float x, float y, float scale, dino::Color color) {
    dino::Mesh mesh;
    mesh.name = "hud";
    const float inv_w = 1.0f / static_cast<float>(font.atlas_width);
    const float inv_h = 1.0f / static_cast<float>(font.atlas_height);
    float pen_x = x;
    float pen_y = y;
    // The retail font atlas is binary (0/255) and sampled point-wise; snap
    // the origin to whole pixels so each glyph quad samples its own texels
    // instead of bleeding into the neighbouring glyph.
    pen_x = std::round(pen_x);
    pen_y = std::round(pen_y);
    const float origin_x = pen_x;
    for (const unsigned char ch : text) {
        if (ch == '\n') {
            pen_x = origin_x;
            pen_y += static_cast<float>(font.line_height) * scale;
            continue;
        }
        const dino::FntGlyph* glyph = font.glyph(ch);
        if (glyph == nullptr) {
            pen_x += static_cast<float>(font.font_size) * 0.5f * scale;
            continue;
        }
        const float w = static_cast<float>(glyph->width()) * scale;
        const float h = static_cast<float>(glyph->height()) * scale;
        const float u0 = static_cast<float>(glyph->x0) * inv_w;
        const float u1 = static_cast<float>(glyph->x1 + 1) * inv_w;
        const float v0 = static_cast<float>(glyph->y0) * inv_h;
        const float v1 = static_cast<float>(glyph->y1 + 1) * inv_h;
        const auto base = static_cast<std::uint32_t>(mesh.positions.size());
        mesh.positions.push_back({pen_x, pen_y, 0.0f});
        mesh.positions.push_back({pen_x + w, pen_y, 0.0f});
        mesh.positions.push_back({pen_x + w, pen_y + h, 0.0f});
        mesh.positions.push_back({pen_x, pen_y + h, 0.0f});
        mesh.normals.insert(mesh.normals.end(), 4, {0.0f, 0.0f, 1.0f});
        // build_vertices adds +1 to v, so pass v-1 to get the atlas coordinate.
        mesh.uvs.push_back({u0, v0 - 1.0f});
        mesh.uvs.push_back({u1, v0 - 1.0f});
        mesh.uvs.push_back({u1, v1 - 1.0f});
        mesh.uvs.push_back({u0, v1 - 1.0f});
        mesh.colors.insert(mesh.colors.end(), 4, color);
        mesh.indices.push_back(base + 0);
        mesh.indices.push_back(base + 1);
        mesh.indices.push_back(base + 2);
        mesh.indices.push_back(base + 0);
        mesh.indices.push_back(base + 2);
        mesh.indices.push_back(base + 3);
        // Retail advances one pixel less than the glyph's ink width, so
        // neighbouring glyphs touch. Measured on the retail menu captures:
        // "PANIC IN THE SAVANNA" = 135 px and FONT14 sum(width)-(n-1) = 137,
        // "CAMPAIGNS" (FONT19) = 100 px and sum(width)-(n-1) = 100.
        pen_x += static_cast<float>(glyph->width() - 1) * scale;
    }
    return mesh;
}

float text_width(const dino::Fnt& font, const std::string& text, float scale) {
    float width = 0.0f;
    int glyphs = 0;
    for (const unsigned char ch : text) {
        if (ch == '\n') {
            continue;
        }
        const dino::FntGlyph* glyph = font.glyph(ch);
        width += glyph != nullptr ? static_cast<float>(glyph->width()) * scale
                                  : static_cast<float>(font.font_size) * 0.5f * scale;
        if (glyph != nullptr) {
            ++glyphs;
        }
    }
    // Same one-pixel tightening as build_text_mesh.
    if (glyphs > 1) {
        width -= static_cast<float>(glyphs - 1) * scale;
    }
    return width;
}

dino::Mesh build_text_block(const dino::Fnt& font, const std::string& text,
                            float x, float y, float scale, dino::Color color,
                            const std::string& justify, float box_w, float box_h,
                            float line_pitch) {
    dino::Mesh out;
    out.name = "ui_text";
    std::vector<std::string> lines;
    std::string line;
    for (const char ch : text) {
        if (ch == '\n') {
            lines.push_back(line);
            line.clear();
        } else {
            line.push_back(ch);
        }
    }
    lines.push_back(line);
    const float line_height =
        line_pitch > 0.0f ? line_pitch
                          : static_cast<float>(font.line_height) * scale;
    // Retail centres a text block on the font's cell height (12 for FONT14,
    // 16 for FONT19), not its line height, and truncates the half pixel:
    // a 34-px list row gets (34-12)/2 = 11, a 27-px title plaque with FONT19
    // gets (27-16)/2 = 5.5 -> 5. Both match the retail captures exactly.
    const float cell_height =
        static_cast<float>(font.cell_height) * scale;
    const float total_height =
        cell_height +
        line_height * static_cast<float>(lines.size() > 0 ? lines.size() - 1 : 0);
    float start_y = y;
    if (justify.find("DOWN") != std::string::npos) {
        start_y = y + box_h - total_height;
    } else if (justify.find("CENTER") != std::string::npos ||
               justify.find("Y") != std::string::npos) {
        start_y = y + std::trunc((box_h - total_height) * 0.5f);
    }
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const float width = text_width(font, lines[i], scale);
        float line_x = x;
        if (justify.find("RIGHT") != std::string::npos) {
            line_x = x + box_w - width;
        } else if (justify.find("CENTER") != std::string::npos ||
                   justify.find("X") != std::string::npos) {
            line_x = x + (box_w - width) * 0.5f;
        }
        const dino::Mesh glyphs = build_text_mesh(
            font, lines[i], line_x, start_y + static_cast<float>(i) * line_height,
            scale, color);
        append_mesh(out, glyphs);
    }
    return out;
}

std::string wrap_text(const std::string& text, std::size_t width) {
    std::string out;
    std::string word;
    std::size_t line = 0;
    auto flush = [&]() {
        if (word.empty()) {
            return;
        }
        if (line != 0 && line + 1 + word.size() > width) {
            out += '\n';
            line = 0;
        } else if (line != 0) {
            out += ' ';
            ++line;
        }
        out += word;
        line += word.size();
        word.clear();
    };
    for (std::size_t i = 0; i <= text.size(); ++i) {
        const char ch = i < text.size() ? text[i] : ' ';
        if (ch == '\\' && i + 1 < text.size() && text[i + 1] == 'n') {
            flush();
            out += '\n';
            line = 0;
            ++i;
        } else if (ch == '\n') {
            flush();
            out += '\n';
            line = 0;
        } else if (ch == ' ' || ch == '\t' || ch == '\r') {
            flush();
        } else {
            word.push_back(ch);
        }
    }
    flush();
    return out;
}

// Pixel-accurate word wrap for list paragraphs. Retail wraps the campaign
// briefing at the list column width (CAMPAIGN_INFO_DESCRIPTION: 235 - 22 =
// 213 px), not at a fixed character count; the simulation with the FONT14
// advances reproduces the retail line breaks exactly at 213.
std::vector<std::string> wrap_lines_pixels(const dino::Fnt& font, float scale,
                                           const std::string& text,
                                           float width) {
    std::vector<std::string> lines;
    std::string paragraph;
    std::string word;
    auto flush_paragraph = [&]() {
        std::string current;
        for (const char ch : paragraph) {
            if (ch == ' ' || ch == '\t' || ch == '\r') {
                if (word.empty()) {
                    continue;
                }
                const std::string candidate =
                    current.empty() ? word : current + " " + word;
                if (!current.empty() &&
                    text_width(font, candidate, scale) > width) {
                    lines.push_back(current);
                    current = word;
                } else {
                    current = candidate;
                }
                word.clear();
            } else {
                word.push_back(ch);
            }
        }
        if (!word.empty()) {
            const std::string candidate =
                current.empty() ? word : current + " " + word;
            if (!current.empty() &&
                text_width(font, candidate, scale) > width) {
                lines.push_back(current);
                current = word;
            } else {
                current = candidate;
            }
            word.clear();
        }
        lines.push_back(current);
        paragraph.clear();
    };
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char ch = text[i];
        if (ch == '\\' && i + 1 < text.size() && text[i + 1] == 'n') {
            // The .loc records store paragraph breaks as the two characters
            // `\` `n`; resolve_loc_text keeps them literal.
            flush_paragraph();
            ++i;
        } else if (ch == '\n') {
            flush_paragraph();
        } else {
            paragraph.push_back(ch);
        }
    }
    flush_paragraph();
    return lines;
}

std::string limit_lines(const std::string& text, std::size_t max_lines) {
    std::size_t line = 1;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n') {
            ++line;
            if (line > max_lines) {
                return text.substr(0, i);
            }
        }
    }
    return text;
}

