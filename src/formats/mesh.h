#pragma once

#include <cstddef>
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

struct Material {
    std::string texture;
    std::string name;
};

struct Mesh {
    std::string name;
    std::uint32_t hash = 0;
    std::vector<Vec3> positions;
    std::vector<std::uint32_t> indices;
    std::vector<Vec2> uvs;
    std::vector<Vec3> normals;
    std::vector<Color> colors;
    std::vector<Material> materials;

    std::size_t triangle_count() const { return indices.size() / 3; }

    // Texture of the first material, if the tail carried a material table.
    const std::string& texture_name() const {
        static const std::string empty;
        return materials.empty() ? empty : materials.front().texture;
    }
};

struct MeshBody {
    Mesh mesh;
    std::size_t tail_offset = 0;  // first byte after the normal block
};

std::vector<std::uint8_t> read_binary_file(const std::string& path);

// Parses a mesh body starting at the vertex_count u32.
MeshBody parse_mesh_body(const std::vector<std::uint8_t>& data, std::size_t offset,
                         std::string name, std::uint32_t hash);

// Reads the retail .mesh format. Throws std::runtime_error on malformed input.
Mesh load_mesh(const std::string& path);

}  // namespace dino
