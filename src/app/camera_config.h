#pragma once

#include <string>

namespace dino {

// `Data/Config/camera.ini` `[Config1]`; defaults match FUN_005e5560.
struct CameraConfig {
    float move_speed_min = 1.0f;
    float move_speed_max = 20.0f;
    float progress_speed_min = 0.05f;
    float progress_speed_max = 0.2f;
    float rotation_speed_min = 1.570796f;  // rad/s
    float rotation_speed_max = 1.570796f;  // rad/s
    float elevation_min = 3.0f;
    float elevation_max = 500.0f;
    float distance_min = 10.0f;
    float distance_max = 300.0f;
    float target_hardness_min = 6.0f;
    float target_hardness_max = 2.0f;
    float position_hardness_min = 6.0f;
    float position_hardness_max = 2.0f;
    float progress = 0.1f;  // new-game / saved camera progress

    static float lerp(float a, float b, float t);

    float move_speed(float t) const;
    float progress_speed(float t) const;
    float rotation_speed(float t) const;  // rad/s
    float elevation(float t) const;
    float distance(float t) const;
    float target_hardness(float t) const;
    float position_hardness(float t) const;
};

// Missing file/keys keep the defaults; malformed values are ignored.
CameraConfig load_camera_config(const std::string& path);

}  // namespace dino
