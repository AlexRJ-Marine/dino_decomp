#include "terrain.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "../formats/mesh.h"  // read_binary_file

namespace dino {

std::vector<std::uint8_t> load_raw_u8(const std::string& path, int& width, int& height) {
    auto data = read_binary_file(path);
    if (data.empty()) {
        throw std::runtime_error("raw: empty file " + path);
    }
    const auto side = static_cast<int>(std::lround(std::sqrt(static_cast<double>(data.size()))));
    if (side * side != static_cast<int>(data.size())) {
        throw std::runtime_error("raw: not a square 8-bit heightmap: " + path);
    }
    width = side;
    height = side;
    return data;
}

Terrain build_terrain(const std::vector<std::uint8_t>& heights, int width, int height,
                      float world_size, float height_scale) {
    if (width < 2 || height < 2 || static_cast<std::size_t>(width) * height != heights.size()) {
        throw std::runtime_error("terrain: bad heightmap dimensions");
    }
    Terrain out;
    out.width = width;
    out.height = height;
    out.world_size = world_size;
    out.height_scale = height_scale;
    out.heights = heights;
    out.min_height = 255;
    out.max_height = 0;
    for (const auto h : heights) {
        out.min_height = std::min(out.min_height, h);
        out.max_height = std::max(out.max_height, h);
    }

    Mesh& mesh = out.mesh;
    mesh.name = "terrain";
    mesh.positions.reserve(static_cast<std::size_t>(width) * height);
    mesh.normals.reserve(static_cast<std::size_t>(width) * height);
    mesh.uvs.reserve(static_cast<std::size_t>(width) * height);

    // The engine heightfield is a 512-sample map over the 1024-unit world
    // (`map = world * 0.5`, `FUN_005c0b80`), so sample 0 sits at -512 and the
    // spacing is `world_size / width` (2 units), not `world_size / (w - 1)`.
    const float cell_x = world_size / static_cast<float>(width);
    const float cell_y = world_size / static_cast<float>(height);
    for (int y = 0; y < height; ++y) {
        const float v = static_cast<float>(y) / static_cast<float>(height - 1);
        for (int x = 0; x < width; ++x) {
            const float u = static_cast<float>(x) / static_cast<float>(width - 1);
            const float px = (static_cast<float>(x) - static_cast<float>(width) * 0.5f) * cell_x;
            const float py = (static_cast<float>(y) - static_cast<float>(height) * 0.5f) * cell_y;
            const float pz = static_cast<float>(heights[static_cast<std::size_t>(y) * width + x]) *
                             height_scale;
            mesh.positions.push_back({px, py, pz});

            const int xm = std::max(0, x - 1);
            const int xp = std::min(width - 1, x + 1);
            const int ym = std::max(0, y - 1);
            const int yp = std::min(height - 1, y + 1);
            const float hx = static_cast<float>(
                heights[static_cast<std::size_t>(y) * width + xp] -
                heights[static_cast<std::size_t>(y) * width + xm]) * height_scale /
                (static_cast<float>(xp - xm) * cell_x);
            const float hy = static_cast<float>(
                heights[static_cast<std::size_t>(yp) * width + x] -
                heights[static_cast<std::size_t>(ym) * width + x]) * height_scale /
                (static_cast<float>(yp - ym) * cell_y);
            const float nx = -hx;
            const float ny = -hy;
            const float nz = 1.0f;
            const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
            mesh.normals.push_back({nx / len, ny / len, nz / len});
            // Engine shader expects v_tex = 1 + v; map image row 0 to world y=0.
            mesh.uvs.push_back({u, -1.0f + v});
        }
    }

    mesh.indices.reserve(static_cast<std::size_t>(width - 1) * (height - 1) * 6);
    for (int y = 0; y < height - 1; ++y) {
        for (int x = 0; x < width - 1; ++x) {
            const std::uint32_t a = static_cast<std::uint32_t>(y * width + x);
            const std::uint32_t b = a + 1;
            const std::uint32_t c = a + static_cast<std::uint32_t>(width);
            const std::uint32_t d = c + 1;
            mesh.indices.push_back(a);
            mesh.indices.push_back(b);
            mesh.indices.push_back(c);
            mesh.indices.push_back(b);
            mesh.indices.push_back(d);
            mesh.indices.push_back(c);
        }
    }
    return out;
}

float Terrain::height_at(float world_x, float world_y) const {
    if (width < 2 || height < 2 || heights.empty() || world_size <= 0.0f) {
        return 0.0f;
    }
    const float u = (world_x / world_size + 0.5f) * static_cast<float>(width);
    const float v = (world_y / world_size + 0.5f) * static_cast<float>(height);
    const float cu = std::clamp(u, 0.0f, static_cast<float>(width - 1));
    const float cv = std::clamp(v, 0.0f, static_cast<float>(height - 1));
    const int x0 = static_cast<int>(cu);
    const int y0 = static_cast<int>(cv);
    const int x1 = std::min(width - 1, x0 + 1);
    const int y1 = std::min(height - 1, y0 + 1);
    const float fx = cu - static_cast<float>(x0);
    const float fy = cv - static_cast<float>(y0);
    auto sample = [&](int x, int y) {
        return static_cast<float>(heights[static_cast<std::size_t>(y) * width + x]);
    };
    const float h00 = sample(x0, y0);
    const float h10 = sample(x1, y0);
    const float h01 = sample(x0, y1);
    const float h11 = sample(x1, y1);
    const float h0 = h00 + (h10 - h00) * fx;
    const float h1 = h01 + (h11 - h01) * fx;
    return (h0 + (h1 - h0) * fy) * height_scale;
}

}  // namespace dino
