#include "ui_geometry.h"

#include <algorithm>
#include <cstdio>
#include <vector>

#include "../ui/overlay.h"

namespace dino::app {

void Bounds::add(const dino::Vec3& p) {
    min.x = std::min(min.x, p.x);
    min.y = std::min(min.y, p.y);
    min.z = std::min(min.z, p.z);
    max.x = std::max(max.x, p.x);
    max.y = std::max(max.y, p.y);
    max.z = std::max(max.z, p.z);
}

dino::Vec3 Bounds::center() const {
    return {(min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f,
            (min.z + max.z) * 0.5f};
}

float Bounds::extent() const {
    return std::max({max.x - min.x, max.y - min.y, max.z - min.z});
}

dino::Mesh build_screen_rect(float x, float y, float w, float h,
                             dino::Color color) {
    dino::Mesh mesh;
    mesh.name = "screen_rect";
    mesh.positions = {{x, y, 0}, {x + w, y, 0}, {x + w, y + h, 0}, {x, y + h, 0}};
    mesh.normals.assign(4, {0.0f, 0.0f, 1.0f});
    mesh.colors.assign(4, color);
    mesh.indices = {0, 1, 2, 0, 2, 3};
    return mesh;
}

dino::Mesh build_ui_quad(float x, float y, float w, float h, float u0, float v0,
                         float u1, float v1, dino::Color color) {
    dino::Mesh mesh;
    mesh.name = "ui_quad";
    mesh.positions = {{x, y, 0}, {x + w, y, 0}, {x + w, y + h, 0}, {x, y + h, 0}};
    mesh.normals.assign(4, {0.0f, 0.0f, 1.0f});
    // build_vertices adds +1 to v.
    mesh.uvs = {{u0, v0 - 1.0f}, {u1, v0 - 1.0f}, {u1, v1 - 1.0f}, {u0, v1 - 1.0f}};
    mesh.colors.assign(4, color);
    mesh.indices = {0, 1, 2, 0, 2, 3};
    return mesh;
}

void map_park_pointer(float window_x, float window_y, int& design_x,
                      int& design_y) {
    design_x = static_cast<int>(window_x);
    design_y = static_cast<int>(window_y);
}

UiSourceRect ui_frame_rect(const UiTexture& texture,
                           const dino::ui::Graphic& graphic, int logical_frame) {
    UiSourceRect rect{graphic.rect.x, graphic.rect.y, graphic.rect.width,
                      graphic.rect.height};
    std::vector<int> used;
    for (int frame = 0; frame < graphic.objects; ++frame) {
        if (std::find(graphic.skip.begin(), graphic.skip.end(), frame) ==
            graphic.skip.end()) {
            used.push_back(frame);
        }
    }
    const auto it = std::find(used.begin(), used.end(), logical_frame);
    if (it == used.end() || graphic.rect.width <= 0 ||
        graphic.rect.height <= 0) {
        rect.valid = false;
        return rect;
    }
    const int slot = static_cast<int>(it - used.begin());
    const bool horizontal =
        graphic.rect.x + static_cast<int>(used.size()) * graphic.rect.width <=
        texture.width;
    rect.x = horizontal ? graphic.rect.x + slot * graphic.rect.width
                        : graphic.rect.x;
    rect.y = horizontal ? graphic.rect.y
                        : graphic.rect.y + slot * graphic.rect.height;
    if (rect.x + rect.w > texture.width || rect.y + rect.h > texture.height) {
        rect.valid = false;
    }
    return rect;
}

bool MinimapTarget::create(GlFns& gl, int new_size) {
    destroy(gl);
    size = new_size;
    glGenTextures(1, &color);
    glBindTexture(GL_TEXTURE_2D, color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    gl.GenFramebuffers(1, &fbo);
    gl.BindFramebuffer(GL_FRAMEBUFFER, fbo);
    gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                            color, 0);
    gl.GenRenderbuffers(1, &depth);
    gl.BindRenderbuffer(GL_RENDERBUFFER, depth);
    gl.RenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, size, size);
    gl.FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                               GL_RENDERBUFFER, depth);
    const GLenum status = gl.CheckFramebufferStatus(GL_FRAMEBUFFER);
    gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "minimap FBO incomplete: 0x%x\n", status);
        destroy(gl);
        return false;
    }
    return true;
}

void MinimapTarget::destroy(GlFns& gl) {
    if (fbo != 0) {
        gl.DeleteFramebuffers(1, &fbo);
        fbo = 0;
    }
    if (depth != 0) {
        gl.DeleteRenderbuffers(1, &depth);
        depth = 0;
    }
    if (color != 0) {
        glDeleteTextures(1, &color);
        color = 0;
    }
    size = 0;
}

}  // namespace dino::app
