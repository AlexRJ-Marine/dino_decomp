#include "bsi.h"

#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace dino {
namespace {

std::uint32_t read_u32(const std::vector<std::uint8_t>& data,
                       std::size_t offset) {
    if (offset + 4 > data.size()) {
        throw std::runtime_error("bsi: truncated u32");
    }
    return static_cast<std::uint32_t>(data[offset]) |
           (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}

float read_f32(const std::vector<std::uint8_t>& data, std::size_t offset) {
    const std::uint32_t bits = read_u32(data, offset);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

// `<u32 length><length bytes including NUL>` with printable ASCII only.
bool read_string(const std::vector<std::uint8_t>& data, std::size_t offset,
                 std::string& text, std::size_t& next) {
    if (offset + 4 > data.size()) {
        return false;
    }
    const std::uint32_t length = read_u32(data, offset);
    if (length < 2 || length > 400 || offset + 4 + length > data.size()) {
        return false;
    }
    const std::size_t begin = offset + 4;
    if (data[begin + length - 1] != 0) {
        return false;
    }
    std::string value;
    value.reserve(length - 1);
    for (std::size_t i = 0; i + 1 < length; ++i) {
        const std::uint8_t byte = data[begin + i];
        if (byte != 0 && (byte < 32 || byte >= 127)) {
            return false;
        }
        if (byte == 0) {
            break;
        }
        value.push_back(static_cast<char>(byte));
    }
    text = std::move(value);
    next = begin + length;
    return true;
}

bool looks_like_tail_string(const std::string& text) {
    if (text.size() < 4) {
        return false;
    }
    const bool prefix = text.rfind("DATA\\", 0) == 0 ||
                        text.rfind("FOOD_", 0) == 0;
    const bool suffix = text.size() > 5 &&
                        (text.compare(text.size() - 5, 5, ".mesh") == 0 ||
                         text.compare(text.size() - 4, 4, ".tga") == 0 ||
                         text.compare(text.size() - 4, 4, ".ini") == 0);
    bool alpha = true;
    for (const char c : text) {
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))) {
            alpha = false;
            break;
        }
    }
    return prefix || suffix || alpha;
}

}  // namespace

std::string FoodSourceDef::feeder_mesh() const {
    for (const std::string& text : tail_strings) {
        std::string lower = text;
        for (char& c : lower) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (lower.size() > 5 &&
            lower.compare(lower.size() - 5, 5, ".mesh") == 0 &&
            lower.find("feed") != std::string::npos) {
            return text;
        }
    }
    return {};
}

std::string FoodSourceDef::ui_name() const {
    for (const std::string& text : tail_strings) {
        if (text.rfind("FOOD_", 0) == 0) {
            return text;
        }
    }
    return {};
}

FoodSourceDef load_bsi(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot open bsi: " + path);
    }
    std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(in)),
                                   std::istreambuf_iterator<char>());
    if (data.size() < 29) {
        throw std::runtime_error("bsi: shorter than header: " + path);
    }
    FoodSourceDef source;
    std::string base = path;
    const std::size_t slash = base.find_last_of("/\\");
    if (slash != std::string::npos) {
        base = base.substr(slash + 1);
    }
    const std::size_t dot = base.find_last_of('.');
    if (dot != std::string::npos) {
        base.resize(dot);
    }
    source.name = base;
    source.nutrition = read_f32(data, 0);
    source.quantity = read_f32(data, 4);
    source.price = static_cast<int>(read_u32(data, 8));
    source.kind_id = static_cast<int>(read_u32(data, 12));
    source.flags = static_cast<int>(read_u32(data, 24));
    source.flag28 = data[28];

    std::size_t offset = 29;
    for (int i = 0; i < 4; ++i) {
        std::string text;
        std::size_t next = 0;
        if (!read_string(data, offset, text, next)) {
            throw std::runtime_error("bsi: asset " + std::to_string(i) +
                                     " is not a string at " +
                                     std::to_string(offset) + ": " + path);
        }
        source.assets.push_back(std::move(text));
        offset = next;
    }
    for (std::size_t at = offset; at + 4 <= data.size(); ++at) {
        std::string text;
        std::size_t next = 0;
        if (read_string(data, at, text, next) &&
            looks_like_tail_string(text)) {
            source.tail_strings.push_back(std::move(text));
            at = next - 1;
        }
    }
    return source;
}

std::vector<std::string> load_food_source_list(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot open food source list: " + path);
    }
    std::vector<std::string> names;
    std::string line;
    while (std::getline(in, line)) {
        const std::size_t comment = line.find("//");
        if (comment != std::string::npos) {
            line.resize(comment);
        }
        std::size_t begin = 0;
        while (begin < line.size() &&
               std::isspace(static_cast<unsigned char>(line[begin]))) {
            ++begin;
        }
        std::size_t end = line.size();
        while (end > begin &&
               std::isspace(static_cast<unsigned char>(line[end - 1]))) {
            --end;
        }
        if (begin == end) {
            continue;
        }
        names.push_back(line.substr(begin, end - begin));
    }
    if (!names.empty()) {
        bool numeric = true;
        for (const char c : names.front()) {
            if (!std::isdigit(static_cast<unsigned char>(c))) {
                numeric = false;
                break;
            }
        }
        if (numeric) {
            names.erase(names.begin());
        }
    }
    return names;
}

}  // namespace dino
