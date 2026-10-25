#include "mesh.h"

#include <cstring>
#include <fstream>
#include <stdexcept>
#include <utility>

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

    const std::uint8_t uv_flag = u8(d, off, "uv_flag");
    ++off;
    if (uv_flag) {
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
    if (normal_flag) {
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

    // Optional per-vertex RGBA table in the tail (65 dinosaur meshes).
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
    return MeshBody{std::move(mesh), tail_offset};
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
