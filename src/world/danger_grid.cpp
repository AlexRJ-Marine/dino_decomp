#include "danger_grid.h"

#include <algorithm>
#include <cmath>

namespace dino {

void DangerGrid::clear() {
    cells_.fill(0);
}

void DangerGrid::stamp(float x, float y, int radius, int threat) {
    if (threat == 0) {
        return;
    }
    // Retail works in integer world units and uses an arithmetic shift, i.e.
    // floor(x / 8). Floor the float position first so the port matches.
    const int ix = static_cast<int>(std::floor(x));
    const int iy = static_cast<int>(std::floor(y));
    const int r = std::max(0, radius);
    const int x0 = (ix - r) >> 3;
    const int x1 = (ix + r) >> 3;
    const int y0 = (iy - r) >> 3;
    const int y1 = (iy + r) >> 3;
    for (int gx = x0; gx <= x1; ++gx) {
        if (gx < 0 || gx >= kWidth) {
            continue;
        }
        for (int gy = y0; gy <= y1; ++gy) {
            if (gy < 0 || gy >= kHeight) {
                continue;
            }
            cell(gx, gy) += threat;
        }
    }
}

int DangerGrid::at(float x, float y) const {
    if (!std::isfinite(x) || !std::isfinite(y)) {
        return 0;
    }
    const int gx = static_cast<int>(std::floor(x)) >> 3;
    const int gy = static_cast<int>(std::floor(y)) >> 3;
    if (gx < 0 || gx >= kWidth || gy < 0 || gy >= kHeight) {
        return 0;
    }
    return cell(gx, gy);
}

int secu_from_grid(const DangerGrid& grid, float x, float y, int threat,
                   int health, int max_health) {
    const int health_term =
        max_health > 0 ? (health - max_health) * 100 / max_health : 0;
    const int cell = grid.at(x, y);
    // C++ integer division truncates toward zero, matching the retail
    // __ftol((cell - threat) * 0.1f) and IDIV health term.
    const int threat_term = (cell - threat) / 10;
    return std::max(0, 99 + health_term - threat_term);
}

}  // namespace dino
