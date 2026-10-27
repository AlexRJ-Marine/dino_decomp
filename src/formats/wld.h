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

// One `drsVisitor` world object (simple base). `field_20` is the global `.bvi`
// profile id (0..9); `field_1c` matches the simulation visitor record id.
struct WorldVisitor {
    std::uint32_t object_id = 0;
    std::uint32_t field_1c = 0;
    std::uint32_t class_tag = 0;
    std::uint32_t field_20 = 0;
    std::uint32_t field_24 = 0;
    std::uint32_t field_28 = 0;
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

// One 0x50-byte simulation visitor record.
struct SavVisitor {
    std::uint32_t object_id = 0;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::uint32_t flags = 0;
    std::uint32_t unknown_10 = 0;
    std::uint32_t state = 0;
    float fx = 0.0f;
    float fy = 0.0f;
    float dx = 0.0f;
    float dy = 0.0f;
    std::uint32_t unknown_28 = 0;
    std::uint32_t zero_2c = 0;
    std::uint32_t visitor_id = 0;
    std::array<std::uint32_t, 7> needs{};
};

// spPark economy stream (writer 0x00585780, 0x21a bytes at park block
// +0x30). The budget panel reads the *yesterday* block; the current block
// holds the counters accumulated since the last rollover. Poison counters
// (0xBAADF00D) mean "not settled yet".
struct SavEconomy {
    float sellback = 1.0f;
    int food_total = 0;
    int food_count = 0;
    int secu_total = 0;
    int secu_count = 0;
    int ticket_total = 0;
    int ticket_count = 0;
    std::array<int, 2> staff_buckets{};
    std::array<int, 13> shop_revenue{};
    std::array<int, 13> shop_sales{};
    int dinoachat_count = 0;
    int dinoachat_total = 0;
    int construction_count = 0;
    int construction_total = 0;
    int laboaction_count = 0;
    int laboaction_total = 0;
    int ventes_count = 0;
    int ventes_total = 0;
    std::array<int, 2> yesterday_staff{};
    std::array<int, 2> yesterday_food{};
    std::array<int, 2> yesterday_secu{};
    std::array<int, 13> yesterday_revenue{};
    std::array<int, 13> yesterday_sales{};
    std::array<int, 13> yesterday_totals{};
};

// spSimuManager prefix after the world block + show catalog.
struct SavSimulation {
    std::vector<RoadRect> roads;
    std::vector<SavVisitor> visitors;
    SavEconomy economy;
    // Park record header + schedule tail (writer order in
    // tools/formats/sav_write.py). `park_offset` is the absolute file
    // offset of the 0x293-byte block.
    std::size_t park_offset = 0;
    std::uint32_t park_class = 0;
    std::uint32_t park_header_a = 0;
    std::uint32_t park_header_b = 0;
    float park_header_c = 0.0f;
    std::array<std::uint8_t, 32> park_reserved{};
    std::array<std::uint8_t, 2> bankruptcy_flags{};
    std::array<int, 2> cumulative_staff{};
    std::array<int, 2> cumulative_food{};
    std::array<int, 2> cumulative_secu{};
    std::array<int, 13> cumulative_revenue{};
    std::array<int, 13> cumulative_sales{};
    std::array<int, 13> cumulative_totals{};
    float schedule_current = 0.0f;
    std::uint32_t schedule_field = 0;
    std::array<std::uint32_t, 11> schedule_tasks{};
    std::array<std::uint8_t, 12> schedule_trailer{};
    std::uint8_t schedule_final = 0;
    int budget = 0;
    int day = 0;
    float elapsed = 0.0f;
    bool has_park = false;
};

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
    std::vector<WorldVisitor> visitors;
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

// Reads the spSimuManager prefix after the world block + 39-show catalog:
// road rectangles, the three empty lists, the 0x50-byte visitor records and
// the park block's budget/day/elapsed. Fences are in the world block.
SavSimulation read_sav_simulation(const std::string& path);

// Rebuild the 0x293-byte park record in retail writer order (see
// tools/formats/sav_write.py).
std::vector<std::uint8_t> write_sav_park_block(
    const SavSimulation& simulation);

// Read a .sav, rebuild its park block and compare it with the file bytes.
bool sav_park_roundtrip(const std::string& path, std::string* error);

}  // namespace dino
