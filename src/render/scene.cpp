// Scene geometry and the main draw call (see scene.h).

#include "scene.h"

#include "../formats/math.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

dino::Mesh make_water_plane(float half_size, float z, float tiles) {
    dino::Mesh mesh;
    mesh.name = "water";
    mesh.positions = {{-half_size, -half_size, z},
                      {half_size, -half_size, z},
                      {half_size, half_size, z},
                      {-half_size, half_size, z}};
    mesh.normals = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
    // build_vertices adds +1 to v; final UVs are 0..tiles.
    mesh.uvs = {{0.0f, -1.0f}, {tiles, -1.0f}, {tiles, tiles - 1.0f},
                {0.0f, tiles - 1.0f}};
    mesh.indices = {0, 1, 2, 0, 2, 3};
    return mesh;
}

dino::Mesh make_sky_face(int index, float half_size) {
    const float h = half_size;
    dino::Mesh mesh;
    mesh.name = "skybox";
    dino::Vec3 normal{0, 0, 1};
    // Retail face mapping (skybox geometry builder 0x00600f80, draw
    // 0x00600df0): front = -X, left = +Y, back = +X, right = -Y,
    // top = +Z, bottom = -Z. Each face's vertices are in UV order
    // (0,0), (1,0), (0,1), (1,1); v=0 is the PNG top row. The shader adds
    // 1 to v, so the stored values are (0,-1), (1,-1), (0,0), (1,0).
    switch (index) {
        case 0:  // front (-X)
            mesh.positions = {{-h, -h, h}, {-h, h, h}, {-h, -h, -h}, {-h, h, -h}};
            normal = {1, 0, 0};
            break;
        case 1:  // back (+X)
            mesh.positions = {{h, h, h}, {h, -h, h}, {h, h, -h}, {h, -h, -h}};
            normal = {-1, 0, 0};
            break;
        case 2:  // left (+Y)
            mesh.positions = {{-h, h, h}, {h, h, h}, {-h, h, -h}, {h, h, -h}};
            normal = {0, -1, 0};
            break;
        case 3:  // right (-Y)
            mesh.positions = {{h, -h, h}, {-h, -h, h}, {h, -h, -h}, {-h, -h, -h}};
            normal = {0, 1, 0};
            break;
        case 4:  // top (+Z)
            mesh.positions = {{h, -h, h}, {h, h, h}, {-h, -h, h}, {-h, h, h}};
            normal = {0, 0, -1};
            break;
        default:  // bottom (-Z)
            mesh.positions = {{-h, -h, -h}, {-h, h, -h}, {h, -h, -h}, {h, h, -h}};
            normal = {0, 0, 1};
            break;
    }
    mesh.normals.assign(4, normal);
    mesh.uvs = {{0.0f, -1.0f}, {1.0f, -1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f}};
    mesh.indices = {0, 1, 2, 2, 1, 3};
    return mesh;
}

dino::Mat4 perspective(float fov_deg, float aspect, float near_plane, float far_plane) {
    dino::Mat4 out;
    const float f = 1.0f / std::tan(fov_deg * dino::kPi / 360.0f);
    out.m[0] = f / aspect;
    out.m[5] = f;
    out.m[10] = (far_plane + near_plane) / (near_plane - far_plane);
    out.m[14] = -1.0f;
    out.m[11] = (2.0f * far_plane * near_plane) / (near_plane - far_plane);
    out.m[15] = 0.0f;
    return out;
}

dino::Mat4 look_at(const dino::Vec3& eye, const dino::Vec3& target, const dino::Vec3& up) {
    const dino::Vec3 f = dino::normalize({target.x - eye.x, target.y - eye.y, target.z - eye.z});
    const dino::Vec3 s = dino::normalize(cross(f, up));
    const dino::Vec3 u = cross(s, f);
    dino::Mat4 out;
    out.m[0] = s.x;  out.m[1] = s.y;  out.m[2] = s.z;  out.m[3] = -dot(s, eye);
    out.m[4] = u.x;  out.m[5] = u.y;  out.m[6] = u.z;  out.m[7] = -dot(u, eye);
    out.m[8] = -f.x; out.m[9] = -f.y; out.m[10] = -f.z; out.m[11] = dot(f, eye);
    out.m[12] = 0.0f; out.m[13] = 0.0f; out.m[14] = 0.0f; out.m[15] = 1.0f;
    return out;
}

dino::Mat4 ortho(float left, float right, float bottom, float top,
                 float near_plane, float far_plane) {
    // Row-vector convention, matching look_at() and the transposed upload
    // used by the renderer: translation lives in row 3 (m[3], m[7], m[11]).
    dino::Mat4 out;
    out.m[0] = 2.0f / (right - left);
    out.m[5] = 2.0f / (top - bottom);
    out.m[10] = -2.0f / (far_plane - near_plane);
    out.m[3] = -(right + left) / (right - left);
    out.m[7] = -(top + bottom) / (top - bottom);
    out.m[11] = -(far_plane + near_plane) / (far_plane - near_plane);
    out.m[15] = 1.0f;
    return out;
}

int draw_scene(const GpuMesh& mesh, GLuint texture,
                const std::vector<DrawItem>& extras,
                const SkyboxGL& skybox,
                GLuint program,
                const GlFns& gl, const dino::Vec3& eye, const dino::Vec3& center,
                int width, int height, float fov_deg,
                bool cull, GLuint detail_texture) {
    const dino::Mat4 proj = perspective(fov_deg,
                                        static_cast<float>(width) / std::max(1, height),
                                        0.1f, 20000.0f);
    const dino::Mat4 view = look_at(eye, center, {0.0f, 0.0f, 1.0f});
    const dino::Mat4 mvp = dino::multiply(proj, view);
    const Frustum frustum(mvp);
    int culled = 0;

    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    gl.UseProgram(program);
    gl.UniformMatrix4fv(gl.GetUniformLocation(program, "u_mvp"), 1, GL_TRUE, mvp.m);
    const float light[3] = {-0.35f, -0.68f, 0.72f};
    const float light_color[3] = {0.92f, 0.93f, 0.95f};
    const float ambient[3] = {0.62f, 0.64f, 0.68f};
    gl.Uniform3fv(gl.GetUniformLocation(program, "u_light_dir"), 1, light);
    gl.Uniform3fv(gl.GetUniformLocation(program, "u_light_color"), 1, light_color);
    gl.Uniform3fv(gl.GetUniformLocation(program, "u_ambient"), 1, ambient);
    gl.Uniform1i(gl.GetUniformLocation(program, "u_use_texture"), texture != 0 ? 1 : 0);
    gl.Uniform1i(gl.GetUniformLocation(program, "u_texture"), 0);
    gl.Uniform1i(gl.GetUniformLocation(program, "u_texture2"), 1);
    gl.Uniform1i(gl.GetUniformLocation(program, "u_use_texture2"), 0);
    gl.Uniform1i(gl.GetUniformLocation(program, "u_modulate2x"), 0);
    gl.Uniform4f(gl.GetUniformLocation(program, "u_uv2_transform"), 1.0f, 1.0f,
                 0.0f, 0.0f);
    gl.Uniform2f(gl.GetUniformLocation(program, "u_uv_offset"), 0.0f, 0.0f);
    gl.Uniform1f(gl.GetUniformLocation(program, "u_alpha"), 1.0f);
    {
        const float white[3] = {1.0f, 1.0f, 1.0f};
        gl.Uniform3fv(gl.GetUniformLocation(program, "u_tint"), 1, white);
    }
    const GLint alpha_ref_loc = gl.GetUniformLocation(program, "u_alpha_ref");
    gl.Uniform1f(alpha_ref_loc, -1.0f);

    // Data/States/FogParam.txt: linear 1500..2000, colour (255,220,165).
    const GLint camera_loc = gl.GetUniformLocation(program, "u_camera");
    const GLint fog_color_loc = gl.GetUniformLocation(program, "u_fog_color");
    const GLint fog_near_loc = gl.GetUniformLocation(program, "u_fog_near");
    const GLint fog_far_loc = gl.GetUniformLocation(program, "u_fog_far");
    const GLint fog_enable_loc = gl.GetUniformLocation(program, "u_fog_enable");
    const float scene_fog_color[3] = {1.0f, 0.862745f, 0.647059f};
    const float camera_pos[3] = {eye.x, eye.y, eye.z};
    gl.Uniform3fv(camera_loc, 1, camera_pos);
    gl.Uniform1i(fog_enable_loc, 1);
    gl.Uniform3fv(fog_color_loc, 1, scene_fog_color);
    gl.Uniform1f(fog_near_loc, 1500.0f);
    gl.Uniform1f(fog_far_loc, 2000.0f);

    const GLint unlit_loc = gl.GetUniformLocation(program, "u_unlit");
    if (skybox.ready) {
        dino::Mat4 follow;
        follow.m[3] = eye.x;
        follow.m[7] = eye.y;
        follow.m[11] = eye.z;
        const dino::Mat4 sky_mvp = dino::multiply(mvp, follow);
        gl.UniformMatrix4fv(gl.GetUniformLocation(program, "u_mvp"), 1, GL_TRUE,
                            sky_mvp.m);
        gl.Uniform1i(unlit_loc, 1);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        gl.Uniform1i(gl.GetUniformLocation(program, "u_use_texture"), 1);
        for (int i = 0; i < 6; ++i) {
            if (skybox.faces[i].vao == 0 || skybox.textures[i] == 0) {
                continue;
            }
            glBindTexture(GL_TEXTURE_2D, skybox.textures[i]);
            gl.BindVertexArray(skybox.faces[i].vao);
            glDrawElements(GL_TRIANGLES, skybox.faces[i].index_count,
                           GL_UNSIGNED_INT, nullptr);
        }
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        gl.UniformMatrix4fv(gl.GetUniformLocation(program, "u_mvp"), 1, GL_TRUE,
                            mvp.m);
        gl.Uniform1i(gl.GetUniformLocation(program, "u_use_texture"),
                     texture != 0 ? 1 : 0);
    }
    gl.Uniform1i(unlit_loc, 0);

    if (texture != 0) {
        gl.ActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
    }
    // Retail terrain state DATA\states\detailmap.ini (bound by 0x5f3f10 before
    // gpkLandscape 0x5fb8b0): unlit, no fog, basemap AddSigned detail map on
    // texture coordinates world/16 (landscape parameter 1/16 at 0x5fb8b0).
    const bool detail = detail_texture != 0 && texture != 0;
    if (detail) {
        gl.ActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, detail_texture);
        gl.ActiveTexture(GL_TEXTURE0);
        gl.Uniform1i(gl.GetUniformLocation(program, "u_use_texture2"), 1);
        gl.Uniform1i(gl.GetUniformLocation(program, "u_add_signed"), 1);
        gl.Uniform1f(gl.GetUniformLocation(program, "u_uv2_world"), 1.0f / 16.0f);
        gl.Uniform1i(unlit_loc, 1);
    }
    gl.BindVertexArray(mesh.vao);
    glDrawElements(GL_TRIANGLES, mesh.index_count, GL_UNSIGNED_INT, nullptr);
    if (detail) {
        gl.Uniform1i(gl.GetUniformLocation(program, "u_use_texture2"), 0);
        gl.Uniform1i(gl.GetUniformLocation(program, "u_add_signed"), 0);
        gl.Uniform1f(gl.GetUniformLocation(program, "u_uv2_world"), 0.0f);
        gl.Uniform1i(unlit_loc, 0);
    }

    const GLint use_texture_loc = gl.GetUniformLocation(program, "u_use_texture");
    const GLint alpha_loc = gl.GetUniformLocation(program, "u_alpha");
    const GLint use_texture2_loc =
        gl.GetUniformLocation(program, "u_use_texture2");
    const GLint modulate2x_loc =
        gl.GetUniformLocation(program, "u_modulate2x");
    const GLint uv2_loc = gl.GetUniformLocation(program, "u_uv2_transform");
    const GLint uv_offset_loc = gl.GetUniformLocation(program, "u_uv_offset");
    const GLint tint_loc = gl.GetUniformLocation(program, "u_tint");
    const float white_tint[3] = {1.0f, 1.0f, 1.0f};
    for (const DrawItem& item : extras) {
        if (item.mesh == nullptr || item.mesh->vao == 0) {
            continue;
        }
        if (item.visible != nullptr && *item.visible == 0) {
            continue;
        }
        if (item.bound[3] >= 0.0f && frustum.outside(item.bound)) {
            if (cull) {
                ++culled;
                continue;
            }
        }
        gl.Uniform1i(use_texture_loc, item.texture != 0 ? 1 : 0);
        gl.Uniform1i(unlit_loc, item.unlit ? 1 : 0);
        gl.Uniform3fv(tint_loc, 1, item.tint != nullptr ? item.tint : white_tint);
        gl.Uniform1f(alpha_loc, item.alpha);
        gl.Uniform1f(alpha_ref_loc, item.alpha_test ? 128.0f / 255.0f : -1.0f);
        gl.Uniform1i(use_texture2_loc, item.texture2 != 0 ? 1 : 0);
        gl.Uniform1i(modulate2x_loc, item.modulate2x ? 1 : 0);
        gl.Uniform4f(uv2_loc, item.uv2_scale[0], item.uv2_scale[1],
                     item.uv2_offset[0], item.uv2_offset[1]);
        // The terrain's detail pass leaves `u_uv2_world` at 1/16; without
        // this reset every later texture2 item (paddock ground) samples
        // world/16 instead of its UVs.
        gl.Uniform1f(gl.GetUniformLocation(program, "u_uv2_world"), 0.0f);
        gl.Uniform2f(uv_offset_loc, item.uv_offset[0], item.uv_offset[1]);
        if (item.fog == 1) {
            // Data/States/FogParam_WaterSurface.txt: 100..700, black.
            const float water_fog[3] = {0.0f, 0.0f, 0.0f};
            gl.Uniform3fv(fog_color_loc, 1, water_fog);
            gl.Uniform1f(fog_near_loc, 100.0f);
            gl.Uniform1f(fog_far_loc, 700.0f);
        } else {
            gl.Uniform3fv(fog_color_loc, 1, scene_fog_color);
            gl.Uniform1f(fog_near_loc, 1500.0f);
            gl.Uniform1f(fog_far_loc, 2000.0f);
        }
        const bool blend =
            item.alpha < 1.0f || item.additive || item.alpha_blend;
        if (blend) {
            glEnable(GL_BLEND);
            if (item.additive) {
                glBlendFunc(GL_ONE, GL_ONE);
            } else {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }
            glDepthMask(item.depth_write ? GL_TRUE : GL_FALSE);
        }
        if (item.texture != 0) {
            glBindTexture(GL_TEXTURE_2D, item.texture);
        }
        if (item.texture2 != 0) {
            gl.ActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, item.texture2);
            gl.ActiveTexture(GL_TEXTURE0);
        }
        gl.BindVertexArray(item.mesh->vao);
        glDrawElements(GL_TRIANGLES, item.mesh->index_count, GL_UNSIGNED_INT, nullptr);
        if (blend) {
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }
    }
    gl.BindVertexArray(0);
    gl.Uniform1i(use_texture2_loc, 0);
    gl.Uniform1i(modulate2x_loc, 0);
    gl.Uniform2f(uv_offset_loc, 0.0f, 0.0f);
    gl.Uniform1f(alpha_ref_loc, -1.0f);
    gl.UseProgram(0);
    return culled;
}
