#include "reports.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../formats/path.h"

#include "../formats/bai.h"
#include "../formats/actions_bad.h"
#include "../formats/bbi.h"
#include "../formats/bei.h"
#include "../formats/bfi.h"
#include "../formats/bsi.h"
#include "../formats/bvi.h"
#include "../formats/campaign.h"
#include "../formats/dino_data.h"
#include "../formats/general_params.h"
#include "../formats/image.h"
#include "../formats/imp.h"
#include "../formats/lnd.h"
#include "../formats/loc.h"
#include "../formats/market.h"
#include "../formats/mesh.h"
#include "../formats/mpg.h"
#include "../formats/sav.h"
#include "../formats/shw.h"
#include "../formats/wld.h"
#include "../selftest.h"
#include "../world/danger_grid.h"
#include "../world/dino_actions.h"
#include "../world/port_save.h"
#include "../world/vegetation.h"
#include "asset_paths.h"
#include "capture.h"

namespace dino::app {

namespace {

std::string join_args(const std::vector<std::string>& args) {
    std::string text;
    for (const std::string& arg : args) {
        if (!text.empty()) {
            text += " ";
        }
        text += arg;
    }
    return text.empty() ? "-" : text;
}

int run_campaign_info(const std::string& scenario_root) {
    try {
        const std::vector<dino::Campaign> campaigns =
            dino::load_campaigns(scenario_root);
        int missions = 0;
        int objectives = 0;
        int unknown = 0;
        for (const dino::Campaign& campaign : campaigns) {
            missions += static_cast<int>(campaign.missions.size());
            for (const dino::CampaignMission& mission : campaign.missions) {
                objectives += static_cast<int>(mission.objectives.size());
                for (const dino::CampaignObjective& objective :
                     mission.objectives) {
                    if (objective.objective_id < 0) {
                        ++unknown;
                    }
                }
            }
        }
        std::printf(
            "campaigns: %zu missions: %d objectives: %d unknown: %d verbs: %d\n",
            campaigns.size(), missions, objectives, unknown,
            dino::objective_verb_count());
        for (const dino::Campaign& campaign : campaigns) {
            std::printf(
                "campaign %s island=%s budget=%d sellback=%.2f missions=%zu "
                "world=%s title=%s\n",
                campaign.name.c_str(), campaign.island.c_str(),
                campaign.budget, campaign.sell_back, campaign.missions.size(),
                campaign.world_file.c_str(), campaign.title_id.c_str());
            for (const dino::CampaignMission& mission : campaign.missions) {
                std::printf("  mission %s objectives=%zu title=%s\n",
                            mission.name.c_str(), mission.objectives.size(),
                            mission.title_id.c_str());
                for (const dino::CampaignObjective& objective :
                     mission.objectives) {
                    std::printf("    objective id=%d verb=%s args=%s desc=%s\n",
                                objective.objective_id,
                                objective.verb.c_str(),
                                join_args(objective.args).c_str(),
                                objective.description_id.c_str());
                }
            }
        }
        return unknown == 0 ? 0 : 1;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "campaign info: %s\n", e.what());
        return 1;
    }
}

int run_campaign_coverage(const std::string& scenario_root) {
    try {
        const std::vector<dino::Campaign> campaigns =
            dino::load_campaigns(scenario_root);
        std::map<int, int> used;
        int total = 0;
        int implemented = 0;
        int pending = 0;
        int unknown = 0;
        for (const dino::Campaign& campaign : campaigns) {
            for (const dino::CampaignMission& mission : campaign.missions) {
                for (const dino::CampaignObjective& objective :
                     mission.objectives) {
                    ++total;
                    if (objective.objective_id < 0) {
                        ++unknown;
                        continue;
                    }
                    ++used[objective.objective_id];
                    if (dino::objective_verb_implemented(
                            objective.objective_id)) {
                        ++implemented;
                    } else {
                        ++pending;
                    }
                }
            }
        }
        std::printf(
            "campaign coverage: %d objectives, %d implemented, %d pending, "
            "%d unknown\n",
            total, implemented, pending, unknown);
        for (int id = 0; id < dino::objective_verb_count(); ++id) {
            const auto it = used.find(id);
            const int count = it == used.end() ? 0 : it->second;
            const bool impl = dino::objective_verb_implemented(id);
            const char* status =
                impl ? "implemented" : (count > 0 ? "pending" : "unused");
            std::printf("verb id=%d %s objectives=%d %s\n", id,
                        dino::objective_verb_name(id), count, status);
        }
        return unknown == 0 ? 0 : 1;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "campaign coverage: %s\n", e.what());
        return 1;
    }
}

int run_campaign_mission_info(const std::string& mission_path) {
    try {
        const dino::CampaignMission mission =
            dino::load_campaign_mission(mission_path);
        std::printf("mission %s objectives=%zu title=%s\n",
                    mission.name.c_str(), mission.objectives.size(),
                    mission.title_id.c_str());
        for (const dino::CampaignObjective& objective : mission.objectives) {
            std::printf("objective id=%d verb=%s args=%s desc=%s\n",
                        objective.objective_id, objective.verb.c_str(),
                        join_args(objective.args).c_str(),
                        objective.description_id.c_str());
        }
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "campaign mission: %s\n", e.what());
        return 1;
    }
}

}  // namespace

std::optional<int> run_cli_reports(const Options& opt) {
    if (opt.selftest) {
        return run_selftest();
    }
    if (!opt.video_info_path.empty()) {
        try {
            const dino::MpgInfo info = dino::probe_mpg(opt.video_info_path);
            std::printf(
                "video: %s width=%d height=%d duration=%.3f fps=%.3f "
                "audio=%dHz streams=%d\n",
                opt.video_info_path.c_str(), info.width, info.height,
                info.duration, info.frame_rate, info.audio_samplerate,
                info.audio_streams);
            return 0;
        } catch (const std::exception& e) {
            std::fprintf(stderr, "video info: %s\n", e.what());
            return 1;
        }
    }
    if (!opt.video_frame_path.empty()) {
        try {
            const dino::MpgFrame frame = dino::decode_mpg_frame(
                opt.video_frame_path, opt.video_frame_index);
            if (opt.video_out_path.empty()) {
                std::fprintf(stderr, "video frame: --video-out is required\n");
                return 2;
            }
            std::vector<std::uint8_t> rgba(
                static_cast<std::size_t>(frame.width) * frame.height * 4, 255);
            for (int y = 0; y < frame.height; ++y) {
                const std::uint8_t* src =
                    frame.rgb.data() +
                    static_cast<std::size_t>(frame.height - 1 - y) *
                        frame.width * 3;
                std::uint8_t* dst =
                    rgba.data() +
                    static_cast<std::size_t>(y) * frame.width * 4;
                for (int x = 0; x < frame.width; ++x) {
                    dst[x * 4 + 0] = src[x * 3 + 0];
                    dst[x * 4 + 1] = src[x * 3 + 1];
                    dst[x * 4 + 2] = src[x * 3 + 2];
                }
            }
            write_bmp(opt.video_out_path, frame.width, frame.height, rgba);
            std::printf("video frame: %s index=%d time=%.3f %dx%d\n",
                        opt.video_frame_path.c_str(), frame.index, frame.time,
                        frame.width, frame.height);
            return 0;
        } catch (const std::exception& e) {
            std::fprintf(stderr, "video frame: %s\n", e.what());
            return 1;
        }
    }
    if (!opt.campaign_coverage_dir.empty()) {
        return run_campaign_coverage(opt.campaign_coverage_dir);
    }
    if (!opt.campaign_info_dir.empty()) {
        return run_campaign_info(opt.campaign_info_dir);
    }
    if (!opt.campaign_mission_path.empty()) {
        return run_campaign_mission_info(opt.campaign_mission_path);
    }

    if (!opt.sav_info_path.empty()) {
        try {
            const dino::WldWorld world =
                dino::read_sav_world(opt.sav_info_path);
            const dino::SavSimulation sim =
                dino::read_sav_simulation(opt.sav_info_path);
            std::string name;
            for (std::size_t i = 0; i + 1 < sim.name.size(); i += 2) {
                const unsigned char lo = sim.name[i];
                const unsigned char hi = sim.name[i + 1];
                if (lo == 0 && hi == 0) {
                    break;
                }
                name.push_back(hi == 0 && lo >= 0x20
                                   ? static_cast<char>(lo)
                                   : '?');
            }
            std::printf("sav: version=%u slot=%u name='%s'\n", world.version,
                        sim.slot, name.c_str());
            const dino::SavWorldRecords& records = world.records;
            std::printf("sav world: campaign=%u mission=%u objectives=%zu\n",
                        records.campaign_id, records.mission_id,
                        records.objectives.size());
            std::printf(
                "sav objects: fences=%zu shows=%zu animals=%zu employees=%zu "
                "buildings=%zu visitors=%zu dinosaurs=%zu roads=%zu\n",
                records.fences.size(), records.shows.size(),
                records.animals.size(), records.employees.size(),
                records.buildings.size(), records.visitors.size(),
                records.dinosaurs.size(), sim.roads.size());
            for (const auto& b : records.buildings) {
                std::printf("sav building: kind=%u cell=%u,%u state=%u\n",
                            b.kind, b.cell_x, b.cell_y, b.state);
            }
            for (const auto& r : sim.roads) {
                std::printf("sav road: %d,%d-%d,%d\n", r.x0, r.y0, r.x1, r.y1);
            }
            std::printf(
                "sav flags: feature=%zu building=%zu food=%zu misc=%zu\n",
                records.feature_flags.size(), records.building_flags.size(),
                records.food_flags.size(), records.misc_flags.size());
            std::printf("sav catalog: %zu\n", sim.catalog.size());
            std::printf("sav park: budget=%d day=%d elapsed=%.3f\n",
                        sim.budget, sim.day, sim.elapsed);
            std::printf(
                "sav sim: visitors=%zu stores=%zu labs=%zu "
                "model=%d,%d,%d,%d,%d ai=%u,%u,%u,%u,%u "
                "camera=%.2f,%.2f,%.2f,%.2f\n",
                sim.visitors.size(), sim.dino_store.size(), sim.labs.size(),
                sim.visitor_model[0], sim.visitor_model[1],
                sim.visitor_model[2], sim.visitor_model[3],
                sim.visitor_model[4], sim.ai_counts[0], sim.ai_counts[1],
                sim.ai_counts[2], sim.ai_counts[3], sim.ai_counts[4],
                sim.camera[0], sim.camera[1], sim.camera[2], sim.camera[3]);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "sav info failed: %s\n", e.what());
            return 2;
        }
        return 0;
    }

    if (!opt.sav_roundtrip_path.empty()) {
        std::string error;
        bool ok = false;
        try {
            ok = dino::sav_park_roundtrip(opt.sav_roundtrip_path, &error);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "sav read failed: %s\n", e.what());
            return 2;
        }
        std::printf("sav roundtrip: %s%s\n",
                    ok ? "whole file identical" : "sav MISMATCH",
                    error.c_str());
        if (!opt.sav_write_path.empty()) {
            if (!ok && !opt.sav_force) {
                std::fprintf(stderr,
                             "sav write refused: rebuild differs (--sav-force "
                             "to write anyway)\n");
                return 1;
            }
            try {
                const std::string* new_name =
                    opt.sav_name.empty() ? nullptr : &opt.sav_name;
                const std::vector<std::uint8_t> rebuilt =
                    dino::rebuild_sav(opt.sav_roundtrip_path, new_name,
                                      opt.sav_slot, opt.sav_budget);
                std::ofstream out(opt.sav_write_path, std::ios::binary);
                if (!out) {
                    std::fprintf(stderr, "cannot open %s\n",
                                 opt.sav_write_path.c_str());
                    return 2;
                }
                out.write(reinterpret_cast<const char*>(rebuilt.data()),
                          static_cast<std::streamsize>(rebuilt.size()));
                if (!out) {
                    std::fprintf(stderr, "cannot write %s\n",
                                 opt.sav_write_path.c_str());
                    return 2;
                }
                std::printf("sav written: %zu bytes -> %s\n", rebuilt.size(),
                            opt.sav_write_path.c_str());
            } catch (const std::exception& e) {
                std::fprintf(stderr, "sav write failed: %s\n", e.what());
                return 2;
            }
        }
        return ok ? 0 : 1;
    }
    if (opt.wld_info) {
        if (opt.wld_path.empty()) {
            std::fprintf(stderr, "--wld-info requires --wld FILE\n");
            return 2;
        }
        try {
            std::string lower = opt.wld_path;
            for (char& c : lower) {
                c = static_cast<char>(
                    std::tolower(static_cast<unsigned char>(c)));
            }
            const bool is_save =
                lower.size() >= 4 && lower.compare(lower.size() - 4, 4, ".sav") == 0;
            const dino::WldWorld world =
                is_save ? dino::read_sav_world(opt.wld_path)
                        : dino::read_wld(opt.wld_path);
            std::vector<std::string> building_names;
            try {
                building_names = dino::load_building_list(dino::asset_path(
                    opt.asset_root, "Data/Design/buildings/BuildingList.ini"));
            } catch (const std::exception& e) {
                std::fprintf(stderr, "building list: %s\n", e.what());
            }
            std::printf(
                "wld: %s version=%u mode=%u fences=%u shows=%u animals=%u "
                "employees=%u buildings=%zu visitors=%u dinosaurs=%u roads=%zu "
                "camera=%d\n",
                opt.wld_path.c_str(), world.version, world.mode,
                world.fence_count, world.show_count, world.animal_count,
                world.employee_count, world.buildings.size(),
                world.visitor_count, world.dinosaur_count, world.roads.size(),
                world.has_camera ? 1 : 0);
            for (const dino::WorldBuilding& b : world.buildings) {
                const char* name =
                    b.kind < building_names.size()
                        ? building_names[b.kind].c_str()
                        : "?";
                std::printf(
                    "building: kind=%u name=%s x=%u y=%u state=%u obj=%u "
                    "linked=%u link=%u class=%u\n",
                    b.kind, name, b.cell_x, b.cell_y, b.state, b.object_id,
                    b.linked_id, b.link, b.class_tag);
            }
            for (const dino::RoadRect& road : world.roads) {
                std::printf("road: %d,%d - %d,%d\n", road.x0, road.y0, road.x1,
                            road.y1);
            }
            for (const dino::SavWorldPlaced& show : world.records.shows) {
                std::printf(
                    "show: obj=%u start=%u,%u end=%u,%u class=%u field44=%u "
                    "field48=%u waypoints=%zu\n",
                    show.object_id, show.start_x, show.start_y, show.end_x,
                    show.end_y, show.class_tag, show.field_44, show.field_48,
                    show.waypoints.size());
            }
            for (const dino::SavWorldAnimal& animal : world.records.animals) {
                std::printf("animal: obj=%u field1c=%u field20=%u\n",
                            animal.object_id, animal.field_1c, animal.field_20);
            }
            for (const dino::SavWorldEmployee& employee :
                 world.records.employees) {
                std::printf("employee: obj=%u field1c=%u\n", employee.object_id,
                            employee.field_1c);
            }
            if (world.has_simulation_lists) {
                std::printf("sim lists: food_sources=%zu employees=%zu\n",
                            world.food_sources.size(),
                            world.employees_sim.size());
                for (const dino::WldFoodSource& source : world.food_sources) {
                    std::printf(
                        "food source: id=%u cell=%u,%u pos=%.3f,%.3f "
                        "dir=%.3f,%.3f\n",
                        source.id, source.cell_x, source.cell_y,
                        static_cast<double>(source.x),
                        static_cast<double>(source.y),
                        static_cast<double>(source.dx),
                        static_cast<double>(source.dy));
                }
                for (const dino::WldEmployeeState& employee :
                     world.employees_sim) {
                    std::printf(
                        "sim employee: id=%u cell=%u,%u pos=%.3f,%.3f "
                        "dir=%.3f,%.3f state=%u\n",
                        employee.id, employee.cell_x, employee.cell_y,
                        static_cast<double>(employee.x),
                        static_cast<double>(employee.y),
                        static_cast<double>(employee.dx),
                        static_cast<double>(employee.dy), employee.state);
                }
            }
            if (world.has_camera) {
                std::printf("camera:");
                for (const float value : world.camera) {
                    std::printf(" %.3f", value);
                }
                std::printf("\n");
            }
            return 0;
        } catch (const std::exception& e) {
            std::fprintf(stderr, "wld-info: %s\n", e.what());
            return 1;
        }
    }

    if (!opt.actions_bad_info.empty()) {
        try {
            const dino::ActionsBad table =
                dino::load_actions_bad(opt.actions_bad_info);
            std::printf("actions bad: families=%d kinds=%d\n",
                        dino::ActionsBad::kFamilies, dino::ActionsBad::kKinds);
            const std::array<int, 7> kinds = {3, 4, 5, 9, 0x19, 0x1c, 0x27};
            for (int family = 0; family < dino::ActionsBad::kFamilies;
                 ++family) {
                std::printf("family %d:", family);
                for (const int kind : kinds) {
                    std::printf(" dur%d=%.2f", kind,
                                static_cast<double>(dino::action_duration(
                                    table, family, kind)));
                }
                // Rest-gauge scale `FUN_00571540` (`block f[105+kind]`).
                std::printf(" scale1=%.2f scale2=%.2f scale30=%.2f",
                            static_cast<double>(
                                dino::action_scale(table, family, 1)),
                            static_cast<double>(
                                dino::action_scale(table, family, 2)),
                            static_cast<double>(
                                dino::action_scale(table, family, 0x1e)));
                std::printf("\n");
            }
            return 0;
        } catch (const std::exception& e) {
            std::fprintf(stderr, "actions bad: %s\n", e.what());
            return 1;
        }
    }

    if (opt.bai_info) {
        try {
            const std::vector<dino::BaiAction> actions =
                dino::load_action_table(opt.asset_root);
            std::size_t pre = 0;
            std::size_t weights = 0;
            std::size_t needs = 0;
            std::size_t grants = 0;
            std::size_t elements = 0;
            std::size_t events = 0;
            std::array<std::size_t, 7> groups{};
            std::array<std::size_t, 6> classes{};
            for (const dino::BaiAction& action : actions) {
                pre += action.preconditions.size();
                weights += action.effect_weights.size();
                needs += action.need_deltas.size();
                grants += action.stat_grants.size();
                elements += action.elements.size();
                for (const dino::BaiElement& element : action.elements) {
                    events += element.events.size();
                }
                if (action.special_action < groups.size()) {
                    ++groups[action.special_action];
                }
                if (action.effect_class < classes.size()) {
                    ++classes[action.effect_class];
                }
            }
            std::printf(
                "actions: %zu preconditions=%zu weights=%zu need_deltas=%zu "
                "grants=%zu elements=%zu events=%zu\n",
                actions.size(), pre, weights, needs, grants, elements, events);
            std::printf("groups:");
            for (std::size_t i = 0; i < groups.size(); ++i) {
                std::printf(" %zu=%zu", i, groups[i]);
            }
            std::printf("\nclasses:");
            for (std::size_t i = 0; i < classes.size(); ++i) {
                std::printf(" %zu=%zu", i, classes[i]);
            }
            std::printf("\n");
            if (!actions.empty()) {
                std::printf("first=%s last=%s\n", actions.front().name.c_str(),
                            actions.back().name.c_str());
            }
            return 0;
        } catch (const std::exception& e) {
            std::fprintf(stderr, "bai-info: %s\n", e.what());
            return 1;
        }
    }

    if (opt.market_info || !opt.store_ini_path.empty()) {
        try {
            const bool explicit_store = !opt.store_ini_path.empty();
            const dino::Market market = dino::load_market(resolve_asset(
                opt.asset_root,
                explicit_store ? opt.store_ini_path
                               : "Data/Design/Scenario/Market.ini"));
            const char* label = explicit_store ? "store" : "market";
            int min_price = market.entries.front().price;
            int max_price = min_price;
            for (const dino::MarketEntry& entry : market.entries) {
                min_price = std::min(min_price, entry.price);
                max_price = std::max(max_price, entry.price);
            }
            std::printf("%s: entries=%zu stores=%zu prices=%d..%d\n", label,
                        market.entries.size(), market.stores.size(),
                        min_price, max_price);
            std::printf("%s stores:", label);
            for (const int store : market.stores) {
                std::size_t count = 0;
                for (const dino::MarketEntry& entry : market.entries) {
                    if (entry.store == store) {
                        ++count;
                    }
                }
                std::printf(" %d=%zu", store, count);
            }
            std::printf("\n");
            const dino::MarketEntry& first = market.entries.front();
            const dino::MarketEntry& last = market.entries.back();
            std::printf("first=%s %d %s last=%s %d %s\n", first.name.c_str(),
                        first.price, first.genotype.c_str(), last.name.c_str(),
                        last.price, last.genotype.c_str());
            return 0;
        } catch (const std::exception& e) {
            std::fprintf(stderr, "market-info: %s\n", e.what());
            return 1;
        }
    }

    if (!opt.food_info_dir.empty()) {
        try {
            const std::string dir =
                resolve_asset(opt.asset_root, opt.food_info_dir);
            const std::vector<std::string> names =
                dino::load_food_source_list(dir + "/FoodSourceList.ini");
            for (const std::string& name : names) {
                const dino::FoodSourceDef source =
                    dino::load_bsi(dir + "/" + name + ".bsi");
                std::printf(
                    "food: %s nutrition=%.3g quantity=%.3g price=%d kind=%d "
                    "flags=%d feeder=%s ui=%s\n",
                    source.name.c_str(), source.nutrition, source.quantity,
                    source.price, source.kind_id, source.flags,
                    source.feeder_mesh().c_str(), source.ui_name().c_str());
            }
            std::printf("food sources: %zu\n", names.size());
            return 0;
        } catch (const std::exception& e) {
            std::fprintf(stderr, "food-info: %s\n", e.what());
            return 1;
        }
    }

    if (opt.employee_info) {
        try {
            const std::vector<dino::EmployeeProfile> profiles =
                dino::load_employee_list(
                    resolve_asset(opt.asset_root, "Data/Design/employees"));
            for (const dino::EmployeeProfile& profile : profiles) {
                std::printf(
                    "employee: %s kind=%d wage=%d height=%.2f walk=%.2f "
                    "action=%.2f work=%.2f meshes=%zu sounds=%zu\n",
                    profile.name.c_str(), profile.kind, profile.wage,
                    profile.height, profile.walk_speed, profile.action_speed,
                    profile.work_duration, profile.meshes.size(),
                    profile.sounds.size());
            }
            std::printf("employee profiles: %zu\n", profiles.size());
            return 0;
        } catch (const std::exception& e) {
            std::fprintf(stderr, "employee-info: %s\n", e.what());
            return 1;
        }
    }

    if (!opt.show_info_path.empty()) {
        try {
            std::vector<std::string> paths;
            const std::filesystem::path input(opt.show_info_path);
            if (std::filesystem::is_directory(input)) {
                for (const auto& entry :
                     std::filesystem::directory_iterator(input)) {
                    if (entry.is_regular_file() &&
                        entry.path().extension() == ".shw") {
                        paths.push_back(entry.path().string());
                    }
                }
                std::sort(paths.begin(), paths.end());
            } else {
                paths.push_back(input.string());
            }
            const std::vector<std::string> feature_names =
                dino::load_feature_list(resolve_asset(
                    opt.asset_root, "Data/Design/attractions/features"));
            std::vector<dino::BfiFeature> bfi_features;
            try {
                bfi_features = dino::load_bfi_dir(resolve_asset(
                    opt.asset_root, "Data/Design/attractions/features"));
            } catch (const std::exception& e) {
                std::fprintf(stderr, "show-info .bfi: %s\n", e.what());
            }
            std::printf("bfi features: %zu\n", bfi_features.size());
            const auto find_bfi = [&](const std::string& name)
                -> const dino::BfiFeature* {
                for (const dino::BfiFeature& bfi : bfi_features) {
                    if (bfi.name.size() != name.size()) {
                        continue;
                    }
                    bool same = true;
                    for (std::size_t i = 0; i < name.size(); ++i) {
                        if (std::tolower(static_cast<unsigned char>(bfi.name[i])) !=
                            std::tolower(static_cast<unsigned char>(name[i]))) {
                            same = false;
                            break;
                        }
                    }
                    if (same) {
                        return &bfi;
                    }
                }
                return nullptr;
            };
            const bool list_features = paths.size() == 1;
            for (const std::string& path : paths) {
                const dino::ShwShow show = dino::load_shw(path);
                std::printf(
                    "show: %s name=%s cost=%d judge=%d dims=%dx%d wall=%d "
                    "cages=%d,%d,%d,%d features=%zu teams=%zu\n",
                    std::filesystem::path(path).stem().string().c_str(),
                    show.name.c_str(), show.cost, show.judge_kind, show.width,
                    show.height, show.wall_type, show.cage_size[0],
                    show.cage_size[1], show.cage_size[2], show.cage_size[3],
                    show.features.size(), show.teams.size());
                std::vector<int> cells(
                    static_cast<std::size_t>(show.width) * show.height, 0);
                int overlaps = 0;
                for (const auto& feature : show.features) {
                    const std::string feature_name =
                        feature[0] >= 0 &&
                                feature[0] <
                                    static_cast<int>(feature_names.size())
                            ? feature_names[static_cast<std::size_t>(
                                  feature[0])]
                            : std::string("?");
                    const dino::BfiFeature* bfi = find_bfi(feature_name);
                    int fw = bfi != nullptr ? bfi->width : 1;
                    int fh = bfi != nullptr ? bfi->height : 1;
                    if ((feature[1] & 1) != 0) {
                        std::swap(fw, fh);
                    }
                    for (int dy = 0; dy < fh; ++dy) {
                        for (int dx = 0; dx < fw; ++dx) {
                            const int cx = feature[2] + dx;
                            const int cy = feature[3] + dy;
                            if (cx >= 0 && cy >= 0 && cx < show.width &&
                                cy < show.height) {
                                if (cells[static_cast<std::size_t>(cy) *
                                              show.width +
                                          cx]++ > 0) {
                                    ++overlaps;
                                }
                            }
                        }
                    }
                    if (list_features) {
                        std::printf(
                            "show feature: idx=%d rot=%d x=%d y=%d name=%s "
                            "w=%d h=%d\n",
                            feature[0], feature[1], feature[2], feature[3],
                            feature_name.c_str(), fw, fh);
                    }
                }
                int occupied = 0;
                for (const int cell : cells) {
                    if (cell > 0) {
                        ++occupied;
                    }
                }
                std::printf("show tiling: %s cells=%d overlaps=%d\n",
                            std::filesystem::path(path)
                                .stem()
                                .string()
                                .c_str(),
                            occupied, overlaps);
                if (list_features) {
                    for (const auto& team : show.teams) {
                        std::printf("show team: cage=%d team=%d\n", team.first,
                                    team.second);
                    }
                }
            }
            std::printf("shows: %zu\n", paths.size());
            return 0;
        } catch (const std::exception& e) {
            std::fprintf(stderr, "show-info: %s\n", e.what());
            return 1;
        }
    }

    if (opt.dino_actions_info) {
        try {
            const std::vector<dino::BaiAction> actions =
                dino::load_action_table(opt.asset_root);
            if (actions.size() != 64) {
                std::fprintf(stderr, "dino-actions: expected 64 actions, got %zu\n",
                             actions.size());
                return 1;
            }
            // The retail behavior graph only contains the self/special actions
            // for these needs; the other 60 need a perceived world target.
            const std::vector<std::string> nodes = {
                "Exploration", "Eat", "EatForSmallies", "Sleep"};
            const auto make_needs = [](int food, int violence, int secu,
                                       int repos, int size) {
                dino::DinoNeeds needs;
                needs.value = {food, violence, secu, repos};
                needs.size_cm = size;
                return needs;
            };
            struct Scenario {
                const char* name;
                dino::DinoNeeds needs;
            };
            const Scenario scenarios[] = {
                {"starving", make_needs(20, 90, 50, 90, 250)},
                {"tired", make_needs(70, 90, 90, 20, 250)},
                {"both", make_needs(20, 90, 50, 20, 250)},
                {"full", make_needs(99, 99, 99, 99, 250)},
                {"small", make_needs(20, 90, 50, 90, 150)},
            };
            std::printf("dino actions: %zu nodes=%zu\n", actions.size(),
                        nodes.size());
            for (const Scenario& scenario : scenarios) {
                const dino::DinoNeeds& needs = scenario.needs;
                std::printf("dino states %s: %d %d %d %d\n", scenario.name,
                            dino::need_state(needs.value[0]),
                            dino::need_state(needs.value[1]),
                            dino::need_state(needs.value[2]),
                            dino::need_state(needs.value[3]));
                const auto choice =
                    dino::choose_action(actions, needs, nodes, true);
                if (choice) {
                    std::printf("dino choose %s: %s %d\n", scenario.name,
                                choice->name.c_str(), choice->score);
                } else {
                    std::printf("dino choose %s: idle\n", scenario.name);
                }
            }
            dino::DinoActionLoopConfig loop_config;
            loop_config.action_scale = 0.1;  // 3 s per action in the probe
            dino::DinoActionLoop loop(&actions,
                                      make_needs(20, 90, 50, 20, 250),
                                      loop_config);
            std::printf("dino timeline:\n");
            std::string previous;
            double t = 0.0;
            for (int step = 0; step < 400; ++step) {
                loop.tick(0.1);
                t += 0.1;
                const std::string& name = loop.action_name();
                if (name != previous) {
                    const dino::DinoNeeds& n = loop.needs();
                    std::printf(
                        "dino timeline %.1f: %s food=%d violence=%d secu=%d "
                        "repos=%d\n",
                        t, name.c_str(), n.value[0], n.value[1], n.value[2],
                        n.value[3]);
                    previous = name;
                }
            }
            // Action-target policy (`node+0x3c` vtable +0x14): the target
            // position feeds the chooser's danger-grid threat term.
            {
                dino::DangerGrid grid;
                grid.cell(12, 12) = 500;  // target cell (100, 100)
                grid.cell(25, 25) = 300;  // actor cell  (200, 200)
                dino::ActionTarget box;
                box.kind = dino::ActionTarget::Kind::Box;
                box.x = 100;
                box.y = 100;
                std::printf(
                    "dino target box: %d\n",
                    dino::chooser_threat_score(box, 200, 200, 40, 50, grid));
                dino::ActionTarget actor;
                actor.kind = dino::ActionTarget::Kind::Actor;
                actor.x = 200;
                actor.y = 200;
                std::printf(
                    "dino target actor: %d\n",
                    dino::chooser_threat_score(actor, 200, 200, 40, 50, grid));
                dino::ActionTarget none;
                std::printf(
                    "dino target none: %d\n",
                    dino::chooser_threat_score(none, 200, 200, 40, 50, grid));
                const auto threat_choice = dino::choose_action(
                    actions, make_needs(20, 90, 50, 90, 250), nodes, true,
                    [&](const dino::BaiAction& action) {
                        return action.name == "Eat"
                                   ? dino::chooser_threat_score(
                                         box, 200, 200, 40, 50, grid)
                                   : 0;
                    });
                if (threat_choice) {
                    std::printf("dino choose threatbox: %s %d\n",
                                threat_choice->name.c_str(),
                                threat_choice->score);
                }
                dino::DinoActionLoop threat_loop(
                    &actions, make_needs(20, 90, 50, 90, 250), loop_config);
                threat_loop.set_threat_provider(
                    [](const dino::BaiAction& action) {
                        return action.name == "Eat" ? -100000 : 0;
                    });
                threat_loop.tick(0.1);
                std::printf("dino loop threat: %s\n",
                            threat_loop.action_name().c_str());
            }
            // Completion writers `FUN_00571190`: case 0xd copies phenotype
            // +0x44 into the grid threat cache; case 0x1a copies +0x80 into
            // the stamp radius. Drive both through the public loop API.
            {
                std::vector<dino::BaiAction> cache_actions(2);
                cache_actions[0].name = "GetCourage";
                cache_actions[1].name = "ObstAttackSize1";
                dino::DinoNeeds cache_needs;
                cache_needs.combined_attack = 54;
                cache_needs.obstacle_radius = 7;
                dino::DinoActionLoop cache_loop(&cache_actions, cache_needs);
                cache_loop.restore(cache_needs, 0, 0.01);
                cache_loop.tick(0.02);
                std::printf("dino cache getcourage: threat=%d\n",
                            cache_loop.needs().threat);
                cache_loop.restore(cache_needs, 1, 0.01);
                cache_loop.tick(0.02);
                std::printf("dino cache obst: radius=%d\n",
                            cache_loop.needs().threat_radius);
            }
            return 0;
        } catch (const std::exception& e) {
            std::fprintf(stderr, "dino-actions: %s\n", e.what());
            return 1;
        }
    }

    return std::nullopt;
}

}  // namespace dino::app
