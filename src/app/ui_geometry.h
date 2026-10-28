// Small UI geometry and render-target helpers shared by the app shell.
//
// The mesh builders return CPU meshes; the caller uploads them. Coordinates
// are logical window points (design points), matching the retail overlay.

#pragma once

#include <SDL3/SDL_opengl.h>

#include "../formats/mesh.h"
#include "../render/gl_api.h"
#include "../ui/overlay.h"

namespace dino::app {

struct Bounds {
    dino::Vec3 min{1e30f, 1e30f, 1e30f};
    dino::Vec3 max{-1e30f, -1e30f, -1e30f};

    void add(const dino::Vec3& p);
    dino::Vec3 center() const;
    float extent() const;
};

struct UiTexture {
    GLuint id = 0;
    int width = 0;
    int height = 0;
};

struct UiSourceRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    bool valid = true;
};

dino::Mesh build_screen_rect(float x, float y, float w, float h,
                             dino::Color color);

dino::Mesh build_ui_quad(float x, float y, float w, float h, float u0, float v0,
                         float u1, float v1, dino::Color color);

// Park HUD widgets are re-anchored to the full logical window by
// Overlay::layout_for_size(), so SDL window points are already park design
// points. The menu is the exception: it is drawn as an 800x600 rect centred
// in the window and subtracts the letterbox origin separately.
void map_park_pointer(float window_x, float window_y, int& design_x,
                      int& design_y);

UiSourceRect ui_frame_rect(const UiTexture& texture,
                           const dino::ui::Graphic& graphic, int logical_frame);

// Offscreen colour+depth target for MAP_RENDERVIEW (136x136 in the retail
// HUD). The minimap is a render target, not a crop of the basemap.
struct MinimapTarget {
    GLuint fbo = 0;
    GLuint color = 0;
    GLuint depth = 0;
    int size = 0;

    bool create(GlFns& gl, int new_size);
    void destroy(GlFns& gl);
};

}  // namespace dino::app
