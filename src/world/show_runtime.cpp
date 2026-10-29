#include "show_runtime.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>

#include "../formats/shw.h"
#include "../formats/path.h"

namespace dino {
namespace {

std::string lower(std::string value) {
    for (char& c : value) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return value;
}

}  // namespace

std::vector<ShowCatalogEntry> load_show_catalog(
    const std::string& asset_root, const CampaignMission& mission,
    const Loc& loc) {
    std::vector<std::string> stems;
    for (int i = 0; i < 10; ++i) {
        const auto it =
            mission.values.find("SHOWS.SHOWS" + std::to_string(i));
        if (it == mission.values.end()) {
            continue;
        }
        std::istringstream input(it->second);
        std::string stem;
        while (input >> stem) {
            if (std::find(stems.begin(), stems.end(), stem) == stems.end()) {
                stems.push_back(stem);
            }
        }
    }

    std::vector<ShowCatalogEntry> catalog;
    catalog.reserve(stems.size());
    for (const std::string& stem : stems) {
        try {
            ShowCatalogEntry entry;
            entry.stem = stem;
            entry.path = asset_path(
                asset_root,
                "Data/Design/Scenario/Show/" + stem + ".shw");
            if (!std::filesystem::is_regular_file(entry.path)) {
                continue;
            }
            const ShwShow show = load_shw(entry.path);
            entry.name_id = show.name;
            const std::string* title = loc.find(show.name);
            entry.title = title != nullptr ? *title : show.name;
            entry.cost = show.cost;
            entry.judge_kind = show.judge_kind;
            entry.width = show.width;
            entry.height = show.height;
            entry.wall_type = show.wall_type;
            entry.cage_size = show.cage_size;
            catalog.push_back(std::move(entry));
        } catch (const std::exception&) {
            // A mission may list a show whose `.shw` is absent from this
            // install; the rest of the catalog stays usable.
            continue;
        }
    }
    return catalog;
}

int find_show_catalog_index(const std::vector<ShowCatalogEntry>& catalog,
                            const std::string& name) {
    for (std::size_t i = 0; i < catalog.size(); ++i) {
        if (show_catalog_entry_matches(catalog[i], name)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool show_catalog_entry_matches(const ShowCatalogEntry& entry,
                                const std::string& name) {
    const std::string key = lower(name);
    if (lower(entry.stem) == key || lower(entry.name_id) == key) {
        return true;
    }
    std::string bare = lower(entry.name_id);
    if (!bare.empty() && bare.front() == '&') {
        bare.erase(bare.begin());
    }
    return bare == key;
}

int ShowRuntime::place(const ShowCatalogEntry& entry, int cell_x, int cell_y,
                       const std::vector<int>& profile_ids) {
    int slot = 0;
    for (int candidate = 1; candidate <= 10; ++candidate) {
        const bool used = std::any_of(
            shows_.begin(), shows_.end(),
            [candidate](const ParkShow& show) { return show.slot == candidate; });
        if (!used) {
            slot = candidate;
            break;
        }
    }
    if (slot == 0) {
        return -1;
    }
    ParkShow show;
    show.entry = entry;
    show.cell_x = cell_x;
    show.cell_y = cell_y;
    show.slot = slot;
    show.record.type = entry.judge_kind;
    show.record.zero_scores(profile_ids);
    shows_.push_back(std::move(show));
    return static_cast<int>(shows_.size()) - 1;
}

bool ShowRuntime::set_participants(int index, std::vector<int> participants) {
    if (index < 0 || index >= static_cast<int>(shows_.size())) {
        return false;
    }
    ParkShow& show = shows_[static_cast<std::size_t>(index)];
    show.participants = std::move(participants);
    show.record.participant_ids = show.participants;
    show.record.participants =
        static_cast<int>(show.record.participant_ids.size());
    return true;
}

bool ShowRuntime::set_slot(int index, int slot) {
    if (index < 0 || index >= static_cast<int>(shows_.size()) || slot < 0 ||
        slot > 10) {
        return false;
    }
    shows_[static_cast<std::size_t>(index)].slot = slot;
    return true;
}

bool ShowRuntime::set_iterations(int index, int iterations) {
    if (index < 0 || index >= static_cast<int>(shows_.size()) ||
        iterations < 1) {
        return false;
    }
    shows_[static_cast<std::size_t>(index)].iterations = iterations;
    return true;
}

bool ShowRuntime::set_locked(int index, bool locked) {
    if (index < 0 || index >= static_cast<int>(shows_.size())) {
        return false;
    }
    shows_[static_cast<std::size_t>(index)].locked = locked;
    return true;
}

bool ShowRuntime::set_enabled(int index, bool enabled) {
    if (index < 0 || index >= static_cast<int>(shows_.size())) {
        return false;
    }
    shows_[static_cast<std::size_t>(index)].enabled = enabled;
    return true;
}

bool ShowRuntime::set_valid(int index, bool valid) {
    if (index < 0 || index >= static_cast<int>(shows_.size())) {
        return false;
    }
    shows_[static_cast<std::size_t>(index)].valid = valid;
    return true;
}

void ShowRuntime::clear() {
    shows_.clear();
    last_day_ = -1;
    last_hour_ = -1;
}

ParkShow* ShowRuntime::at(int index) {
    if (index < 0 || index >= static_cast<int>(shows_.size())) {
        return nullptr;
    }
    return &shows_[static_cast<std::size_t>(index)];
}

const ParkShow* ShowRuntime::at(int index) const {
    if (index < 0 || index >= static_cast<int>(shows_.size())) {
        return nullptr;
    }
    return &shows_[static_cast<std::size_t>(index)];
}

std::vector<CampaignShowState> ShowRuntime::campaign_states() const {
    std::vector<CampaignShowState> states;
    states.reserve(shows_.size());
    for (const ParkShow& show : shows_) {
        states.push_back(show.record);
    }
    return states;
}

void ShowRuntime::reset_day() {
    for (ParkShow& show : shows_) {
        show.done = 0;
        show.elapsed = 0.0f;
        show.state = 0;
    }
}

void ShowRuntime::start_hour(int hour_index) {
    for (ParkShow& show : shows_) {
        if (show.slot != hour_index || !show.enabled || !show.valid) {
            continue;
        }
        if (show.state != 0 && show.state != 1) {
            continue;
        }
        show.done = 0;
        show.elapsed = 0.0f;
        show.state = 2;
    }
}

}  // namespace dino
