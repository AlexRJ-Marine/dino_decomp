#include "general_params.h"

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

#include "bytes.h"

namespace dino {

GeneralParams load_general_params(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("general params: cannot open " + path);
    }
    const std::vector<std::uint8_t> d((std::istreambuf_iterator<char>(in)),
                                      std::istreambuf_iterator<char>());
    GeneralParams p;
    std::size_t off = 0;
    for (float& value : p.field_110) {
        value = read_f32(d, off, "general params float");
        off += 4;
    }
    for (std::string& sound : p.sounds) {
        sound = read_string(d, off, "general params sound");
    }
    p.whistle = read_string(d, off, "general params whistle");
    p.field_154 = read_f32(d, off, "general params +0x154");
    p.field_150 = read_f32(d, off + 4, "general params +0x150");
    p.field_1a0 = read_f32(d, off + 8, "general params +0x1a0");
    p.field_1a4 = read_u32(d, off + 12, "general params +0x1a4");
    p.field_1f8 = read_f32(d, off + 16, "general params +0x1f8");
    off += 20;
    for (float& value : p.matrix_1a8) {
        value = read_f32(d, off, "general params matrix");
        off += 4;
    }
    for (float& value : p.vector_1e8) {
        value = read_f32(d, off, "general params vector");
        off += 4;
    }
    if (off != d.size()) {
        throw std::runtime_error("general params: trailing bytes in " + path);
    }
    return p;
}

}  // namespace dino
