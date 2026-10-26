#include "campaign.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string_view>

#include "../ui/ini.h"

namespace dino {
namespace {

std::string upper(std::string value) {
    for (char& c : value) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return value;
}

std::string lower(std::string value) {
    for (char& c : value) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return value;
}

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::vector<std::string> split_words(const std::string& value) {
    std::vector<std::string> words;
    std::size_t i = 0;
    while (i < value.size()) {
        while (i < value.size() &&
               std::isspace(static_cast<unsigned char>(value[i]))) {
            ++i;
        }
        const std::size_t start = i;
        while (i < value.size() &&
               !std::isspace(static_cast<unsigned char>(value[i]))) {
            ++i;
        }
        if (i > start) {
            words.push_back(value.substr(start, i - start));
        }
    }
    return words;
}

// #INCLUDE expansion appends the base sections after the child sections, so
// walk backwards: the child (earlier) overwrites the base (later).
std::map<std::string, std::string> flatten(const dino::ui::IniFile& file) {
    std::map<std::string, std::string> values;
    for (auto section = file.sections.rbegin(); section != file.sections.rend();
         ++section) {
        const std::string section_name = upper(section->name);
        for (const auto& [key, value] : section->values) {
            values[section_name + "." + upper(key)] = value;
        }
    }
    return values;
}

std::string get(const std::map<std::string, std::string>& values,
                const std::string& section, const std::string& key) {
    const auto it = values.find(upper(section) + "." + upper(key));
    return it == values.end() ? std::string() : it->second;
}

int to_int(const std::string& value, int fallback = 0) {
    try {
        return std::stoi(trim(value));
    } catch (...) {
        return fallback;
    }
}

double to_double(const std::string& value, double fallback = 1.0) {
    try {
        return std::stod(trim(value));
    } catch (...) {
        return fallback;
    }
}

std::vector<std::string> read_list(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("campaign: cannot open " + path);
    }
    std::string line;
    int count = -1;
    std::vector<std::string> names;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const std::string text = trim(line);
        if (text.empty()) {
            continue;
        }
        if (count < 0) {
            count = to_int(text, 0);
            if (count > 10000) {
                throw std::runtime_error("campaign: bad list count in " + path);
            }
            continue;
        }
        names.push_back(text);
        if (static_cast<int>(names.size()) >= count) {
            break;
        }
    }
    if (count < 0 || static_cast<int>(names.size()) != count) {
        throw std::runtime_error("campaign: list count mismatch in " + path);
    }
    return names;
}

const char* const kObjectiveVerbs[] = {
    "treasury",          "marchandizing",    "dyingdinos",
    "aggressivedinos",   "genotypedinos",    "cutedinos",
    "hybriddinos",       "notconnectedbuildings", "connectedbuildings",
    "visitors",          "scaritydinos",     "speeddinos",
    "staminadinos",      "cleverdinos",      "sizedagilitydinos",
    "franckstrangedinos","employees",        "hungrydinos",
    "isolateddinos",     "happyvisitors",    "keephappyvisitors",
    "carnivordinos",     "ornateddinos",     "cameraaltitude",
    "happyshow",         "fightshow",        "scientificshow",
    "happydinoshow",     "delaylimit",       "buyalldinos",
    "dinoexclusiveshow", "prettystrangedino", "bankrupt",
};

}  // namespace

int objective_verb_count() {
    return static_cast<int>(std::size(kObjectiveVerbs));
}

int objective_verb_id(const std::string& verb) {
    const std::string wanted = lower(trim(verb));
    for (int i = 0; i < objective_verb_count(); ++i) {
        if (wanted == kObjectiveVerbs[i]) {
            return i;
        }
    }
    return -1;
}

const char* objective_verb_name(int objective_id) {
    if (objective_id < 0 || objective_id >= objective_verb_count()) {
        return "";
    }
    return kObjectiveVerbs[objective_id];
}

CampaignMission load_campaign_mission(const std::string& mission_ini) {
    const std::filesystem::path path(mission_ini);
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("campaign: missing mission " + mission_ini);
    }
    CampaignMission mission;
    mission.path = path.string();
    mission.name = path.stem().string();
    mission.values = flatten(dino::ui::load_ini(path.string()));
    mission.title_id = get(mission.values, "BRIEFING", "TITLE");
    mission.description_id = get(mission.values, "BRIEFING", "DESCRIPTIONTEXT");
    mission.hint_id = get(mission.values, "BRIEFING", "HINTTEXT");

    for (int i = 1; i <= 5; ++i) {
        const std::string suffix = std::to_string(i);
        const std::string command =
            get(mission.values, "OBJECTIVES", "OBJECTIVE" + suffix);
        if (trim(command).empty()) {
            continue;
        }
        const std::vector<std::string> words = split_words(command);
        if (words.empty()) {
            continue;
        }
        CampaignObjective objective;
        objective.command = command;
        objective.verb = lower(words.front());
        objective.args.assign(words.begin() + 1, words.end());
        objective.description_id =
            get(mission.values, "OBJECTIVES", "OBJECTIVE" + suffix + "_DESC");
        objective.objective_id = objective_verb_id(objective.verb);
        mission.objectives.push_back(std::move(objective));
    }
    return mission;
}

Campaign load_campaign(const std::string& campaign_ini) {
    const std::filesystem::path path(campaign_ini);
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("campaign: missing campaign " + campaign_ini);
    }
    Campaign campaign;
    campaign.path = path.string();
    campaign.name = path.parent_path().filename().string();
    campaign.values = flatten(dino::ui::load_ini(path.string()));
    campaign.island = get(campaign.values, "LANDSCAPE", "ISLAND");
    campaign.world_file = get(campaign.values, "CONTEXT", "WORLDFILE");
    campaign.budget = to_int(get(campaign.values, "CONTEXT", "BUDGET"));
    campaign.sell_back =
        to_double(get(campaign.values, "CONTEXT", "SELLBACKFACTOR"), 1.0);
    campaign.title_id = get(campaign.values, "BRIEFING", "TITLE");
    campaign.description_id =
        get(campaign.values, "BRIEFING", "DESCRIPTIONTEXT");
    campaign.ending_text_id = get(campaign.values, "BRIEFING", "ENDINGTEXT");
    campaign.ending_video = get(campaign.values, "BRIEFING", "ENDINGVIDEO");
    campaign.ending_video_name =
        get(campaign.values, "BRIEFING", "ENDINGVIDEONAME");

    const std::string list_name = get(campaign.values, "MISSION", "MISSIONLIST");
    if (!list_name.empty()) {
        const std::filesystem::path list_path = path.parent_path() / list_name;
        for (const std::string& name : read_list(list_path.string())) {
            campaign.missions.push_back(load_campaign_mission(
                (path.parent_path() / (name + ".ini")).string()));
        }
    }
    return campaign;
}

std::vector<Campaign> load_campaigns(const std::string& scenario_root) {
    const std::filesystem::path root(scenario_root);
    const std::vector<std::string> names =
        read_list((root / "CampaignList.ini").string());
    std::vector<Campaign> campaigns;
    campaigns.reserve(names.size());
    for (const std::string& name : names) {
        campaigns.push_back(
            load_campaign((root / name / (name + ".ini")).string()));
    }
    return campaigns;
}

}  // namespace dino
