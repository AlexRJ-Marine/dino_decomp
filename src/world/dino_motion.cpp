#include "dino_motion.h"

#include <algorithm>
#include <cmath>

#include "../formats/math.h"

namespace dino {
namespace {

float normalize_degrees(float angle) {
    while (angle > 180.0f) {
        angle -= 360.0f;
    }
    while (angle <= -180.0f) {
        angle += 360.0f;
    }
    return angle;
}

}  // namespace

DinoMover::DinoMover(float x, float y, float angle_deg, float walk_speed,
                     float run_speed)
    : x_(x), y_(y), walk_speed_(walk_speed), run_speed_(run_speed) {
    set_facing_degrees(angle_deg);
}

void DinoMover::set_speeds(float walk_speed, float run_speed) {
    walk_speed_ = std::max(0.0f, walk_speed);
    run_speed_ = std::max(0.0f, run_speed);
}

void DinoMover::set_turn_durations(float step_seconds, float uturn_seconds) {
    turn_seconds_ = std::max(0.0f, step_seconds);
    uturn_seconds_ = std::max(0.0f, uturn_seconds);
}

void DinoMover::set_facing_degrees(float angle_deg) {
    angle_deg_ = normalize_degrees(angle_deg);
    const float radians = angle_deg_ * kPi / 180.0f;
    fx_ = std::cos(radians);
    fy_ = std::sin(radians);
}

void DinoMover::set_position(float x, float y, float angle_deg) {
    x_ = x;
    y_ = y;
    set_facing_degrees(angle_deg);
}

void DinoMover::set_bounds(float x0, float y0, float x1, float y1,
                           float inset) {
    has_bounds_ = true;
    bounds_x0_ = std::min(x0, x1) + inset;
    bounds_y0_ = std::min(y0, y1) + inset;
    bounds_x1_ = std::max(x0, x1) - inset;
    bounds_y1_ = std::max(y0, y1) - inset;
}

void DinoMover::clear_bounds() { has_bounds_ = false; }

void DinoMover::start(int kind, float x, float y, int mode, float threshold) {
    // `FUN_00573f40`: a command that arrives while moving cancels the
    // current record's arrival (`mode = 0`) and keeps at most one "next".
    if (active_) {
        mode_ = 0;
        while (queue_.size() > 1) {
            queue_.pop_back();
        }
    }
    queue(kind, x, y, mode, threshold);
}

void DinoMover::queue(int kind, float x, float y, int mode, float threshold) {
    MoveCommand command;
    command.x = x;
    command.y = y;
    command.threshold = threshold;
    command.kind = kind;
    command.mode = mode;
    queue_.push_back(command);
    // `FUN_00573dc0`: append and start when the mover is idle.
    if (!active_) {
        activate_next();
    }
}

void DinoMover::clear() {
    queue_.clear();
    active_ = false;
    blocked_ = false;
    state_ = 0;
    kind_ = 0;
    mode_ = 0;
    elapsed_ = 0.0f;
    duration_ = 0.0f;
    turn_steps_ = 0;
    turn_sign_ = 0;
    turn_left_ = 0.0f;
}

void DinoMover::activate_next() {
    if (queue_.empty()) {
        active_ = false;
        state_ = 0;
        kind_ = 0;
        mode_ = 0;
        return;
    }
    const MoveCommand& command = queue_.front();
    active_ = true;
    blocked_ = false;
    kind_ = command.kind;
    mode_ = command.mode;
    target_x_ = command.x;
    target_y_ = command.y;
    elapsed_ = 0.0f;
    duration_ = command.threshold;
    begin_segment();
}

void DinoMover::begin_segment() {
    state_ = 1;
    turn_steps_ = 0;
    turn_sign_ = 0;
    turn_step_degrees_ = 0.0f;
    turn_left_ = 0.0f;
    if (mode_ == 0) {
        // `FUN_00574450`: mode 0 is "no integration, instant arrival".
        finish_segment();
        return;
    }
    if (mode_ != 1) {
        return;
    }
    const float dx = target_x_ - x_;
    const float dy = target_y_ - y_;
    const float distance = std::sqrt(dx * dx + dy * dy);
    if (distance <= kArriveRadius) {
        finish_segment();
        return;
    }
    const float nx = dx / distance;
    const float ny = dy / distance;
    const float dot = fx_ * nx + fy_ * ny;
    // `FUN_00575410`: fVar2 = desired.y*facing.x - facing.y*desired.x.
    const float cross = ny * fx_ - fy_ * nx;
    const float desired =
        std::atan2(ny, nx) * 180.0f / dino::kPi;
    if (dot >= 0.99f) {
        set_facing_degrees(desired);
        return;
    }
    if (dot >= 0.9238795f) {
        // <= 22.5 degrees: `steps = 0`, newFacing = desired.
        set_facing_degrees(desired);
        return;
    }
    int steps = 0;
    bool u_turn = false;
    if (dot >= 0.38268343f) {
        // 22.5 .. 67.5 degrees: one 45-degree record. Retail skips the turn
        // for running dinos of size class <= 1; the mover does not carry the
        // size class yet.
        steps = 1;
    } else if (dot >= 0.0f) {
        // 67.5 .. 90 degrees: two 45-degree records.
        steps = 2;
    } else {
        // > 90 degrees: one 180-degree record (side 5).
        u_turn = true;
    }
    if (u_turn) {
        set_facing_degrees(desired + 180.0f);
        turn_steps_ = 1;
        turn_step_degrees_ = 180.0f;
        turn_sign_ = 1;
        state_ = 2;
        turn_left_ = uturn_seconds_;
        return;
    }
    // `FUN_00575410`: pre-rotate the desired vector by the turn side
    // (`cross < 0` -> side 3 = +45, else side 4 = -45), then insert the
    // opposite-side turn records (kind 4 for cross < 0, else kind 3) that
    // walk the facing back onto `desired`.
    const float pre_sign = cross < 0.0f ? 1.0f : -1.0f;
    set_facing_degrees(desired +
                       pre_sign * static_cast<float>(steps) *
                           kTurnStepDegrees);
    turn_steps_ = steps;
    turn_step_degrees_ = kTurnStepDegrees;
    turn_sign_ = cross < 0.0f ? -1 : 1;
    state_ = 2;
    turn_left_ = turn_seconds_;
}

void DinoMover::set_desired_facing() {
    const float dx = target_x_ - x_;
    const float dy = target_y_ - y_;
    if (dx * dx + dy * dy > 0.0f) {
        set_facing_degrees(std::atan2(dy, dx) * 180.0f / dino::kPi);
    }
}

void DinoMover::finish_segment() {
    if (!queue_.empty()) {
        queue_.pop_front();
    }
    if (queue_.empty()) {
        active_ = false;
        state_ = 0;
        kind_ = 0;
        mode_ = 0;
        // The blocked flag stays latched (`mover+0x64`) until the next
        // record is activated; `activate_next` clears it.
        return;
    }
    activate_next();
}

bool DinoMover::step_allowed(float nx, float ny) const {
    if (!has_bounds_) {
        return true;
    }
    return nx >= bounds_x0_ && nx <= bounds_x1_ && ny >= bounds_y0_ &&
           ny <= bounds_y1_;
}

float DinoMover::speed_for_kind(int kind) const {
    // `FUN_00574930`: kind 1/0x34 walk, kind 2/0x35 run. Every other kind is
    // a gesture/turn/attack whose forward/lateral table slots are zero; the
    // runtime probe measured 0.0000 displacement for those.
    if (kind == 2 || kind == 0x35) {
        return run_speed_;
    }
    if (kind == 1 || kind == 0x34) {
        return walk_speed_;
    }
    return 0.0f;
}

bool DinoMover::tick(double dt) {
    if (!active_ || dt <= 0.0) {
        return false;
    }
    const float step = static_cast<float>(dt);
    bool changed = false;

    if (state_ == 2) {
        turn_left_ -= step;
        int guard = 0;
        while (turn_left_ <= 0.0f && turn_steps_ > 0 && guard < 8) {
            ++guard;
            set_facing_degrees(angle_deg_ +
                               static_cast<float>(turn_sign_) *
                                   turn_step_degrees_);
            --turn_steps_;
            changed = true;
            if (turn_steps_ <= 0) {
                // The pre-rotation + records land exactly on the target
                // vector; set it explicitly to avoid float drift.
                set_desired_facing();
                state_ = 1;
                turn_left_ = 0.0f;
                break;
            }
            turn_left_ += turn_seconds_;
        }
        return changed;
    }

    if (state_ != 1) {
        return false;
    }

    if (mode_ == 2) {
        elapsed_ += step;
        if (elapsed_ >= duration_) {
            finish_segment();
            return true;
        }
    }

    const float dx = target_x_ - x_;
    const float dy = target_y_ - y_;
    const float distance = std::sqrt(dx * dx + dy * dy);
    const float dir_x = fx_;
    const float dir_y = fy_;
    if (mode_ == 1 && distance <= kArriveRadius) {
        finish_segment();
        return true;
    }

    const float travel = speed_for_kind(kind_) * step;
    if (travel <= 0.0f) {
        return changed;
    }
    if (travel >= distance) {
        if (!step_allowed(target_x_, target_y_)) {
            blocked_ = true;
            finish_segment();
            return true;
        }
        x_ = target_x_;
        y_ = target_y_;
        finish_segment();
        return true;
    }
    const float nx = x_ + dir_x * travel;
    const float ny = y_ + dir_y * travel;
    if (!step_allowed(nx, ny)) {
        // `FUN_0055af70` returns 0: the step is refused. Retail sets the
        // blocked flag and lets the behaviour re-plan; the port ends the
        // record so the chooser can pick another action.
        blocked_ = true;
        finish_segment();
        return true;
    }
    x_ = nx;
    y_ = ny;
    return true;
}

}  // namespace dino
