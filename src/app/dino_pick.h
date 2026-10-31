// Screen picking for the park dinosaurs and the DINO_INFO_PANEL opener.
//
// Retail hit-tests through a pixel-ID buffer (`FUN_005e0da0`) that holds
// every drawn object. The port keeps the last posed CPU mesh per dinosaur
// (`ImportedDino::pick_mesh`) and casts the same 80-degree camera ray as
// `pick_terrain`. The heightfield occludes; buildings/vegetation are not in
// the pick set yet, so a dinosaur behind them can still be picked.

#ifndef DINO_APP_DINO_PICK_H
#define DINO_APP_DINO_PICK_H

#include <vector>

#include "../formats/math.h"
#include "../ui/park.h"
#include "../world/imported_dino.h"
#include "../world/terrain.h"

namespace dino::app {

// Index into `dinos`, or -1. The hit must be nearer than the terrain and the
// nearest triangle of all candidates wins.
int pick_dino_index(const dino::Terrain& terrain, const dino::Vec3& eye,
                    const dino::Vec3& target,
                    const std::vector<dino::ImportedDino>& dinos,
                    float mouse_x, float mouse_y, int view_width,
                    int view_height);

// Opens `DINO_INFO_PANEL` on `dinos[index]` (retail `FUN_005135d0` stores the
// picked actor at the panel and shows it).
void open_dino_panel(dino::ui::ParkHud& park,
                     const std::vector<dino::ImportedDino>& dinos, int index);

}  // namespace dino::app

#endif  // DINO_APP_DINO_PICK_H
