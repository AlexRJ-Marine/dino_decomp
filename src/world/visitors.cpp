#include "visitors.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
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
        return n > 0 ? static_cast<int>(next() % static_cast<unsigned>(n)) : 0;
    }
};

float cell_center_x(const BuildGrid& grid, int x) {
    return grid.origin_x + (static_cast<float>(x) + 0.5f) * grid.cell_size;
}

float cell_center_y(const BuildGrid& grid, int y) {
    return grid.origin_y + (static_cast<float>(y) + 0.5f) * grid.cell_size;
}

unsigned visitor_seed(int x, int y, std::size_t count) {
    return static_cast<unsigned>(x) * 73856093u ^
           static_cast<unsigned>(y) * 19349663u ^
           static_cast<unsigned>(count);
}

unsigned next_random(Visitor& visitor) {
    visitor.rng = visitor.rng * 1664525u + 1013904223u;
    return visitor.rng;
}

int random_range(Visitor& visitor, int n) {
    return n > 0 ? static_cast<int>(next_random(visitor) %
                                    static_cast<unsigned>(n))
                 : 0;
}

int clamp_need(int value) {
    return std::max(0, std::min(99, value));
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

bool nearest_path_cell(const BuildGrid& grid, float wx, float wy, int& out_x,
                       int& out_y) {
    bool found = false;
    float best = std::numeric_limits<float>::max();
    for (int y = 0; y < grid.height; ++y) {
        for (int x = 0; x < grid.width; ++x) {
            if (!grid.path_at(x, y)) {
                continue;
            }
            const float dx = cell_center_x(grid, x) - wx;
            const float dy = cell_center_y(grid, y) - wy;
            const float distance = dx * dx + dy * dy;
            if (distance < best) {
                best = distance;
                out_x = x;
                out_y = y;
                found = true;
            }
        }
    }
    return found;
}

std::vector<std::pair<int, int>> bfs_path(const BuildGrid& grid, int sx,
                                          int sy, int tx, int ty) {
    std::vector<std::pair<int, int>> path;
    if (!grid.path_at(sx, sy) || !grid.path_at(tx, ty)) {
        return path;
    }
    if (sx == tx && sy == ty) {
        return path;
    }
    const int width = grid.width;
    const int height = grid.height;
    const int start = sy * width + sx;
    const int target = ty * width + tx;
    std::vector<int> previous(static_cast<std::size_t>(width) * height, -1);
    std::vector<int> queue;
    queue.reserve(static_cast<std::size_t>(width) * height / 4 + 1);
    previous[start] = start;
    queue.push_back(start);
    static constexpr int kDx[4] = {1, -1, 0, 0};
    static constexpr int kDy[4] = {0, 0, 1, -1};
    std::size_t head = 0;
    while (head < queue.size()) {
        const int current = queue[head++];
        if (current == target) {
            break;
        }
        const int cx = current % width;
        const int cy = current / width;
        for (int i = 0; i < 4; ++i) {
            const int nx = cx + kDx[i];
            const int ny = cy + kDy[i];
            if (!grid.path_at(nx, ny)) {
                continue;
            }
            const int next = ny * width + nx;
            if (previous[next] != -1) {
                continue;
            }
            previous[next] = current;
            queue.push_back(next);
        }
    }
    if (previous[target] == -1) {
        return path;
    }
    for (int at = target; at != start; at = previous[at]) {
        path.emplace_back(at % width, at / width);
    }
    std::reverse(path.begin(), path.end());
    return path;
}

bool move_toward(Visitor& visitor, float tx, float ty, float speed, float dt) {
    const float dx = tx - visitor.x;
    const float dy = ty - visitor.y;
    const float distance = std::sqrt(dx * dx + dy * dy);
    visitor.phase += dt * 3.0f;
    if (distance < 1e-4f) {
        visitor.x = tx;
        visitor.y = ty;
        return true;
    }
    const float step = speed * dt;
    if (distance <= step) {
        visitor.x = tx;
        visitor.y = ty;
        return true;
    }
    visitor.x += dx / distance * step;
    visitor.y += dy / distance * step;
    visitor.angle = std::atan2(dy, dx);
    return false;
}

void arrive_at_cell(Visitor& visitor, int x, int y) {
    visitor.prev_x = visitor.cell_x;
    visitor.prev_y = visitor.cell_y;
    visitor.cell_x = x;
    visitor.cell_y = y;
    visitor.target_x = x;
    visitor.target_y = y;
}

float attraction_appeal(const Attraction& attraction,
                        const VisitorProfile& profile) {
    float sum = 0.0f;
    for (std::size_t i = 0; i < attraction.appeal_weights.size(); ++i) {
        sum += attraction.appeal_weights[i] * profile.weights[i];
    }
    return std::max(-100.0f, std::min(100.0f, sum));
}

float attraction_appeal(const Attraction& attraction,
                        const std::vector<VisitorProfile>& profiles,
                        int profile) {
    if (profile < 0 || profile >= static_cast<int>(profiles.size())) {
        return 0.0f;
    }
    return attraction_appeal(attraction, profiles[profile]);
}

void start_leaving(Visitor& visitor, const BuildGrid& grid, int exit_x,
                   int exit_y) {
    int tx = exit_x;
    int ty = exit_y;
    if (!grid.path_at(tx, ty)) {
        const bool exit_in_bounds = grid.in_bounds(exit_x, exit_y);
        const float wx = exit_in_bounds ? cell_center_x(grid, exit_x)
                                        : grid.origin_x;
        const float wy = exit_in_bounds ? cell_center_y(grid, exit_y)
                                        : grid.origin_y;
        if (!nearest_path_cell(grid, wx, wy, tx, ty)) {
            visitor.done = true;
            return;
        }
    }
    visitor.route = bfs_path(grid, visitor.cell_x, visitor.cell_y, tx, ty);
    visitor.route_index = 0;
    visitor.state = VisitorState::Leave;
    visitor.target_attraction = -1;
    if (visitor.route.empty()) {
        visitor.done = visitor.cell_x == tx && visitor.cell_y == ty;
    }
}

void apply_attraction_need(Visitor& visitor, const Attraction& attraction,
                           int profile,
                           const std::vector<VisitorProfile>& profiles) {
    if (attraction.need_delta1 > 0) {
        visitor.need1 = clamp_need(visitor.need1 + attraction.need_delta1);
    }
    if (attraction.need_delta2 > 0) {
        visitor.need2 = clamp_need(visitor.need2 + attraction.need_delta2);
    }
    const float scale =
        attraction.appeal_scale > 0.01f ? attraction.appeal_scale : 1.0f;
    const int target = static_cast<int>(
        attraction_appeal(attraction, profiles, profile));
    const int scaled = static_cast<int>(static_cast<float>(target) / scale);
    if (target < 0) {
        // Retail leaves need0 unbounded below: index 0 skips the 0 clamp.
        visitor.need0 += scaled;
    } else if (target <= visitor.need0_at_use) {
        visitor.need0 -= 1;
    } else {
        visitor.need0 = std::min(scaled + visitor.need0_at_use, target);
    }
    visitor.need0 = std::min(99, visitor.need0);
}

// Retail timed decay (FUN_00589cd0 -> FUN_00576320/FUN_005763f0):
// need1 -1 / 10 s; need2 -1 / 7.5 s in state 1 and -3 in state 3;
// need0 -1 / 12.5 s in states 0/1 and -10 in state 3, with the state-2
// appeal target applied on the same 12.5 s tick.
bool tick_needs(Visitor& visitor, float dt,
                const std::vector<Attraction>& attractions,
                const std::vector<VisitorProfile>& profiles) {
    visitor.decay1 += dt;
    while (visitor.decay1 >= 10.0f) {
        visitor.decay1 -= 10.0f;
        // FUN_00576320: an attraction that feeds the visitor pauses the
        // need1 decay for the duration of the use.
        const bool feeding =
            visitor.state == VisitorState::Use &&
            visitor.target_attraction >= 0 &&
            visitor.target_attraction <
                static_cast<int>(attractions.size()) &&
            attractions[static_cast<std::size_t>(
                visitor.target_attraction)].need_delta1 != 0;
        if (!feeding) {
            visitor.need1 = clamp_need(visitor.need1 - 1);
            if (visitor.need1 <= 0) {
                return true;
            }
        }
    }
    visitor.decay2 += dt;
    while (visitor.decay2 >= 7.5f) {
        visitor.decay2 -= 7.5f;
        if (visitor.state == VisitorState::Travel) {
            visitor.need2 = clamp_need(visitor.need2 - 1);
        } else if (visitor.state == VisitorState::Leave) {
            visitor.need2 = clamp_need(visitor.need2 - 3);
        }
        if (visitor.need2 <= 0) {
            return true;
        }
    }
    visitor.decay0 += dt;
    while (visitor.decay0 >= 12.5f) {
        visitor.decay0 -= 12.5f;
        if (visitor.state == VisitorState::Use &&
            visitor.target_attraction >= 0 &&
            visitor.target_attraction <
                static_cast<int>(attractions.size())) {
            apply_attraction_need(
                visitor,
                attractions[static_cast<std::size_t>(
                    visitor.target_attraction)],
                visitor.profile, profiles);
        } else if (visitor.state == VisitorState::Leave) {
            visitor.need0 -= 10;
        } else {
            visitor.need0 -= 1;
        }
        if (visitor.need0 < 0) {
            return true;
        }
    }
    return false;
}

void choose_attraction(Visitor& visitor, const BuildGrid& grid,
                       const std::vector<Attraction>& attractions,
                       const std::vector<VisitorProfile>& profiles,
                       int exit_x, int exit_y) {
    // Retail: 1 in 5 visitors leaves instead of picking another attraction.
    if (random_range(visitor, 5) == 0) {
        start_leaving(visitor, grid, exit_x, exit_y);
        return;
    }
    int best = -1;
    long long best_score = std::numeric_limits<long long>::min();
    int best_cell_x = -1;
    int best_cell_y = -1;
    for (std::size_t i = 0; i < attractions.size(); ++i) {
        const Attraction& attraction = attractions[i];
        if (!attraction.enabled || attraction.category == 10) {
            continue;
        }
        int tx = 0;
        int ty = 0;
        if (!nearest_path_cell(grid, attraction.world_x, attraction.world_y, tx,
                               ty)) {
            continue;
        }
        if (attraction.radius > 0.0f) {
            const float dx = (cell_center_x(grid, tx) - visitor.x);
            const float dy = (cell_center_y(grid, ty) - visitor.y);
            if (std::sqrt(dx * dx + dy * dy) > attraction.radius) {
                continue;
            }
        }
        const int distance =
            std::abs(visitor.cell_x - tx) + std::abs(visitor.cell_y - ty);
        const VisitorProfile fallback_profile;
        const VisitorProfile& scoring_profile =
            visitor.profile >= 0 &&
                    visitor.profile < static_cast<int>(profiles.size())
                ? profiles[static_cast<std::size_t>(visitor.profile)]
                : fallback_profile;
        const long long score = attraction_score(
            attraction, scoring_profile, visitor.need0, visitor.need1,
            visitor.need2, distance, random_range(visitor, 6000) - 3000);
        if (score > best_score) {
            best_score = score;
            best = static_cast<int>(i);
            best_cell_x = tx;
            best_cell_y = ty;
        }
    }
    if (best < 0) {
        start_leaving(visitor, grid, exit_x, exit_y);
        return;
    }
    visitor.route =
        bfs_path(grid, visitor.cell_x, visitor.cell_y, best_cell_x, best_cell_y);
    visitor.route_index = 0;
    visitor.target_attraction = best;
    visitor.state = VisitorState::Travel;
    if (visitor.route.empty()) {
        if (visitor.cell_x == best_cell_x && visitor.cell_y == best_cell_y) {
            visitor.state = VisitorState::Use;
            visitor.use_timer = 0.0f;
            visitor.need0_at_use = visitor.need0;
        } else {
            // The target is disconnected from the path network.
            visitor.target_attraction = -1;
            start_leaving(visitor, grid, exit_x, exit_y);
        }
    }
}

void update_ai(Visitor& visitor, const BuildGrid& grid, float dt,
               const std::vector<Attraction>& attractions,
               const std::vector<VisitorProfile>& profiles, int exit_x,
               int exit_y) {
    const float walk_speed =
        visitor.profile >= 0 &&
                visitor.profile < static_cast<int>(profiles.size()) &&
                profiles[visitor.profile].walk_speed > 0.05f
            ? profiles[visitor.profile].walk_speed
            : 1.6f;
    if (tick_needs(visitor, dt, attractions, profiles) &&
        visitor.state != VisitorState::Leave) {
        start_leaving(visitor, grid, exit_x, exit_y);
    }
    switch (visitor.state) {
        case VisitorState::Choose:
            choose_attraction(visitor, grid, attractions, profiles, exit_x,
                              exit_y);
            break;
        case VisitorState::Travel: {
            if (visitor.target_attraction < 0 ||
                visitor.target_attraction >=
                    static_cast<int>(attractions.size())) {
                start_leaving(visitor, grid, exit_x, exit_y);
                break;
            }
            if (visitor.route_index >= visitor.route.size()) {
                visitor.state = VisitorState::Use;
                visitor.use_timer = 0.0f;
                visitor.need0_at_use = visitor.need0;
                break;
            }
            const auto [tx, ty] = visitor.route[visitor.route_index];
            const float wx = cell_center_x(grid, tx);
            const float wy = cell_center_y(grid, ty);
            if (move_toward(visitor, wx, wy, walk_speed, dt)) {
                arrive_at_cell(visitor, tx, ty);
                ++visitor.route_index;
                if (visitor.route_index >= visitor.route.size()) {
                    visitor.state = VisitorState::Use;
                    visitor.use_timer = 0.0f;
                    visitor.need0_at_use = visitor.need0;
                }
            }
            break;
        }
        case VisitorState::Use: {
            if (visitor.target_attraction < 0 ||
                visitor.target_attraction >=
                    static_cast<int>(attractions.size())) {
                visitor.state = VisitorState::Choose;
                break;
            }
            const Attraction& attraction =
                attractions[static_cast<std::size_t>(
                    visitor.target_attraction)];
            visitor.use_timer += dt;
            if (visitor.use_timer >= std::max(0.1f, attraction.duration)) {
                apply_attraction_need(visitor, attraction, visitor.profile,
                                      profiles);
                visitor.target_attraction = -1;
                visitor.state = VisitorState::Choose;
            }
            break;
        }
        case VisitorState::Leave: {
            if (visitor.route_index >= visitor.route.size()) {
                visitor.done = true;
                break;
            }
            const auto [tx, ty] = visitor.route[visitor.route_index];
            const float wx = cell_center_x(grid, tx);
            const float wy = cell_center_y(grid, ty);
            if (move_toward(visitor, wx, wy, walk_speed, dt)) {
                arrive_at_cell(visitor, tx, ty);
                ++visitor.route_index;
                if (visitor.route_index >= visitor.route.size()) {
                    visitor.done = true;
                }
            }
            break;
        }
    }
}

}  // namespace

long long attraction_score(const Attraction& attraction,
                           const VisitorProfile& profile, int need0,
                           int need1, int need2, int distance,
                           int random_base) {
    const float appeal = attraction_appeal(attraction, profile);
    const float scale =
        attraction.appeal_scale > 0.01f ? attraction.appeal_scale : 1.0f;
    const double n0 = 99 - need0;
    const double n1 = 99 - need1;
    const double n2 = 99 - need2;
    double score = random_base;
    score += (static_cast<double>(appeal) / scale) * n0 * n0;
    score += static_cast<double>(attraction.need_delta1) * n1 * n1;
    score += static_cast<double>(attraction.need_delta2) * n2 * n2;
    score -= 300.0 * distance;
    score -= 500.0 * attraction.price;
    return static_cast<long long>(score);
}

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

int VisitorSystem::spawn(const BuildGrid& grid, int count,
                         const std::vector<VisitorProfile>& profiles,
                         int exit_x, int exit_y, unsigned seed) {
    (void)exit_x;
    (void)exit_y;
    visitors.clear();
    std::vector<std::pair<int, int>> cells;
    for (int y = 0; y < grid.height; ++y) {
        for (int x = 0; x < grid.width; ++x) {
            if (grid.path_at(x, y)) {
                cells.emplace_back(x, y);
            }
        }
    }
    if (cells.empty() || count <= 0 || profiles.empty()) {
        return 0;
    }
    Rng rng(seed);
    int total_weight = 0;
    for (const VisitorProfile& profile : profiles) {
        total_weight += std::max(0, profile.spawn_weight);
    }
    if (total_weight <= 0) {
        total_weight = static_cast<int>(profiles.size());
    }
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
        int roll = rng.range(total_weight);
        int profile = 0;
        for (std::size_t p = 0; p < profiles.size(); ++p) {
            roll -= std::max(0, profiles[p].spawn_weight);
            if (roll < 0) {
                profile = static_cast<int>(p);
                break;
            }
        }
        visitor.profile = profile;
        visitor.need0 = 99;
        visitor.need1 = 99;
        visitor.need2 = 99;
        visitor.state = VisitorState::Choose;
        visitor.rng = visitor_seed(cell.first, cell.second,
                                   visitors.size() + 1) ^
                      (static_cast<unsigned>(i) * 2654435761u);
        visitor.phase = rng.unit() * 2.0f;
        visitor.angle = 0.0f;
        visitors.push_back(visitor);
    }
    return static_cast<int>(visitors.size());
}

void VisitorSystem::update(const BuildGrid& grid, const Terrain& terrain,
                           float dt,
                           const std::vector<Attraction>& attractions,
                           int exit_x, int exit_y,
                           const std::vector<VisitorProfile>& profiles) {
    (void)terrain;
    if (visitors.empty() || grid.width <= 0) {
        return;
    }
    const bool use_ai = !attractions.empty();
    if (use_ai) {
        for (Visitor& visitor : visitors) {
            if (visitor.done) {
                continue;
            }
            update_ai(visitor, grid, dt, attractions, profiles, exit_x, exit_y);
        }
        visitors.erase(
            std::remove_if(visitors.begin(), visitors.end(),
                           [](const Visitor& visitor) { return visitor.done; }),
            visitors.end());
        return;
    }
    const float step = speed * dt;
    for (Visitor& visitor : visitors) {
        if (visitor.target_x == visitor.cell_x &&
            visitor.target_y == visitor.cell_y) {
            Rng rng(visitor_seed(visitor.cell_x, visitor.cell_y,
                                 visitors.size()));
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
            Rng rng(visitor_seed(visitor.cell_x, visitor.cell_y,
                                 visitors.size()));
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
