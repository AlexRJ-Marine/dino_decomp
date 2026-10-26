#pragma once

#include <cstdint>
#include <string>

namespace dino {

// `.sav` header: version, slot, length-prefixed UTF-16LE name, 12 reserved
// zeros. The world/simulation/AI/scenario managers follow.
struct SavHeader {
    std::uint32_t version = 0;
    std::uint32_t slot = 0;
    std::string name;
};

SavHeader read_sav_header(const std::string& path);

}  // namespace dino
