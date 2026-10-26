#pragma once

#include <array>
#include <cstddef>

namespace dino {

// The retail danger grid (`FUN_005774F0` / `FUN_00571B50` / `FUN_00571440`).
//
// 128 x 128 cells of 8 raw world units (0..1024, the `.raw`/`.lnd` coordinate
// space). Every second `FUN_00589CD0` clears it, stamps each dinosaur's
// `+0x2a0` threat into the cells within its `+0x27c` radius, and recomputes
// every dinosaur's SECU from the cell under its position.
class DangerGrid {
public:
    // Retail uses an arithmetic shift by 3 (`>> 3`) on integer world units.
    static constexpr int kCellShift = 3;
    static constexpr int kCellSize = 1 << kCellShift;
    static constexpr int kWidth = 128;
    static constexpr int kHeight = 128;
    static constexpr std::size_t kCellCount =
        static_cast<std::size_t>(kWidth) * static_cast<std::size_t>(kHeight);
    static_assert(kCellSize == 8, "retail cells are 8 world units");

    // `FUN_005774F0`: zero every cell value.
    void clear();

    // `FUN_00571B50`: add `threat` to every cell in
    // [(x - radius) >> 3, (x + radius) >> 3] x
    // [(y - radius) >> 3, (y + radius) >> 3].
    // `x`/`y` are raw world units; out-of-island cells are ignored.
    void stamp(float x, float y, int radius, int threat);

    // Cell value under a raw world position (0 when outside the grid).
    int at(float x, float y) const;

    // Direct cell access for tests and diagnostics. `x`/`y` are cell indices.
    int& cell(int x, int y) {
        return cells_[static_cast<std::size_t>(x) * kHeight +
                      static_cast<std::size_t>(y)];
    }
    int cell(int x, int y) const {
        return cells_[static_cast<std::size_t>(x) * kHeight +
                      static_cast<std::size_t>(y)];
    }

    const std::array<int, kCellCount>& cells() const { return cells_; }

private:
    std::array<int, kCellCount> cells_{};
};

// `FUN_00571440`: SECU from the grid cell under the dinosaur.
//
//   secu = 99 + trunc((health - max_health) * 100 / max_health)
//             - trunc((cell - threat) * 0.1)
//   if (secu < 0) secu = 0
//
// `threat` is the dinosaur's own `+0x2a0` stamp value, `health`/`max_health`
// are the `int16 +0x16` / `int16 +0x1d4` pair. All divisions truncate toward
// zero like the retail `IDIV` / `__ftol` pair.
int secu_from_grid(const DangerGrid& grid, float x, float y, int threat,
                   int health, int max_health);

}  // namespace dino
