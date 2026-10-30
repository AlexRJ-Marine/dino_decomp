// Linear-blend skinning on the CPU.
//
// The retail `.skin` names its bones and vertices; this maps them onto the
// `.skel` hierarchy and deforms the bind mesh. No SDL, no GL: the result is a
// plain dino::Mesh that the renderer uploads like any other.

#ifndef DINO_WORLD_SKINNING_H
#define DINO_WORLD_SKINNING_H

#include <vector>

#include "../formats/anim.h"
#include "../formats/mesh.h"
#include "../formats/skel.h"
#include "../formats/skin.h"

// Fills `vertex_bone` (one bone per vertex, -1 = rigid) and `bone_skel`
// (skin bone -> skeleton index). Throws when a skin bone is not in the
// skeleton.
void build_skin_mapping(const dino::Skin& skin, const dino::Skeleton& skeleton,
                        std::vector<int>& vertex_bone, std::vector<int>& bone_skel);

// Poses `bind` at `time`; `bind_from_skin` picks skin.inverse_bind instead of
// the skeleton rest inverse (pachy LOD2/LOD3 store a bent-neck bind).
dino::Mesh build_skinned_mesh(const dino::Mesh& bind, const dino::Skin& skin,
                              const dino::Skeleton& skeleton,
                              const dino::Animation* anim, double time,
                              const std::vector<int>& vertex_bone,
                              const std::vector<int>& bone_skel,
                              bool bind_from_skin);

#endif  // DINO_WORLD_SKINNING_H
