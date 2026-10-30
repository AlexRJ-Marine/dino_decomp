#include "asset_paths.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <utility>

#include "../formats/image.h"
#include "../formats/path.h"
#include "../formats/tga.h"
#include "../formats/shw.h"
#include "../world/vegetation.h"
#include "cli.h"

namespace dino::app {

std::string resolve_asset(const std::string& root, const std::string& path) {
    return dino::asset_path(root, path);
}

void load_asset_tga(const std::string& root, const char* relative,
                    dino::Image& image, const char* what) {
    try {
        image = dino::load_tga(dino::asset_path(root, relative));
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s: %s\n", what, e.what());
    }
}

std::vector<PortShowSpec> parse_show_specs(
    const std::vector<std::string>& specs, const std::string& asset_root) {
    std::vector<PortShowSpec> shows;
    for (const std::string& spec : specs) {
        const std::size_t colon = spec.find(':');
        PortShowSpec show;
        show.name = spec.substr(0, colon);
        if (colon != std::string::npos) {
            const char* cursor = spec.c_str() + colon + 1;
            while (*cursor != '\0') {
                char* next = nullptr;
                const long value = std::strtol(cursor, &next, 10);
                if (next == cursor) {
                    ++cursor;
                    continue;
                }
                show.participants.push_back(static_cast<int>(value));
                cursor = next;
            }
        }
        std::string path = show.name;
        if (!std::filesystem::is_regular_file(path)) {
            path = resolve_asset(asset_root,
                                 "Data/Design/Scenario/Show/" + show.name +
                                     ".shw");
        }
        try {
            show.type = dino::load_shw(path).judge_kind;
        } catch (const std::exception& e) {
            std::fprintf(stderr, "show %s: %s\n", show.name.c_str(), e.what());
        }
        shows.push_back(std::move(show));
    }
    return shows;
}

PortNameSpec parse_name_spec(const std::string& spec) {
    PortNameSpec out;
    const std::size_t colon = spec.find(':');
    out.name = spec.substr(0, colon);
    if (colon == std::string::npos) {
        return out;
    }
    const char* cursor = spec.c_str() + colon + 1;
    while (*cursor != '\0') {
        char* next = nullptr;
        const long value = std::strtol(cursor, &next, 10);
        if (next == cursor) {
            ++cursor;
            continue;
        }
        out.values.push_back(static_cast<int>(value));
        cursor = next;
    }
    return out;
}

std::string show_slot_text(int slot) {
    if (slot < 1 || slot > 10) {
        return "--:--";
    }
    const int hour = 9 + slot;
    int display = hour % 12;
    if (display == 0) {
        display = 12;
    }
    char text[16];
    std::snprintf(text, sizeof(text), "%02d:00 %s", display,
                  hour < 12 ? "AM" : "PM");
    return text;
}

std::vector<dino::VisitorProfile> load_easy_visitor_profiles(
    const std::string& asset_root) {
    static const char* kFiles[6] = {
        "OldMan.bvi",     "SurferMan.bvi", "ParentMan.bvi",
        "KidMan.bvi",     "HooliganMan.bvi", "ScientistMan.bvi"};
    std::vector<dino::VisitorProfile> profiles;
    profiles.reserve(6);
    for (const char* file : kFiles) {
        dino::VisitorProfile profile = dino::load_bvi(resolve_asset(
            asset_root, std::string("Data/Design/visitors/") + file));
        profile.spawn_weight = 30;
        profile.spawn_cap = 100;
        profiles.push_back(std::move(profile));
    }
    return profiles;
}

std::vector<dino::VisitorProfile> load_all_visitor_profiles(
    const std::string& asset_root) {
    static const char* kFiles[10] = {
        "SurferMan.bvi",   "HooliganMan.bvi", "ParentMan.bvi",
        "BlaseMan.bvi",    "OldMan.bvi",      "RicheMan.bvi",
        "ScientistMan.bvi", "FanMan.bvi",     "SpdMan.bvi",
        "KidMan.bvi"};
    std::vector<dino::VisitorProfile> profiles;
    profiles.reserve(10);
    for (const char* file : kFiles) {
        profiles.push_back(dino::load_bvi(resolve_asset(
            asset_root, std::string("Data/Design/visitors/") + file)));
    }
    return profiles;
}

std::vector<std::string> read_island_list(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot open island list: " + path);
    }
    std::vector<std::string> names;
    int count = -1;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }
        if (count < 0) {
            count = std::atoi(line.c_str());
            continue;
        }
        names.push_back(line);
        if (static_cast<int>(names.size()) == count) {
            break;
        }
    }
    if (count <= 0 || static_cast<int>(names.size()) != count) {
        throw std::runtime_error("bad island list: " + path);
    }
    return names;
}

}  // namespace dino::app
