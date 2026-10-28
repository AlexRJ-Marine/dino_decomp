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
    // Per-triangle material index from the chunk trailer (load_mesh_chunks).
    std::vector<std::uint16_t> triangle_materials;

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
// Returns the first chunk only (the whole file for 516 of 536 retail meshes).
Mesh load_mesh(const std::string& path);

// Reads every chunk of a .mesh. `chunk_count` (0x0D) is 2..4 in 20 retail
// files, e.g. entrance_lod1.mesh = boat, radar, ponton2 (gangway) and ponton
// (dock). Each chunk is `u32 name_len, name, body, u8 colour flag, u32
// colour count, colours, u32 material count, materials, u32 triangle count,
// u16 material per triangle, 16 zero bytes`; the file ends after the last
// chunk (all 536 retail meshes, notes/entrance-dock-road-recon.md).
std::vector<Mesh> load_mesh_chunks(const std::string& path);

// Splits a chunk into one mesh per material that its triangles use, so each
// part can be drawn with its own texture. Vertices are shared, unused ones
// are kept.
std::vector<Mesh> split_by_material(const Mesh& chunk);

}  // namespace dino
