#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace dino {

// One `drsBuilding` record from the shared world-object block.
// `link` is the per-object extra u32 written by the list loop, after the
// 7-u32 record itself.
struct WorldBuilding {
    std::uint32_t object_id = 0;
    std::uint32_t linked_id = 0;
    std::uint32_t class_tag = 0;  // 0x18 in versions 50..53
    std::uint32_t kind = 0;       // index into BuildingList.ini
    std::uint32_t cell_x = 0;
    std::uint32_t cell_y = 0;
    std::uint32_t state = 0;
    std::uint32_t link = 0;
};

struct RoadRect {
    std::int32_t x0 = 0;
    std::int32_t y0 = 0;
    std::int32_t x1 = 0;
    std::int32_t y1 = 0;
};

// One `drsFence` placed base. start/end are the world-unit rectangle.
struct WorldFence {
    std::uint32_t object_id = 0;
    std::uint32_t field_14 = 0;
    std::int32_t start_x = 0;
    std::int32_t start_y = 0;
    std::int32_t end_x = 0;
    std::int32_t end_y = 0;
    std::uint32_t field_44 = 0;
    std::uint32_t field_48 = 0;
};

// One `drsDinosaur` world record: object links + the 18 gene pairs.
struct WorldDinosaur {
    std::uint32_t object_id = 0;
    std::uint32_t field_1c = 0;
    std::array<std::pair<std::uint32_t, std::uint32_t>, 18> genotype{};
};

// The subset of a `.wld`/`.sav` world block the port needs.
// See docs/formats/wld.md; the readers mirror tools/formats/wld.py.
struct WldWorld {
    std::uint32_t version = 0;
    std::uint32_t mode = 0;
    std::uint32_t campaign_id = 0;
    std::uint32_t mission_id = 0;
    std::uint32_t fence_count = 0;
    std::uint32_t show_count = 0;
    std::uint32_t animal_count = 0;
    std::uint32_t employee_count = 0;
    std::uint32_t visitor_count = 0;
    std::uint32_t dinosaur_count = 0;
    std::vector<WorldBuilding> buildings;
    std::vector<WorldFence> fences;
    std::vector<WorldDinosaur> dinosaurs;
    std::vector<RoadRect> roads;
    std::array<float, 10> camera{};
    bool has_camera = false;
    std::size_t world_end = 0;  // first byte after the parsed world block
};

// Campaign `.wld` (version 50..53): header (version, mode), world block,
// simulation roads, 40-byte camera tail.
WldWorld read_wld(const std::string& path);

// Retail `.sav`: header (version, slot, UTF-16 name, 12 reserved bytes) then
// the same world block with the campaign flag clear. The show catalog that
// follows is left unread; `world_end` points at its count.
WldWorld read_sav_world(const std::string& path);

}  // namespace dino
