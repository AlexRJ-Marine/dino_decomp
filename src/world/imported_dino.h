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
    // Stable identity for paddock occupants, the info panel and sales. The
    // retail actor id; campaign imports used their vector index in older
    // port saves (id 0 = unassigned, assigned sequentially on load).
    int id = 0;
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
    // `FUN_005724d0` queued the 0x15 5 s death record once health hit 0.
    bool death_queued = false;
    DinoNeeds needs;
    DinoAttributes attributes;  // phenotype snapshot for objectives
    std::optional<DinoActionLoop> action_loop;
    // `saDinosaur+0x2c8`: the move-command queue and integrator. Retail's
    // behavior update queues records; this tick is independent of the action
    // loop and writes the live position/facing.
    DinoMover mover;
    // Eat stand cell (`FUN_0055b5b0`) cached for the current element;
    // -1 = not computed. Retail state 0 computes it once and state 1 walks
    // to it; recomputing every frame would retarget the mover.
    int eat_stand_x = -1;
    int eat_stand_y = -1;
    // `FUN_0056c3b0`: the Eat element was played and its meal applied once.
    bool eat_applied = false;
    // Feeder index the cached stand cell belongs to; -1 = none. The nearest
    // feeder can change as the dino moves or a source empties.
    int eat_feeder = -1;
    // RunAway: the `0x35` goto was issued; retail state 3 no longer re-scans.
    bool runaway_moved = false;
    // Last posed world-space mesh (for screen picking). Replaced each frame
    // by the app's `pose_imported_dino` result; `nullptr` before the first
    // pose.
    std::shared_ptr<const Mesh> pick_mesh;
    float x = 0.0f;
    float y = 0.0f;
    float angle = 0.0f;
    double phase = 0.0;
};

}  // namespace dino
