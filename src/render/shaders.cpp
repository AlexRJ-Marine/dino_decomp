// GLSL 3.30 sources for the scene/UI program.
//
// Moved out of build_program so the shader text is readable on its own and the
// C++ around it is not 300 lines of string literal.
//
// Attribute locations: 0 position, 1 normal, 2 uv, 3 colour.
// Scene stages: detail map (coordinate set 1, world/16), water stage 0/1,
// signed ambient modulation, alpha reference (fence laser), fog.

#include "shaders.h"

const char* const kSceneVertex = R"GLSL(
#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;
layout(location = 3) in vec4 a_color;
uniform mat4 u_mvp;
uniform vec4 u_uv2_transform;
uniform vec2 u_uv_offset;
uniform float u_uv2_world;
out vec3 v_normal;
out vec2 v_uv;
out vec2 v_uv2;
out vec4 v_color;
out vec3 v_world;
void main() {
    gl_Position = u_mvp * vec4(a_pos, 1.0);
    v_normal = a_normal;
    // WaterSurface stage 0: the retail builder scrolls this layer with
    // -offset (u from +0x14, v from +0x10). Other materials pass 0.
    v_uv = a_uv + u_uv_offset;
    // WaterSurface stage 1 uses texture coordinate set 1: the same water
    // texture with a slightly different tiling and a small offset.
    v_uv2 = a_uv * u_uv2_transform.xy + u_uv2_transform.zw;
    // DetailMap.ini stage 1 (coordinate set 1): world units / 16.
    if (u_uv2_world > 0.0) {
        v_uv2 = a_pos.xy * u_uv2_world;
    }
    v_color = a_color;
    v_world = a_pos;
}
)GLSL";
const char* const kSceneFragment = R"GLSL(
#version 330 core
in vec3 v_normal;
in vec2 v_uv;
in vec2 v_uv2;
in vec4 v_color;
in vec3 v_world;
uniform sampler2D u_texture;
uniform sampler2D u_texture2;
uniform int u_use_texture;
uniform int u_use_texture2;
uniform int u_modulate2x;
uniform int u_add_signed;
uniform vec3 u_light_dir;
uniform vec3 u_light_color;
uniform vec3 u_ambient;
uniform float u_alpha;
uniform float u_alpha_ref;
uniform int u_unlit;
// gscMaterialBank pulse colour (bank 4 additive lamps, bank 1 selected wall).
uniform vec3 u_tint;
uniform vec3 u_camera;
uniform vec3 u_fog_color;
uniform float u_fog_near;
uniform float u_fog_far;
uniform int u_fog_enable;
out vec4 frag_color;
void main() {
    vec3 n = normalize(v_normal);
    if (!gl_FrontFacing) n = -n;
    float key = max(dot(n, u_light_dir), 0.0);
    float fill = max(dot(n, -u_light_dir), 0.0) * 0.25;
    vec3 base = v_color.rgb;
    vec4 texel = vec4(1.0);
    if (u_use_texture != 0) {
        texel = texture(u_texture, v_uv);
        base *= texel.rgb;
    }
    // WaterSurface.ini: stage 1 Modulate2x (Texture x Current, doubled),
    // alpha SelectArg1(Texture), texture coordinate set 1.
    if (u_use_texture2 != 0 && u_add_signed != 0) {
        // DetailMap.ini: stage 0 SelectArg1(basemap), stage 1
        // AddSigned(detail, current) = detail + current - 0.5.
        vec4 detail = texture(u_texture2, v_uv2);
        base = clamp(texel.rgb + detail.rgb - 0.5, 0.0, 1.0);
    } else if (u_use_texture2 != 0) {
        vec4 texel2 = texture(u_texture2, v_uv2);
        if (u_modulate2x != 0) {
            texel2.rgb = clamp(texel2.rgb * 2.0, 0.0, 1.0);
        }
        base *= texel2.rgb;
        texel.a *= texel2.a;
    }
    // Palette material colour: additive lamps (bank 4) and the selected
    // wall's bank 1 diffuse (stage 1 Modulate2x, factor 1..2).
    base *= u_tint;
    // mgStateAlphaTest Ref 128 Func Greater (TextureAlpha*.ini).
    if (u_alpha_ref >= 0.0 && u_use_texture != 0 && texel.a <= u_alpha_ref) {
        discard;
    }
    if (u_unlit == 2) {
        frag_color = vec4(v_color.rgb, texel.r * u_alpha * v_color.a);
        return;
    }
    vec3 color = base;
    if (u_unlit == 0) {
        color = base * (u_ambient + u_light_color * (key + fill));
        color = pow(clamp(color, 0.0, 1.0), vec3(0.85));
        if (u_fog_enable != 0) {
            float dist = length(v_world - u_camera);
            float fog = clamp((dist - u_fog_near) /
                                  max(1.0, u_fog_far - u_fog_near),
                              0.0, 1.0);
            color = mix(color, u_fog_color, fog);
        }
    }
    float out_alpha = u_alpha * v_color.a;
    if (u_use_texture != 0) {
        out_alpha *= texel.a;
    }
    frag_color = vec4(color, out_alpha);
}
)GLSL";
