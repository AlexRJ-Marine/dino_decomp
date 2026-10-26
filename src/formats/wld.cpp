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
    return out;
}

SavSimulation read_sav_simulation(const std::string& path) {
    const std::vector<std::uint8_t> data = read_binary_file(path);
    const WldWorld world = read_sav_world(path);
    Reader r(data, world.world_end, data.size(), world.version, path);
    SavSimulation out;

    auto skip_prefixed = [&](const char* what) {
        const std::uint32_t n = r.count(what, 1 << 20);
        r.skip(n, what);
    };

    // Show catalog: ASCII path, UTF-16 name, six parameters.
    const std::uint32_t shows = r.count("show catalog count", 10000);
    for (std::uint32_t i = 0; i < shows; ++i) {
        skip_prefixed("show path");
        skip_prefixed("show name");
        r.skip(24, "show parameters");
    }

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
        visitor.state = read_u32(data, rec + 0x14, "visitor state");
        visitor.dx = read_f32(data, rec + 0x20, "visitor dx");
        visitor.dy = read_f32(data, rec + 0x24, "visitor dy");
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
    return out;
}

}  // namespace dino
