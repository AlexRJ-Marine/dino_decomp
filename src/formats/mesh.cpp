#include "mesh.h"

#include <cstring>
#include <fstream>
#include <stdexcept>
#include <utility>

#include "bytes.h"

namespace dino {
namespace {

void need(const std::vector<std::uint8_t>& d, std::size_t off, std::size_t n,
          const char* what) {
    if (off > d.size() || n > d.size() - off) {
        throw std::runtime_error(std::string("mesh: ") + what + " past EOF");
    }
}

std::uint8_t u8(const std::vector<std::uint8_t>& d, std::size_t off, const char* what) {
    need(d, off, 1, what);
    return d[off];
}

std::uint32_t u32(const std::vector<std::uint8_t>& d, std::size_t off, const char* what) {
    need(d, off, 4, what);
    return static_cast<std::uint32_t>(d[off]) |
           (static_cast<std::uint32_t>(d[off + 1]) << 8) |
           (static_cast<std::uint32_t>(d[off + 2]) << 16) |
           (static_cast<std::uint32_t>(d[off + 3]) << 24);
}

float f32(const std::vector<std::uint8_t>& d, std::size_t off, const char* what) {
    const std::uint32_t bits = u32(d, off, what);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

}  // namespace

std::vector<std::uint8_t> read_binary_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot open " + path);
    }
    in.seekg(0, std::ios::end);
    const auto size = in.tellg();
    if (size < 0) {
        throw std::runtime_error("cannot size " + path);
    }
    in.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
    if (!data.empty()) {
        in.read(reinterpret_cast<char*>(data.data()), size);
        if (!in) {
            throw std::runtime_error("short read " + path);
        }
    }
    return data;
}

MeshBody parse_mesh_body(const std::vector<std::uint8_t>& d, std::size_t off,
                         std::string name, std::uint32_t hash) {
    Mesh mesh;
    mesh.name = std::move(name);
    mesh.hash = hash;

    const std::uint32_t vertex_count = u32(d, off, "vertex_count");
    off += 4;
    if (vertex_count > 5'000'000u) {
        throw std::runtime_error("mesh: implausible vertex count");
    }
    need(d, off, static_cast<std::size_t>(vertex_count) * 12, "positions");
    mesh.positions.reserve(vertex_count);
    for (std::uint32_t i = 0; i < vertex_count; ++i) {
        mesh.positions.push_back({f32(d, off, "position x"),
                                  f32(d, off + 4, "position y"),
                                  f32(d, off + 8, "position z")});
        off += 12;
    }

    const std::uint32_t index_count = u32(d, off, "index_count");
    off += 4;
    if (index_count > 30'000'000u || index_count % 3 != 0) {
        throw std::runtime_error("mesh: bad index count");
    }
    need(d, off, static_cast<std::size_t>(index_count) * 4, "indices");
    mesh.indices.reserve(index_count);
    for (std::uint32_t i = 0; i < index_count; ++i) {
        const std::uint32_t index = u32(d, off, "index");
        off += 4;
        if (index >= vertex_count) {
            throw std::runtime_error("mesh: index out of range");
        }
        mesh.indices.push_back(index);
    }

    // Each optional block is `u8 flag, u32 count`; a cleared flag still
    // carries a zero count (entrance_crash/destroy, helper_building).
    const std::uint8_t uv_flag = u8(d, off, "uv_flag");
    ++off;
    if (!uv_flag) {
        off += 4;
    } else {
        const std::uint32_t uv_count = u32(d, off, "uv_count");
        off += 4;
        if (uv_count != vertex_count) {
            throw std::runtime_error("mesh: uv count mismatch");
        }
        need(d, off, static_cast<std::size_t>(uv_count) * 8, "uvs");
        mesh.uvs.reserve(uv_count);
        for (std::uint32_t i = 0; i < uv_count; ++i) {
            mesh.uvs.push_back({f32(d, off, "u"), f32(d, off + 4, "v")});
            off += 8;
        }
    }

    const std::uint8_t normal_flag = u8(d, off, "normal_flag");
    ++off;
    if (!normal_flag) {
        off += 4;
    } else {
        const std::uint32_t normal_count = u32(d, off, "normal_count");
        off += 4;
        if (normal_count != vertex_count) {
            throw std::runtime_error("mesh: normal count mismatch");
        }
        need(d, off, static_cast<std::size_t>(normal_count) * 12, "normals");
        mesh.normals.reserve(normal_count);
        for (std::uint32_t i = 0; i < normal_count; ++i) {
            mesh.normals.push_back({f32(d, off, "normal x"),
                                    f32(d, off + 4, "normal y"),
                                    f32(d, off + 8, "normal z")});
            off += 12;
        }
    }

    const std::size_t tail_offset = off;

    // Optional per-vertex RGBA table in the tail (65 dinosaur meshes). The
    // block is a u8 flag followed by a u32 count, always present in retail
    // meshes (flag 0 on the landscape meshes).
    if (off + 5 <= d.size() && d[off] == 1) {
        const std::uint32_t color_count = u32(d, off + 1, "color_count");
        if (color_count == vertex_count &&
            off + 5 + static_cast<std::size_t>(color_count) * 4 <= d.size()) {
            std::size_t c = off + 5;
            mesh.colors.reserve(color_count);
            for (std::uint32_t i = 0; i < color_count; ++i) {
                mesh.colors.push_back({d[c], d[c + 1], d[c + 2], d[c + 3]});
                c += 4;
            }
        }
    }

    // Material table: u32 count, then per material 21 bytes + texture name +
    // material name. The two candidate offsets cover the colour-block flag
    // byte being present (retail) or absent (defensive).
    auto parse_materials = [&](std::size_t start) -> bool {
        if (start + 4 > d.size()) {
            return false;
        }
        const std::uint32_t material_count = u32(d, start, "material_count");
        if (material_count == 0 || material_count > 64) {
            return false;
        }
        std::size_t pos = start + 4;
        std::vector<Material> parsed;
        try {
            for (std::uint32_t i = 0; i < material_count; ++i) {
                need(d, pos, 21, "material header");
                pos += 21;
                Material material;
                material.texture = read_string(d, pos, "material texture");
                material.name = read_string(d, pos, "material name");
                parsed.push_back(std::move(material));
            }
        } catch (const std::exception&) {
            return false;
        }
        if (parsed.empty()) {
            return false;
        }
        mesh.materials = std::move(parsed);
        return true;
    };
    if (!parse_materials(tail_offset + 5)) {
        (void)parse_materials(tail_offset);
    }
    return MeshBody{std::move(mesh), tail_offset};
}

namespace {

void check_header(const std::vector<std::uint8_t>& d, const std::string& path) {
    static constexpr std::uint8_t kMagic[5] = {1, 3, 0, 0, 0};
    need(d, 0, 21, "header");
    if (std::memcmp(d.data(), kMagic, 5) != 0) {
        throw std::runtime_error("mesh: bad magic in " + path);
    }
    if (u32(d, 9, "sentinel") != 0x12345678u) {
        throw std::runtime_error("mesh: bad sentinel in " + path);
    }
}

std::string chunk_name(const std::vector<std::uint8_t>& d, std::size_t& off,
                       const std::string& path) {
    const std::uint32_t name_len = u32(d, off, "name_len");
    if (name_len == 0 || name_len > 256) {
        throw std::runtime_error("mesh: bad name length in " + path);
    }
    need(d, off + 4, name_len, "name");
    if (d[off + 4 + name_len - 1] != 0) {
        throw std::runtime_error("mesh: name is not NUL-terminated in " + path);
    }
    std::string name(reinterpret_cast<const char*>(d.data() + off + 4), name_len - 1);
    off += 4 + name_len;
    return name;
}

}  // namespace

std::vector<Mesh> load_mesh_chunks(const std::string& path) {
    const auto d = read_binary_file(path);
    check_header(d, path);
    const std::uint32_t hash = u32(d, 5, "hash");
    const std::uint32_t chunk_count = u32(d, 13, "chunk_count");
    if (chunk_count == 0 || chunk_count > 64) {
        throw std::runtime_error("mesh: bad chunk count in " + path);
    }
    std::vector<Mesh> chunks;
    std::size_t off = 17;
    for (std::uint32_t c = 0; c < chunk_count; ++c) {
        std::string name = chunk_name(d, off, path);
        MeshBody body = parse_mesh_body(d, off, std::move(name), hash);
        Mesh mesh = std::move(body.mesh);
        off = body.tail_offset;
        const std::uint8_t color_flag = u8(d, off, "color flag");
        const std::uint32_t color_count = u32(d, off + 1, "color count");
        off += 5;
        if (color_flag) {
            need(d, off, static_cast<std::size_t>(color_count) * 4, "colors");
            off += static_cast<std::size_t>(color_count) * 4;
        }
        const std::uint32_t material_count = u32(d, off, "material count");
        off += 4;
        if (material_count > 64) {
            throw std::runtime_error("mesh: bad material count in " + path);
        }
        mesh.materials.clear();
        for (std::uint32_t i = 0; i < material_count; ++i) {
            need(d, off, 21, "material header");
            off += 21;
            Material material;
            material.texture = read_string(d, off, "material texture");
            material.name = read_string(d, off, "material name");
            mesh.materials.push_back(std::move(material));
        }
        const std::uint32_t triangle_count = u32(d, off, "triangle count");
        off += 4;
        if (triangle_count != mesh.triangle_count()) {
            throw std::runtime_error("mesh: triangle material count mismatch in " +
                                     path);
        }
        need(d, off, static_cast<std::size_t>(triangle_count) * 2 + 16,
             "triangle materials");
        mesh.triangle_materials.reserve(triangle_count);
        for (std::uint32_t i = 0; i < triangle_count; ++i) {
            mesh.triangle_materials.push_back(static_cast<std::uint16_t>(
                d[off] | (d[off + 1] << 8)));
            off += 2;
        }
        off += 16;  // zero in every retail chunk
        chunks.push_back(std::move(mesh));
    }
    if (off != d.size()) {
        throw std::runtime_error("mesh: trailing data after chunks in " + path);
    }
    return chunks;
}

std::vector<Mesh> split_by_material(const Mesh& chunk) {
    if (chunk.materials.size() <= 1 ||
        chunk.triangle_materials.size() != chunk.triangle_count()) {
        return {chunk};
    }
    std::vector<Mesh> parts;
    for (std::size_t m = 0; m < chunk.materials.size(); ++m) {
        Mesh part;
        part.name = chunk.name;
        part.hash = chunk.hash;
        part.positions = chunk.positions;
        part.uvs = chunk.uvs;
        part.normals = chunk.normals;
        part.colors = chunk.colors;
        part.materials = {chunk.materials[m]};
        for (std::size_t t = 0; t < chunk.triangle_count(); ++t) {
            if (chunk.triangle_materials[t] != m) {
                continue;
            }
            part.indices.insert(part.indices.end(), chunk.indices.begin() + t * 3,
                                chunk.indices.begin() + t * 3 + 3);
            part.triangle_materials.push_back(0);
        }
        if (!part.indices.empty()) {
            parts.push_back(std::move(part));
        }
    }
    return parts;
}

Mesh load_mesh(const std::string& path) {
    const auto d = read_binary_file(path);
    static constexpr std::uint8_t kMagic[5] = {1, 3, 0, 0, 0};
    need(d, 0, 21, "header");
    if (std::memcmp(d.data(), kMagic, 5) != 0) {
        throw std::runtime_error("mesh: bad magic in " + path);
    }
    const std::uint32_t hash = u32(d, 5, "hash");
    const std::uint32_t sentinel = u32(d, 9, "sentinel");
    const std::uint32_t name_len = u32(d, 17, "name_len");
    if (sentinel != 0x12345678u) {
        throw std::runtime_error("mesh: bad sentinel in " + path);
    }
    if (name_len == 0 || name_len > 256) {
        throw std::runtime_error("mesh: bad name length in " + path);
    }
    need(d, 21, name_len, "name");
    if (d[21 + name_len - 1] != 0) {
        throw std::runtime_error("mesh: name is not NUL-terminated in " + path);
    }
    std::string name(reinterpret_cast<const char*>(d.data() + 21), name_len - 1);
    return parse_mesh_body(d, 21 + name_len, std::move(name), hash).mesh;
}

}  // namespace dino
