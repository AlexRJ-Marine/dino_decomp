#include "skel.h"

#include <cstring>
#include <functional>
#include <stdexcept>
#include <utility>

#include "bytes.h"

namespace dino {

int Skeleton::index_of(const std::string& name) const {
    for (std::size_t i = 0; i < bones.size(); ++i) {
        if (bones[i].name == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::vector<Mat4> Skeleton::world_matrices() const {
    std::vector<Mat4> locals;
    locals.reserve(bones.size());
    for (const auto& bone : bones) {
        locals.push_back(bone.local);
    }
    return world_matrices(locals);
}

std::vector<Mat4> Skeleton::world_matrices(const std::vector<Mat4>& locals) const {
    if (locals.size() != bones.size()) {
        throw std::runtime_error("skeleton: local matrix count mismatch");
    }
    const int n = static_cast<int>(bones.size());
    std::vector<Mat4> world(static_cast<std::size_t>(n));
    std::vector<int> state(static_cast<std::size_t>(n), 0);
    std::function<const Mat4&(int)> resolve = [&](int i) -> const Mat4& {
        if (state[i] == 2) {
            return world[i];
        }
        if (state[i] == 1) {
            throw std::runtime_error("skeleton: hierarchy cycle");
        }
        state[i] = 1;
        const int p = bones[i].parent;
        world[i] = p < 0 ? locals[i] : multiply(locals[i], resolve(p));
        state[i] = 2;
        return world[i];
    };
    for (int i = 0; i < n; ++i) {
        resolve(i);
    }
    return world;
}

Skeleton load_skel(const std::string& path) {
    const auto d = read_binary_file(path);
    static constexpr std::uint8_t kMagic[5] = {1, 3, 0, 0, 0};
    if (d.size() < 0x11 || std::memcmp(d.data(), kMagic, 5) != 0) {
        throw std::runtime_error("skel: bad magic in " + path);
    }
    const std::uint32_t hash = read_u32(d, 5, "hash");
    if (read_u32(d, 9, "sentinel") != 0x56789ABCu) {
        throw std::runtime_error("skel: bad sentinel in " + path);
    }
    const std::uint32_t count = read_u32(d, 0x0D, "bone count");
    if (count == 0 || count > 10'000) {
        throw std::runtime_error("skel: bad bone count in " + path);
    }

    std::size_t off = 0x11;
    std::vector<Mat4> matrices(count);
    need_bytes(d, off, static_cast<std::size_t>(count) * 64, "matrices");
    for (std::uint32_t i = 0; i < count; ++i) {
        for (int k = 0; k < 16; ++k) {
            matrices[i].m[k] = read_f32(d, off + static_cast<std::size_t>(k) * 4, "matrix");
        }
        off += 64;
    }

    const std::uint32_t child_count = read_u32(d, off, "child count");
    off += 4;
    if (child_count != count) {
        throw std::runtime_error("skel: child count mismatch in " + path);
    }
    for (std::uint32_t i = 0; i < child_count; ++i) {
        const std::uint32_t n = read_u32(d, off, "child list count");
        off += 4;
        need_bytes(d, off, static_cast<std::size_t>(n) * 4, "child list");
        off += static_cast<std::size_t>(n) * 4;
    }

    const std::uint32_t parent_count = read_u32(d, off, "parent count");
    off += 4;
    if (parent_count != count) {
        throw std::runtime_error("skel: parent count mismatch in " + path);
    }
    std::vector<int> parents(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        parents[i] = read_i32(d, off, "parent");
        off += 4;
    }

    const std::uint32_t name_count = read_u32(d, off, "name count");
    off += 4;
    if (name_count != count) {
        throw std::runtime_error("skel: name count mismatch in " + path);
    }
    std::vector<std::string> names(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        names[i] = read_string(d, off, "bone name");
    }

    const std::uint32_t flag_count = read_u32(d, off, "flag count");
    off += 4;
    if (flag_count != count) {
        throw std::runtime_error("skel: flag count mismatch in " + path);
    }
    need_bytes(d, off, static_cast<std::size_t>(flag_count) * 4, "flags");

    Skeleton skeleton;
    skeleton.hash = hash;
    skeleton.bones.resize(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        skeleton.bones[i].name = std::move(names[i]);
        skeleton.bones[i].local = matrices[i];
        skeleton.bones[i].parent = parents[i];
    }
    return skeleton;
}

}  // namespace dino
