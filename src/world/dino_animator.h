#pragma once

#include "imported_dino.h"
#include "terrain.h"

namespace dino {

// Poses an imported dinosaur at `time` and places the mesh on the terrain.
// CPU-only; the caller uploads the result. This is the animation half of the
// old `Game` dino loop.
Mesh pose_imported_dino(const ImportedDino& dinosaur,
                        const DinoAssets& assets, const Terrain& terrain,
                        double time);

}  // namespace dino
