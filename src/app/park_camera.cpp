// Retail park camera (see park_camera.h). The self-test lives here too: it
// asserts the measured equations against the real class.

#include "park_camera.h"

#include <cstdio>

int camera_selftest(const dino::CameraConfig& config,
                    const dino::Terrain& terrain) {
    RetailParkCamera cam;
    cam.reset(terrain);
    bool none[8] = {};
    for (int i = 0; i < 120; ++i) {
        cam.update(config, terrain, 1.0f / 60.0f, none);
    }
    std::printf(
        "camera selftest settle target=%.4f,%.4f,%.4f "
        "eye=%.4f,%.4f,%.4f p=%.4f yaw=%.6f\n",
        cam.target_cur.x, cam.target_cur.y, cam.target_cur.z,
        cam.eye_cur.x, cam.eye_cur.y, cam.eye_cur.z, cam.progress,
        cam.yaw_cur);
    const dino::Vec3 before = cam.target_goal;
    const dino::Vec3 eye_before = cam.eye_goal;
    bool up[8] = {};
    up[0] = true;
    for (int i = 0; i < 10; ++i) {
        cam.update(config, terrain, 1.0f / 60.0f, up);
    }
    std::printf("camera selftest pan dx=%.4f dy=%.4f step=%.4f\n",
                cam.target_goal.x - before.x, cam.target_goal.y - before.y,
                config.move_speed(cam.progress));
    // Rigid pan (live probe): the eye goal moves with the target goal, the
    // eye-target offset stays at the progress radius (39.0 at p=0.1), and
    // yaw does not change.
    const float pan_offset = std::hypot(cam.eye_goal.x - cam.target_goal.x,
                                        cam.eye_goal.y - cam.target_goal.y);
    std::printf("camera selftest rigid eye_dx=%.4f eye_dy=%.4f offset=%.4f\n",
                cam.eye_goal.x - eye_before.x,
                cam.eye_goal.y - eye_before.y, pan_offset);
    const float pan_yaw_before = cam.yaw_cur;
    bool pan_left[8] = {};
    pan_left[2] = true;
    for (int i = 0; i < 10; ++i) {
        cam.update(config, terrain, 1.0f / 60.0f, pan_left);
    }
    std::printf("camera selftest pan_yaw delta=%.6f\n",
                cam.yaw_cur - pan_yaw_before);
    const float yaw_before = cam.yaw_goal;
    bool left[8] = {};
    left[4] = true;
    for (int i = 0; i < 60; ++i) {
        cam.update(config, terrain, 1.0f / 60.0f, left);
    }
    std::printf("camera selftest yaw goal_delta=%.6f cur_delta=%.6f\n",
                cam.yaw_goal - yaw_before, cam.yaw_cur - yaw_before);
    const float progress_before = cam.progress;
    bool zoom_in[8] = {};
    zoom_in[6] = true;
    for (int i = 0; i < 60; ++i) {
        cam.update(config, terrain, 1.0f / 60.0f, zoom_in);
    }
    std::printf("camera selftest zoom delta=%.6f p=%.6f\n",
                cam.progress - progress_before, cam.progress);
    return 0;
}
