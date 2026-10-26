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

Skin parse_skin_at(const std::vector<std::uint8_t>& d, const std::string& path,
                   std::size_t block, const std::string& mesh_name) {
    const std::uint32_t hash = read_u32(d, 5, "hash");
    if (read_u32(d, 9, "sentinel") != 0x02468ACEu) {
        throw std::runtime_error("skin: bad sentinel in " + path);
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
    std::vector<Color> colors;
    if (color_flag && color_count) {
        need_bytes(d, off, static_cast<std::size_t>(color_count) * 4, "colors");
        colors.reserve(color_count);
        for (std::uint32_t i = 0; i < color_count; ++i) {
            colors.push_back({d[off], d[off + 1], d[off + 2], d[off + 3]});
            off += 4;
        }
    }

    const std::uint32_t material_count = read_u32(d, off, "material count");
    off += 4;
    std::vector<Material> materials;
    materials.reserve(material_count);
    for (std::uint32_t i = 0; i < material_count; ++i) {
        need_bytes(d, off, 21, "material header");
        off += 21;
        Material material;
        material.texture = read_string(d, off, "texture name");
        material.name = read_string(d, off, "material name");
        materials.push_back(std::move(material));
    }

    const std::uint32_t triangle_count = read_u32(d, off, "triangle count");
    off += 4;
    if (triangle_count != body.mesh.triangle_count()) {
        throw std::runtime_error("skin: triangle material count mismatch in " +
                                 path);
    }
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
    skin.mesh.colors = std::move(colors);
    skin.mesh.materials = std::move(materials);
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
    // Some building skins carry extra bytes after the last skin record (the
    // Python reader exposes the same range as `trailing`), so stop here.
    return skin;
}

}  // namespace

Skin load_skin(const std::string& path, const std::string& mesh_name) {
    const std::vector<std::uint8_t> d = read_binary_file(path);
    static constexpr std::uint8_t kMagic[5] = {1, 3, 0, 0, 0};
    if (d.size() < 0x11 || std::memcmp(d.data(), kMagic, 5) != 0) {
        throw std::runtime_error("skin: bad magic in " + path);
    }
    const std::size_t block = find_named_block(d, mesh_name);
    if (block == std::string::npos) {
        throw std::runtime_error("skin: full mesh block '" + mesh_name +
                                 "' not found in " + path);
    }
    return parse_skin_at(d, path, block, mesh_name);
}

Skin load_skin_auto(const std::string& path) {
    const std::vector<std::uint8_t> d = read_binary_file(path);
    static constexpr std::uint8_t kMagic[5] = {1, 3, 0, 0, 0};
    if (d.size() < 0x11 || std::memcmp(d.data(), kMagic, 5) != 0) {
        throw std::runtime_error("skin: bad magic in " + path);
    }
    // Without a sibling .mesh to anchor the full-mesh vertex count, try every
    // length-prefixed name whose following u32 is a plausible vertex count.
    // The full mesh is the first block whose tail parses as a material table
    // + triangle materials; several building skins have zero skin bones and
    // additional static parts after it.
    for (std::size_t pos = 0x11; pos + 8 <= d.size(); ++pos) {
        const std::uint32_t len = read_u32(d, pos, "candidate name length");
        if (len < 2 || len > 256 || pos + 4 + len + 4 > d.size()) {
            continue;
        }
        if (d[pos + 4 + len - 1] != 0) {
            continue;
        }
        bool printable = true;
        for (std::uint32_t i = 0; i + 1 < len; ++i) {
            const std::uint8_t c = d[pos + 4 + i];
            if (c != 0 && (c < 32 || c >= 127)) {
                printable = false;
                break;
            }
        }
        if (!printable) {
            continue;
        }
        const std::uint32_t vertex_count =
            read_u32(d, pos + 4 + len, "candidate vertex count");
        if (vertex_count == 0 || vertex_count > 500000) {
            continue;
        }
        const std::string name(
            reinterpret_cast<const char*>(d.data() + pos + 4), len - 1);
        try {
            Skin candidate = parse_skin_at(d, path, pos, name);
            if (!candidate.mesh.positions.empty() &&
                !candidate.mesh.materials.empty()) {
                return candidate;
            }
        } catch (const std::exception&) {
            continue;
        }
    }
    throw std::runtime_error("skin: no full mesh block found in " + path);
}

}  // namespace dino
