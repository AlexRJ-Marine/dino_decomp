// Bitmap-font text: atlas upload, mesh builders and word wrap.
//
// The retail `.fnt` reader lives in `formats/fnt.h`; this is the render side
// that turns glyph advances into quads. Split out of the app shell because
// the HUD, the menu and the campaign panels all build text the same way.

#ifndef DINO_RENDER_TEXT_H
#define DINO_RENDER_TEXT_H

#include <SDL3/SDL_opengl.h>

#include <string>
#include <vector>

#include "../formats/fnt.h"
#include "../formats/mesh.h"

// Uploads the font's glyph atlas (GL_ALPHA / 0..1 pages) and returns its id.
GLuint upload_font_atlas(const dino::Fnt& font);

// One quad per glyph, in pixels at `scale`, baseline at y = 0.
dino::Mesh build_text_mesh(const dino::Fnt& font, const std::string& text,
                           float x, float y, float scale, dino::Color color);

float text_width(const dino::Fnt& font, const std::string& text, float scale);

// Multi-line block: honours '\n' and the retail justification/box rules.
dino::Mesh build_text_block(const dino::Fnt& font, const std::string& text,
                            float x, float y, float scale, dino::Color color,
                            const std::string& justify, float box_w, float box_h,
                            float line_pitch = 0.0f);

// Word wrap. The pixel variant reproduces the retail line breaks for the
// campaign briefing column (213 px, see the comment in text.cpp).
std::string wrap_text(const std::string& text, std::size_t width);
std::vector<std::string> wrap_lines_pixels(const dino::Fnt& font, float scale,
                                           const std::string& text, float max_width);

std::string limit_lines(const std::string& text, std::size_t max_lines);

#endif  // DINO_RENDER_TEXT_H
