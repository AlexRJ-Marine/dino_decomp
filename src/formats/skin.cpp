#include "skin.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <utility>

#include "bytes.h"

namespace dino {
namespace {

std::size_t find_named_block(const std::vector<std::uint8_t>& d,
                             const std::string& name) {
    if (name.empty() || name.size() > 255) {
        return std::string::npos;
    }
    std::vector<std::uint8_t> pattern;
    const std::uint32_t len = static_cast<std::uint32_t>(name.size() + 1);
    pattern.push_back(static_cast<std::uint8_t>(len & 0xFF));
    pattern.push_back(static_cast<std::uint8_t>((len >> 8) & 0xFF));
    pattern.push_back(static_cast<std::uint8_t>((len >> 16) & 0xFF));
    pattern.push_back(static_cast<std::uint8_t>((len >> 24) & 0xFF));
    pattern.insert(pattern.end(), name.begin(), name.end());
    pattern.push_back(0);
    auto it = std::search(d.begin() + 0x11, d.end(), pattern.begin(), pattern.end());
    if (it == d.end()) {
        return std::string::npos;
    }
    return static_cast<std::size_t>(it - d.begin());
}

}  // namespace

Skin load_skin(const std::string& path, const std::string& mesh_name) {
    const auto d = read_binary_file(path);
    static constexpr std::uint8_t kMagic[5] = {1, 3, 0, 0, 0};
    if (d.size() < 0x11 || std::memcmp(d.data(), kMagic, 5) != 0) {
        throw std::runtime_error("skin: bad magic in " + path);
    }
    const std::uint32_t hash = read_u32(d, 5, "hash");
    if (read_u32(d, 9, "sentinel") != 0x02468ACEu) {
        throw std::runtime_error("skin: bad sentinel in " + path);
    }
    const std::size_t block = find_named_block(d, mesh_name);
    if (block == std::string::npos) {
        throw std::runtime_error("skin: full mesh block '" + mesh_name + "' not found in " + path);
    }
    const std::uint32_t name_len = read_u32(d, block, "name length");
    MeshBody body = parse_mesh_body(d, block + 4 + name_len, mesh_name, hash);
    if (body.mesh.positions.empty()) {
        throw std::runtime_error("skin: empty full mesh in " + path);
    }

    std::size_t off = body.tail_offset;
    const std::uint8_t color_flag = read_u8(d, off, "color flag");
    ++off;
    const std::uint32_t color_count = read_u32(d, off, "color count");
    off += 4;
    if (color_flag && color_count) {
        need_bytes(d, off, static_cast<std::size_t>(color_count) * 4, "colors");
        off += static_cast<std::size_t>(color_count) * 4;
    }

    const std::uint32_t material_count = read_u32(d, off, "material count");
    off += 4;
    for (std::uint32_t i = 0; i < material_count; ++i) {
        need_bytes(d, off, 21, "material header");
        off += 21;
        (void)read_string(d, off, "texture name");
        (void)read_string(d, off, "material name");
    }

    const std::uint32_t triangle_count = read_u32(d, off, "triangle count");
    off += 4;
    need_bytes(d, off, static_cast<std::size_t>(triangle_count) * 2, "triangle materials");
    off += static_cast<std::size_t>(triangle_count) * 2;
    (void)read_u32(d, off, "unknown 0");
    (void)read_u32(d, off + 4, "unknown 1");
    (void)read_u32(d, off + 8, "unknown 2");
    off += 12;
    const std::uint32_t bone_count = read_u32(d, off, "skin bone count");
    off += 4;
    if (bone_count > 10'000) {
        throw std::runtime_error("skin: implausible skin bone count in " + path);
    }

    Skin skin;
    skin.hash = hash;
    skin.mesh = std::move(body.mesh);
    skin.bones.resize(bone_count);
    for (std::uint32_t i = 0; i < bone_count; ++i) {
        SkinBone& bone = skin.bones[i];
        bone.name = read_string(d, off, "skin bone name");
        const std::uint32_t nidx = read_u32(d, off, "skin bone vertex count");
        off += 4;
        need_bytes(d, off, static_cast<std::size_t>(nidx) * 4, "skin bone vertices");
        bone.vertices.reserve(nidx);
        for (std::uint32_t k = 0; k < nidx; ++k) {
            const std::uint32_t index = read_u32(d, off, "skin vertex index");
            off += 4;
            if (index >= skin.mesh.positions.size()) {
                throw std::runtime_error("skin: vertex index out of range in " + path);
            }
            bone.vertices.push_back(index);
        }
        const std::uint32_t nw = read_u32(d, off, "skin bone weight count");
        off += 4;
        if (nw != nidx) {
            throw std::runtime_error("skin: weight count mismatch in " + path);
        }
        need_bytes(d, off, static_cast<std::size_t>(nw) * 4, "skin bone weights");
        bone.weights.reserve(nw);
        for (std::uint32_t k = 0; k < nw; ++k) {
            bone.weights.push_back(read_f32(d, off, "skin weight"));
            off += 4;
        }
        need_bytes(d, off, 64, "inverse bind");
        for (int k = 0; k < 16; ++k) {
            bone.inverse_bind.m[k] = read_f32(d, off + static_cast<std::size_t>(k) * 4,
                                              "inverse bind");
        }
        off += 64;
    }
    if (off != d.size()) {
        throw std::runtime_error("skin: trailing data in " + path);
    }
    return skin;
}

}  // namespace dino
