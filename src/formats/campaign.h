#pragma once

#include <map>
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

}  // namespace dino
