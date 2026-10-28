// Shows and attractions: scoring, participants, placement and the
// attraction-list actions.

#include "game.h"

namespace dino::app {

void Game::sync_show_identity(dino::CampaignShowState& show,
                                  const std::vector<int>& participants) {
    show.participant_ids.clear();
    show.participant_hybrid.clear();
    for (int index : participants) {
        if (index < 0 ||
            index >= static_cast<int>(imported_dinos.size())) {
            continue;
        }
        show.participant_ids.push_back(index);
        show.participant_hybrid.push_back(
            imported_dinos[static_cast<std::size_t>(index)]
                .attributes.originality);
    }
    show.participants =
        static_cast<int>(show.participant_ids.size());

}

void Game::fill_show_scores(dino::CampaignShowState& show,
                                const std::vector<int>& participants) {
    sync_show_identity(show, participants);
    if (all_visitor_profiles.empty()) {
        try {
            all_visitor_profiles =
                load_all_visitor_profiles(opt.asset_root);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "all visitor profiles: %s\n", e.what());
        }
    }
    const std::vector<dino::VisitorProfile>& show_profiles =
        all_visitor_profiles.empty() ? visitor_profiles
                                     : all_visitor_profiles;
    for (const dino::VisitorProfile& profile : show_profiles) {
        float best = -1.0e9f;
        for (int index : show.participant_ids) {
            const auto needs = dino::dino_show_needs(
                imported_dinos[static_cast<std::size_t>(index)]
                    .attributes,
                phenotype_table);
            float dot = 0.0f;
            for (std::size_t i = 0; i < needs.size(); ++i) {
                dot += needs[i] * profile.weights[i];
            }
            best = std::max(best, std::clamp(dot, -100.0f, 100.0f));
        }
        if (best > -1.0e8f) {
            show.set_score(profile.personality_id,
                           std::clamp(best, -130.0f, 130.0f));
        }
    }

}

std::vector<int> Game::show_profile_ids() {
    if (all_visitor_profiles.empty()) {
        try {
            all_visitor_profiles =
                load_all_visitor_profiles(opt.asset_root);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "all visitor profiles: %s\n", e.what());
        }
    }
    const std::vector<dino::VisitorProfile>& profiles =
        all_visitor_profiles.empty() ? visitor_profiles
                                     : all_visitor_profiles;
    std::vector<int> ids;
    ids.reserve(profiles.size());
    for (const dino::VisitorProfile& profile : profiles) {
        ids.push_back(profile.personality_id);
    }
    return ids;

}

void Game::apply_show_specs() {
    if (show_placements_applied) {
        return;
    }
    show_placements_applied = true;
    if (show_catalog.empty() && !opt.place_show_specs.empty()) {
        std::printf("show: mission catalog is empty\n");
        return;
    }
    const std::vector<int> profile_ids = show_profile_ids();
    for (const std::string& raw : opt.place_show_specs) {
        const PortNameSpec spec = parse_name_spec(raw);
        const int catalog_index =
            dino::find_show_catalog_index(show_catalog, spec.name);
        if (catalog_index < 0) {
            std::printf("show: %s not in the mission catalog\n",
                        spec.name.c_str());
            continue;
        }
        int x = 64 + 8 * show_runtime.count();
        int y = 64;
        if (spec.values.size() >= 2) {
            x = spec.values[0];
            y = spec.values[1];
        }
        const int index = show_runtime.place(
            show_catalog[static_cast<std::size_t>(catalog_index)], x, y,
            profile_ids);
        if (index < 0) {
            std::printf("show: no free hour slot for %s\n",
                        spec.name.c_str());
            continue;
        }
        if (opt.show_locked) {
            show_runtime.set_locked(index, true);
        }
        const dino::ParkShow* show = show_runtime.at(index);
        std::printf(
            "show: placed %s at %d,%d slot=%d (%s) size=%dx%d\n",
            show->entry.stem.c_str(), show->cell_x, show->cell_y,
            show->slot, show_slot_text(show->slot).c_str(),
            show->entry.width, show->entry.height);
    }
    for (const std::string& raw : opt.show_participant_specs) {
        const PortNameSpec spec = parse_name_spec(raw);
        bool matched = false;
        for (int i = 0; i < show_runtime.count(); ++i) {
            dino::ParkShow* show = show_runtime.at(i);
            if (!dino::show_catalog_entry_matches(show->entry, spec.name)) {
                continue;
            }
            matched = true;
            show_runtime.set_participants(i, spec.values);
            sync_show_identity(show->record, spec.values);
            std::printf("show: %s participants=%zu\n",
                        show->entry.stem.c_str(),
                        show->participants.size());
        }
        if (!matched) {
            std::printf("show: %s is not placed\n", spec.name.c_str());
        }
    }
    for (const std::string& raw : opt.show_hour_specs) {
        const PortNameSpec spec = parse_name_spec(raw);
        if (spec.values.empty()) {
            continue;
        }
        for (int i = 0; i < show_runtime.count(); ++i) {
            dino::ParkShow* show = show_runtime.at(i);
            if (!dino::show_catalog_entry_matches(show->entry, spec.name)) {
                continue;
            }
            if (show_runtime.set_slot(i, spec.values[0])) {
                std::printf("show: %s slot=%d (%s)\n",
                            show->entry.stem.c_str(), show->slot,
                            show_slot_text(show->slot).c_str());
            }
        }
    }
    for (const std::string& raw : opt.show_iteration_specs) {
        const PortNameSpec spec = parse_name_spec(raw);
        if (spec.values.empty()) {
            continue;
        }
        for (int i = 0; i < show_runtime.count(); ++i) {
            dino::ParkShow* show = show_runtime.at(i);
            if (!dino::show_catalog_entry_matches(show->entry, spec.name)) {
                continue;
            }
            if (show_runtime.set_iterations(i, spec.values[0])) {
                std::printf("show: %s iterations=%d\n",
                            show->entry.stem.c_str(), show->iterations);
            }
        }
    }
    for (int i = 0; i < show_runtime.count(); ++i) {
        const dino::ParkShow* show = show_runtime.at(i);
        std::printf(
            "show schedule: %s slot=%d (%s) iterations=%d "
            "participants=%zu locked=%d\n",
            show->entry.stem.c_str(), show->slot,
            show_slot_text(show->slot).c_str(), show->iterations,
            show->participants.size(), show->locked ? 1 : 0);
    }

}

void Game::auto_assign_show_participants(int show_index) {
    dino::ParkShow* show = show_runtime.at(show_index);
    if (show == nullptr) {
        return;
    }
    std::array<int, 4> remaining = show->entry.cage_size;
    std::vector<int> participants;
    for (int i = 0; i < static_cast<int>(imported_dinos.size()); ++i) {
        const int length = imported_dinos[static_cast<std::size_t>(i)]
                               .needs.size_cm;
        int size_class = 3;
        for (int category = 0; category < 4; ++category) {
            if (length <=
                phenotype_table.size_category_max[static_cast<std::size_t>(
                    category)]) {
                size_class = category;
                break;
            }
        }
        if (remaining[static_cast<std::size_t>(size_class)] <= 0) {
            continue;
        }
        --remaining[static_cast<std::size_t>(size_class)];
        participants.push_back(i);
    }
    if (participants.empty()) {
        return;
    }
    show_runtime.set_participants(show_index, participants);
    sync_show_identity(show->record, participants);
    std::printf("show: %s participants=%zu (auto-assigned by cage size)\n",
                show->entry.stem.c_str(), show->participants.size());

}

void Game::handle_show_place(int catalog_index) {
    if (catalog_index < 0 ||
        catalog_index >= static_cast<int>(show_catalog.size())) {
        std::printf("show: no show selected\n");
        return;
    }
    const dino::ShowCatalogEntry& entry =
        show_catalog[static_cast<std::size_t>(catalog_index)];
    if (park_budget < entry.cost) {
        std::printf(
            "show: %s needs %d, budget %d (WARNING_NOT_ENOUGH_MONEY)\n",
            entry.stem.c_str(), entry.cost, park_budget);
        park.show_message(dino::ui::resolve_loc_text(
            "&WARNING_NOT_ENOUGH_MONEY", park.loc));
        return;
    }
    ensure_build_ready();
    pending_show_index = catalog_index;
    build_tool = 5;
    park.overlay.set_visible("PREDEFINED_SHOW_PANEL", false);
    std::printf(
        "show: %s selected, cost=%d size=%dx%d (click the ground)\n",
        entry.stem.c_str(), entry.cost, entry.width, entry.height);

}

void Game::place_show_at(int rx, int ry) {
    if (pending_show_index < 0 ||
        pending_show_index >= static_cast<int>(show_catalog.size())) {
        return;
    }
    const dino::ShowCatalogEntry& entry =
        show_catalog[static_cast<std::size_t>(pending_show_index)];
    const int w = std::max(1, entry.width);
    const int h = std::max(1, entry.height);
    const float cs = build_grid.cell_size;
    // Record coordinate -> build cell: `cell = floor(record / cell_size)`
    // (docs/platform/world-import.md; the grid origin is -world_size/2).
    const int gx0 =
        static_cast<int>(std::floor(static_cast<float>(rx) / cs));
    const int gy0 =
        static_cast<int>(std::floor(static_cast<float>(ry) / cs));
    const int gx1 = static_cast<int>(
        std::floor(static_cast<float>(rx + w - 1) / cs));
    const int gy1 = static_cast<int>(
        std::floor(static_cast<float>(ry + h - 1) / cs));
    auto cell_buildable = [&](int x, int y) {
        return terrain.buildable(
            build_grid.origin_x + (static_cast<float>(x) + 0.5f) * cs,
            build_grid.origin_y + (static_cast<float>(y) + 0.5f) * cs);
    };
    bool blocked = false;
    for (int y = gy0; y <= gy1 && !blocked; ++y) {
        for (int x = gx0; x <= gx1 && !blocked; ++x) {
            if (x < 0 || y < 0 || x >= build_grid.width ||
                y >= build_grid.height || !cell_buildable(x, y)) {
                blocked = true;
            }
        }
    }
    if (blocked) {
        std::printf("show: %s footprint blocked\n", entry.stem.c_str());
        return;
    }
    if (park_budget < entry.cost) {
        std::printf(
            "show: %s needs %d, budget %d (WARNING_NOT_ENOUGH_MONEY)\n",
            entry.stem.c_str(), entry.cost, park_budget);
        return;
    }
    const std::vector<int> profile_ids = show_profile_ids();
    const int index = show_runtime.place(entry, rx, ry, profile_ids);
    if (index < 0) {
        std::printf("show: no free hour slot for %s\n",
                    entry.stem.c_str());
        return;
    }
    park_budget -= entry.cost;
    budget_ledger.spend_construction(entry.cost);
    auto_assign_show_participants(index);
    const dino::ParkShow* show = show_runtime.at(index);
    std::printf(
        "show: placed %s at %d,%d slot=%d (%s) size=%dx%d budget=%d\n",
        show->entry.stem.c_str(), show->cell_x, show->cell_y, show->slot,
        show_slot_text(show->slot).c_str(), show->entry.width,
        show->entry.height, park_budget);
    build_tool = 0;
    pending_show_index = -1;

}

int Game::find_show_by_slot(int slot) {
    for (int i = 0; i < show_runtime.count(); ++i) {
        const dino::ParkShow* show = show_runtime.at(i);
        if (show != nullptr && show->slot == slot) {
            return i;
        }
    }
    return -1;

}

void Game::handle_attraction_action(const std::string& action) {
    const std::size_t last_space = action.find_last_of(' ');
    if (last_space == std::string::npos) {
        return;
    }
    const int slot = std::atoi(action.c_str() + last_space + 1);
    const std::string verb =
        action.substr(std::strlen("attraction "),
                      last_space - std::strlen("attraction "));
    const int index = find_show_by_slot(slot);
    if (verb == "select" || verb == "selection_light") {
        if (index >= 0) {
            const dino::ParkShow* show = show_runtime.at(index);
            attraction_selected_slot = slot;
            std::printf("show: selected %s slot=%d\n",
                        show->entry.stem.c_str(), slot);
        }
        return;
    }
    if (index < 0) {
        std::printf("show: no show in slot %d\n", slot);
        return;
    }
    dino::ParkShow* show = show_runtime.at(index);
    if (verb == "lock") {
        show_runtime.set_locked(index, true);
        show_runtime.set_enabled(index, false);
        std::printf("show: %s slot=%d closed to the public\n",
                    show->entry.stem.c_str(), slot);
    } else if (verb == "unlock") {
        show_runtime.set_locked(index, false);
        show_runtime.set_enabled(index, true);
        std::printf("show: %s slot=%d open to the public\n",
                    show->entry.stem.c_str(), slot);
    } else if (verb == "iteration_up" || verb == "iteration_down") {
        const int delta = verb == "iteration_up" ? 1 : -1;
        const int value = std::clamp(show->iterations + delta, 1, 10);
        if (value != show->iterations) {
            show_runtime.set_iterations(index, value);
            std::printf("show: %s iterations=%d\n",
                        show->entry.stem.c_str(), value);
        }
    } else if (verb == "permutation") {
        const int other_slot = slot % 10 + 1;
        const int other = find_show_by_slot(other_slot);
        show_runtime.set_slot(index, other_slot);
        if (other >= 0) {
            show_runtime.set_slot(other, slot);
        }
        std::printf("show: %s slot=%d (%s) permuted with slot %d\n",
                    show->entry.stem.c_str(), other_slot,
                    show_slot_text(other_slot).c_str(), slot);
    }

}

bool Game::ends_with(const std::string& text, const std::string& suffix) {
    return text.size() >= suffix.size() &&
           text.compare(text.size() - suffix.size(), suffix.size(),
                        suffix) == 0;

}

}  // namespace dino::app
