#pragma once

#include <array>
#include <string>

namespace dino {

// `Data/Design/Actions.bad` is a count plus five equal 462-float blocks.
// `FUN_00615e80`/`FUN_00616...` read each block into the per-family action
// record (`0x81c` bytes) in this order:
//   3 floats header, 51 x 2 floats (forward/lateral), 51 floats (scale),
//   51 x 5 floats, then 51 floats of **duration**.
// So the duration of kind k is block float `411 + k`, not the `0x1bc/4`
// offset the first mover recon read (that lands in the 51 x 5 table and is
// why it saw 1.0 s for every turn).
//
// Family order is the simu_trans Part 10 order:
//   0 HighQuadruped, 1 LightQuadruped, 2 HeavyBiped, 3 LightBiped,
//   4 LowQuadruped.
struct ActionsBad {
    static constexpr int kFamilies = 5;
    static constexpr int kKinds = 51;
    static constexpr int kBlockFloats = 462;
    static constexpr int kDurationOffset = 411;
    // The 51 scale floats (`f[105+kind]`, runtime `block+0x354+kind*4`):
    // walk 0.5, run 2.0, sleep sentinel 0x1e -10 (rest-gauge update
    // `FUN_00571540`). Identical in all five blocks.
    static constexpr int kScaleOffset = 105;
    static constexpr int kFileBytes = 4 + kFamilies * kBlockFloats * 4;

    bool valid = false;
    std::array<std::array<float, kKinds>, kFamilies> durations{};
    std::array<std::array<float, kKinds>, kFamilies> scales{};
};

// Throws std::runtime_error on a wrong size/count. The retail file is exactly
// 9,244 bytes with a leading block count of 5.
ActionsBad load_actions_bad(const std::string& path);

// `FUN_00575de0` for the non-walk/run kinds: the per-family duration table.
// `family` is the Part 10 index (0..4). The two biped indices are swapped
// relative to the file's block order: family 2 (size class < 2) uses the
// 1.2/1.5 block 3, family 3 (size class >= 2) uses the 2.0 block 2. Verified
// on the 20 Cpg04 dinos: their size classes match the retail DINORESET
// `f1e0` values 20/20 and the measured turns are 15 x 1.2 s (family 2) and
// 4 x 2.0 s (family 3); family 4 uses block 4 (2.0/1.5).
// Returns 0 for an unknown family/kind (callers keep their fallback).
float action_duration(const ActionsBad& table, int family, int kind);

// `FUN_00571540`: the per-activity rest-gauge scale `f[105+kind]`.
float action_scale(const ActionsBad& table, int family, int kind);

}  // namespace dino
