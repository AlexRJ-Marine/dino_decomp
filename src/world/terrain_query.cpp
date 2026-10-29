// Terrain queries (see terrain_query.h).

#include "terrain_query.h"

#include "../formats/math.h"

#include <algorithm>
#include <cmath>

// Ray-marches the heightfield from the camera through a screen pixel.
bool pick_terrain(const dino::Terrain& terrain, const dino::Vec3& eye,
                  const dino::Vec3& target, float mouse_x, float mouse_y,
                  int width, int height, dino::Vec3& hit) {
    if (terrain.world_size <= 0.0f) {
        return false;
    }
    const dino::Vec3 forward = dino::normalize({target.x - eye.x, target.y - eye.y,
                                                target.z - eye.z});
    const dino::Vec3 right = dino::normalize(cross(forward, {0.0f, 0.0f, 1.0f}));
    const dino::Vec3 up = cross(right, forward);
    // The park terrain is rendered with draw_scene's 80-degree FOV
    // (`terrain.world_size > 0`), so the pick ray must use the same value;
    // the old 33-degree value was the single-dino viewer and made the ground
    // point drift away from the cursor toward the screen edges.
    const float tan_half = std::tan(80.0f * dino::kPi / 360.0f);
    const float aspect = static_cast<float>(width) / static_cast<float>(std::max(1, height));
    const float ndc_x = (2.0f * mouse_x / static_cast<float>(width) - 1.0f) *
                        tan_half * aspect;
    const float ndc_y = (1.0f - 2.0f * mouse_y / static_cast<float>(height)) * tan_half;
    const dino::Vec3 dir = dino::normalize({forward.x + right.x * ndc_x + up.x * ndc_y,
                                            forward.y + right.y * ndc_x + up.y * ndc_y,
                                            forward.z + right.z * ndc_x + up.z * ndc_y});

    auto at = [&](float t) {
        return dino::Vec3{eye.x + dir.x * t, eye.y + dir.y * t, eye.z + dir.z * t};
    };
    auto above = [&](const dino::Vec3& p) {
        return p.z - terrain.height_at(p.x, p.y);
    };
    float previous_t = 0.5f;
    for (float t = 1.0f; t < 2500.0f; t += 2.0f) {
        const dino::Vec3 p = at(t);
        if (above(p) <= 0.0f) {
            float lo = previous_t;
            float hi = t;
            for (int i = 0; i < 24; ++i) {
                const float mid = (lo + hi) * 0.5f;
                if (above(at(mid)) <= 0.0f) {
                    hi = mid;
                } else {
                    lo = mid;
                }
            }
            hit = at(hi);
            const float half = terrain.world_size * 0.5f;
            return hit.x >= -half && hit.x <= half && hit.y >= -half && hit.y <= half;
        }
        previous_t = t;
    }
    return false;
}

// A disc at ground level so a placed model reads as standing on the terrain.
dino::Mesh make_contact_shadow(float x, float y, float z, float radius) {
    constexpr int kSegments = 40;
    dino::Mesh mesh;
    mesh.name = "contact_shadow";
    const dino::Color color{26, 36, 22, 255};
    mesh.positions.push_back({x, y, z});
    mesh.normals.push_back({0.0f, 0.0f, 1.0f});
    mesh.uvs.push_back({0.0f, 0.0f});
    mesh.colors.push_back(color);
    for (int i = 0; i < kSegments; ++i) {
        const float angle = 2.0f * dino::kPi * static_cast<float>(i) /
                            static_cast<float>(kSegments);
        mesh.positions.push_back({x + std::cos(angle) * radius,
                                  y + std::sin(angle) * radius, z});
        mesh.normals.push_back({0.0f, 0.0f, 1.0f});
        mesh.uvs.push_back({0.0f, 0.0f});
        mesh.colors.push_back(color);
    }
    for (int i = 0; i < kSegments; ++i) {
        mesh.indices.push_back(0);
        mesh.indices.push_back(static_cast<std::uint32_t>(1 + i));
        mesh.indices.push_back(static_cast<std::uint32_t>(1 + (i + 1) % kSegments));
    }
    return mesh;
}

dino::Mesh place_on_terrain(dino::Mesh mesh, const dino::Terrain& terrain,
                            float x, float y, float angle_degrees) {
    const float half = terrain.world_size * 0.5f;
    const float base_z = terrain.height_at(x - half, y - half);
    const float radians = angle_degrees * dino::kPi / 180.0f;
    const float cs = std::cos(radians);
    const float sn = std::sin(radians);
    const bool has_normals = mesh.normals.size() == mesh.positions.size();
    for (std::size_t i = 0; i < mesh.positions.size(); ++i) {
        const dino::Vec3 p = mesh.positions[i];
        mesh.positions[i] = {x - half + cs * p.x - sn * p.y,
                             y - half + sn * p.x + cs * p.y,
                             base_z + p.z};
        if (has_normals) {
            const dino::Vec3 n = mesh.normals[i];
            mesh.normals[i] = dino::normalize({cs * n.x - sn * n.y,
                                               sn * n.x + cs * n.y,
                                               n.z});
        }
    }
    return mesh;
}
