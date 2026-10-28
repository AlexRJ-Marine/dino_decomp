#include "campaign.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
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

bool objective_verb_implemented(int objective_id) {
    switch (objective_id) {
        case 0:   // treasury
        case 1:   // marchandizing
        case 2:   // dyingdinos
        case 3:   // aggressivedinos
        case 4:   // genotypedinos
        case 5:   // cutedinos
        case 6:   // hybriddinos
        case 7:   // notconnectedbuildings
        case 8:   // connectedbuildings
        case 9:   // visitors
        case 11:  // speeddinos
        case 12:  // staminadinos
        case 13:  // cleverdinos
        case 14:  // sizedagilitydinos
        case 15:  // franckstrangedinos
        case 16:  // employees
        case 17:  // hungrydinos
        case 18:  // isolateddinos
        case 19:  // happyvisitors
        case 22:  // ornateddinos
        case 23:  // cameraaltitude
        case 24:  // happyshow
        case 25:  // fightshow
        case 26:  // scientificshow
        case 27:  // happydinoshow
        case 28:  // delaylimit
        case 29:  // buyalldinos
        case 30:  // dinoexclusiveshow
        case 31:  // prettystrangedino
        case 32:  // bankrupt
            return true;
        default:
            return false;
    }
}

CampaignObjective parse_objective_command(const std::string& command,
                                          const std::string& description_id) {
    CampaignObjective objective;
    objective.command = command;
    objective.description_id = description_id;
    const std::vector<std::string> words = split_words(command);
    if (words.empty()) {
        return objective;
    }
    objective.verb = lower(words.front());
    objective.args.assign(words.begin() + 1, words.end());
    objective.objective_id = objective_verb_id(objective.verb);
    return objective;
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
        mission.objectives.push_back(parse_objective_command(
            command, get(mission.values, "OBJECTIVES",
                         "OBJECTIVE" + suffix + "_DESC")));
    }
    return mission;
}

std::optional<Campaign> load_parent_campaign(
    const CampaignMission& mission) {
    if (mission.path.empty()) {
        return std::nullopt;
    }
    const std::filesystem::path dir =
        std::filesystem::path(mission.path).parent_path();
    const std::string campaign_name = dir.filename().string();
    if (campaign_name.empty()) {
        return std::nullopt;
    }
    try {
        return load_campaign((dir / (campaign_name + ".ini")).string());
    } catch (const std::exception&) {
    }
    return std::nullopt;
}

std::optional<CampaignMission> next_campaign_mission(
    const CampaignMission& current) {
    const std::optional<Campaign> campaign = load_parent_campaign(current);
    if (!campaign) {
        return std::nullopt;
    }
    for (std::size_t i = 0; i + 1 < campaign->missions.size(); ++i) {
        if (campaign->missions[i].name == current.name) {
            return campaign->missions[i + 1];
        }
    }
    return std::nullopt;
}

void write_campaign_unlock(const std::string& path, std::uint32_t value) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("cannot write campaign unlock " + path);
    }
    const unsigned char bytes[4] = {
        static_cast<unsigned char>(value & 0xFF),
        static_cast<unsigned char>((value >> 8) & 0xFF),
        static_cast<unsigned char>((value >> 16) & 0xFF),
        static_cast<unsigned char>((value >> 24) & 0xFF),
    };
    out.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
    if (!out) {
        throw std::runtime_error("short write to campaign unlock " + path);
    }
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

std::uint32_t read_campaign_unlock(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return 1;
    }
    unsigned char bytes[4]{};
    in.read(reinterpret_cast<char*>(bytes), sizeof(bytes));
    if (in.gcount() < static_cast<std::streamsize>(sizeof(bytes))) {
        return 1;
    }
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
}

unsigned campaign_number(const std::string& name) {
    unsigned number = 0;
    bool digits = false;
    for (const char c : name) {
        if (c >= '0' && c <= '9') {
            number = number * 10 + static_cast<unsigned>(c - '0');
            digits = true;
        } else if (digits) {
            break;
        }
    }
    return digits ? number : 0;
}

bool campaign_unlocked(const Campaign& campaign, std::uint32_t value) {
    const unsigned number = campaign_number(campaign.name);
    if (number == 0) {
        return true;
    }
    return number <= value;
}

bool campaign_ending_unlocked(const Campaign& campaign,
                              std::uint32_t value) {
    const unsigned number = campaign_number(campaign.name);
    if (number == 0) {
        return true;
    }
    return number < value;
}

std::map<std::string, std::string> load_campaign_progress(
    const std::string& path) {
    std::map<std::string, std::string> progress;
    std::ifstream in(path);
    if (!in) {
        return progress;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        std::istringstream parts(line);
        std::string campaign;
        std::string mission;
        if (parts >> campaign >> mission && !campaign.empty() &&
            campaign[0] != '#') {
            progress[campaign] = mission;
        }
    }
    return progress;
}

void save_campaign_progress(
    const std::map<std::string, std::string>& progress,
    const std::string& path) {
    if (path.empty()) {
        return;
    }
    const std::filesystem::path target(path);
    if (!target.parent_path().empty()) {
        std::filesystem::create_directories(target.parent_path());
    }
    std::ofstream out(target);
    if (!out) {
        throw std::runtime_error("cannot write campaign progress " + path);
    }
    for (const auto& [campaign, mission] : progress) {
        out << campaign << ' ' << mission << '\n';
    }
    if (!out) {
        throw std::runtime_error("short write to campaign progress " + path);
    }
}

}  // namespace dino
