#include "visitors.h"

#include <cmath>
#include <utility>

#include "../formats/math.h"

namespace dino {
namespace {

constexpr float kPi = 3.14159265f;

struct Rng {
    unsigned state = 1;
    explicit Rng(unsigned seed) : state(seed ? seed : 1) {}
    unsigned next() {
        state = state * 1664525u + 1013904223u;
        return state;
    }
    float unit() {
        return static_cast<float>(next() >> 8) / 16777216.0f;
    }
    int range(int n) {
        return n > 0 ? static_cast<int>(unit() * static_cast<float>(n)) : 0;
    }
};

float cell_center_x(const BuildGrid& grid, int x) {
    const float half = grid.cell_size * static_cast<float>(grid.width) * 0.5f;
    return -half + (static_cast<float>(x) + 0.5f) * grid.cell_size;
}

float cell_center_y(const BuildGrid& grid, int y) {
    const float half = grid.cell_size * static_cast<float>(grid.width) * 0.5f;
    return -half + (static_cast<float>(y) + 0.5f) * grid.cell_size;
}

void choose_target(Visitor& visitor, const BuildGrid& grid, Rng& rng) {
    static constexpr int kDx[4] = {1, -1, 0, 0};
    static constexpr int kDy[4] = {0, 0, 1, -1};
    int candidates[4];
    int count = 0;
    for (int i = 0; i < 4; ++i) {
        const int nx = visitor.cell_x + kDx[i];
        const int ny = visitor.cell_y + kDy[i];
        if (!grid.path_at(nx, ny)) {
            continue;
        }
        if (nx == visitor.prev_x && ny == visitor.prev_y) {
            continue;
        }
        candidates[count++] = i;
    }
    if (count == 0) {
        // Dead end or a single cell: allow walking back.
        for (int i = 0; i < 4; ++i) {
            const int nx = visitor.cell_x + kDx[i];
            const int ny = visitor.cell_y + kDy[i];
            if (grid.path_at(nx, ny)) {
                candidates[count++] = i;
            }
        }
    }
    if (count == 0) {
        visitor.target_x = visitor.cell_x;
        visitor.target_y = visitor.cell_y;
        return;
    }
    const int pick = candidates[rng.range(count)];
    visitor.target_x = visitor.cell_x + kDx[pick];
    visitor.target_y = visitor.cell_y + kDy[pick];
}

}  // namespace

int VisitorSystem::spawn(const BuildGrid& grid, int count, unsigned seed) {
    visitors.clear();
    std::vector<std::pair<int, int>> cells;
    for (int y = 0; y < grid.height; ++y) {
        for (int x = 0; x < grid.width; ++x) {
            if (grid.path_at(x, y)) {
                cells.emplace_back(x, y);
            }
        }
    }
    if (cells.empty() || count <= 0) {
        return 0;
    }
    Rng rng(seed);
    visitors.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        const auto& cell = cells[static_cast<std::size_t>(rng.range(
            static_cast<int>(cells.size())))];
        Visitor visitor;
        visitor.cell_x = cell.first;
        visitor.cell_y = cell.second;
        visitor.target_x = cell.first;
        visitor.target_y = cell.second;
        visitor.x = cell_center_x(grid, cell.first);
        visitor.y = cell_center_y(grid, cell.second);
        choose_target(visitor, grid, rng);
        visitor.angle = std::atan2(
            cell_center_y(grid, visitor.target_y) - visitor.y,
            cell_center_x(grid, visitor.target_x) - visitor.x);
        visitor.phase = rng.unit() * 2.0f;
        visitors.push_back(visitor);
    }
    return static_cast<int>(visitors.size());
}

void VisitorSystem::update(const BuildGrid& grid, const Terrain& terrain,
                           float dt) {
    (void)terrain;
    if (visitors.empty() || grid.width <= 0) {
        return;
    }
    const float step = speed * dt;
    for (Visitor& visitor : visitors) {
        if (visitor.target_x == visitor.cell_x &&
            visitor.target_y == visitor.cell_y) {
            Rng rng(static_cast<unsigned>(visitor.cell_x * 73856093 ^
                                          visitor.cell_y * 19349663 ^
                                          static_cast<unsigned>(visitors.size())));
            choose_target(visitor, grid, rng);
            if (visitor.target_x == visitor.cell_x &&
                visitor.target_y == visitor.cell_y) {
                continue;
            }
        }
        const float tx = cell_center_x(grid, visitor.target_x);
        const float ty = cell_center_y(grid, visitor.target_y);
        const float dx = tx - visitor.x;
        const float dy = ty - visitor.y;
        const float distance = std::sqrt(dx * dx + dy * dy);
        if (distance <= step || distance < 1e-4f) {
            visitor.x = tx;
            visitor.y = ty;
            visitor.prev_x = visitor.cell_x;
            visitor.prev_y = visitor.cell_y;
            visitor.cell_x = visitor.target_x;
            visitor.cell_y = visitor.target_y;
            Rng rng(static_cast<unsigned>(visitor.cell_x * 73856093 ^
                                          visitor.cell_y * 19349663 ^
                                          static_cast<unsigned>(visitors.size())));
            choose_target(visitor, grid, rng);
            const float nx = cell_center_x(grid, visitor.target_x);
            const float ny = cell_center_y(grid, visitor.target_y);
            visitor.angle = std::atan2(ny - visitor.y, nx - visitor.x);
        } else {
            visitor.x += dx / distance * step;
            visitor.y += dy / distance * step;
            visitor.angle = std::atan2(dy, dx);
        }
        visitor.phase += dt * 3.0f;
    }
}

Mesh VisitorSystem::build_mesh(const BuildGrid& grid, const Terrain& terrain,
                               const Mesh& pose1, const Mesh& pose2) const {
    Mesh out;
    out.name = "visitors";
    if (visitors.empty() || pose1.positions.empty()) {
        return out;
    }
    const std::size_t per_visitor = pose1.positions.size();
    out.positions.reserve(per_visitor * visitors.size());
    out.normals.reserve(per_visitor * visitors.size());
    out.uvs.reserve(per_visitor * visitors.size());
    out.indices.reserve(pose1.indices.size() * visitors.size());
    const bool has_normals = pose1.normals.size() == pose1.positions.size();
    const bool has_uvs = pose1.uvs.size() == pose1.positions.size();

    for (const Visitor& visitor : visitors) {
        const bool second = static_cast<int>(visitor.phase) % 2 != 0;
        const Mesh& pose =
            second && pose2.positions.size() == per_visitor ? pose2 : pose1;
        const float base_z = terrain.height_at(visitor.x, visitor.y);
        const float cs = std::cos(visitor.angle);
        const float sn = std::sin(visitor.angle);
        const auto base = static_cast<std::uint32_t>(out.positions.size());
        for (std::size_t i = 0; i < pose.positions.size(); ++i) {
            const Vec3& p = pose.positions[i];
            out.positions.push_back({visitor.x + cs * p.x - sn * p.y,
                                     visitor.y + sn * p.x + cs * p.y,
                                     base_z + p.z});
            if (has_normals) {
                const Vec3& n = pose.normals[i];
                out.normals.push_back(normalize({cs * n.x - sn * n.y,
                                                 sn * n.x + cs * n.y, n.z}));
            }
            if (has_uvs) {
                out.uvs.push_back(pose.uvs[i]);
            }
        }
        for (const std::uint32_t index : pose.indices) {
            out.indices.push_back(base + index);
        }
    }
    (void)grid;
    return out;
}

}  // namespace dino
