#pragma once

#include <cstdint>
#include <deque>

namespace dino {

// One 0x18-byte move-command record (queued by `FUN_00573f40` /
// `FUN_00573dc0`):
//   +0x00 f32 target x
//   +0x04 f32 target y
//   +0x08 f32 mode-2 threshold/duration
//   +0x0c u8  flag
//   +0x0d u8  flag
//   +0x10 int move kind
//   +0x14 int arrival mode (0 free, 1 position, 2 timed)
// See notes/dino-ai-gap-audit.md §2.1.
struct MoveCommand {
    float x = 0.0f;
    float y = 0.0f;
    float threshold = 0.0f;
    std::uint8_t flag = 0;
    std::uint8_t flag2 = 0;
    int kind = 1;  // 1 walk, 2 run, 3/4/5 turn, 0x34/0x35 jump
    int mode = 1;  // 0 free, 1 position, 2 timed
};

// `saDinosaur+0x2c8` mover (`FUN_00573370` ctor, `FUN_00574350` tick,
// `FUN_00574450` integrator, `FUN_00574ad0` segment start, `FUN_00575410`
// quantised turn). Positions are map units (metres) and the facing vector is
// the actor's `(cos, sin)`: the port's `place_on_terrain` rotates model +X by
// `angle`, so a facing of `(1,0)` is angle 0.
//
// Retail has no pathfinder for park actions: the mover steers in 45-degree
// steps toward the record target and refuses a step outside the paddock rect
// or into an occupied footprint cell. The first port slice implements the
// straight-line steering + paddock rect; the shared occupancy footprint
// (BuildGrid::occupancy) is wired by the caller when available.
class DinoMover {
public:
    // Turn record timing (`FUN_005758f0` / `FUN_00575de0`): 45 degrees per
    // step, 1.0 s per turn record for kinds 3/4/5 (all five families). A
    // 180-degree turn is a single record (side 5), not four 45 steps.
    static constexpr float kTurnStepDegrees = 45.0f;
    static constexpr float kTurnStepSeconds = 1.0f;
    // `FUN_00575410` buckets (dot product): >= 0.99 snap to the exact
    // desired vector, >= 0.9238795 also snaps (<= 22.5 deg), >= 0.38268343
    // inserts one 45-degree record, >= 0 two, otherwise one 180-degree
    // record. The facing is pre-rotated backwards at plan time so the
    // records land exactly on the desired vector.
    static constexpr float kArriveRadius = 0.01f;

    DinoMover() = default;
    DinoMover(float x, float y, float angle_deg, float walk_speed,
              float run_speed);

    void set_speeds(float walk_speed, float run_speed);
    void set_position(float x, float y, float angle_deg);
    // Paddock confinement (`path+0x1c0`): a step outside the rect inset by
    // `inset` units is refused (`FUN_0055af70` returns 0). `inset` approximates
    // the actor footprint radius (retail `actor+0x13` cells).
    void set_bounds(float x0, float y0, float x1, float y1, float inset);
    void clear_bounds();

    // `FUN_00573f40`: append and start when idle. `FUN_00573dc0`: append only.
    void start(int kind, float x, float y, int mode = 1,
               float threshold = 0.0f);
    void queue(int kind, float x, float y, int mode = 1,
               float threshold = 0.0f);
    void clear();

    // Advance by `dt` seconds. Returns true when the position or facing
    // changed (the caller copies x()/y()/angle_deg() into the actor).
    bool tick(double dt);

    bool active() const { return active_; }
    bool blocked() const { return blocked_; }
    std::size_t queued() const { return queue_.size(); }
    int state() const { return state_; }  // 0 idle, 1 move, 2 turn/wait
    int kind() const { return kind_; }
    float x() const { return x_; }
    float y() const { return y_; }
    float angle_deg() const { return angle_deg_; }
    float facing_x() const { return fx_; }
    float facing_y() const { return fy_; }
    float walk_speed() const { return walk_speed_; }
    float run_speed() const { return run_speed_; }
    float target_x() const { return target_x_; }
    float target_y() const { return target_y_; }

private:
    void activate_next();
    void begin_segment();
    void finish_segment();
    void set_desired_facing();
    bool step_allowed(float nx, float ny) const;
    float speed_for_kind(int kind) const;
    void set_facing_degrees(float angle_deg);

    float x_ = 0.0f;
    float y_ = 0.0f;
    float fx_ = 1.0f;
    float fy_ = 0.0f;
    float angle_deg_ = 0.0f;
    float walk_speed_ = 1.0f;
    float run_speed_ = 3.0f;

    bool has_bounds_ = false;
    float bounds_x0_ = 0.0f;
    float bounds_y0_ = 0.0f;
    float bounds_x1_ = 0.0f;
    float bounds_y1_ = 0.0f;

    std::deque<MoveCommand> queue_;
    bool active_ = false;
    bool blocked_ = false;
    int state_ = 0;

    int kind_ = 0;
    int mode_ = 0;
    float target_x_ = 0.0f;
    float target_y_ = 0.0f;
    float elapsed_ = 0.0f;
    float duration_ = 0.0f;

    // Turn-in-progress state.
    int turn_steps_ = 0;
    int turn_sign_ = 0;
    float turn_step_degrees_ = 0.0f;
    float turn_left_ = 0.0f;
};

}  // namespace dino
