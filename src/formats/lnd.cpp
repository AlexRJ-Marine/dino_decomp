#include "lnd.h"

#include <stdexcept>

#include "bytes.h"
#include "mesh.h"  // read_binary_file

namespace dino {

Lnd load_lnd(const std::string& path) {
    const auto d = read_binary_file(path);
    std::size_t off = 0;
    std::string fields[8];
    for (auto& field : fields) {
        field = read_string(d, off, "lnd string");
    }
    need_bytes(d, off, 16, "lnd header");
    Lnd lnd;
    lnd.name_key = fields[0];
    lnd.basemap = fields[1];
    lnd.detailmap = fields[2];
    lnd.heightmap = fields[3];
    lnd.skybox = fields[4];
    lnd.water = fields[5];
    lnd.water2 = fields[6];
    lnd.occupation = fields[7];
    lnd.unknown_float = read_f32(d, off, "lnd unknown float");
    lnd.width = read_u32(d, off + 4, "lnd width");
    lnd.height = read_u32(d, off + 8, "lnd height");
    lnd.unknown_int = read_u32(d, off + 12, "lnd unknown int");
    off += 16;
    lnd.music = read_string(d, off, "lnd music");
    while (off < d.size()) {
        lnd.vegetation.push_back(read_string(d, off, "lnd vegetation"));
    }
    if (lnd.vegetation.empty()) {
        throw std::runtime_error("lnd: missing vegetation list in " + path);
    }
    return lnd;
}

}  // namespace dino
