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

void append_mesh_scaled(Mesh& dst, const Mesh& src, const Vec3& offset,
                        float angle, const Vec3& scale) {
    const bool has_normals = src.normals.size() == src.positions.size();
    const bool has_uvs = src.uvs.size() == src.positions.size();
    const auto base = static_cast<std::uint32_t>(dst.positions.size());
    const float cs = std::cos(angle);
    const float sn = std::sin(angle);
    for (std::size_t i = 0; i < src.positions.size(); ++i) {
        const Vec3& p = src.positions[i];
        const float sx = p.x * scale.x;
        const float sy = p.y * scale.y;
        dst.positions.push_back({offset.x + cs * sx - sn * sy,
                                 offset.y + sn * sx + cs * sy,
                                 offset.z + p.z * scale.z});
        if (has_normals) {
            const Vec3& n = src.normals[i];
            dst.normals.push_back(normalize(
                {cs * (n.x / (scale.x != 0.0f ? scale.x : 1.0f)) -
                     sn * (n.y / (scale.y != 0.0f ? scale.y : 1.0f)),
                 sn * (n.x / (scale.x != 0.0f ? scale.x : 1.0f)) +
                     cs * (n.y / (scale.y != 0.0f ? scale.y : 1.0f)),
                 n.z / (scale.z != 0.0f ? scale.z : 1.0f)}));
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

WorldRect building_door(const BuildingDef& def, int cell_x, int cell_y,
                        int state) {
    const float w0 = static_cast<float>(def.footprint_w) - 1.0f;
    const float w1 = static_cast<float>(def.footprint_h) - 1.0f;
    const auto& d = def.door;
    auto t = [](float v) { return static_cast<int>(v); };  // __ftol truncates
    switch (((state % 4) + 4) % 4) {
        case 0:
            return {cell_x + t(d[0]), cell_y + t(d[1]), cell_x + t(d[2]),
                    cell_y + t(d[3])};
        case 1:
            return {cell_x + t(w1 - d[1]), cell_y + t(d[0]),
                    cell_x + t(w1 - d[3]), cell_y + t(d[2])};
        case 2:
            return {cell_x + t(w0 - d[0]), cell_y + t(w1 - d[1]),
                    cell_x + t(w0 - d[2]), cell_y + t(w1 - d[3])};
        default:
            return {cell_x + t(d[1]), cell_y + t(w0 - d[0]),
                    cell_x + t(d[3]), cell_y + t(w0 - d[2])};
    }
}

WorldRect entrance_access_road(const BuildingDef& def, int cell_x, int cell_y,
                               int state) {
    const WorldRect door = building_door(def, cell_x, cell_y, state);
    // `((v + (v >> 31 & 3)) >> 2) * 4`: divide by 4 toward zero.
    const int cx = ((door.x0 + door.x1) / 2) / 4 * 4;
    const int cy = ((door.y0 + door.y1) / 2) / 4 * 4;
    switch (((state % 4) + 4) % 4) {
        case 0:
            return {cx - 8, cy, cx + 52, cy};
        case 1:
            return {cx, cy - 8, cx, cy + 52};
        case 2:
            return {cx - 52, cy, cx + 8, cy};
        default:
            return {cx, cy - 52, cx, cy + 8};
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
    // TranslatorConstants.ini SizeCategory1..4MaxLength.
    if (size_cm <= 200) {
        return 0;
    }
    if (size_cm <= 620) {
        return 1;
    }
    if (size_cm <= 900) {
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

int RoadSegment::cells() const {
    return std::abs(x1 - x0) + std::abs(y1 - y0) + 1;
}

RoadSegment road_segment(int start_x, int start_y, int cursor_x, int cursor_y) {
    RoadSegment road{start_x, start_y, cursor_x, cursor_y};
    if (std::abs(cursor_y - start_y) < std::abs(cursor_x - start_x)) {
        road.y1 = start_y;
    } else {
        road.x1 = start_x;
    }
    return road;
}

int road_cost(const RoadSegment& road, std::uint32_t price) {
    // Retail uses the signed (end - start) sum, which wraps to ~1e9 for runs
    // toward -x/-y (unverified quirk); the port uses the distance.
    const int units = 4 * (std::abs(road.x1 - road.x0) + std::abs(road.y1 - road.y0));
    return static_cast<int>((static_cast<std::uint32_t>(units) * price) >> 2);
}

const char* road_warning(RoadCheck check) {
    switch (check) {
    case RoadCheck::BusySpace:
        return "&WARNING_BUSY_SPACE";
    case RoadCheck::InsidePerimeter:
        return "&WARNING_INSIDE_PERIMETER";
    case RoadCheck::TooShort:
        return "&WARNING_ROAD_TOO_SHORT";
    case RoadCheck::Ok:
        break;
    }
    return "";
}

namespace {

// 0x58ce80 / 0x58cfd0: the run's world box [4*x0, 4*x1 + 3] lies strictly
// inside a fence rectangle.
bool road_inside_perimeter(int x0, int y0, int x1, int y1,
                           const std::vector<WorldRect>& perimeters) {
    const int wx0 = 4 * std::min(x0, x1);
    const int wy0 = 4 * std::min(y0, y1);
    const int wx1 = 4 * std::max(x0, x1) + 3;
    const int wy1 = 4 * std::max(y0, y1) + 3;
    for (const WorldRect& r : perimeters) {
        const int rx0 = std::min(r.x0, r.x1);
        const int rx1 = std::max(r.x0, r.x1);
        const int ry0 = std::min(r.y0, r.y1);
        const int ry1 = std::max(r.y0, r.y1);
        if (rx0 < wx0 && ry0 < wy0 && wx1 < rx1 && wy1 < ry1) {
            return true;
        }
    }
    return false;
}

}  // namespace

RoadCheck check_road_start(const BuildGrid& grid, int x, int y,
                           const std::vector<WorldRect>& perimeters,
                           const RoadBusyFn& busy) {
    if (road_inside_perimeter(x, y, x, y, perimeters)) {
        return RoadCheck::InsidePerimeter;
    }
    if (grid.in_bounds(x, y) && !grid.path_at(x, y) && busy && busy(x, y)) {
        return RoadCheck::BusySpace;
    }
    return RoadCheck::Ok;
}

RoadCheck check_road(const BuildGrid& grid, const RoadSegment& road,
                     const std::vector<WorldRect>& perimeters,
                     const RoadBusyFn& busy) {
    if (road_inside_perimeter(road.x0, road.y0, road.x1, road.y1, perimeters)) {
        return RoadCheck::InsidePerimeter;
    }
    if (road.x0 == road.x1 && road.y0 == road.y1) {
        return RoadCheck::TooShort;
    }
    const int sx = road.x1 > road.x0 ? 1 : (road.x1 < road.x0 ? -1 : 0);
    const int sy = road.y1 > road.y0 ? 1 : (road.y1 < road.y0 ? -1 : 0);
    int x = road.x0;
    int y = road.y0;
    for (int i = 0; i < road.cells(); ++i) {
        // Cells outside the map are skipped, as in the retail loop.
        if (grid.in_bounds(x, y) && !grid.path_at(x, y) && busy && busy(x, y)) {
            return RoadCheck::BusySpace;
        }
        if (i + 1 < road.cells()) {
            // Retail fences occupy pathmap cells; the port stores them on
            // cell edges, so crossing one is the same "busy" failure.
            const bool fence = sx > 0   ? grid.fence_at(x, y, 1)
                               : sx < 0 ? grid.fence_at(x - 1, y, 1)
                               : sy > 0 ? grid.fence_at(x, y + 1, 0)
                                        : grid.fence_at(x, y, 0);
            if (fence) {
                return RoadCheck::BusySpace;
            }
        }
        x += sx;
        y += sy;
    }
    return RoadCheck::Ok;
}

int place_road(BuildGrid& grid, const RoadSegment& road) {
    const int sx = road.x1 > road.x0 ? 1 : (road.x1 < road.x0 ? -1 : 0);
    const int sy = road.y1 > road.y0 ? 1 : (road.y1 < road.y0 ? -1 : 0);
    int added = 0;
    int x = road.x0;
    int y = road.y0;
    for (int i = 0; i < road.cells(); ++i) {
        if (grid.in_bounds(x, y) && !grid.path_at(x, y)) {
            grid.set_path(x, y, true);
            ++added;
        }
        x += sx;
        y += sy;
    }
    return added;
}

Mesh road_helper_mesh(const Mesh& helper_box, const RoadSegment& road,
                      const BuildGrid& grid, const Terrain& terrain) {
    Mesh out;
    out.name = "road_helper";
    const int cells = road.cells();
    if (cells <= 0 || helper_box.positions.empty()) {
        return out;
    }
    const int sx = road.x1 > road.x0 ? 1 : (road.x1 < road.x0 ? -1 : 0);
    const int sy = road.y1 > road.y0 ? 1 : (road.y1 < road.y0 ? -1 : 0);
    // Retail draws one helper box per 4-unit step (0x5fa708..0x5fa891).
    // Keep that shape; the preview item writes depth so the box's back/side
    // faces do not add a second layer and saturate to white.
    const float inset = (grid.cell_size - 1.0f) * 0.5f;
    int x = road.x0;
    int y = road.y0;
    for (int i = 0; i < cells; ++i) {
        const float wx =
            grid.origin_x + static_cast<float>(x) * grid.cell_size + inset;
        const float wy =
            grid.origin_y + static_cast<float>(y) * grid.cell_size + inset;
        append_mesh_scaled(
            out, helper_box, {wx, wy, terrain.height_at(wx, wy)}, 0.0f,
            {grid.cell_size, grid.cell_size, grid.cell_size});
        x += sx;
        y += sy;
    }
    // White vertex colours keep the unlit helper at the exact material
    // colour (default mesh vertices would scale it by 220/255).
    out.colors.assign(out.positions.size(), Color{});
    return out;
}

Mesh fence_preview_disc(float x, float y, float z, float radius,
                        int segments) {
    Mesh out;
    out.name = "fence_preview";
    if (segments < 3) {
        segments = 3;
    }
    out.positions.push_back({x, y, z});
    out.normals.push_back({0.0f, 0.0f, 1.0f});
    out.uvs.push_back({0.0f, 0.0f});
    const float step = 2.0f * kPi / static_cast<float>(segments);
    for (int i = 0; i <= segments; ++i) {
        const float a = static_cast<float>(i) * step;
        out.positions.push_back(
            {x + radius * std::cos(a), y + radius * std::sin(a), z});
        out.normals.push_back({0.0f, 0.0f, 1.0f});
        out.uvs.push_back({0.0f, 0.0f});
    }
    for (int i = 1; i <= segments; ++i) {
        out.indices.push_back(0);
        out.indices.push_back(static_cast<std::uint32_t>(i));
        out.indices.push_back(static_cast<std::uint32_t>(i + 1));
    }
    // Default mesh vertices build with a 220/255 grey; a white colour array
    // keeps the unlit disc at the exact retail texture colour.
    out.colors.assign(out.positions.size(), Color{});
    return out;
}

void paint_roads(Image& image, const BuildGrid& grid, float world_size) {
    if (image.width <= 0 || image.height <= 0 || world_size <= 0.0f ||
        image.rgba.size() < static_cast<std::size_t>(image.width) * image.height * 4) {
        return;
    }
    const std::uint8_t fill[3] = {253, 198, 137};
    const std::uint8_t border[3] = {170, 130, 90};
    const bool outline = image.width == 1024 && image.height == 1024;
    const float sx = static_cast<float>(image.width) / world_size;
    const float sy = static_cast<float>(image.height) / world_size;
    const float half = world_size * 0.5f;
    auto put = [&](int px, int py, const std::uint8_t* rgb) {
        if (px < 0 || py < 0 || px >= image.width || py >= image.height) {
            return;
        }
        std::uint8_t* p = &image.rgba[(static_cast<std::size_t>(py) * image.width + px) * 4];
        p[0] = rgb[0];
        p[1] = rgb[1];
        p[2] = rgb[2];
        p[3] = 255;
    };
    for (int y = 0; y < grid.height; ++y) {
        for (int x = 0; x < grid.width; ++x) {
            if (!grid.path_at(x, y)) {
                continue;
            }
            // Retail world units (0..world_size) of this cell.
            const float u0 = grid.origin_x + x * grid.cell_size + half;
            const float w0 = grid.origin_y + y * grid.cell_size + half;
            const int px0 = static_cast<int>(std::floor(u0 * sx));
            const int px1 = static_cast<int>(std::floor((u0 + grid.cell_size) * sx)) - 1;
            // v = (h - 1) - y * scale: world y grows upward, rows downward.
            const int row_bottom = image.height - 1 - static_cast<int>(std::floor(w0 * sy));
            const int row_top =
                image.height - static_cast<int>(std::floor((w0 + grid.cell_size) * sy));
            const bool open_w = !grid.path_at(x - 1, y);
            const bool open_e = !grid.path_at(x + 1, y);
            const bool open_s = !grid.path_at(x, y - 1);
            const bool open_n = !grid.path_at(x, y + 1);
            for (int py = row_top; py <= row_bottom; ++py) {
                for (int px = px0; px <= px1; ++px) {
                    const bool edge =
                        outline && ((open_w && px == px0) || (open_e && px == px1) ||
                                    (open_n && py == row_top) ||
                                    (open_s && py == row_bottom));
                    put(px, py, edge ? border : fill);
                }
            }
        }
    }
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
