#include "bai.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "bytes.h"
#include "mesh.h"

namespace dino {
namespace {

constexpr std::size_t kNameSize = 20;
constexpr std::size_t kHeaderSize = 0x56;

class Cursor {
public:
    Cursor(const std::vector<std::uint8_t>& data, const std::string& path)
        : data_(data), path_(path) {}

    void need(std::size_t size, const char* what) const {
        if (size > data_.size() - offset_) {
            throw std::runtime_error("bai: " + std::string(what) +
                                     " past EOF in " + path_);
        }
    }
    std::uint8_t u8(const char* what) {
        need(1, what);
        return data_[offset_++];
    }
    std::uint32_t u32(const char* what) {
        need(4, what);
        const std::uint32_t value = read_u32(data_, offset_, what);
        offset_ += 4;
        return value;
    }
    std::int32_t i32(const char* what) {
        need(4, what);
        const std::int32_t value = read_i32(data_, offset_, what);
        offset_ += 4;
        return value;
    }
    float f32(const char* what) {
        need(4, what);
        const float value = read_f32(data_, offset_, what);
        offset_ += 4;
        return value;
    }
    std::string text(std::size_t size, const char* what) {
        need(size, what);
        std::string value(reinterpret_cast<const char*>(data_.data() + offset_),
                          size);
        offset_ += size;
        const std::size_t nul = value.find('\0');
        if (nul != std::string::npos) {
            value.resize(nul);
        }
        return value;
    }
    bool done() const { return offset_ == data_.size(); }
    std::size_t offset() const { return offset_; }

private:
    const std::vector<std::uint8_t>& data_;
    const std::string& path_;
    std::size_t offset_ = 0;
};

BaiElement read_element(Cursor& c) {
    BaiElement element;
    element.time = c.u32("element time");
    const std::uint32_t bones = c.u32("element bone count");
    if (bones > 10000) {
        throw std::runtime_error("bai: implausible bone count");
    }
    element.bones.reserve(bones);
    for (std::uint32_t i = 0; i < bones; ++i) {
        element.bones.push_back(c.u32("element bone"));
    }
    element.duration = c.f32("element duration");
    const std::uint32_t texture = c.u32("element texture length");
    if (texture > 4096) {
        throw std::runtime_error("bai: implausible texture length");
    }
    element.texture = c.text(texture, "element texture");
    const std::uint32_t events = c.u32("element event count");
    if (events > 10000) {
        throw std::runtime_error("bai: implausible event count");
    }
    element.events.reserve(events);
    for (std::uint32_t i = 0; i < events; ++i) {
        BaiEvent event;
        event.kind = c.u32("event kind");
        event.value = c.i32("event value");
        event.percent = c.i32("event percent");
        element.events.push_back(event);
    }
    for (std::uint32_t& param : element.params) {
        param = c.u32("element param");
    }
    return element;
}

std::string join_path(const std::string& root, const std::string& relative) {
    std::string path = relative;
    std::replace(path.begin(), path.end(), '\\', '/');
    if (!path.empty() && (path[0] == '/' || (path.size() > 1 && path[1] == ':'))) {
        return path;
    }
    if (root.empty()) {
        return path;
    }
    return root + "/" + path;
}

}  // namespace

BaiAction load_bai(const std::string& path) {
    const std::vector<std::uint8_t> data = read_binary_file(path);
    if (data.size() < kHeaderSize) {
        throw std::runtime_error("bai: shorter than header: " + path);
    }
    BaiAction action;
    action.name = std::string(
        reinterpret_cast<const char*>(data.data()), kNameSize);
    const std::size_t nul = action.name.find('\0');
    if (nul != std::string::npos) {
        action.name.resize(nul);
    }
    if (action.name.empty()) {
        throw std::runtime_error("bai: empty action name in " + path);
    }
    action.field_14 = read_u32(data, 0x14, "field_14");
    action.field_18 = read_f32(data, 0x18, "field_18");
    action.field_1c = read_u32(data, 0x1c, "field_1c");
    action.flag_20 = data[0x20];
    action.special_action = read_u32(data, 0x21, "special_action");
    action.eat_flag = data[0x25];
    action.field_26 = read_u32(data, 0x26, "field_26");
    action.field_2a = read_f32(data, 0x2a, "field_2a");
    action.field_2e = read_f32(data, 0x2e, "field_2e");
    action.field_32 = read_u32(data, 0x32, "field_32");
    for (std::size_t i = 0; i < action.params.size(); ++i) {
        action.params[i] =
            read_u32(data, 0x36 + i * 4, "action param");
    }

    Cursor c(data, path);
    // Skip the fixed header.
    c.text(kHeaderSize, "header");
    action.consume_flag = c.u8("consume flag");

    const std::uint32_t pre = c.u32("precondition count");
    if (pre > 10000) {
        throw std::runtime_error("bai: implausible precondition count");
    }
    action.preconditions.reserve(pre);
    for (std::uint32_t i = 0; i < pre; ++i) {
        BaiPrecondition value;
        const std::uint32_t attribute = c.u32("precondition attribute");
        const std::uint32_t comparison = c.u32("precondition comparison");
        value.threshold = c.u32("precondition threshold");
        value.attribute = attribute;
        value.comparison = comparison;
        action.preconditions.push_back(value);
    }

    const std::uint32_t weights = c.u32("weight count");
    if (weights > 10000) {
        throw std::runtime_error("bai: implausible weight count");
    }
    action.effect_weights.reserve(weights);
    for (std::uint32_t i = 0; i < weights; ++i) {
        BaiWeight value;
        const std::uint32_t attribute = c.u32("weight attribute");
        value.unused = c.u32("weight unused");
        value.weight = c.u32("weight value");
        value.attribute = attribute;
        action.effect_weights.push_back(value);
    }

    const std::uint32_t needs = c.u32("need count");
    if (needs > 10000) {
        throw std::runtime_error("bai: implausible need count");
    }
    action.need_deltas.reserve(needs);
    for (std::uint32_t i = 0; i < needs; ++i) {
        BaiNeedDelta value;
        value.need = c.u32("need id");
        value.delta = c.i32("need delta");
        action.need_deltas.push_back(value);
    }

    const std::uint32_t grants = c.u32("grant count");
    if (grants > 10000) {
        throw std::runtime_error("bai: implausible grant count");
    }
    action.stat_grants.reserve(grants);
    for (std::uint32_t i = 0; i < grants; ++i) {
        BaiGrant value;
        value.attribute = c.u32("grant attribute");
        value.value = c.u32("grant value");
        action.stat_grants.push_back(value);
    }

    action.effect_class = c.u32("effect class");
    const std::uint32_t effects = c.u32("element count");
    if (effects > 10000) {
        throw std::runtime_error("bai: implausible element count");
    }
    action.elements.reserve(effects);
    for (std::uint32_t i = 0; i < effects; ++i) {
        action.elements.push_back(read_element(c));
    }
    if (!c.done()) {
        throw std::runtime_error("bai: " +
                                 std::to_string(data.size() - c.offset()) +
                                 " trailing bytes in " + path);
    }
    return action;
}

std::vector<std::string> load_action_list(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot open action list: " + path);
    }
    std::vector<std::string> names;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const std::size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line.compare(first, 2, "//") == 0) {
            continue;
        }
        line = line.substr(first);
        if (names.empty() &&
            line.find_first_not_of("0123456789") == std::string::npos) {
            continue;
        }
        names.push_back(line);
    }
    return names;
}

std::vector<BaiAction> load_action_table(const std::string& asset_root) {
    const std::string dir =
        join_path(asset_root, "Data/Design/attractions/actions");
    const std::vector<std::string> names =
        load_action_list(dir + "/ActionList.ini");
    std::vector<BaiAction> actions;
    actions.reserve(names.size());
    for (const std::string& name : names) {
        actions.push_back(load_bai(dir + "/" + name + ".bai"));
    }
    return actions;
}

}  // namespace dino
