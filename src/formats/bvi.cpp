#include "bvi.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace dino {
namespace {

std::vector<std::uint8_t> read_all(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot open bvi: " + path);
    }
    in.seekg(0, std::ios::end);
    const std::streamoff size = in.tellg();
    in.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
    in.read(reinterpret_cast<char*>(data.data()), size);
    if (!in) {
        throw std::runtime_error("truncated bvi: " + path);
    }
    return data;
}

std::uint32_t u32(const std::vector<std::uint8_t>& data, std::size_t off) {
    if (off + 4 > data.size()) {
        throw std::runtime_error("bvi: truncated u32");
    }
    return static_cast<std::uint32_t>(data[off]) |
           (static_cast<std::uint32_t>(data[off + 1]) << 8) |
           (static_cast<std::uint32_t>(data[off + 2]) << 16) |
           (static_cast<std::uint32_t>(data[off + 3]) << 24);
}

float f32(const std::vector<std::uint8_t>& data, std::size_t off) {
    const std::uint32_t bits = u32(data, off);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

}  // namespace

VisitorProfile load_bvi(const std::string& path) {
    const auto data = read_all(path);
    if (data.size() < 96) {
        throw std::runtime_error("bvi shorter than header: " + path);
    }
    VisitorProfile profile;
    const std::size_t slash = path.find_last_of("/\\");
    const std::string filename =
        slash == std::string::npos ? path : path.substr(slash + 1);
    const std::size_t dot = filename.find_last_of('.');
    profile.name = dot == std::string::npos ? filename : filename.substr(0, dot);
    profile.personality_id = static_cast<int>(u32(data, 0));
    profile.walk_speed = f32(data, 16);
    if (!(profile.walk_speed > 0.05f && profile.walk_speed < 10.0f)) {
        profile.walk_speed = 1.6f;
    }
    for (std::size_t i = 0; i < profile.weights.size(); ++i) {
        const float value = f32(data, 28 + i * 4);
        profile.weights[i] = value;
    }
    // File offset 0x34: per-profile ticket multiplier (1.0..2.5).
    profile.price_scale = f32(data, 0x34);
    if (!(profile.price_scale > 0.25f && profile.price_scale < 4.0f)) {
        profile.price_scale = 1.0f;
    }
    return profile;
}

}  // namespace dino
