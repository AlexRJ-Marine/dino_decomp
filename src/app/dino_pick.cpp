// Dinosaur screen picking and the DINO_INFO_PANEL view model
// (see dino_pick.h).

#include "dino_pick.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "../world/dino_actions.h"
#include "../world/terrain_query.h"

namespace dino::app {

int pick_dino_index(const dino::Terrain& terrain, const dino::Vec3& eye,
                    const dino::Vec3& target,
                    const std::vector<dino::ImportedDino>& dinos,
                    float mouse_x, float mouse_y, int view_width,
                    int view_height) {
    dino::Vec3 origin;
    dino::Vec3 direction;
    screen_ray(eye, target, mouse_x, mouse_y, view_width, view_height, 80.0f,
               origin, direction);
    // The retail test is a pixel-ID buffer (`FUN_005e0da0`); the nearest
    // posed-mesh triangle in front of the heightfield is the same selection
    // for a dinosaur. Ground occlusion is exact; buildings and vegetation
    // are not in this pick set.
    float best_t = 1e30f;
    dino::Vec3 ground;
    if (pick_terrain(terrain, eye, target, mouse_x, mouse_y, view_width,
                     view_height, ground)) {
        const dino::Vec3 delta{ground.x - eye.x, ground.y - eye.y,
                               ground.z - eye.z};
        best_t = dino::dot(delta, direction);
    }
    auto hit_triangle = [&](const dino::Vec3& a, const dino::Vec3& b,
                            const dino::Vec3& c, float& t) {
        const dino::Vec3 e1{b.x - a.x, b.y - a.y, b.z - a.z};
        const dino::Vec3 e2{c.x - a.x, c.y - a.y, c.z - a.z};
        const dino::Vec3 p = dino::cross(direction, e2);
        const float det = dino::dot(e1, p);
        if (std::fabs(det) < 1e-8f) {
            return false;
        }
        const float inv = 1.0f / det;
        const dino::Vec3 s{origin.x - a.x, origin.y - a.y, origin.z - a.z};
        const float u = dino::dot(s, p) * inv;
        if (u < 0.0f || u > 1.0f) {
            return false;
        }
        const dino::Vec3 q = dino::cross(s, e1);
        const float v = dino::dot(direction, q) * inv;
        if (v < 0.0f || u + v > 1.0f) {
            return false;
        }
        t = dino::dot(e2, q) * inv;
        return t > 0.0f;
    };
    int best = -1;
    for (std::size_t i = 0; i < dinos.size(); ++i) {
        const auto& mesh = dinos[i].pick_mesh;
        if (!mesh || mesh->indices.size() < 3) {
            continue;
        }
        for (std::size_t tri = 0; tri + 2 < mesh->indices.size(); tri += 3) {
            const std::uint32_t ia = mesh->indices[tri];
            const std::uint32_t ib = mesh->indices[tri + 1];
            const std::uint32_t ic = mesh->indices[tri + 2];
            if (ia >= mesh->positions.size() || ib >= mesh->positions.size() ||
                ic >= mesh->positions.size()) {
                continue;
            }
            float t = 0.0f;
            if (hit_triangle(mesh->positions[ia], mesh->positions[ib],
                             mesh->positions[ic], t) &&
                t < best_t) {
                best_t = t;
                best = static_cast<int>(i);
            }
        }
    }
    return best;
}

void open_dino_panel(dino::ui::ParkHud& park,
                     const std::vector<dino::ImportedDino>& dinos,
                     int index) {
    if (index < 0 || index >= static_cast<int>(dinos.size())) {
        return;
    }
    const dino::ImportedDino& dinosaur =
        dinos[static_cast<std::size_t>(index)];
    const dino::DinoNeeds& needs =
        dinosaur.action_loop ? dinosaur.action_loop->needs()
                             : dinosaur.needs;
    const dino::DinoAttributes& attrs = dinosaur.attributes;
    dino::ui::DinoPanelData data;
    data.name = dinosaur.species;
    data.id = dinosaur.id;
    data.gauges = {attrs.perception, attrs.beauty, attrs.attack_total / 5,
                   attrs.aggression, attrs.armor,
                   static_cast<int>(attrs.walk_speed * 20.0f),
                   attrs.agility, attrs.intelligence, attrs.courage,
                   attrs.stamina,
                   std::max(0, 100 - attrs.conventional_appeal)};
    data.need_values = {needs.value[0], needs.value[1], needs.value[2],
                        needs.value[3]};
    for (int axis = 0; axis < 4; ++axis) {
        const int state = dino::need_state(
            data.need_values[static_cast<std::size_t>(axis)]);
        data.need_panel_states[static_cast<std::size_t>(axis)] =
            state >= 3 ? 0 : state;
    }
    park.show_dino_info(data);
    std::printf("dino panel open: %s id=%d\n", dinosaur.species.c_str(),
                dinosaur.id);
}

}  // namespace dino::app
