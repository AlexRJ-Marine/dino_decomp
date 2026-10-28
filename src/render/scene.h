// Scene geometry and the main draw call.
//
// Matrix helpers, the skybox/water builders, the draw-item description and
// draw_scene(), which binds the program and walks the item list. Split out of
// the app shell so the frame loop is not also the renderer.

#ifndef DINO_RENDER_SCENE_H
#define DINO_RENDER_SCENE_H

#include <SDL3/SDL_opengl.h>

#include <array>
#include <vector>

#include "../formats/math.h"
#include "../formats/mesh.h"
#include "gl_api.h"
#include "mesh_gpu.h"

// The retail draw 0x00600df0 sets world = Scale(100) * Translate(camera) on a
// unit cube from the builder, so the box is 200 units across and always
// centred on the eye (draw_scene adds the translation).
constexpr float kSkyboxHalfSize = 100.0f;

struct SkyboxGL {
    std::array<GpuMesh, 6> faces;
    std::array<GLuint, 6> textures{};
    bool ready = false;
};

struct DrawItem {
    const GpuMesh* mesh = nullptr;
    GLuint texture = 0;
    float alpha = 1.0f;
    // Water: Data/States/WaterSurface.ini uses src One / dest One / Add.
    bool additive = false;
    // Additive items normally leave the depth buffer alone; a solid preview
    // box needs the nearest face to win so back/side faces do not add a
    // second layer and saturate to white.
    bool depth_write = false;
    // Skip lighting for flat preview quads whose sampled colour must match
    // retail exactly (enclosure disc).
    bool unlit = false;
    // SrcAlpha/InvSrcAlpha even when alpha is 1 (gatAttraction ground with a
    // ragged cut-out edge and ZWrite off).
    bool alpha_blend = false;
    // Live RGB multiplier for the gscMaterialBank pulse (null = white).
    const float* tint = nullptr;
    // Cut-out textures (trees): discard texels with alpha <= 128/255.
    bool alpha_test = false;
    // 0 = scene fog (FogParam.txt), 1 = water fog (FogParam_WaterSurface.txt).
    int fog = 0;
    // WaterSurface stage 1: same texture on coordinate set 1, Modulate2x.
    GLuint texture2 = 0;
    bool modulate2x = false;
    float uv_offset[2] = {0.0f, 0.0f};
    float uv2_scale[2] = {1.0f, 1.0f};
    float uv2_offset[2] = {0.0f, 0.0f};
    // Vegetation sector batches: drawn only while the flag is non-zero.
    const unsigned char* visible = nullptr;
    // World bounding sphere (x, y, z, radius); radius < 0 = never culled.
    float bound[4] = {0.0f, 0.0f, 0.0f, -1.0f};
};

// Port-only view-frustum test (retail landscape objects have none; they are
// cut by LOD distance only). Planes come from rows of the column-vector
// clip matrix: w +/- x, w +/- y, w +/- z.
struct Frustum {
    float planes[6][4];
    explicit Frustum(const dino::Mat4& clip) {
        const float* m = clip.m;
        for (int i = 0; i < 3; ++i) {
            for (int s = 0; s < 2; ++s) {
                float* plane = planes[i * 2 + s];
                const float sign = s == 0 ? 1.0f : -1.0f;
                for (int k = 0; k < 4; ++k) {
                    plane[k] = m[12 + k] + sign * m[i * 4 + k];
                }
                const float len = std::sqrt(plane[0] * plane[0] +
                                            plane[1] * plane[1] +
                                            plane[2] * plane[2]);
                for (int k = 0; k < 4; ++k) {
                    plane[k] /= len;
                }
            }
        }
    }
    bool outside(const float sphere[4]) const {
        for (const auto& plane : planes) {
            if (plane[0] * sphere[0] + plane[1] * sphere[1] +
                    plane[2] * sphere[2] + plane[3] < -sphere[3]) {
                return true;
            }
        }
        return false;
    }
};

dino::Mesh make_water_plane(float half_size, float z, float tiles);
dino::Mesh make_sky_face(int index, float half_size);

dino::Mat4 perspective(float fov_deg, float aspect, float near_plane, float far_plane);
dino::Mat4 ortho(float left, float right, float bottom, float top,
                 float near_plane, float far_plane);
dino::Mat4 look_at(const dino::Vec3& eye, const dino::Vec3& target, const dino::Vec3& up);
dino::Vec3 cross(const dino::Vec3& a, const dino::Vec3& b);
float dot(const dino::Vec3& a, const dino::Vec3& b);

// Returns how many bounded extras the frustum test skipped.
int draw_scene(const GpuMesh& mesh, GLuint texture,
               const std::vector<DrawItem>& extras,
               const SkyboxGL& skybox,
               GLuint program,
               const GlFns& gl, const dino::Vec3& eye, const dino::Vec3& center,
               int width, int height, float fov_deg = 33.0f,
               bool cull = true, GLuint detail_texture = 0);

#endif  // DINO_RENDER_SCENE_H
