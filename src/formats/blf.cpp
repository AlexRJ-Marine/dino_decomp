#include "blf.h"

#include <stdexcept>
#include <utility>

#include "bytes.h"
#include "mesh.h"  // read_binary_file

namespace dino {

Blf load_blf(const std::string& path) {
    const auto d = read_binary_file(path);
    if (d.size() < 16) {
        throw std::runtime_error("blf: shorter than header in " + path);
    }
    Blf blf;
    blf.version = read_u32(d, 0, "blf version");
    blf.lod_count = read_u32(d, 4, "blf lod count");
    blf.unknown = read_u32(d, 8, "blf unknown");
    blf.texture_count = read_u32(d, 12, "blf texture count");
    if (blf.version != 2) {
        throw std::runtime_error("blf: unsupported version in " + path);
    }
    if (blf.lod_count == 0 || blf.lod_count > 16) {
        throw std::runtime_error("blf: bad LOD count in " + path);
    }
    std::size_t off = 16;
    blf.lods.reserve(blf.lod_count);
    for (std::uint32_t i = 0; i < blf.lod_count; ++i) {
        blf.lods.push_back(read_string(d, off, "blf lod path"));
    }
    blf.texture_dir = read_string(d, off, "blf texture dir");
    if (off != d.size()) {
        throw std::runtime_error("blf: trailing data in " + path);
    }
    return blf;
}

}  // namespace dino
