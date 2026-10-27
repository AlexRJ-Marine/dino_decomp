#include "wld.h"

#include <cstring>
#include <stdexcept>

#include "bytes.h"
#include "mesh.h"

namespace dino {
namespace {

constexpr std::uint32_t kMinVersion = 50;
constexpr std::uint32_t kMaxVersion = 53;
constexpr std::size_t kSavHeaderExtra = 12;
constexpr std::size_t kCameraBytes = 40;

class Reader {
public:
    Reader(const std::vector<std::uint8_t>& data, std::size_t offset,
           std::size_t end, std::uint32_t version, const std::string& path)
        : data_(data), off_(offset), end_(end), version_(version), path_(path) {
        if (end_ > data_.size() || off_ > end_) {
            throw std::runtime_error("wld: bad range in " + path_);
        }
    }

    std::uint32_t u32(const char* what) {
        need(4, what);
        const std::uint32_t value = read_u32(data_, off_, what);
        off_ += 4;
        return value;
    }

    std::int32_t i32(const char* what) {
        return static_cast<std::int32_t>(u32(what));
    }

    void skip(std::size_t size, const char* what) {
        need(size, what);
        off_ += size;
    }

    std::vector<std::uint8_t> bytes(std::size_t size, const char* what) {
        need(size, what);
        std::vector<std::uint8_t> out(data_.begin() + off_,
                                       data_.begin() + off_ + size);
        off_ += size;
        return out;
    }

    std::uint32_t count(const char* what, std::uint32_t limit = 200000) {
        const std::uint32_t value = u32(what);
        if (value > limit) {
            throw std::runtime_error("wld: implausible count in " + path_);
        }
        return value;
    }

    void need(std::size_t size, const char* what) const {
        if (size > end_ - off_) {
            throw std::runtime_error(std::string("wld: ") + what +
                                     " past world end in " + path_);
        }
    }

    std::size_t offset() const { return off_; }
    std::uint32_t version() const { return version_; }

private:
    const std::vector<std::uint8_t>& data_;
    std::size_t off_ = 0;
    std::size_t end_ = 0;
    std::uint32_t version_ = 0;
    const std::string& path_;
};

void read_waypoints(Reader& r) {
    const std::uint32_t n = r.count("waypoint count", 10000);
    for (std::uint32_t i = 0; i < n; ++i) {
        if (r.version() >= 3) {
            (void)r.u32("waypoint flags");
        }
        (void)r.u32("waypoint key");
        (void)r.u32("waypoint x");
        (void)r.u32("waypoint y");
        (void)r.u32("waypoint z");
        (void)r.u32("waypoint extra");
    }
}

void read_placed_base(Reader& r) {
    (void)r.u32("object id");
    (void)r.u32("field 14");
    (void)r.u32("start x");
    (void)r.u32("start y");
    (void)r.u32("end x");
    (void)r.u32("end y");
    read_waypoints(r);
}

WorldFence read_fence(Reader& r) {
    WorldFence fence;
    fence.object_id = r.u32("fence object id");
    fence.field_14 = r.u32("fence field 14");
    fence.start_x = static_cast<std::int32_t>(r.u32("fence start x"));
    fence.start_y = static_cast<std::int32_t>(r.u32("fence start y"));
    fence.end_x = static_cast<std::int32_t>(r.u32("fence end x"));
    fence.end_y = static_cast<std::int32_t>(r.u32("fence end y"));
    read_waypoints(r);
    (void)r.u32("fence class tag");
    fence.field_14 = r.u32("fence field 14 repeat");
    fence.start_x = static_cast<std::int32_t>(r.u32("fence start x repeat"));
    fence.start_y = static_cast<std::int32_t>(r.u32("fence start y repeat"));
    fence.end_x = static_cast<std::int32_t>(r.u32("fence end x repeat"));
    fence.end_y = static_cast<std::int32_t>(r.u32("fence end y repeat"));
    if (r.version() >= 2) {
        fence.field_44 = r.u32("fence field 44");
    }
    if (r.version() >= 0x25) {
        fence.field_48 = r.u32("fence field 48");
    }
    return fence;
}

void read_show(Reader& r) {
    read_placed_base(r);
    (void)r.u32("show class tag");
    if (r.version() >= 2) {
        (void)r.u32("show field 44");
    }
    if (r.version() >= 0x25) {
        (void)r.u32("show field 48");
    }
}

void read_animal(Reader& r) {
    (void)r.u32("animal object id");
    (void)r.u32("animal field 1c");
    if (r.version() > 0x32) {
        (void)r.u32("animal field 20");
    }
}

void read_employee(Reader& r) {
    (void)r.u32("employee object id");
    (void)r.u32("employee field 1c");
}

WorldBuilding read_building(Reader& r) {
    WorldBuilding b;
    b.object_id = r.u32("building object id");
    b.linked_id = r.u32("building linked id");
    b.class_tag = r.u32("building class tag");
    b.kind = r.u32("building kind");
    b.cell_x = r.u32("building cell x");
    b.cell_y = r.u32("building cell y");
    b.state = r.u32("building state");
    b.link = r.u32("building link");
    return b;
}

WorldVisitor read_visitor(Reader& r) {
    WorldVisitor v;
    v.object_id = r.u32("visitor object id");
    v.field_1c = r.u32("visitor field 1c");
    v.class_tag = r.u32("visitor class tag");
    v.field_20 = r.u32("visitor field 20");
    v.field_24 = r.u32("visitor field 24");
    if (r.version() >= 5) {
        v.field_28 = r.u32("visitor field 28");
    }
    return v;
}

WorldDinosaur read_dinosaur(Reader& r) {
    WorldDinosaur dinosaur;
    dinosaur.object_id = r.u32("dinosaur object id");
    dinosaur.field_1c = r.u32("dinosaur field 1c");
    if (r.version() >= 0x25) {
        (void)r.u32("dinosaur field 24");
        (void)r.u32("dinosaur field 28");
        (void)r.u32("dinosaur field 2c");
    }
    (void)r.u32("dinosaur class tag");
    const int pairs = r.version() > 0x15 ? 18 : 16;
    for (int i = 0; i < pairs; ++i) {
        const std::uint32_t a = r.u32("dinosaur allele a");
        const std::uint32_t b = r.u32("dinosaur allele b");
        dinosaur.genotype[static_cast<std::size_t>(i)] = {a, b};
    }
    return dinosaur;
}

void read_unlock_array(Reader& r, const char* what) {
    const std::uint32_t n = r.count(what, 10000);
    r.skip(n, what);
}

void read_world_block(Reader& r, bool campaign, WldWorld& out) {
    if (!campaign) {
        out.campaign_id = r.u32("campaign id");
        out.mission_id = r.u32("mission id");
        if (r.version() > 9) {
            const std::uint32_t objectives = r.count("objective count", 10000);
            r.skip(static_cast<std::size_t>(objectives) * 12, "objectives");
        }
    }

    out.fence_count = r.count("fence count", 10000);
    out.fences.reserve(out.fence_count);
    for (std::uint32_t i = 0; i < out.fence_count; ++i) {
        out.fences.push_back(read_fence(r));
    }

    out.show_count = r.count("show count", 10000);
    for (std::uint32_t i = 0; i < out.show_count; ++i) {
        read_show(r);
    }

    if (r.version() > 1) {
        out.animal_count = r.count("animal count", 10000);
        for (std::uint32_t i = 0; i < out.animal_count; ++i) {
            read_animal(r);
        }
    }

    if (r.version() > 2) {
        out.employee_count = r.count("employee count", 10000);
        for (std::uint32_t i = 0; i < out.employee_count; ++i) {
            read_employee(r);
        }
    }

    const std::uint32_t buildings = r.count("building count", 10000);
    out.buildings.reserve(buildings);
    for (std::uint32_t i = 0; i < buildings; ++i) {
        out.buildings.push_back(read_building(r));
    }

    if (!campaign) {
        out.visitor_count = r.count("visitor count", 100000);
        out.visitors.reserve(out.visitor_count);
        for (std::uint32_t i = 0; i < out.visitor_count; ++i) {
            out.visitors.push_back(read_visitor(r));
        }
    }

    if (r.version() > 1) {
        out.dinosaur_count = r.count("dinosaur count", 100000);
        out.dinosaurs.reserve(out.dinosaur_count);
        for (std::uint32_t i = 0; i < out.dinosaur_count; ++i) {
            out.dinosaurs.push_back(read_dinosaur(r));
        }
    }

    if (!campaign) {
        read_unlock_array(r, "feature flags");
        read_unlock_array(r, "building flags");
        read_unlock_array(r, "food flags");
        read_unlock_array(r, "misc flags");
        // The show catalog count follows; sav.py owns that structure.
    }

    out.world_end = r.offset();
}

// Writer-oriented parse of the save's world block. Mirrors
// tools/formats/wld.py::read_world_block so every byte the writer needs is
// retained (the display-side readers above keep the subset the port uses).
SavWorldWaypoint read_record_waypoint(Reader& r) {
    SavWorldWaypoint point;
    if (r.version() >= 3) {
        point.flags = r.u32("waypoint flags");
    }
    point.key = r.u32("waypoint key");
    if (r.version() < 0x25) {
        point.key += 0x1E61;
    }
    point.x = r.u32("waypoint x");
    point.y = r.u32("waypoint y");
    point.z = r.u32("waypoint z");
    point.extra = r.u32("waypoint extra");
    return point;
}

SavWorldPlaced read_record_placed(Reader& r) {
    SavWorldPlaced placed;
    placed.object_id = r.u32("placed object id");
    placed.field_14_first = r.u32("placed field 14 first");
    placed.start_x = r.u32("placed start x");
    placed.start_y = r.u32("placed start y");
    placed.end_x = r.u32("placed end x");
    placed.end_y = r.u32("placed end y");
    if (r.version() > 1) {
        const std::uint32_t n = r.count("waypoint count", 10000);
        placed.waypoints.reserve(n);
        for (std::uint32_t i = 0; i < n; ++i) {
            placed.waypoints.push_back(read_record_waypoint(r));
        }
    }
    return placed;
}

void read_sav_records(Reader& r, WldWorld& out) {
    SavWorldRecords& w = out.records;
    w.campaign_id = r.u32("campaign id");
    w.mission_id = r.u32("mission id");
    if (r.version() > 9) {
        const std::uint32_t objectives = r.count("objective count", 10000);
        w.objectives.reserve(objectives);
        for (std::uint32_t i = 0; i < objectives; ++i) {
            std::array<std::uint32_t, 3> objective{};
            objective[0] = r.u32("objective a");
            objective[1] = r.u32("objective b");
            objective[2] = r.u32("objective c");
            w.objectives.push_back(objective);
        }
    }

    const std::uint32_t fences = r.count("fence count", 10000);
    w.fences.reserve(fences);
    for (std::uint32_t i = 0; i < fences; ++i) {
        SavWorldPlaced fence = read_record_placed(r);
        fence.class_tag = r.u32("fence class tag");
        fence.field_14 = r.u32("fence field 14");
        const std::vector<std::uint8_t> rect =
            r.bytes(fence.extra16.size(), "fence repeated rect");
        for (std::size_t b = 0; b < rect.size(); ++b) {
            fence.extra16[b] = rect[b];
        }
        if (r.version() >= 2) {
            fence.field_44 = r.u32("fence field 44");
        }
        if (r.version() >= 0x25) {
            fence.field_48 = r.u32("fence field 48");
        }
        w.fences.push_back(std::move(fence));
    }

    const std::uint32_t shows = r.count("show count", 10000);
    w.shows.reserve(shows);
    for (std::uint32_t i = 0; i < shows; ++i) {
        SavWorldPlaced show = read_record_placed(r);
        show.class_tag = r.u32("show class tag");
        if (r.version() >= 2) {
            show.field_44 = r.u32("show field 44");
        }
        if (r.version() >= 0x25) {
            show.field_48 = r.u32("show field 48");
        }
        if (r.version() < 2) {
            const std::uint32_t legacy = r.count("legacy show count", 10000);
            r.skip(static_cast<std::size_t>(legacy) * 0x18,
                   "legacy show records");
        }
        w.shows.push_back(std::move(show));
    }

    if (r.version() > 1) {
        const std::uint32_t animals = r.count("animal count", 10000);
        w.animals.reserve(animals);
        for (std::uint32_t i = 0; i < animals; ++i) {
            SavWorldAnimal animal;
            animal.object_id = r.u32("animal object id");
            animal.field_1c = r.u32("animal field 1c");
            if (r.version() > 0x32) {
                animal.field_20 = r.u32("animal field 20");
            }
            w.animals.push_back(animal);
        }
    }

    if (r.version() > 2) {
        const std::uint32_t employees = r.count("employee count", 10000);
        w.employees.reserve(employees);
        for (std::uint32_t i = 0; i < employees; ++i) {
            SavWorldEmployee employee;
            employee.object_id = r.u32("employee object id");
            employee.field_1c = r.u32("employee field 1c");
            w.employees.push_back(employee);
        }
    }

    const std::uint32_t buildings = r.count("building count", 10000);
    w.buildings.reserve(buildings);
    for (std::uint32_t i = 0; i < buildings; ++i) {
        SavWorldBuilding building;
        building.object_id = r.u32("building object id");
        building.linked_id = r.u32("building linked id");
        building.class_tag = r.u32("building class tag");
        building.kind = r.u32("building kind");
        building.cell_x = r.u32("building cell x");
        building.cell_y = r.u32("building cell y");
        building.state = r.u32("building state");
        building.link = r.u32("building link");
        w.buildings.push_back(building);
    }

    // Save games carry the visitor list except in the legacy range.
    if (!(r.version() >= 6 && r.version() <= 0x10)) {
        const std::uint32_t visitors = r.count("visitor count", 100000);
        w.visitors.reserve(visitors);
        for (std::uint32_t i = 0; i < visitors; ++i) {
            SavWorldVisitor visitor;
            visitor.object_id = r.u32("visitor object id");
            visitor.field_1c = r.u32("visitor field 1c");
            visitor.class_tag = r.u32("visitor class tag");
            visitor.field_20 = r.u32("visitor field 20");
            visitor.field_24 = r.u32("visitor field 24");
            if (r.version() >= 5) {
                visitor.field_28 = r.u32("visitor field 28");
            }
            w.visitors.push_back(visitor);
        }
    }

    if (r.version() > 1) {
        const std::uint32_t dinosaurs = r.count("dinosaur count", 100000);
        w.dinosaurs.reserve(dinosaurs);
        for (std::uint32_t i = 0; i < dinosaurs; ++i) {
            SavWorldDinosaur dinosaur;
            dinosaur.object_id = r.u32("dinosaur object id");
            dinosaur.field_1c = r.u32("dinosaur field 1c");
            if (r.version() >= 0x25) {
                dinosaur.field_24 = r.u32("dinosaur field 24");
                dinosaur.field_28 = r.u32("dinosaur field 28");
                dinosaur.field_2c = r.u32("dinosaur field 2c");
            }
            dinosaur.class_tag = r.u32("dinosaur class tag");
            dinosaur.pairs = r.version() > 0x15 ? 18 : 16;
            for (int g = 0; g < dinosaur.pairs; ++g) {
                const std::uint32_t first = r.u32("dinosaur allele a");
                const std::uint32_t second = r.u32("dinosaur allele b");
                dinosaur.genotype[static_cast<std::size_t>(g)] = {first,
                                                                  second};
            }
            w.dinosaurs.push_back(dinosaur);
        }
    }

    w.feature_flags = r.bytes(r.count("feature flag count", 10000),
                              "feature flags");
    w.building_flags = r.bytes(r.count("building flag count", 10000),
                               "building flags");
    w.food_flags = r.bytes(r.count("food flag count", 10000), "food flags");
    w.misc_flags = r.bytes(r.count("misc flag count", 10000), "misc flags");
}

void check_version(std::uint32_t version, const std::string& path) {
    if (version < kMinVersion || version > kMaxVersion) {
        throw std::runtime_error("wld: unsupported version " +
                                 std::to_string(version) + " in " + path);
    }
}

}  // namespace

WldWorld read_wld(const std::string& path) {
    const std::vector<std::uint8_t> data = read_binary_file(path);
    if (data.size() < 8) {
        throw std::runtime_error("wld: shorter than header: " + path);
    }
    WldWorld out;
    out.version = read_u32(data, 0, "wld version");
    check_version(out.version, path);
    std::size_t off = 4;
    out.mode = read_u32(data, off, "wld mode");
    off += 4;

    std::size_t body_end = data.size();
    if (data.size() >= kCameraBytes) {
        body_end = data.size() - kCameraBytes;
        for (int i = 0; i < 10; ++i) {
            out.camera[static_cast<std::size_t>(i)] =
                read_f32(data, body_end + static_cast<std::size_t>(i) * 4,
                         "wld camera");
        }
        out.has_camera = true;
    }

    Reader r(data, off, body_end, out.version, path);
    read_world_block(r, true, out);

    const std::uint32_t road_count = r.count("road count", 100000);
    out.roads.reserve(road_count);
    for (std::uint32_t i = 0; i < road_count; ++i) {
        RoadRect road;
        road.x0 = r.i32("road x0");
        road.y0 = r.i32("road y0");
        road.x1 = r.i32("road x1");
        road.y1 = r.i32("road y1");
        out.roads.push_back(road);
    }

    // The simulation object lists follow the road rectangles; the camera is
    // the last 40 bytes of the file. The intermediate lists are not decoded
    // yet, so do not require the roads to end at the camera.
    return out;
}

WldWorld read_sav_world(const std::string& path) {
    const std::vector<std::uint8_t> data = read_binary_file(path);
    if (data.size() < 0x18) {
        throw std::runtime_error("wld: shorter than save header: " + path);
    }
    WldWorld out;
    out.version = read_u32(data, 0, "save version");
    check_version(out.version, path);
    const std::uint32_t name_bytes = read_u32(data, 8, "save name length");
    const std::size_t offset = 0x18 + static_cast<std::size_t>(name_bytes);
    if (name_bytes == 0 || (name_bytes & 1) != 0 || offset > data.size()) {
        throw std::runtime_error("wld: bad save-name length in " + path);
    }
    Reader r(data, offset, data.size(), out.version, path);
    read_world_block(r, false, out);

    // Parse the same bytes again into the writer model and require it to end
    // exactly where the display readers stopped.
    Reader records(data, offset, out.world_end, out.version, path);
    read_sav_records(records, out);
    if (records.offset() != out.world_end) {
        throw std::runtime_error("wld: world records disagree with the block "
                                 "size in " + path);
    }
    return out;
}

SavSimulation read_sav_simulation(const std::string& path) {
    const std::vector<std::uint8_t> data = read_binary_file(path);
    const WldWorld world = read_sav_world(path);
    Reader r(data, world.world_end, data.size(), world.version, path);
    SavSimulation out;
    const std::uint32_t name_len = read_u32(data, 8, "save name length");
    const std::size_t world_start = 24 + name_len;
    out.header_raw.assign(data.begin(), data.begin() + world_start);

    // Show catalog: ASCII path, UTF-16 name, six parameters.
    const std::uint32_t shows = r.count("show catalog count", 10000);
    for (std::uint32_t i = 0; i < shows; ++i) {
        SavShowEntry entry;
        std::uint32_t n = r.count("show path", 1 << 20);
        std::size_t at = r.offset();
        entry.path.assign(data.begin() + at, data.begin() + at + n);
        r.skip(n, "show path");
        n = r.count("show name", 1 << 20);
        at = r.offset();
        entry.name.assign(data.begin() + at, data.begin() + at + n);
        r.skip(n, "show name");
        for (std::uint32_t& param : entry.params) {
            param = r.count("show parameter", 0xFFFFFFFFu);
        }
        out.catalog.push_back(std::move(entry));
    }

    out.state_offset = r.offset();
    const std::uint32_t road_count = r.count("simulation road count", 10000);
    out.roads.reserve(road_count);
    for (std::uint32_t i = 0; i < road_count; ++i) {
        RoadRect road;
        road.x0 = r.i32("road x0");
        road.y0 = r.i32("road y0");
        road.x1 = r.i32("road x1");
        road.y1 = r.i32("road y1");
        out.roads.push_back(road);
    }

    // Three simulation lists are empty in retail saves.
    for (int i = 0; i < 3; ++i) {
        const std::uint32_t count = r.count("simulation list count", 10000);
        if (count != 0) {
            throw std::runtime_error(
                "wld: non-empty simulation list in " + path);
        }
    }

    const std::uint32_t visitors = r.count("visitor count", 10000);
    out.visitors.reserve(visitors);
    for (std::uint32_t i = 0; i < visitors; ++i) {
        const std::size_t rec = r.offset();
        r.skip(0x50, "visitor record");
        SavVisitor visitor;
        visitor.object_id = read_u32(data, rec, "visitor object id");
        visitor.x = read_i32(data, rec + 0x04, "visitor x");
        visitor.y = read_i32(data, rec + 0x08, "visitor y");
        visitor.flags = read_u32(data, rec + 0x0c, "visitor flags");
        visitor.unknown_10 = read_u32(data, rec + 0x10, "visitor +0x10");
        visitor.state = read_u32(data, rec + 0x14, "visitor state");
        visitor.fx = read_f32(data, rec + 0x18, "visitor fx");
        visitor.fy = read_f32(data, rec + 0x1c, "visitor fy");
        visitor.dx = read_f32(data, rec + 0x20, "visitor dx");
        visitor.dy = read_f32(data, rec + 0x24, "visitor dy");
        visitor.unknown_28 = read_u32(data, rec + 0x28, "visitor +0x28");
        visitor.zero_2c = read_u32(data, rec + 0x2c, "visitor +0x2c");
        visitor.visitor_id = read_u32(data, rec + 0x30, "visitor id");
        for (std::size_t n = 0; n < visitor.needs.size(); ++n) {
            visitor.needs[n] =
                read_u32(data, rec + 0x34 + n * 4, "visitor need");
        }
        out.visitors.push_back(visitor);
    }

    const std::uint32_t park_count = r.count("park count", 10);
    if (park_count != 1) {
        throw std::runtime_error("wld: unexpected park count in " + path);
    }
    const std::size_t block = r.offset();
    if (block + 0x293 > data.size()) {
        throw std::runtime_error("wld: truncated park block in " + path);
    }
    out.budget = static_cast<int>(read_u32(data, block + 0x30, "park budget"));
    out.day = static_cast<int>(read_u32(data, block + 0x24a, "park day"));
    out.elapsed = read_f32(data, block + 0x24e, "park elapsed");
    out.has_park = true;

    // spPark economy stream at block+0x30 (writer order, see
    // tools/formats/sav_graph.py::_read_park_stream and
    // tools/formats/economy.py).
    const std::size_t e = block + 0x30;
    auto eu = [&](std::size_t off) {
        return static_cast<int>(read_u32(data, e + off, "park economy"));
    };
    out.economy.sellback = read_f32(data, e + 0x04, "park sellback");
    out.economy.food_total = eu(0x08);
    out.economy.food_count = eu(0x0c);
    out.economy.secu_total = eu(0x10);
    out.economy.secu_count = eu(0x14);
    out.economy.ticket_total = eu(0x18);
    out.economy.ticket_count = eu(0x1c);
    // 0x20: two bankruptcy bytes; 0x22: two staff buckets.
    out.economy.staff_buckets[0] = eu(0x22);
    out.economy.staff_buckets[1] = eu(0x26);
    for (int i = 0; i < 13; ++i) {
        out.economy.shop_revenue[i] = eu(0x2a + i * 4);
        out.economy.shop_sales[i] = eu(0x5e + i * 4);
    }
    out.economy.dinoachat_count = eu(0x92);
    out.economy.dinoachat_total = eu(0x96);
    out.economy.construction_count = eu(0x9a);
    out.economy.construction_total = eu(0x9e);
    out.economy.laboaction_count = eu(0xa2);
    out.economy.laboaction_total = eu(0xa6);
    out.economy.ventes_count = eu(0xaa);
    out.economy.ventes_total = eu(0xae);
    out.economy.yesterday_staff[0] = eu(0xb2);
    out.economy.yesterday_staff[1] = eu(0xb6);
    out.economy.yesterday_food[0] = eu(0xba);
    out.economy.yesterday_food[1] = eu(0xbe);
    out.economy.yesterday_secu[0] = eu(0xc2);
    out.economy.yesterday_secu[1] = eu(0xc6);
    for (int i = 0; i < 13; ++i) {
        out.economy.yesterday_revenue[i] = eu(0xca + i * 4);
        out.economy.yesterday_sales[i] = eu(0xfe + i * 4);
        out.economy.yesterday_totals[i] = eu(0x132 + i * 4);
    }
    out.park_offset = block;
    out.park_class = read_u32(data, block + 0x00, "park class");
    out.park_header_a = read_u32(data, block + 0x04, "park header a");
    out.park_header_b = read_u32(data, block + 0x08, "park header b");
    out.park_header_c = read_f32(data, block + 0x0c, "park header c");
    for (std::size_t i = 0; i < out.park_reserved.size(); ++i) {
        out.park_reserved[i] = data[block + 0x10 + i];
    }
    out.bankruptcy_flags[0] = data[e + 0x20];
    out.bankruptcy_flags[1] = data[e + 0x21];
    for (std::size_t i = 0; i < 2; ++i) {
        out.cumulative_staff[i] = eu(0x166 + i * 4);
        out.cumulative_food[i] = eu(0x16e + i * 4);
        out.cumulative_secu[i] = eu(0x176 + i * 4);
    }
    for (int i = 0; i < 13; ++i) {
        out.cumulative_revenue[i] = eu(0x17e + i * 4);
        out.cumulative_sales[i] = eu(0x1b2 + i * 4);
        out.cumulative_totals[i] = eu(0x1e6 + i * 4);
    }
    out.schedule_current = read_f32(data, block + 0x252, "schedule current");
    out.schedule_field = read_u32(data, block + 0x256, "schedule field");
    for (std::size_t i = 0; i < out.schedule_tasks.size(); ++i) {
        out.schedule_tasks[i] =
            read_u32(data, block + 0x25a + i * 4, "schedule task");
    }
    for (std::size_t i = 0; i < out.schedule_trailer.size(); ++i) {
        out.schedule_trailer[i] = data[block + 0x286 + i];
    }
    out.schedule_final = data[block + 0x292];

    // Simulation tail after the park block: dino store, visitor model, lab
    // slots, AI counts/entries, camera and the trailing bytes.
    std::size_t o = block + 0x293;
    auto tu32 = [&](const char* what) {
        const std::uint32_t value = read_u32(data, o, what);
        o += 4;
        return value;
    };
    auto tf32 = [&](const char* what) {
        const float value = read_f32(data, o, what);
        o += 4;
        return value;
    };
    auto tbytes = [&](std::uint32_t count, const char* what) {
        if (o + count > data.size()) {
            throw std::runtime_error(std::string("wld: truncated ") + what);
        }
        std::vector<std::uint8_t> bytes(data.begin() + o,
                                        data.begin() + o + count);
        o += count;
        return bytes;
    };
    const std::uint32_t dino_count = tu32("dino store count");
    for (std::uint32_t i = 0; i < dino_count; ++i) {
        SavDinoStoreEntry entry;
        entry.name = tbytes(tu32("dino name length"), "dino name");
        entry.price = tu32("dino price");
        entry.path = tbytes(tu32("dino path length"), "dino path");
        entry.description =
            tbytes(tu32("dino description length"), "dino description");
        out.dino_store.push_back(std::move(entry));
    }
    for (int i = 0; i < 5; ++i) {
        out.visitor_model[i] = static_cast<int>(tu32("visitor model"));
    }
    const std::uint32_t lab_count = tu32("lab count");
    for (std::uint32_t i = 0; i < lab_count; ++i) {
        SavLabRecord record;
        record.flag = data[o++];
        if (record.flag == 0) {
            for (auto& gene : record.genes) {
                gene.first = tu32("lab gene a");
                gene.second = tu32("lab gene b");
            }
            record.name = tbytes(tu32("lab name length"), "lab name");
        }
        out.labs.push_back(std::move(record));
    }
    for (int i = 0; i < 5; ++i) {
        out.ai_counts[i] = tu32("AI count");
    }
    std::string lower = path;
    for (char& c : lower) c = static_cast<char>(std::tolower(c));
    const bool auto_save = lower.find("auto") != std::string::npos;
    if (!auto_save) {
        for (std::uint32_t i = 0; i < out.ai_counts[4]; ++i) {
            std::array<std::uint8_t, 17> entry{};
            for (std::size_t b = 0; b < entry.size(); ++b) {
                entry[b] = data[o + b];
            }
            o += entry.size();
            out.ai_entries.push_back(entry);
        }
    }
    for (int i = 0; i < 10; ++i) {
        out.camera[i] = tf32("camera");
    }
    out.tail_extra.assign(data.begin() + o, data.end());
    return out;
}

namespace {

void put_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        out.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

void put_i32(std::vector<std::uint8_t>& out, int value) {
    put_u32(out, static_cast<std::uint32_t>(value));
}

void put_f32(std::vector<std::uint8_t>& out, float value) {
    union {
        float f;
        std::uint32_t u;
    } conv;
    conv.f = value;
    put_u32(out, conv.u);
}

void put_placed(std::vector<std::uint8_t>& out, const SavWorldPlaced& placed) {
    put_u32(out, placed.object_id);
    put_u32(out, placed.field_14_first);
    put_u32(out, placed.start_x);
    put_u32(out, placed.start_y);
    put_u32(out, placed.end_x);
    put_u32(out, placed.end_y);
    put_u32(out, static_cast<std::uint32_t>(placed.waypoints.size()));
    for (const SavWorldWaypoint& point : placed.waypoints) {
        put_u32(out, point.flags);
        put_u32(out, point.key);
        put_u32(out, point.x);
        put_u32(out, point.y);
        put_u32(out, point.z);
        put_u32(out, point.extra);
    }
}

}  // namespace

std::vector<std::uint8_t> write_sav_park_block(
    const SavSimulation& s) {
    std::vector<std::uint8_t> out;
    out.reserve(0x293);
    put_u32(out, s.park_class);
    put_u32(out, s.park_header_a);
    put_u32(out, s.park_header_b);
    put_f32(out, s.park_header_c);
    out.insert(out.end(), s.park_reserved.begin(), s.park_reserved.end());
    put_u32(out, static_cast<std::uint32_t>(s.budget));
    put_f32(out, s.economy.sellback);
    put_i32(out, s.economy.food_total);
    put_i32(out, s.economy.food_count);
    put_i32(out, s.economy.secu_total);
    put_i32(out, s.economy.secu_count);
    put_i32(out, s.economy.ticket_total);
    put_i32(out, s.economy.ticket_count);
    out.push_back(s.bankruptcy_flags[0]);
    out.push_back(s.bankruptcy_flags[1]);
    put_i32(out, s.economy.staff_buckets[0]);
    put_i32(out, s.economy.staff_buckets[1]);
    for (int value : s.economy.shop_revenue) put_i32(out, value);
    for (int value : s.economy.shop_sales) put_i32(out, value);
    put_i32(out, s.economy.dinoachat_count);
    put_i32(out, s.economy.dinoachat_total);
    put_i32(out, s.economy.construction_count);
    put_i32(out, s.economy.construction_total);
    put_i32(out, s.economy.laboaction_count);
    put_i32(out, s.economy.laboaction_total);
    put_i32(out, s.economy.ventes_count);
    put_i32(out, s.economy.ventes_total);
    for (int value : s.economy.yesterday_staff) put_i32(out, value);
    for (int value : s.economy.yesterday_food) put_i32(out, value);
    for (int value : s.economy.yesterday_secu) put_i32(out, value);
    for (int value : s.economy.yesterday_revenue) put_i32(out, value);
    for (int value : s.economy.yesterday_sales) put_i32(out, value);
    for (int value : s.economy.yesterday_totals) put_i32(out, value);
    for (int value : s.cumulative_staff) put_i32(out, value);
    for (int value : s.cumulative_food) put_i32(out, value);
    for (int value : s.cumulative_secu) put_i32(out, value);
    for (int value : s.cumulative_revenue) put_i32(out, value);
    for (int value : s.cumulative_sales) put_i32(out, value);
    for (int value : s.cumulative_totals) put_i32(out, value);
    put_u32(out, static_cast<std::uint32_t>(s.day));
    put_f32(out, s.elapsed);
    put_f32(out, s.schedule_current);
    put_u32(out, s.schedule_field);
    for (std::uint32_t task : s.schedule_tasks) put_u32(out, task);
    out.insert(out.end(), s.schedule_trailer.begin(), s.schedule_trailer.end());
    out.push_back(s.schedule_final);
    return out;
}

std::vector<std::uint8_t> write_sav_simulation(
    const SavSimulation& s) {
    std::vector<std::uint8_t> out;
    put_u32(out, static_cast<std::uint32_t>(s.roads.size()));
    for (const RoadRect& road : s.roads) {
        put_i32(out, road.x0);
        put_i32(out, road.y0);
        put_i32(out, road.x1);
        put_i32(out, road.y1);
    }
    for (int i = 0; i < 3; ++i) put_u32(out, 0);
    put_u32(out, static_cast<std::uint32_t>(s.visitors.size()));
    for (const SavVisitor& v : s.visitors) {
        put_u32(out, v.object_id);
        put_i32(out, v.x);
        put_i32(out, v.y);
        put_u32(out, v.flags);
        put_u32(out, v.unknown_10);
        put_u32(out, v.state);
        put_f32(out, v.fx);
        put_f32(out, v.fy);
        put_f32(out, v.dx);
        put_f32(out, v.dy);
        put_u32(out, v.unknown_28);
        put_u32(out, v.zero_2c);
        put_u32(out, v.visitor_id);
        for (std::uint32_t need : v.needs) put_u32(out, need);
    }
    put_u32(out, 1);
    const std::vector<std::uint8_t> park = write_sav_park_block(s);
    out.insert(out.end(), park.begin(), park.end());
    put_u32(out, static_cast<std::uint32_t>(s.dino_store.size()));
    for (const SavDinoStoreEntry& entry : s.dino_store) {
        put_u32(out, static_cast<std::uint32_t>(entry.name.size()));
        out.insert(out.end(), entry.name.begin(), entry.name.end());
        put_u32(out, entry.price);
        put_u32(out, static_cast<std::uint32_t>(entry.path.size()));
        out.insert(out.end(), entry.path.begin(), entry.path.end());
        put_u32(out, static_cast<std::uint32_t>(entry.description.size()));
        out.insert(out.end(), entry.description.begin(), entry.description.end());
    }
    for (int value : s.visitor_model) put_i32(out, value);
    put_u32(out, static_cast<std::uint32_t>(s.labs.size()));
    for (const SavLabRecord& record : s.labs) {
        out.push_back(record.flag);
        if (record.flag == 0) {
            for (const auto& gene : record.genes) {
                put_u32(out, gene.first);
                put_u32(out, gene.second);
            }
            put_u32(out, static_cast<std::uint32_t>(record.name.size()));
            out.insert(out.end(), record.name.begin(), record.name.end());
        }
    }
    for (std::uint32_t count : s.ai_counts) put_u32(out, count);
    for (const auto& entry : s.ai_entries) {
        out.insert(out.end(), entry.begin(), entry.end());
    }
    for (float value : s.camera) put_f32(out, value);
    out.insert(out.end(), s.tail_extra.begin(), s.tail_extra.end());
    return out;
}

std::vector<std::uint8_t> write_sav_world(const WldWorld& world) {
    const SavWorldRecords& w = world.records;
    const std::uint32_t version = world.version;
    std::vector<std::uint8_t> out;
    put_u32(out, w.campaign_id);
    put_u32(out, w.mission_id);
    if (version > 9) {
        put_u32(out, static_cast<std::uint32_t>(w.objectives.size()));
        for (const std::array<std::uint32_t, 3>& objective : w.objectives) {
            put_u32(out, objective[0]);
            put_u32(out, objective[1]);
            put_u32(out, objective[2]);
        }
    }
    put_u32(out, static_cast<std::uint32_t>(w.fences.size()));
    for (const SavWorldPlaced& fence : w.fences) {
        put_placed(out, fence);
        put_u32(out, fence.class_tag);
        put_u32(out, fence.field_14);
        out.insert(out.end(), fence.extra16.begin(), fence.extra16.end());
        put_u32(out, fence.field_44);
        put_u32(out, fence.field_48);
    }
    put_u32(out, static_cast<std::uint32_t>(w.shows.size()));
    for (const SavWorldPlaced& show : w.shows) {
        put_placed(out, show);
        put_u32(out, show.class_tag);
        put_u32(out, show.field_44);
        put_u32(out, show.field_48);
    }
    if (version > 1) {
        put_u32(out, static_cast<std::uint32_t>(w.animals.size()));
        for (const SavWorldAnimal& animal : w.animals) {
            put_u32(out, animal.object_id);
            put_u32(out, animal.field_1c);
            if (version > 0x32) {
                put_u32(out, animal.field_20);
            }
        }
    }
    if (version > 2) {
        put_u32(out, static_cast<std::uint32_t>(w.employees.size()));
        for (const SavWorldEmployee& employee : w.employees) {
            put_u32(out, employee.object_id);
            put_u32(out, employee.field_1c);
        }
    }
    put_u32(out, static_cast<std::uint32_t>(w.buildings.size()));
    for (const SavWorldBuilding& building : w.buildings) {
        put_u32(out, building.object_id);
        put_u32(out, building.linked_id);
        put_u32(out, building.class_tag);
        put_u32(out, building.kind);
        put_u32(out, building.cell_x);
        put_u32(out, building.cell_y);
        put_u32(out, building.state);
        put_u32(out, building.link);
    }
    if (!(version >= 6 && version <= 0x10)) {
        put_u32(out, static_cast<std::uint32_t>(w.visitors.size()));
        for (const SavWorldVisitor& visitor : w.visitors) {
            put_u32(out, visitor.object_id);
            put_u32(out, visitor.field_1c);
            put_u32(out, visitor.class_tag);
            put_u32(out, visitor.field_20);
            put_u32(out, visitor.field_24);
            if (version >= 5) {
                put_u32(out, visitor.field_28);
            }
        }
    }
    if (version > 1) {
        put_u32(out, static_cast<std::uint32_t>(w.dinosaurs.size()));
        for (const SavWorldDinosaur& dinosaur : w.dinosaurs) {
            put_u32(out, dinosaur.object_id);
            put_u32(out, dinosaur.field_1c);
            if (version >= 0x25) {
                put_u32(out, dinosaur.field_24);
                put_u32(out, dinosaur.field_28);
                put_u32(out, dinosaur.field_2c);
            }
            put_u32(out, dinosaur.class_tag);
            for (int g = 0; g < dinosaur.pairs; ++g) {
                put_u32(out, dinosaur.genotype[static_cast<std::size_t>(g)]
                                 .first);
                put_u32(out, dinosaur.genotype[static_cast<std::size_t>(g)]
                                 .second);
            }
        }
    }
    put_u32(out, static_cast<std::uint32_t>(w.feature_flags.size()));
    out.insert(out.end(), w.feature_flags.begin(), w.feature_flags.end());
    put_u32(out, static_cast<std::uint32_t>(w.building_flags.size()));
    out.insert(out.end(), w.building_flags.begin(), w.building_flags.end());
    put_u32(out, static_cast<std::uint32_t>(w.food_flags.size()));
    out.insert(out.end(), w.food_flags.begin(), w.food_flags.end());
    put_u32(out, static_cast<std::uint32_t>(w.misc_flags.size()));
    out.insert(out.end(), w.misc_flags.begin(), w.misc_flags.end());
    return out;
}

std::vector<std::uint8_t> write_sav_catalog(
    const SavSimulation& s) {
    std::vector<std::uint8_t> out;
    put_u32(out, static_cast<std::uint32_t>(s.catalog.size()));
    for (const SavShowEntry& entry : s.catalog) {
        put_u32(out, static_cast<std::uint32_t>(entry.path.size()));
        out.insert(out.end(), entry.path.begin(), entry.path.end());
        put_u32(out, static_cast<std::uint32_t>(entry.name.size()));
        out.insert(out.end(), entry.name.begin(), entry.name.end());
        for (std::uint32_t param : entry.params) put_u32(out, param);
    }
    return out;
}

std::vector<std::uint8_t> write_sav_whole(const SavSimulation& s,
                                          const WldWorld& world) {
    std::vector<std::uint8_t> out = s.header_raw;
    const std::vector<std::uint8_t> world_bytes = write_sav_world(world);
    out.insert(out.end(), world_bytes.begin(), world_bytes.end());
    const std::vector<std::uint8_t> catalog = write_sav_catalog(s);
    out.insert(out.end(), catalog.begin(), catalog.end());
    const std::vector<std::uint8_t> simulation = write_sav_simulation(s);
    out.insert(out.end(), simulation.begin(), simulation.end());
    return out;
}

std::vector<std::uint8_t> rebuild_sav(const std::string& path) {
    const WldWorld world = read_sav_world(path);
    const SavSimulation simulation = read_sav_simulation(path);
    return write_sav_whole(simulation, world);
}

bool sav_park_roundtrip(const std::string& path, std::string* error) {
    const std::vector<std::uint8_t> data = read_binary_file(path);
    const WldWorld world = read_sav_world(path);
    const SavSimulation simulation = read_sav_simulation(path);
    if (!simulation.has_park) {
        *error = ": no park block";
        return false;
    }
    const std::vector<std::uint8_t> rebuilt = write_sav_park_block(simulation);
    if (rebuilt.size() != 0x293) {
        *error = ": rebuilt park block has the wrong size";
        return false;
    }
    if (simulation.park_offset + rebuilt.size() > data.size()) {
        *error = ": park block past EOF";
        return false;
    }
    for (std::size_t i = 0; i < rebuilt.size(); ++i) {
        if (data[simulation.park_offset + i] != rebuilt[i]) {
            char message[64];
            std::snprintf(message, sizeof(message), ": park first diff at 0x%zX",
                          simulation.park_offset + i);
            *error = message;
            return false;
        }
    }
    const std::vector<std::uint8_t> whole = write_sav_whole(simulation, world);
    if (whole.size() != data.size()) {
        *error = ": whole-file length mismatch";
        return false;
    }
    for (std::size_t i = 0; i < whole.size(); ++i) {
        if (data[i] != whole[i]) {
            char message[64];
            std::snprintf(message, sizeof(message),
                          ": whole-file first diff at 0x%zX", i);
            *error = message;
            return false;
        }
    }
    return true;
}

}  // namespace dino
