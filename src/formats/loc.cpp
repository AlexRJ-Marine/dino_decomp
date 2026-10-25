#include "loc.h"

#include <cstdint>
#include <stdexcept>
#include <vector>

#include "mesh.h"  // read_binary_file

namespace dino {
namespace {

void append_utf8(std::string& out, std::uint32_t code) {
    if (code < 0x80) {
        out.push_back(static_cast<char>(code));
    } else if (code < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (code >> 6)));
        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xE0 | (code >> 12)));
        out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    }
}

std::string decode_utf16(const std::vector<std::uint8_t>& data, bool& big_endian) {
    big_endian = false;
    std::size_t offset = 0;
    if (data.size() >= 2 && data[0] == 0xFF && data[1] == 0xFE) {
        offset = 2;
    } else if (data.size() >= 2 && data[0] == 0xFE && data[1] == 0xFF) {
        offset = 2;
        big_endian = true;
    }
    std::string out;
    out.reserve(data.size() / 2);
    for (std::size_t i = offset; i + 1 < data.size(); i += 2) {
        const std::uint16_t unit = big_endian
            ? static_cast<std::uint16_t>((data[i] << 8) | data[i + 1])
            : static_cast<std::uint16_t>(data[i] | (data[i + 1] << 8));
        append_utf8(out, unit);
    }
    return out;
}

}  // namespace

Loc load_loc(const std::string& path) {
    const auto data = read_binary_file(path);
    bool big_endian = false;
    const std::string text = decode_utf16(data, big_endian);
    (void)big_endian;

    Loc loc;
    std::string current;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = text.find('\n', start);
        std::string line = text.substr(start, end == std::string::npos
                                                 ? std::string::npos
                                                 : end - start);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const auto eq = line.find('=');
        if (eq != std::string::npos) {
            std::string key = line.substr(0, eq);
            std::string value = line.substr(eq + 1);
            while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) {
                key.pop_back();
            }
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.size() - 2);
            }
            if (key == "FIELD_ID") {
                current = value;
            } else if (key == "TEXT" && !current.empty()) {
                loc.texts[current] = value;
            }
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    if (loc.texts.empty()) {
        throw std::runtime_error("loc: no records in " + path);
    }
    return loc;
}

}  // namespace dino
