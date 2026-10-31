#include "bwi.h"

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
            throw std::runtime_error("bwi: body overruns file");
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
        if (size == 0 || size > 4096) {
            throw std::runtime_error("bwi: implausible string length");
        }
        need(size);
        if (data[offset + size - 1] != 0) {
            throw std::runtime_error("bwi: string is not NUL-terminated");
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
        throw std::runtime_error("bwi: cannot open " + path);
    }
    return std::vector<std::uint8_t>(
        std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

}  // namespace

BwiWall load_bwi(const std::string& path) {
    const std::vector<std::uint8_t> data = read_file(path);
    if (data.size() < 0x40) {
        throw std::runtime_error("bwi: too short: " + path);
    }
    Reader reader{data};
    BwiWall wall;
    wall.name = std::filesystem::path(path).stem().string();
    wall.version = reader.u32();
    if (wall.version != 4) {
        throw std::runtime_error("bwi: unsupported version in " + path);
    }
    wall.height = reader.f32();
    wall.field_08 = reader.u32();
    wall.field_0c = reader.u32();
    wall.field_10 = reader.u32();
    wall.electrified = reader.u8() != 0;
    for (float& scale : wall.scales) {
        scale = reader.f32();
    }
    std::array<std::string, 7> strings;
    for (std::string& text : strings) {
        text = reader.string();
    }
    wall.field_tail = reader.u32();
    wall.switch_material = reader.string();
    wall.switch_height = reader.string();
    if (reader.offset != data.size()) {
        throw std::runtime_error("bwi: trailing bytes in " + path);
    }
    for (std::size_t i = 0; i < wall.textures.size(); ++i) {
        wall.textures[i] = strings[i];
    }
    for (std::size_t i = 0; i < wall.meshes.size(); ++i) {
        wall.meshes[i] = strings[4 + i];
    }
    wall.texture_dir = strings[6];
    return wall;
}

std::vector<BwiWall> load_bwi_dir(const std::string& dir) {
    std::vector<std::filesystem::path> paths;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".bwi") {
            paths.push_back(entry.path());
        }
    }
    std::sort(paths.begin(), paths.end());
    std::vector<BwiWall> walls;
    walls.reserve(paths.size());
    for (const std::filesystem::path& path : paths) {
        walls.push_back(load_bwi(path.string()));
    }
    return walls;
}

}  // namespace dino
