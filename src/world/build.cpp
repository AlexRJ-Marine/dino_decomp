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
    if (change.kind == 0) {
        auto& cell = path[static_cast<std::size_t>(change.y) * width + change.x];
        const bool current = cell != 0;
        if (current != change.value) {
            cell = change.value ? 1 : 0;
            path_count += change.value ? 1 : -1;
        }
    } else {
        const int edge = change.kind - 1;
        auto& cell = fences[static_cast<std::size_t>(change.y) * width + change.x];
        const std::uint8_t mask = static_cast<std::uint8_t>(1u << edge);
        const bool current = (cell & mask) != 0;
        if (current != change.value) {
            cell = static_cast<std::uint8_t>(change.value ? (cell | mask)
                                                          : (cell & ~mask));
            fence_count += change.value ? 1 : -1;
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
