#include "build.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "../formats/math.h"

namespace dino {
namespace {

constexpr float kPi = 3.14159265f;

void append_mesh(Mesh& dst, const Mesh& src, const Vec3& offset, float angle,
                 float scale) {
    const bool has_normals = src.normals.size() == src.positions.size();
    const bool has_uvs = src.uvs.size() == src.positions.size();
    const auto base = static_cast<std::uint32_t>(dst.positions.size());
    const float cs = std::cos(angle);
    const float sn = std::sin(angle);
    for (std::size_t i = 0; i < src.positions.size(); ++i) {
        const Vec3& p = src.positions[i];
        dst.positions.push_back({offset.x + (cs * p.x - sn * p.y) * scale,
                                 offset.y + (sn * p.x + cs * p.y) * scale,
                                 offset.z + p.z * scale});
        if (has_normals) {
            const Vec3& n = src.normals[i];
            dst.normals.push_back(normalize({cs * n.x - sn * n.y,
                                             sn * n.x + cs * n.y, n.z}));
        }
        if (has_uvs) {
            dst.uvs.push_back(src.uvs[i]);
        }
    }
    for (const std::uint32_t index : src.indices) {
        dst.indices.push_back(base + index);
    }
}

}  // namespace

void BuildGrid::init(float new_cell_size, float world_size) {
    cell_size = new_cell_size > 0.0f ? new_cell_size : 4.0f;
    width = static_cast<int>(std::ceil(world_size / cell_size));
    height = width;
    origin_x = -world_size * 0.5f;
    origin_y = -world_size * 0.5f;
    path.assign(static_cast<std::size_t>(width) * height, 0);
    fences.assign(static_cast<std::size_t>(width) * height, 0);
    undo_stack.clear();
    path_count = 0;
    fence_count = 0;
}

bool BuildGrid::path_at(int x, int y) const {
    return in_bounds(x, y) && path[static_cast<std::size_t>(y) * width + x] != 0;
}

bool BuildGrid::fence_at(int x, int y, int edge) const {
    if (!in_bounds(x, y) || edge < 0 || edge > 1) {
        return false;
    }
    return (fences[static_cast<std::size_t>(y) * width + x] >> edge) & 1;
}

void BuildGrid::set_path(int x, int y, bool on) {
    if (!in_bounds(x, y)) {
        return;
    }
    auto& cell = path[static_cast<std::size_t>(y) * width + x];
    const bool current = cell != 0;
    if (current == on) {
        return;
    }
    cell = on ? 1 : 0;
    path_count += on ? 1 : -1;
    undo_stack.push_back({x, y, 0, on});
}

void BuildGrid::set_fence(int x, int y, int edge, bool on) {
    if (!in_bounds(x, y) || edge < 0 || edge > 1) {
        return;
    }
    auto& cell = fences[static_cast<std::size_t>(y) * width + x];
    const std::uint8_t mask = static_cast<std::uint8_t>(1u << edge);
    const bool current = (cell & mask) != 0;
    if (current == on) {
        return;
    }
    cell = static_cast<std::uint8_t>(on ? (cell | mask) : (cell & ~mask));
    fence_count += on ? 1 : -1;
    undo_stack.push_back({x, y, 1 + edge, on});
}

bool BuildGrid::undo() {
    if (undo_stack.empty()) {
        return false;
    }
    const Change change = undo_stack.back();
    undo_stack.pop_back();
    // Changes are only recorded when the value actually changed, so undo always
    // restores the opposite value.
    const bool restore = !change.value;
    if (change.kind == 0) {
        auto& cell = path[static_cast<std::size_t>(change.y) * width + change.x];
        const bool current = cell != 0;
        if (current != restore) {
            cell = restore ? 1 : 0;
            path_count += restore ? 1 : -1;
        }
    } else {
        const int edge = change.kind - 1;
        auto& cell = fences[static_cast<std::size_t>(change.y) * width + change.x];
        const std::uint8_t mask = static_cast<std::uint8_t>(1u << edge);
        const bool current = (cell & mask) != 0;
        if (current != restore) {
            cell = static_cast<std::uint8_t>(restore ? (cell | mask)
                                                     : (cell & ~mask));
            fence_count += restore ? 1 : -1;
        }
    }
    return true;
}

bool BuildGrid::cells_for_world(float wx, float wy, int& x, int& y) const {
    const float gx = (wx - origin_x) / cell_size;
    const float gy = (wy - origin_y) / cell_size;
    if (gx < 0.0f || gy < 0.0f) {
        return false;
    }
    x = static_cast<int>(gx);
    y = static_cast<int>(gy);
    return in_bounds(x, y);
}

BuildMeshes build_placement_meshes(const BuildGrid& grid, const Terrain& terrain,
                                   const Mesh& fence_source) {
    BuildMeshes out;
    if (grid.width <= 0 || grid.height <= 0 || terrain.world_size <= 0.0f) {
        return out;
    }
    const float cs = grid.cell_size;

    out.paths.name = "build_paths";
    out.paths.positions.reserve(static_cast<std::size_t>(grid.path_count) * 4);
    out.paths.normals.reserve(static_cast<std::size_t>(grid.path_count) * 4);
    out.paths.uvs.reserve(static_cast<std::size_t>(grid.path_count) * 4);
    out.paths.indices.reserve(static_cast<std::size_t>(grid.path_count) * 6);

    for (int y = 0; y < grid.height; ++y) {
        for (int x = 0; x < grid.width; ++x) {
            if (!grid.path_at(x, y)) {
                continue;
            }
            const float x0 = grid.origin_x + static_cast<float>(x) * cs;
            const float y0 = grid.origin_y + static_cast<float>(y) * cs;
            const float x1 = x0 + cs;
            const float y1 = y0 + cs;
            const auto base = static_cast<std::uint32_t>(out.paths.positions.size());
            const float z00 = terrain.height_at(x0, y0) + 0.05f;
            const float z10 = terrain.height_at(x1, y0) + 0.05f;
            const float z11 = terrain.height_at(x1, y1) + 0.05f;
            const float z01 = terrain.height_at(x0, y1) + 0.05f;
            out.paths.positions.push_back({x0, y0, z00});
            out.paths.positions.push_back({x1, y0, z10});
            out.paths.positions.push_back({x1, y1, z11});
            out.paths.positions.push_back({x0, y1, z01});
            for (int i = 0; i < 4; ++i) {
                out.paths.normals.push_back({0.0f, 0.0f, 1.0f});
            }
            // build_vertices adds +1 to v; -1..0 maps to 0..1 in the texture.
            out.paths.uvs.push_back({0.0f, -1.0f});
            out.paths.uvs.push_back({1.0f, -1.0f});
            out.paths.uvs.push_back({1.0f, 0.0f});
            out.paths.uvs.push_back({0.0f, 0.0f});
            out.paths.indices.push_back(base + 0);
            out.paths.indices.push_back(base + 1);
            out.paths.indices.push_back(base + 2);
            out.paths.indices.push_back(base + 0);
            out.paths.indices.push_back(base + 2);
            out.paths.indices.push_back(base + 3);
            ++out.path_tiles;
        }
    }

    if (fence_source.positions.empty()) {
        return out;
    }
    float min_y = fence_source.positions.front().y;
    float max_y = min_y;
    float mid_x = 0.0f;
    float mid_y = 0.0f;
    for (const auto& p : fence_source.positions) {
        min_y = std::min(min_y, p.y);
        max_y = std::max(max_y, p.y);
    }
    mid_x = 0.0f;
    mid_y = (min_y + max_y) * 0.5f;
    const float source_length = std::max(0.01f, max_y - min_y);
    const float scale = cs / source_length;

    out.fences.name = "build_fences";
    const std::size_t per_segment = fence_source.positions.size();
    out.fences.positions.reserve(static_cast<std::size_t>(grid.fence_count) * per_segment);
    out.fences.indices.reserve(static_cast<std::size_t>(grid.fence_count) *
                               fence_source.indices.size());

    for (int y = 0; y < grid.height; ++y) {
        for (int x = 0; x < grid.width; ++x) {
            for (int edge = 0; edge < 2; ++edge) {
                if (!grid.fence_at(x, y, edge)) {
                    continue;
                }
                const float x0 = grid.origin_x + static_cast<float>(x) * cs;
                const float y0 = grid.origin_y + static_cast<float>(y) * cs;
                float cx = x0 + cs * 0.5f;
                float cy = y0 + cs * 0.5f;
                float angle = 0.0f;
                if (edge == 0) {  // south edge runs along +X
                    cy = y0;
                    angle = -kPi * 0.5f;
                } else {  // east edge runs along +Y
                    cx = x0 + cs;
                }
                const float base_z = terrain.height_at(cx, cy);
                Vec3 offset{cx - (std::cos(angle) * mid_x - std::sin(angle) * mid_y) * scale,
                            cy - (std::sin(angle) * mid_x + std::cos(angle) * mid_y) * scale,
                            base_z};
                append_mesh(out.fences, fence_source, offset, angle, scale);
                ++out.fence_segments;
            }
        }
    }
    return out;
}

void import_roads(BuildGrid& grid, const std::vector<WorldRect>& roads) {
    if (grid.width <= 0 || grid.height <= 0 || grid.cell_size <= 0.0f) {
        return;
    }
    const float cell = grid.cell_size;
    const int last_x = grid.width - 1;
    const int last_y = grid.height - 1;
    for (const WorldRect& road : roads) {
        const int x0 = std::min(road.x0, road.x1);
        const int x1 = std::max(road.x0, road.x1);
        const int y0 = std::min(road.y0, road.y1);
        const int y1 = std::max(road.y0, road.y1);
        const int cx0 = std::clamp(static_cast<int>(std::floor(x0 / cell)), 0, last_x);
        const int cx1 = std::clamp(static_cast<int>(std::floor(x1 / cell)), 0, last_x);
        const int cy0 = std::clamp(static_cast<int>(std::floor(y0 / cell)), 0, last_y);
        const int cy1 = std::clamp(static_cast<int>(std::floor(y1 / cell)), 0, last_y);
        for (int y = cy0; y <= cy1; ++y) {
            for (int x = cx0; x <= cx1; ++x) {
                grid.set_path(x, y, true);
            }
        }
    }
}

void import_fences(BuildGrid& grid, const std::vector<WorldRect>& fences) {    if (grid.width <= 0 || grid.height <= 0 || grid.cell_size <= 0.0f) {
        return;
    }
    const float cell = grid.cell_size;
    for (const WorldRect& rect : fences) {
        const int x0 = std::min(rect.x0, rect.x1);
        const int x1 = std::max(rect.x0, rect.x1);
        const int y0 = std::min(rect.y0, rect.y1);
        const int y1 = std::max(rect.y0, rect.y1);
        int cx0 = static_cast<int>(std::floor(x0 / cell));
        int cx1 = static_cast<int>(std::floor(std::max(x1 - 1, x0) / cell));
        int cy0 = static_cast<int>(std::floor(y0 / cell));
        int cy1 = static_cast<int>(std::floor(std::max(y1 - 1, y0) / cell));
        cx0 = std::clamp(cx0, 0, grid.width - 1);
        cx1 = std::clamp(cx1, 0, grid.width - 1);
        cy0 = std::clamp(cy0, 0, grid.height - 1);
        cy1 = std::clamp(cy1, 0, grid.height - 1);
        for (int x = cx0; x <= cx1; ++x) {
            grid.set_fence(x, cy0, 0, true);          // south
            grid.set_fence(x, cy1 + 1, 0, true);      // north
        }
        for (int y = cy0; y <= cy1; ++y) {
            grid.set_fence(cx0 - 1, y, 1, true);      // west
            grid.set_fence(cx1, y, 1, true);          // east
        }
    }
}

float paddock_capacity(const WorldRect& rect, float cell_size) {
    if (cell_size <= 0.0f) {
        return 0.0f;
    }
    const float w = static_cast<float>(std::abs(rect.x1 - rect.x0) + 1);
    const float h = static_cast<float>(std::abs(rect.y1 - rect.y0) + 1);
    return (w / cell_size) * (h / cell_size);
}

float paddock_cost(int cost_index) {
    const int index = std::clamp(cost_index, 0, 3);
    return 8.0f * static_cast<float>(1 << index);
}

int paddock_size_class(int size_cm) {
    if (size_cm < 300) {
        return 0;
    }
    if (size_cm < 900) {
        return 1;
    }
    if (size_cm < 1400) {
        return 2;
    }
    return 3;
}

bool paddock_can_add(const Paddock& paddock, int dinosaur_id, int cost_index) {
    if (std::find(paddock.occupants.begin(), paddock.occupants.end(),
                  dinosaur_id) != paddock.occupants.end()) {
        return false;
    }
    return paddock.capacity - paddock.used >= paddock_cost(cost_index);
}

bool paddock_add(Paddock& paddock, int dinosaur_id, int cost_index) {
    if (!paddock_can_add(paddock, dinosaur_id, cost_index)) {
        return false;
    }
    paddock.occupants.push_back(dinosaur_id);
    paddock.used += paddock_cost(cost_index);
    return true;
}

void save_build(const BuildGrid& grid, const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("cannot write build file " + path);
    }
    out << "cell_size " << grid.cell_size << "\n";
    out << "size " << grid.width << " " << grid.height << "\n";
    for (int y = 0; y < grid.height; ++y) {
        for (int x = 0; x < grid.width; ++x) {
            if (grid.path_at(x, y)) {
                out << "path " << x << " " << y << "\n";
            }
            for (int edge = 0; edge < 2; ++edge) {
                if (grid.fence_at(x, y, edge)) {
                    out << "fence " << x << " " << y << " " << edge << "\n";
                }
            }
        }
    }
}

void load_build(BuildGrid& grid, const std::string& path, float world_size) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot read build file " + path);
    }
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        lines.push_back(line);
    }

    float cell_size = 4.0f;
    int stored_width = 0;
    int stored_height = 0;
    for (const auto& text : lines) {
        std::istringstream parts(text);
        std::string key;
        parts >> key;
        if (key == "cell_size") {
            parts >> cell_size;
        } else if (key == "size") {
            parts >> stored_width >> stored_height;
        }
    }
    if (stored_width <= 0 || stored_height <= 0) {
        throw std::runtime_error("build file has no grid size: " + path);
    }
    grid.init(cell_size, world_size);
    if (stored_width != grid.width || stored_height != grid.height) {
        throw std::runtime_error("build grid size does not match the terrain: " +
                                 path);
    }

    for (const auto& text : lines) {
        std::istringstream parts(text);
        std::string key;
        parts >> key;
        if (key == "path") {
            int x = 0, y = 0;
            parts >> x >> y;
            grid.set_path(x, y, true);
        } else if (key == "fence") {
            int x = 0, y = 0, edge = 0;
            parts >> x >> y >> edge;
            grid.set_fence(x, y, edge, true);
        }
    }
    grid.undo_stack.clear();
}

}  // namespace dino
