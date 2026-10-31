#include "bfi.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace dino {
namespace {

struct Reader {
    const std::vector<std::uint8_t>& data;
    std::size_t offset = 0;

    void need(std::size_t size) const {
        if (offset + size > data.size()) {
            throw std::runtime_error("bfi: body overruns file");
        }
    }
    std::uint32_t u32() {
        need(4);
        std::uint32_t value = 0;
        std::memcpy(&value, data.data() + offset, 4);
        offset += 4;
        return value;
    }
    float f32() {
        need(4);
        float value = 0.0f;
        std::memcpy(&value, data.data() + offset, 4);
        offset += 4;
        return value;
    }
    std::uint8_t u8() {
        need(1);
        return data[offset++];
    }
    std::string string() {
        const std::uint32_t size = u32();
        if (size > 4096) {
            throw std::runtime_error("bfi: implausible string length");
        }
        need(size);
        if (size == 0 || data[offset + size - 1] != 0) {
            throw std::runtime_error("bfi: string is not NUL-terminated");
        }
        std::string text(reinterpret_cast<const char*>(data.data() + offset),
                         size - 1);
        offset += size;
        return text;
    }
};

std::vector<std::uint8_t> read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("bfi: cannot open " + path);
    }
    return std::vector<std::uint8_t>(
        std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

}  // namespace

BfiFeature load_bfi(const std::string& path) {
    const std::vector<std::uint8_t> data = read_file(path);
    if (data.size() < 0x35) {
        throw std::runtime_error("bfi: too short: " + path);
    }
    BfiFeature feature;
    feature.name = std::filesystem::path(path).stem().string();
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::memcpy(&width, data.data(), 4);
    std::memcpy(&height, data.data() + 4, 4);
    feature.width = static_cast<int>(width);
    feature.height = static_cast<int>(height);
    std::memcpy(&feature.vertical, data.data() + 8, 4);
    std::memcpy(&feature.variant, data.data() + 0x0C, 4);
    std::memcpy(&feature.price, data.data() + 0x19, 4);

    Reader tag_reader{data, 0x2D};
    feature.class_tag = tag_reader.string();
    Reader reader{data, tag_reader.offset};

    feature.judge_kind = static_cast<int>(reader.u32());
    const std::uint32_t show_count = reader.u32();
    reader.need(show_count);
    reader.offset += show_count;
    feature.food_kind = static_cast<int>(reader.u32());
    feature.food_quantity = static_cast<int>(reader.u32());   // field_3c
    feature.food_nutrition = static_cast<int>(reader.u32());  // field_40
    const std::uint32_t action_count = reader.u32();
    for (std::uint32_t i = 0; i < action_count; ++i) {
        reader.string();
    }
    reader.u32();  // anim_flag
    reader.f32();  // anim_speed
    feature.state_count = static_cast<int>(reader.u32());
    for (int i = 0; i < 8; ++i) {
        feature.mesh_slots.push_back(reader.string());
    }
    feature.texture_dir = reader.string();
    reader.f32();  // model_scale
    for (int i = 0; i < 7; ++i) {
        feature.effects.push_back(reader.string());
    }
    reader.u32();  // field_154
    reader.u32();  // field_158
    reader.u32();  // field_15c
    reader.string();  // billboard_dir
    const std::uint32_t attribute_set_count = reader.u32();
    for (std::uint32_t set = 0; set < attribute_set_count; ++set) {
        const std::uint32_t count = reader.u32();
        for (std::uint32_t i = 0; i < count; ++i) {
            reader.string();
        }
    }
    const std::uint32_t score_count = reader.u32();
    for (std::uint32_t i = 0; i < score_count; ++i) {
        reader.f32();
    }
    for (int i = 0; i < 3; ++i) {
        reader.string();  // sounds
    }
    reader.f32();  // duration
    reader.f32();  // duration_frames
    feature.category = static_cast<int>(reader.u32());
    feature.switch_key = reader.string();
    reader.string();  // trailing_string
    reader.u8();      // tail_flag
    if (reader.offset != data.size()) {
        throw std::runtime_error("bfi: trailing bytes in " + path);
    }
    return feature;
}

std::vector<BfiFeature> load_bfi_feature_list(const std::string& dir) {
    std::ifstream list(dir + "/FeatureList.ini");
    if (!list) {
        throw std::runtime_error("bfi: cannot open FeatureList.ini in " + dir);
    }
    std::uint32_t count = 0;
    if (!(list >> count)) {
        throw std::runtime_error("bfi: bad FeatureList.ini count in " + dir);
    }
    std::vector<BfiFeature> features;
    features.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        std::string name;
        if (!(list >> name)) {
            throw std::runtime_error("bfi: short FeatureList.ini in " + dir);
        }
        features.push_back(load_bfi(dir + "/" + name + ".bfi"));
    }
    return features;
}

std::vector<BfiFeature> load_bfi_dir(const std::string& dir) {
    std::vector<std::filesystem::path> paths;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".bfi") {
            paths.push_back(entry.path());
        }
    }
    std::sort(paths.begin(), paths.end());
    std::vector<BfiFeature> features;
    features.reserve(paths.size());
    for (const auto& path : paths) {
        features.push_back(load_bfi(path.string()));
    }
    return features;
}

}  // namespace dino
