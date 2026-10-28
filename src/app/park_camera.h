// Retail park camera.
//
// State and per-update order are taken from the retail camSwitcher camera
// object (vtable 0x6481a4, update 0x5e5ff0) and confirmed against a live run
// (notes/camera-controls-binary-recon.md, notes/camera-mouse-tilt-recon.md):
//   * target/eye have separate goal and current vectors;
//   * XY use a first-order filter k = dt*lerp(6,2,p), Z is instant;
//   * yaw is low-passed at 5/s and drives the eye offset;
//   * pan is per update (no dt) and moves the target goal by lerp(1,20,p);
//   * rotation (pi/2 rad/s) and progress (0.05..0.2 /s) are dt-scaled;
//   * progress drives horizontal distance 10..300 and elevation 3..500.
// Coordinates are the port's centred world coordinates; the retail ctor
// values (target 512,512,50 / eye 128,128,250) are shifted by world_size/2.
//
// This is app state, not world state: it reads the terrain and the menu
// CameraConfig and is driven by input.

#ifndef DINO_APP_PARK_CAMERA_H
#define DINO_APP_PARK_CAMERA_H

#include "../formats/mesh.h"
#include "../world/terrain.h"
#include "camera_config.h"

#include <algorithm>
#include <cmath>

// Retail park camera (camSwitcher camera object, vtable 0x6481a4, update
// 0x5e5ff0). State and per-update order are taken from the disassembly and
// confirmed against a live retail run (run-test\dino_camera.csv):
//   * target/eye have separate goal and current vectors;
//   * XY use a first-order filter k = dt*lerp(6,2,p), Z is instant;
//   * yaw is low-passed at 10/s and drives the eye offset;
//   * pan is per update (no dt) and moves the target goal by lerp(1,20,p);
//   * rotation (pi/2 rad/s) and progress (0.05..0.2 /s) are dt-scaled;
//   * progress drives horizontal distance 10..300 and elevation 3..500.
// Coordinates are the viewer's centred world coordinates; the retail ctor
// values (target 512,512,50 / eye 128,128,250) are shifted by world_size/2.
struct RetailParkCamera {
    dino::Vec3 target_cur{0.0f, 0.0f, 50.0f};
    dino::Vec3 target_goal{0.0f, 0.0f, 50.0f};
    dino::Vec3 eye_cur{-384.0f, -384.0f, 250.0f};
    dino::Vec3 eye_goal{-384.0f, -384.0f, 250.0f};
    float yaw_cur = 0.0f;   // radians, target -> eye
    float yaw_goal = 0.0f;
    float progress = 0.1f;
    float altitude = 16.0f;  // camera object +0x50
    bool initialised = false;

    void reset(const dino::Terrain& terrain) {
        const float half = terrain.world_size * 0.5f;
        target_cur = target_goal = {0.0f, 0.0f, 50.0f};
        eye_cur = eye_goal = {128.0f - half, 128.0f - half, 250.0f};
        yaw_cur = yaw_goal = 0.0f;
        progress = 0.1f;
        altitude = 16.0f;
        initialised = true;
    }

    void set_saved(const dino::Vec3& eye, const dino::Vec3& target,
                   float saved_progress) {
        eye_cur = eye_goal = eye;
        target_cur = target_goal = target;
        progress = std::clamp(saved_progress, 0.0f, 1.0f);
        yaw_cur = yaw_goal = std::atan2(eye.y - target.y, eye.x - target.x);
        initialised = true;
    }

    void update(const dino::CameraConfig& config, const dino::Terrain& terrain,
                float dt, const bool (&flags)[8]) {
        if (!initialised) {
            reset(terrain);
        }
        const float step_dt = std::max(0.0f, dt);
        // Retail order (0x5e5ff0): target Z glue, then the eye goal, then
        // input (rotate/progress/pan), then smoothing. The live camera probe
        // shows the pan moves eye_goal and target_goal by the same delta and
        // yaw stays 0.0000 throughout a Left/Right pan: the pan is a rigid
        // translation, not a yaw/roll lean.
        target_goal.z +=
            std::max(terrain.height_at(target_cur.x, target_cur.y), altitude) -
            target_cur.z;
        const float radius = config.distance(progress);
        const float height = config.elevation(progress);
        eye_goal.x = target_goal.x + std::cos(yaw_cur) * radius;
        eye_goal.y = target_goal.y + std::sin(yaw_cur) * radius;
        const float eye_terrain = terrain.height_at(eye_goal.x, eye_goal.y);
        eye_goal.z =
            height + std::max({target_goal.z, eye_terrain, altitude});

        // 4/5: rotate; 6/7: progress (zoom). Both are dt-scaled.
        const float rotation = config.rotation_speed(progress) * step_dt;
        if (flags[4]) {
            yaw_goal += rotation;
        }
        if (flags[5]) {
            yaw_goal -= rotation;
        }
        const float progress_step = config.progress_speed(progress) * step_dt;
        if (flags[6]) {
            progress -= progress_step;
        }
        if (flags[7]) {
            progress += progress_step;
        }
        progress = std::clamp(progress, 0.0f, 1.0f);

        // 0..3: pan the target goal along the current view direction. The
        // step is per update: 0x5e5ff0 does not read its dt argument.
        float fx = target_cur.x - eye_cur.x;
        float fy = target_cur.y - eye_cur.y;
        const float length = std::sqrt(fx * fx + fy * fy);
        if (length > 1e-6f) {
            fx /= length;
            fy /= length;
        } else {
            fx = -std::cos(yaw_cur);
            fy = -std::sin(yaw_cur);
        }
        const float pan = config.move_speed(progress);
        float pan_dx = 0.0f;
        float pan_dy = 0.0f;
        if (flags[0]) {
            pan_dx += fx * pan;
            pan_dy += fy * pan;
        }
        if (flags[1]) {
            pan_dx -= fx * pan;
            pan_dy -= fy * pan;
        }
        if (flags[2]) {
            pan_dx += -fy * pan;
            pan_dy += fx * pan;
        }
        if (flags[3]) {
            pan_dx += fy * pan;
            pan_dy += -fx * pan;
        }
        // The target may leave the heightfield by 250 world units. Apply the
        // clamped delta to both goals so the eye keeps its radius/offset and
        // the view direction (yaw) is unchanged.
        const float pan_limit = terrain.world_size * 0.5f + 250.0f;
        const float pan_old_x = target_goal.x;
        const float pan_old_y = target_goal.y;
        target_goal.x =
            std::clamp(target_goal.x + pan_dx, -pan_limit, pan_limit);
        target_goal.y =
            std::clamp(target_goal.y + pan_dy, -pan_limit, pan_limit);
        eye_goal.x += target_goal.x - pan_old_x;
        eye_goal.y += target_goal.y - pan_old_y;

        // First-order filters. Z is instant (pair 1000,1000 -> k = 1).
        // The yaw pairs are 5.0/5.0 in the ctor (0x5e5d67..), not 10.
        yaw_cur += (yaw_goal - yaw_cur) *
                   std::clamp(step_dt * 5.0f, 0.0f, 1.0f);
        const float target_k = std::clamp(
            step_dt * config.target_hardness(progress), 0.0f, 1.0f);
        target_cur.x += (target_goal.x - target_cur.x) * target_k;
        target_cur.y += (target_goal.y - target_cur.y) * target_k;
        target_cur.z = target_goal.z;
        const float eye_k = std::clamp(
            step_dt * config.position_hardness(progress), 0.0f, 1.0f);
        eye_cur.x += (eye_goal.x - eye_cur.x) * eye_k;
        eye_cur.y += (eye_goal.y - eye_cur.y) * eye_k;
        eye_cur.z = eye_goal.z;
    }
};

// Asserts the measured camera equations (pan, yaw low-pass, zoom) and the
// no-roll up vector. Returns 0 on success, 1 on any mismatch.
int camera_selftest(const dino::CameraConfig& config, const dino::Terrain& terrain);

#endif  // DINO_APP_PARK_CAMERA_H
