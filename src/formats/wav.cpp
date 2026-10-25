#include "wav.h"

#include <cstddef>
#include <cstring>
#include <stdexcept>

#include "bytes.h"
#include "mesh.h"  // read_binary_file

namespace dino {
namespace {

std::uint16_t le16(const std::vector<std::uint8_t>& d, std::size_t off) {
    return static_cast<std::uint16_t>(d[off]) |
           static_cast<std::uint16_t>(d[off + 1] << 8);
}

std::uint32_t le32(const std::vector<std::uint8_t>& d, std::size_t off) {
    return static_cast<std::uint32_t>(d[off]) |
           (static_cast<std::uint32_t>(d[off + 1]) << 8) |
           (static_cast<std::uint32_t>(d[off + 2]) << 16) |
           (static_cast<std::uint32_t>(d[off + 3]) << 24);
}

}  // namespace

Wav load_wav(const std::string& path) {
    const auto d = read_binary_file(path);
    if (d.size() < 12 || std::memcmp(d.data(), "RIFF", 4) != 0 ||
        std::memcmp(d.data() + 8, "WAVE", 4) != 0) {
        throw std::runtime_error("wav: not RIFF/WAVE in " + path);
    }
    Wav wav;
    bool have_fmt = false;
    bool have_data = false;
    std::size_t off = 12;
    while (off + 8 <= d.size()) {
        const std::uint32_t size = le32(d, off + 4);
        const std::size_t body = off + 8;
        if (body + size > d.size()) {
            throw std::runtime_error("wav: chunk past EOF in " + path);
        }
        if (std::memcmp(d.data() + off, "fmt ", 4) == 0) {
            if (size < 16) {
                throw std::runtime_error("wav: short fmt chunk in " + path);
            }
            wav.format_tag = le16(d, body);
            wav.channels = le16(d, body + 2);
            wav.sample_rate = le32(d, body + 4);
            wav.byte_rate = le32(d, body + 8);
            wav.block_align = le16(d, body + 12);
            wav.bits_per_sample = le16(d, body + 14);
            have_fmt = true;
        } else if (std::memcmp(d.data() + off, "data", 4) == 0) {
            wav.data_size = size;
            wav.data.assign(d.begin() + static_cast<std::ptrdiff_t>(body),
                            d.begin() + static_cast<std::ptrdiff_t>(body + size));
            have_data = true;
        }
        off = body + size + (size & 1);
    }
    if (!have_fmt || !have_data) {
        throw std::runtime_error("wav: missing fmt/data chunk in " + path);
    }
    if (wav.format_tag != 1) {
        throw std::runtime_error("wav: unsupported format tag in " + path);
    }
    if (wav.channels != 1 && wav.channels != 2) {
        throw std::runtime_error("wav: unsupported channel count in " + path);
    }
    if (wav.bits_per_sample != 8 && wav.bits_per_sample != 16) {
        throw std::runtime_error("wav: unsupported bit depth in " + path);
    }
    const std::uint16_t expected_align = static_cast<std::uint16_t>(
        wav.channels * (wav.bits_per_sample / 8));
    if (wav.block_align != expected_align) {
        throw std::runtime_error("wav: bad block align in " + path);
    }
    wav.frames = wav.block_align ? wav.data_size / wav.block_align : 0;
    wav.duration = wav.sample_rate ? static_cast<double>(wav.frames) / wav.sample_rate : 0.0;
    return wav;
}

}  // namespace dino
