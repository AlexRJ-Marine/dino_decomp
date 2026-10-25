#pragma once

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace dino {

inline void need_bytes(const std::vector<std::uint8_t>& d, std::size_t off,
                       std::size_t n, const char* what) {
    if (off > d.size() || n > d.size() - off) {
        throw std::runtime_error(std::string("format: ") + what + " past EOF");
    }
}

inline std::uint8_t read_u8(const std::vector<std::uint8_t>& d, std::size_t off,
                            const char* what) {
    need_bytes(d, off, 1, what);
    return d[off];
}

inline std::uint32_t read_u32(const std::vector<std::uint8_t>& d, std::size_t off,
                              const char* what) {
    need_bytes(d, off, 4, what);
    return static_cast<std::uint32_t>(d[off]) |
           (static_cast<std::uint32_t>(d[off + 1]) << 8) |
           (static_cast<std::uint32_t>(d[off + 2]) << 16) |
           (static_cast<std::uint32_t>(d[off + 3]) << 24);
}

inline std::int32_t read_i32(const std::vector<std::uint8_t>& d, std::size_t off,
                             const char* what) {
    return static_cast<std::int32_t>(read_u32(d, off, what));
}

inline float read_f32(const std::vector<std::uint8_t>& d, std::size_t off,
                      const char* what) {
    const std::uint32_t bits = read_u32(d, off, what);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

inline std::uint64_t read_u64(const std::vector<std::uint8_t>& d, std::size_t off,
                              const char* what) {
    need_bytes(d, off, 8, what);
    std::uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= static_cast<std::uint64_t>(d[off + i]) << (8 * i);
    }
    return value;
}

inline std::string read_string(const std::vector<std::uint8_t>& d, std::size_t& off,
                               const char* what) {
    const std::uint32_t n = read_u32(d, off, what);
    off += 4;
    if (n == 0 || n > 4096 || off + n > d.size()) {
        throw std::runtime_error(std::string("format: bad ") + what + " length");
    }
    if (d[off + n - 1] != 0) {
        throw std::runtime_error(std::string("format: ") + what + " not NUL-terminated");
    }
    std::string s(reinterpret_cast<const char*>(d.data() + off), n - 1);
    off += n;
    return s;
}

}  // namespace dino
