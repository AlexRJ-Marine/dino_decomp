#pragma once

#include <cstdint>
#include <string>

namespace dino {

// `.sav` header: version, slot, length-prefixed UTF-16LE name, then a
// 12-byte block: mode (0 free game/island, 1 campaign), unused, island
// index (mode 0) / campaign id (mode 1). See `docs/formats/sav.md`
// ("Save-header island"); the world managers follow at 0x18 + name_len.
struct SavHeader {
    std::uint32_t version = 0;
    std::uint32_t slot = 0;
    std::string name;
};

SavHeader read_sav_header(const std::string& path);

}  // namespace dino
