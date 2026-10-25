#include "imp.h"

#include <cmath>
#include <stdexcept>
#include <utility>

#include "bytes.h"
#include "mesh.h"  // read_binary_file

namespace dino {

Imp load_imp(const std::string& path) {
    const auto d = read_binary_file(path);
    std::size_t off = 0;
    Imp imp;
    imp.model = read_string(d, off, "imp model");
    const std::uint32_t count = read_u32(d, off, "imp instance count");
    off += 4;
    if (count > 1'000'000u) {
        throw std::runtime_error("imp: implausible instance count in " + path);
    }
    need_bytes(d, off, static_cast<std::size_t>(count) * 16, "imp instances");
    if (off + static_cast<std::size_t>(count) * 16 != d.size()) {
        throw std::runtime_error("imp: trailing data in " + path);
    }
    imp.instances.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        ImpInstance instance;
        instance.x = read_f32(d, off, "imp x");
        instance.y = read_f32(d, off + 4, "imp y");
        instance.scale = read_f32(d, off + 8, "imp scale");
        instance.angle = read_f32(d, off + 12, "imp angle");
        off += 16;
        if (!std::isfinite(instance.x) || !std::isfinite(instance.y) ||
            !std::isfinite(instance.scale) || !std::isfinite(instance.angle)) {
            throw std::runtime_error("imp: non-finite record in " + path);
        }
        if (instance.scale <= 0.0f || instance.scale > 100.0f) {
            throw std::runtime_error("imp: bad scale in " + path);
        }
        imp.instances.push_back(instance);
    }
    return imp;
}

}  // namespace dino
