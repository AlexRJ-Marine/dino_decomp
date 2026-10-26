#include "sav.h"

#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace dino {
namespace {

std::uint32_t read_u32(std::ifstream& in, const std::string& path) {
    unsigned char bytes[4]{};
    in.read(reinterpret_cast<char*>(bytes), 4);
    if (!in) {
        throw std::runtime_error("truncated u32 in " + path);
    }
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
}

void append_utf8(std::string& out, std::uint32_t code) {
    if (code <= 0x7f) {
        out.push_back(static_cast<char>(code));
    } else if (code <= 0x7ff) {
        out.push_back(static_cast<char>(0xc0 | (code >> 6)));
        out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
    } else if (code <= 0xffff) {
        out.push_back(static_cast<char>(0xe0 | (code >> 12)));
        out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
    } else {
        out.push_back(static_cast<char>(0xf0 | (code >> 18)));
        out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
    }
}

}  // namespace

SavHeader read_sav_header(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot open save: " + path);
    }
    SavHeader header;
    header.version = read_u32(in, path);
    header.slot = read_u32(in, path);
    const std::uint32_t name_bytes = read_u32(in, path);
    if (name_bytes == 0 || name_bytes > 4096 || (name_bytes & 1) != 0) {
        throw std::runtime_error("bad save-name length in " + path);
    }
    std::vector<unsigned char> raw(name_bytes);
    in.read(reinterpret_cast<char*>(raw.data()), name_bytes);
    if (!in) {
        throw std::runtime_error("truncated save name in " + path);
    }
    for (std::size_t i = 0; i + 1 < raw.size(); i += 2) {
        std::uint32_t code = static_cast<std::uint32_t>(raw[i]) |
                             (static_cast<std::uint32_t>(raw[i + 1]) << 8);
        if (code == 0) {
            break;
        }
        // Surrogate pair.
        if (code >= 0xd800 && code <= 0xdbff && i + 3 < raw.size()) {
            const std::uint32_t low =
                static_cast<std::uint32_t>(raw[i + 2]) |
                (static_cast<std::uint32_t>(raw[i + 3]) << 8);
            if (low >= 0xdc00 && low <= 0xdfff) {
                code = 0x10000 + ((code - 0xd800) << 10) + (low - 0xdc00);
                i += 2;
            }
        }
        append_utf8(header.name, code);
    }
    return header;
}

}  // namespace dino
