#include "camera_config.h"

#include <algorithm>
#include <cstdlib>

#include "../formats/ini.h"

namespace dino {

float CameraConfig::lerp(float a, float b, float t) {
    return a + (b - a) * std::clamp(t, 0.0f, 1.0f);
}

float CameraConfig::move_speed(float t) const {
    return lerp(move_speed_min, move_speed_max, t);
}

float CameraConfig::progress_speed(float t) const {
    return lerp(progress_speed_min, progress_speed_max, t);
}

float CameraConfig::rotation_speed(float t) const {
    return lerp(rotation_speed_min, rotation_speed_max, t);
}

float CameraConfig::elevation(float t) const {
    return lerp(elevation_min, elevation_max, t);
}

float CameraConfig::distance(float t) const {
    return lerp(distance_min, distance_max, t);
}

float CameraConfig::target_hardness(float t) const {
    return lerp(target_hardness_min, target_hardness_max, t);
}

float CameraConfig::position_hardness(float t) const {
    return lerp(position_hardness_min, position_hardness_max, t);
}

CameraConfig load_camera_config(const std::string& path) {
    CameraConfig out;
    try {
        const IniFile ini = load_ini(path);
        const IniSection* section = ini.find("Config1");
        if (section == nullptr) {
            return out;
        }
        auto read = [&](const char* key, float& value) {
            if (section->has(key)) {
                value = static_cast<float>(
                    std::atof(section->get(key).c_str()));
            }
        };
        read("MoveSpeedMin", out.move_speed_min);
        read("MoveSpeedMax", out.move_speed_max);
        read("ProgressSpeedMin", out.progress_speed_min);
        read("ProgressSpeedMax", out.progress_speed_max);
        read("RotationSpeedMin", out.rotation_speed_min);
        read("RotationSpeedMax", out.rotation_speed_max);
        read("ElevationMin", out.elevation_min);
        read("ElevationMax", out.elevation_max);
        read("DistanceMin", out.distance_min);
        read("DistanceMax", out.distance_max);
        read("TargetHardnessMin", out.target_hardness_min);
        read("TargetHardnessMax", out.target_hardness_max);
        read("PositionHardnessMin", out.position_hardness_min);
        read("PositionHardnessMax", out.position_hardness_max);
    } catch (const std::exception&) {
        // Missing camera.ini: keep the retail defaults.
    }
    return out;
}

}  // namespace dino
