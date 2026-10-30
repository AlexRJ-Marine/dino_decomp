// Linear-blend skinning (see skinning.h).

#include "skinning.h"

#include "../formats/math.h"

#include <cstdint>
#include <stdexcept>

void build_skin_mapping(const dino::Skin& skin, const dino::Skeleton& skeleton,
                        std::vector<int>& vertex_bone, std::vector<int>& bone_skel) {
    vertex_bone.assign(skin.mesh.positions.size(), -1);
    bone_skel.assign(skin.bones.size(), -1);
    for (std::size_t b = 0; b < skin.bones.size(); ++b) {
        const int index = skeleton.index_of(skin.bones[b].name);
        if (index < 0) {
            throw std::runtime_error("skin bone '" + skin.bones[b].name +
                                     "' is not in the skeleton");
        }
        bone_skel[b] = index;
        for (const std::uint32_t v : skin.bones[b].vertices) {
            vertex_bone[v] = static_cast<int>(b);
        }
    }
}

dino::Mesh build_skinned_mesh(const dino::Mesh& bind, const dino::Skin& skin,
                              const dino::Skeleton& skeleton,
                              const dino::Animation* anim, double time,
                              const std::vector<int>& vertex_bone,
                              const std::vector<int>& bone_skel,
                              bool bind_from_skin) {
    std::vector<dino::Mat4> locals;
    locals.reserve(skeleton.bones.size());
    for (const auto& bone : skeleton.bones) {
        locals.push_back(bone.local);
    }
    if (anim != nullptr) {
        for (const auto& bone : anim->bones) {
            const int index = skeleton.index_of(bone.name);
            if (index >= 0) {
                locals[static_cast<std::size_t>(index)] = bone.sample(time);
            }
        }
    }
    const auto world = skeleton.world_matrices(locals);
    std::vector<dino::Mat4> raw;
    raw.reserve(world.size());
    for (const auto& matrix : world) {
        raw.push_back(dino::raw_centimetres(matrix));
    }

    // `mesh` bind: use the skeleton rest inverse so the standing .mesh is
    // exact at rest for every LOD (pachy LOD2/LOD3 store a bent-neck bind).
    // `skin` bind: the source mesh is in the skin's bind pose, so use the
    // stored inverse_bind.
    std::vector<dino::Mat4> rest_inverse;
    if (!bind_from_skin) {
        const auto rest_world = skeleton.world_matrices();
        rest_inverse.reserve(rest_world.size());
        for (const auto& matrix : rest_world) {
            rest_inverse.push_back(dino::inverse(dino::raw_centimetres(matrix)));
        }
    }

    std::vector<dino::Mat4> skin_matrices(skin.bones.size());
    for (std::size_t i = 0; i < skin.bones.size(); ++i) {
        const int skel_index = bone_skel[i];
        if (skel_index < 0) {
            skin_matrices[i] = dino::identity();
            continue;
        }
        const dino::Mat4& bind_matrix =
            bind_from_skin
                ? skin.bones[i].inverse_bind
                : rest_inverse[static_cast<std::size_t>(skel_index)];
        skin_matrices[i] = dino::multiply(
            bind_matrix, raw[static_cast<std::size_t>(skel_index)]);
    }

    dino::Mesh out = bind;
    const bool has_normals = bind.normals.size() == bind.positions.size();
    for (std::size_t v = 0; v < bind.positions.size(); ++v) {
        const int bone = vertex_bone[v];
        dino::Vec3 p = bind.positions[v];
        dino::Vec3 n = has_normals ? bind.normals[v] : dino::Vec3{0, 0, 1};
        if (bone >= 0) {
            const dino::Mat4& m = skin_matrices[static_cast<std::size_t>(bone)];
            p = dino::transform_point(m, p);
            n = dino::normalize(dino::transform_direction(m, n));
        }
        out.positions[v] = {p.x * 0.01f, p.y * 0.01f, p.z * 0.01f};
        if (has_normals) {
            out.normals[v] = n;
        }
    }
    return out;
}

