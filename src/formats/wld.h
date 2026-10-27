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

// Dino-store entry, lab slot and simulation tail, kept as raw bytes so a
// writer can emit them exactly (see tools/formats/sav_write.py).
struct SavDinoStoreEntry {
    std::vector<std::uint8_t> name;      // UTF-16LE, terminator included
    std::uint32_t price = 0;
    std::vector<std::uint8_t> path;      // ASCII, terminator included
    std::vector<std::uint8_t> description;
};

struct SavShowEntry {
    std::vector<std::uint8_t> path;   // ASCII, terminator included
    std::vector<std::uint8_t> name;   // UTF-16LE, terminator included
    std::array<std::uint32_t, 6> params{};
};

struct SavLabRecord {
    std::uint8_t flag = 0;
    std::array<std::pair<std::uint32_t, std::uint32_t>, 18> genes{};
    std::vector<std::uint8_t> name;      // ASCII, terminator included
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
    std::size_t state_offset = 0;
    // Save header: version comes from the world block; the name is UTF-16LE
    // with its terminator, followed by 12 reserved bytes.
    std::uint32_t slot = 0;
    std::vector<std::uint8_t> name;
    std::array<std::uint8_t, 12> header_tail{};
    std::vector<SavShowEntry> catalog;
    std::vector<SavDinoStoreEntry> dino_store;
    std::array<int, 5> visitor_model{};
    std::vector<SavLabRecord> labs;
    std::array<std::uint32_t, 5> ai_counts{};
    std::vector<std::array<std::uint8_t, 17>> ai_entries;
    std::array<float, 10> camera{};
    std::vector<std::uint8_t> tail_extra;
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
// World-block records retained for the retail .sav writer. Shapes mirror
// tools/formats/wld.py and tools/formats/sav_write.py.
struct SavWorldWaypoint {
    std::uint32_t flags = 0, key = 0, x = 0, y = 0, z = 0, extra = 0;
};

struct SavWorldPlaced {
    std::uint32_t object_id = 0, field_14_first = 0;
    std::uint32_t start_x = 0, start_y = 0, end_x = 0, end_y = 0;
    std::vector<SavWorldWaypoint> waypoints;
    std::uint32_t class_tag = 0, field_14 = 0, field_44 = 0, field_48 = 0;
    std::array<std::uint8_t, 16> extra16{};
};

struct SavWorldAnimal { std::uint32_t object_id = 0, field_1c = 0, field_20 = 0; };
struct SavWorldEmployee { std::uint32_t object_id = 0, field_1c = 0; };
struct SavWorldBuilding {
    std::uint32_t object_id = 0, linked_id = 0, kind = 0;
    std::uint32_t cell_x = 0, cell_y = 0, state = 0, link = 0;
    std::uint32_t class_tag = 0;
};
struct SavWorldVisitor {
    std::uint32_t object_id = 0, field_1c = 0, class_tag = 0;
    std::uint32_t field_20 = 0, field_24 = 0, field_28 = 0;
};
struct SavWorldDinosaur {
    std::uint32_t object_id = 0, field_1c = 0, field_24 = 0;
    std::uint32_t field_28 = 0, field_2c = 0, class_tag = 0;
    int pairs = 18;
    std::array<std::pair<std::uint32_t, std::uint32_t>, 18> genotype{};
};

struct SavWorldRecords {
    std::uint32_t campaign_id = 0, mission_id = 0;
    std::vector<std::array<std::uint32_t, 3>> objectives;
    std::vector<SavWorldPlaced> fences, shows;
    std::vector<SavWorldAnimal> animals;
    std::vector<SavWorldEmployee> employees;
    std::vector<SavWorldBuilding> buildings;
    std::vector<SavWorldVisitor> visitors;
    std::vector<SavWorldDinosaur> dinosaurs;
    std::vector<std::uint8_t> feature_flags, building_flags, food_flags,
        misc_flags;
};

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
    SavWorldRecords records;    // writer view (saves only)
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
std::vector<std::uint8_t> write_sav_simulation(
    const SavSimulation& simulation);
std::vector<std::uint8_t> write_sav_catalog(const SavSimulation& simulation);
// Rebuild the 0x18 + name-length save header.
std::vector<std::uint8_t> write_sav_header(const SavSimulation& simulation,
                                           std::uint32_t version);
// Rebuild a save's world block from the retained records (mirrors
// tools/formats/sav_write.py::write_world).
std::vector<std::uint8_t> write_sav_world(const WldWorld& world);
std::vector<std::uint8_t> write_sav_whole(const SavSimulation& simulation,
                                          const WldWorld& world);

// Rebuild a .sav in memory (header + world + catalog + simulation).
// `new_name` (ASCII), `new_slot` and `new_budget` are optional mutations for
// re-saving; negative values keep the original.
std::vector<std::uint8_t> rebuild_sav(const std::string& path,
                                      const std::string* new_name = nullptr,
                                      int new_slot = -1,
                                      int new_budget = -1);

// Read a .sav, rebuild header + world + catalog + simulation and compare the
// result with the file bytes.
bool sav_park_roundtrip(const std::string& path, std::string* error);

}  // namespace dino
