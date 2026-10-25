#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dino {

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

struct Vec2 {
    float u = 0.0f, v = 0.0f;
};

struct Color {
    std::uint8_t r = 255, g = 255, b = 255, a = 255;
};

struct Mesh {
    std::string name;
    std::uint32_t hash = 0;
    std::vector<Vec3> positions;
    std::vector<std::uint32_t> indices;
    std::vector<Vec2> uvs;
    std::vector<Vec3> normals;
    std::vector<Color> colors;

    std::size_t triangle_count() const { return indices.size() / 3; }
};

// Reads the retail .mesh format. Throws std::runtime_error on malformed input.
Mesh load_mesh(const std::string& path);

}  // namespace dino
