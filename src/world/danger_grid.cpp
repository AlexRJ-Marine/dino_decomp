#include "danger_grid.h"

#include <algorithm>
#include <cmath>

namespace dino {

void DangerGrid::clear() {
    cells_.fill(0);
}

void DangerGrid::stamp(float x, float y, int radius, int threat) {
    if (threat == 0 || radius < 0 || !std::isfinite(x) || !std::isfinite(y)) {
        return;
    }
    // Retail works in integer world units and uses an arithmetic shift, i.e.
    // floor(x / 8). Floor the float position first so the port matches.
    const int ix = static_cast<int>(std::floor(x));
    const int iy = static_cast<int>(std::floor(y));
    const int x0 = std::max(0, (ix - radius) >> kCellShift);
    const int x1 = std::min(kWidth - 1, (ix + radius) >> kCellShift);
    const int y0 = std::max(0, (iy - radius) >> kCellShift);
    const int y1 = std::min(kHeight - 1, (iy + radius) >> kCellShift);
    if (x0 > x1 || y0 > y1) {
        return;
    }
    for (int gx = x0; gx <= x1; ++gx) {
        for (int gy = y0; gy <= y1; ++gy) {
            cell(gx, gy) += threat;
        }
    }
}

int DangerGrid::at(float x, float y) const {
    if (!std::isfinite(x) || !std::isfinite(y)) {
        return 0;
    }
    const int gx = static_cast<int>(std::floor(x)) >> kCellShift;
    const int gy = static_cast<int>(std::floor(y)) >> kCellShift;
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
