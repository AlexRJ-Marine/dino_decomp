#include "bbi.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace dino {
namespace {

std::vector<std::uint8_t> read_all(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot open bbi: " + path);
    }
    in.seekg(0, std::ios::end);
    const std::streamoff size = in.tellg();
    in.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
    in.read(reinterpret_cast<char*>(data.data()), size);
    if (!in) {
        throw std::runtime_error("truncated bbi: " + path);
    }
    return data;
}

std::uint32_t u32(const std::vector<std::uint8_t>& data, std::size_t off) {
    if (off + 4 > data.size()) {
        throw std::runtime_error("bbi: truncated u32");
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

bool read_string(const std::vector<std::uint8_t>& data, std::size_t& off,
                 std::string& out) {
    if (off + 4 > data.size()) {
        return false;
    }
    const std::uint32_t length = u32(data, off);
    if (length < 2 || length > 400 || off + 4 + length > data.size()) {
        return false;
    }
    const std::size_t start = off + 4;
    if (data[start + length - 1] != 0) {
        return false;
    }
    for (std::size_t i = 0; i + 1 < length; ++i) {
        const std::uint8_t c = data[start + i];
        if (c != 0 && (c < 32 || c >= 127)) {
            return false;
        }
    }
    out.assign(reinterpret_cast<const char*>(data.data() + start), length - 1);
    off = start + length;
    return true;
}

bool plausible_tail_string(const std::string& text) {
    if (text.size() < 4) {
        return false;
    }
    if (text.rfind("DATA\\", 0) == 0 || text.rfind("CONSTRUC_", 0) == 0) {
        return true;
    }
    const std::string lower = [&] {
        std::string s = text;
        for (char& c : s) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return s;
    }();
    if (lower.size() >= 4 && lower.compare(lower.size() - 4, 4, ".ini") == 0) {
        return true;
    }
    for (const char* suffix : {".wav", ".tga", ".mesh", ".skin", ".skel", ".anim"}) {
        const std::size_t n = std::char_traits<char>::length(suffix);
        if (lower.size() >= n && lower.compare(lower.size() - n, n, suffix) == 0) {
            return true;
        }
    }
    for (const char c : text) {
        if (!std::isalpha(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return true;
}

}  // namespace

BuildingDef load_bbi(const std::string& path) {
    const auto data = read_all(path);
    if (data.size() < 121) {
        throw std::runtime_error("bbi shorter than header: " + path);
    }
    BuildingDef def;
    const std::size_t slash = path.find_last_of("/\\");
    const std::string filename =
        slash == std::string::npos ? path : path.substr(slash + 1);
    const std::size_t dot = filename.find_last_of('.');
    def.name = dot == std::string::npos ? filename : filename.substr(0, dot);
    def.footprint_w = u32(data, 0);
    def.footprint_h = u32(data, 4);
    def.scale = f32(data, 8);
    def.flag = data[12];
    for (std::size_t i = 0; i < def.header_words.size(); ++i) {
        def.header_words[i] = u32(data, 13 + i * 4);
    }
    def.category = u32(data, 49);
    def.duration = f32(data, 37);
    def.price = u32(data, 25);
    def.appeal_scale = f32(data, 73);
    def.radius_squared = u32(data, 105);
    def.model0 = u32(data, 109);
    def.model1 = f32(data, 113);
    def.model_count = u32(data, 117);

    std::size_t off = 121;
    for (int i = 0; i < 10; ++i) {
        std::string value;
        if (!read_string(data, off, value)) {
            throw std::runtime_error("bbi mesh path " + std::to_string(i) +
                                     " invalid in " + path);
        }
        def.meshes.push_back(std::move(value));
    }
    for (std::size_t pos = off; pos + 4 <= data.size();) {
        std::string value;
        std::size_t next = pos;
        if (read_string(data, next, value) && plausible_tail_string(value)) {
            def.tail_strings.push_back(std::move(value));
            pos = next;
        } else {
            ++pos;
        }
    }
    return def;
}

std::vector<std::string> load_building_list(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot open building list: " + path);
    }
    std::vector<std::string> names;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const auto first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line.compare(first, 2, "//") == 0) {
            continue;
        }
        line = line.substr(first);
        if (line.find_first_not_of("0123456789") == std::string::npos) {
            continue;  // leading count
        }
        names.push_back(line);
    }
    return names;
}

}  // namespace dino
