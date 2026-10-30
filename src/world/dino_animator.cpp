#include "dino_animator.h"

#include <utility>

#include "skinning.h"
#include "terrain_query.h"

namespace dino {

Mesh pose_imported_dino(const ImportedDino& dinosaur,
                        const DinoAssets& assets, const Terrain& terrain,
                        double time) {
    const Animation* anim = assets.anim.bones.empty() ? nullptr : &assets.anim;
    Mesh pose = build_skinned_mesh(assets.skin.mesh, assets.skin,
                                   assets.skeleton, anim, time,
                                   assets.vertex_bone, assets.bone_skel,
                                   false);
    if (assets.mesh.colors.size() == pose.positions.size()) {
        pose.colors = assets.mesh.colors;
    }
    return place_on_terrain(std::move(pose), terrain, dinosaur.x, dinosaur.y,
                            dinosaur.angle);
}

}  // namespace dino
