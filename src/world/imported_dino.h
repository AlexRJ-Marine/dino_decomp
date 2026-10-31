#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../formats/anim.h"
#include "../formats/dino_data.h"
#include "../formats/mesh.h"
#include "../formats/skel.h"
#include "../formats/skin.h"
#include "dino_actions.h"
#include "dino_motion.h"

namespace dino {

// Formats-only dino assets (mesh, skeleton, skin, animation and the two
// mapping tables). GL names stay in the app/render layer; this type is safe
// for `world` and is shared between dinos of the same species/clip.
struct DinoAssets {
    Mesh mesh;
    Skeleton skeleton;
    Skin skin;
    Animation anim;
    std::vector<int> vertex_bone;
    std::vector<int> bone_skel;
};

// Live dinosaur simulation state. The draw/GPU state lives in the app-side
// ImportedDinoDraw at the same index (architecture finding A9).
struct ImportedDino {
    std::shared_ptr<const DinoAssets> assets;
    std::string species;
    std::string family;
    std::string clip;
    bool carnivore = true;  // diet gene 14; drives the food source match
    // Retail's path object fixes the paddock rect (`path+0x1c0`) at creation
    // (`FUN_0055ac30`); it is never re-resolved from the live position. The
    // port adopts the spawn paddock once, so a boundary cell that happens to
    // fall in a neighbouring rect cannot re-home the dino.
    int paddock = -1;
    bool escape_logged = false;
    DinoNeeds needs;
    DinoAttributes attributes;  // phenotype snapshot for objectives
    std::optional<DinoActionLoop> action_loop;
    // `saDinosaur+0x2c8`: the move-command queue and integrator. Retail's
    // behavior update queues records; this tick is independent of the action
    // loop and writes the live position/facing.
    DinoMover mover;
    float x = 0.0f;
    float y = 0.0f;
    float angle = 0.0f;
    double phase = 0.0;
};

}  // namespace dino
