#pragma once

#include <array>
#include <string>
#include <vector>

#include "../formats/campaign.h"
#include "../formats/loc.h"
#include "campaign_objectives.h"

namespace dino {

// One allowed show from the active mission's `[SHOWS]` list. Retail stores
// the `.shw` file stems in `Shows0..Shows9` (for example `Cpg4_Derby_1`),
// resolves each to `Data/Design/Scenario/Show/<stem>.shw`, and shows the
// `.shw` `Name` localisation id (`&cpg4_derby_1`) through `ParkEng.loc`.
struct ShowCatalogEntry {
    std::string stem;     // "Cpg4_Derby_1"
    std::string path;     // resolved .shw path
    std::string name_id;  // "&cpg4_derby_1"
    std::string title;    // resolved display name
    int cost = 0;
    int judge_kind = 0;  // show-type family (1 race, 2 obstacle, 3 fight, ...)
    int width = 0;
    int height = 0;
    int wall_type = 0;
    std::array<int, 4> cage_size{};
};

// Union of the mission's `Shows0..Shows9` values (MissionBase + child), in
// file order with duplicates removed. Missing/malformed `.shw` files are
// skipped; a mission with no show list yields an empty catalog.
std::vector<ShowCatalogEntry> load_show_catalog(
    const std::string& asset_root, const CampaignMission& mission,
    const Loc& loc);

// Case-insensitive lookup by `.shw` stem or `Name` id (with or without the
// leading `&`). Returns -1 when the catalog has no match.
int find_show_catalog_index(const std::vector<ShowCatalogEntry>& catalog,
                            const std::string& name);
bool show_catalog_entry_matches(const ShowCatalogEntry& entry,
                                const std::string& name);

// One placed show arena. The offsets are the retail `snoAttraction` show
// fields documented in notes/park-time-recon.md:
//   +0xd3 enabled, +0xe0 validation (2 = valid), +0xe4 state 0..7,
//   +0x88 performances done, +0x8c performances per day, +0x90 elapsed,
//   +0x94 performance budget, +0x100 hour slot (1..10, 0 = none).
struct ParkShow {
    ShowCatalogEntry entry;
    int cell_x = 0;
    int cell_y = 0;
    int slot = 0;
    int iterations = 1;
    int done = 0;
    int state = 0;
    bool enabled = true;
    bool valid = true;
    bool locked = false;
    float elapsed = 0.0f;
    std::vector<int> participants;  // indices into the viewer's imported dinos
    CampaignShowState record;       // objective-facing snapshot
};

// Day scheduler for placed shows. Retail binds each show to one of the ten
// hour slots (`show+0x100`) and starts it when the schedule manager's cursor
// reaches that slot (FUN_0058c760 -> FUN_0057b110/FUN_0057b130). A show plays
// `iterations` times back to back and then stays idle until the next day.
class ShowRuntime {
public:
    // Places `entry` at a grid cell and assigns the first free slot 1..10.
    // `profile_ids` zero-fills the objective score vector, matching
    // FUN_00580f00 -> FUN_00576860 (sized to the active visitor-profile
    // count at placement). Returns the show index, or -1 when the ten slots
    // are full.
    int place(const ShowCatalogEntry& entry, int cell_x, int cell_y,
              const std::vector<int>& profile_ids);

    bool set_participants(int index, std::vector<int> participants);
    bool set_slot(int index, int slot);
    bool set_iterations(int index, int iterations);
    bool set_locked(int index, bool locked);
    bool set_enabled(int index, bool enabled);
    bool set_valid(int index, bool valid);

    // Performance length in park minutes. Retail's `stShow+0x94` is the
    // GeneralParams+0x110 budget (3000 game seconds) / time_scale:
    // 3000/40 = 75 real seconds = **50 park minutes** at the measured
    // time_scale 40 (`notes/show-budget-runtime-probe.md`; Cpg04's show never
    // performs, so this is the budget field, not a measured performance).
    // Zero completes a performance on the next tick (test hook).
    void set_duration_minutes(float minutes) { duration_minutes_ = minutes; }
    float duration_minutes() const { return duration_minutes_; }

    void clear();
    int count() const { return static_cast<int>(shows_.size()); }
    ParkShow* at(int index);
    const ParkShow* at(int index) const;
    const std::vector<ParkShow>& shows() const { return shows_; }
    std::vector<CampaignShowState> campaign_states() const;

    // One park tick. `hour_index` is floor(elapsed * hours_per_second) (0 at
    // the 9:00 base hour), `day` is the park day counter and `end_of_day`
    // mirrors clock+0x29. `dt` is real seconds; `time_scale` is park minutes
    // per real second. The judge callback runs once per completed
    // performance, before `done` is compared with `iterations`.
    template <typename Judge>
    int tick(int hour_index, int day, bool end_of_day, double dt,
             double time_scale, Judge&& judge) {
        if (day != last_day_) {
            last_day_ = day;
            last_hour_ = -1;
            reset_day();
        }
        if (end_of_day) {
            reset_day();
            return 0;
        }
        if (hour_index != last_hour_) {
            last_hour_ = hour_index;
            start_hour(hour_index);
        }
        int completed = 0;
        const double game_minutes = dt * (time_scale / 60.0);
        for (ParkShow& show : shows_) {
            if (show.state != 2) {
                continue;
            }
            show.elapsed += static_cast<float>(game_minutes);
            if (show.elapsed < duration_minutes_) {
                continue;
            }
            show.elapsed = 0.0f;
            ++show.done;
            judge(show);
            ++completed;
            if (show.done >= show.iterations) {
                show.state = 0;
            }
        }
        return completed;
    }

private:
    void reset_day();
    void start_hour(int hour_index);

    std::vector<ParkShow> shows_;
    int last_day_ = -1;
    int last_hour_ = -1;
    float duration_minutes_ = 50.0f;
};

}  // namespace dino
