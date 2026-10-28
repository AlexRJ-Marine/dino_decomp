#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace dino {

// One `ObjectiveN` line from a mission INI:
//   Objective1="treasury 25000 1"
//   Objective1_Desc="&Cpg4_Mission1_Objective1_Description"
struct CampaignObjective {
    std::string command;        // raw value
    std::string verb;           // lowercase first token
    std::vector<std::string> args;
    std::string description_id; // loc id (may be empty)
    int objective_id = -1;      // 0..32, -1 = unknown verb
};

struct CampaignMission {
    std::string name;
    std::string path;
    std::string title_id;
    std::string description_id;
    std::string hint_id;
    std::vector<CampaignObjective> objectives;
    // Flattened "SECTION.KEY" values after #INCLUDE expansion; child keys
    // override the base (later definitions lose).
    std::map<std::string, std::string> values;
};

struct Campaign {
    std::string name;        // Cpg04
    std::string path;        // .../Cpg04/Cpg04.ini
    std::string island;      // Isl04
    std::string world_file;  // Cpg04_Save.wld
    int budget = 0;
    double sell_back = 1.0;
    std::string title_id;
    std::string description_id;
    std::string ending_text_id;
    std::string ending_video;
    std::string ending_video_name;
    std::vector<CampaignMission> missions;
    std::map<std::string, std::string> values;
};

// `scenario_root` is `Data/Design/Scenario`; reads CampaignList.ini and every
// listed campaign/mission. Throws std::runtime_error on a missing file or a
// malformed list.
std::vector<Campaign> load_campaigns(const std::string& scenario_root);

// Loads one campaign .ini (and its MissionList) without needing the root list.
Campaign load_campaign(const std::string& campaign_ini);

// Loads one mission .ini.
CampaignMission load_campaign_mission(const std::string& mission_ini);

// Next mission in the same campaign (`MissionN+1`), or nullopt at the end.
// Reloads the parent `CpgNN.ini` from the mission path.
std::optional<CampaignMission> next_campaign_mission(
    const CampaignMission& current);

// Parent campaign of a mission, loaded from the mission path's folder.
std::optional<Campaign> load_parent_campaign(const CampaignMission& mission);

// Writes `run/save/dino.dat` as a 4-byte little-endian unlocked-campaign
// count (retail 1..campaigns+1).
void write_campaign_unlock(const std::string& path, std::uint32_t value);

// Port-owned campaign progress: campaign folder -> current mission name.
// Text lines `Cpg04 Cpg04_Mission2`; `#`/blank lines are ignored. A missing
// file is an empty map.
std::map<std::string, std::string> load_campaign_progress(
    const std::string& path);
void save_campaign_progress(
    const std::map<std::string, std::string>& progress,
    const std::string& path);

// Parses one `ObjectiveN` value into verb/args/id. `description_id` is the
// matching `_Desc` localisation id.
CampaignObjective parse_objective_command(
    const std::string& command, const std::string& description_id = "");

// `0x5658d0` verb table: ids 0..32; returns -1 for an unknown verb.
// The retail typo `scaritydinos` is kept.
int objective_verb_id(const std::string& verb);

// Number of known objective verbs (33, including unused ones).
int objective_verb_count();

// Canonical verb name for an id, or "" when out of range.
const char* objective_verb_name(int objective_id);

// True when the campaign evaluator has a case for the verb id. Some
// implemented verbs still rely on partial live sources; show/franck verbs and
// the unused verbs return false.
bool objective_verb_implemented(int objective_id);

// `run/save/dino.dat`: the 4-byte little-endian count of unlocked campaigns
// read by `FUN_005612d0` into `campaign_db+0x1c` (`1` = Cpg01 only). Retail
// raises it with a plain `MOV` to `max(value, campaignNumber+1)` after a
// campaign ending (0x40285d-0x40286a) and writes the same field back. A
// missing or short file defaults to 1, matching retail. There is no bit test
// on the value anywhere in the executable.
std::uint32_t read_campaign_unlock(const std::string& path);

// `Cpg04` -> 4; 0 when the name has no numeric suffix.
unsigned campaign_number(const std::string& name);

// `campaign_number <= value` (campaign-select gating is `row < value`).
// Unknown names stay unlocked so a data change cannot silently hide them.
bool campaign_unlocked(const Campaign& campaign, std::uint32_t value);

// Campaign-ending videos lag one campaign: the cinematics page enables
// `row < value-1` (FUN_00518790), i.e. only endings already watched.
bool campaign_ending_unlocked(const Campaign& campaign, std::uint32_t value);

}  // namespace dino
