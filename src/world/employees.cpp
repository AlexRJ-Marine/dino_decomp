#include "employees.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "../formats/math.h"

namespace dino {
namespace {

constexpr float kFeedStandOffset = 0.8f;

float source_capacity(const FoodSource& source,
                      const std::vector<FoodDef>& defs) {
    if (source.def >= 0 && source.def < static_cast<int>(defs.size())) {
        return defs[static_cast<std::size_t>(source.def)].quantity;
    }
    return 1.0f;
}

}  // namespace

bool EmployeeSystem::hire(const std::vector<EmployeeProfile>& profiles,
                          int profile_index, float x, float y) {
    if (profile_index < 0 ||
        profile_index >= static_cast<int>(profiles.size())) {
        return false;
    }
    Employee employee;
    employee.profile = profile_index;
    employee.x = x;
    employee.y = y;
    employees.push_back(employee);
    return true;
}

int EmployeeSystem::count_role(const std::vector<EmployeeProfile>& profiles,
                               int kind) const {
    int count = 0;
    for (const Employee& employee : employees) {
        if (employee.profile >= 0 &&
            employee.profile < static_cast<int>(profiles.size()) &&
            profiles[static_cast<std::size_t>(employee.profile)].kind == kind) {
            ++count;
        }
    }
    return count;
}

int EmployeeSystem::daily_wages(
    const std::vector<EmployeeProfile>& profiles) const {
    int total = 0;
    for (const Employee& employee : employees) {
        if (employee.profile >= 0 &&
            employee.profile < static_cast<int>(profiles.size())) {
            total += profiles[static_cast<std::size_t>(employee.profile)].wage;
        }
    }
    return total;
}

int EmployeeSystem::active() const {
    int count = 0;
    for (const Employee& employee : employees) {
        if (employee.state != 0) {
            ++count;
        }
    }
    return count;
}

int EmployeeSystem::start_hour(const std::vector<EmployeeProfile>& profiles,
                               const std::vector<FoodSource>& sources,
                               const std::vector<FoodDef>& defs, bool instant) {
    std::vector<int> needy;
    for (std::size_t i = 0; i < sources.size(); ++i) {
        // Retail pushes every registered (paddock, food source) pair on the
        // hour boundary, so a full source is still serviced.
        const bool claimed = std::any_of(
            employees.begin(), employees.end(), [&](const Employee& e) {
                return e.target == static_cast<int>(i) && e.state != 0;
            });
        if (!claimed) {
            needy.push_back(static_cast<int>(i));
        }
    }
    std::sort(needy.begin(), needy.end(), [&](int a, int b) {
        return sources[static_cast<std::size_t>(a)].remaining <
               sources[static_cast<std::size_t>(b)].remaining;
    });

    int assigned = 0;
    std::size_t next = 0;
    for (Employee& employee : employees) {
        if (next >= needy.size()) {
            break;
        }
        if (employee.profile < 0 ||
            employee.profile >= static_cast<int>(profiles.size()) ||
            profiles[static_cast<std::size_t>(employee.profile)].kind != 0 ||
            employee.state != 0) {
            continue;
        }
        const int index = needy[next++];
        employee.target = index;
        employee.work_left = 0.0f;
        if (instant) {
            employee.x = sources[static_cast<std::size_t>(index)].x +
                         kFeedStandOffset;
            employee.y = sources[static_cast<std::size_t>(index)].y;
            employee.state = 2;
            employee.work_left =
                profiles[static_cast<std::size_t>(employee.profile)]
                    .work_duration;
        } else {
            employee.state = 1;
        }
        ++assigned;
    }
    return assigned;
}

FeedResult EmployeeSystem::update(float dt,
                                  const std::vector<EmployeeProfile>& profiles,
                                  std::vector<FoodSource>& sources,
                                  const std::vector<FoodDef>& defs) {
    FeedResult result;
    for (Employee& employee : employees) {
        if (employee.target < 0 ||
            employee.target >= static_cast<int>(sources.size())) {
            employee.target = -1;
            employee.state = 0;
            employee.work_left = 0.0f;
            continue;
        }
        FoodSource& source = sources[static_cast<std::size_t>(employee.target)];
        const EmployeeProfile& profile =
            profiles[static_cast<std::size_t>(employee.profile)];
        if (employee.state == 1) {
            const float dx = source.x + kFeedStandOffset - employee.x;
            const float dy = source.y - employee.y;
            const float distance = std::sqrt(dx * dx + dy * dy);
            const float speed = profile.walk_speed > 0.0f
                                    ? profile.walk_speed
                                    : 1.0f;
            const float step = speed * dt;
            if (distance <= step || distance < 0.05f) {
                employee.x = source.x + kFeedStandOffset;
                employee.y = source.y;
                employee.state = 2;
                employee.work_left = profile.work_duration;
            } else {
                employee.x += dx / distance * step;
                employee.y += dy / distance * step;
            }
        } else if (employee.state == 2) {
            employee.work_left -= dt;
            if (employee.work_left <= 0.0f) {
                const FoodDef* def =
                    (source.def >= 0 &&
                     source.def < static_cast<int>(defs.size()))
                        ? &defs[static_cast<std::size_t>(source.def)]
                        : nullptr;
                if (def != nullptr) {
                    // `FUN_00576d50` level 0: full refill; the `.bsi` price
                    // is charged to the FOOD bucket (`0x585c90`).
                    result.food_spent += set_food_level(source, *def, 0);
                } else {
                    source.remaining = source_capacity(source, defs);
                }
                employee.state = 0;
                employee.target = -1;
                employee.work_left = 0.0f;
                ++serviced;
                ++result.completed;
            }
        }
    }
    return result;
}

Mesh EmployeeSystem::build_mesh(const Terrain& terrain, const Mesh& pose) const {
    Mesh out;
    out.name = "employees";
    if (employees.empty() || pose.positions.empty()) {
        return out;
    }
    const bool has_normals = pose.normals.size() == pose.positions.size();
    const bool has_uvs = pose.uvs.size() == pose.positions.size();
    const float half = terrain.world_size * 0.5f;
    out.positions.reserve(pose.positions.size() * employees.size());
    out.normals.reserve(has_normals ? pose.normals.size() * employees.size() : 0);
    out.uvs.reserve(has_uvs ? pose.uvs.size() * employees.size() : 0);
    out.indices.reserve(pose.indices.size() * employees.size());
    for (const Employee& employee : employees) {
        const float x = employee.x - half;
        const float y = employee.y - half;
        const float base_z = terrain.height_at(x, y);
        const auto base = static_cast<std::uint32_t>(out.positions.size());
        for (std::size_t i = 0; i < pose.positions.size(); ++i) {
            const Vec3& p = pose.positions[i];
            out.positions.push_back({x + p.x, y + p.y, base_z + p.z});
            if (has_normals) {
                out.normals.push_back(pose.normals[i]);
            }
            if (has_uvs) {
                out.uvs.push_back(pose.uvs[i]);
            }
        }
        for (const std::uint32_t index : pose.indices) {
            out.indices.push_back(base + index);
        }
    }
    return out;
}

}  // namespace dino
