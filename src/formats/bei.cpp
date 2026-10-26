#include "bei.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "mesh.h"  // read_binary_file

namespace dino {
namespace {

std::string trim(const std::string& text) {
    std::size_t begin = 0;
    while (begin < text.size() &&
           std::isspace(static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return text.substr(begin, end - begin);
}

std::string lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) {
                       return static_cast<char>(std::tolower(c));
                   });
    return text;
}

bool ends_with_ci(const std::string& text, const char* suffix) {
    const std::size_t length = std::strlen(suffix);
    if (text.size() < length) {
        return false;
    }
    return lower(text.substr(text.size() - length)) == suffix;
}

bool is_alpha_text(const std::string& text) {
    return !text.empty() &&
           std::all_of(text.begin(), text.end(), [](unsigned char c) {
               return std::isalpha(c) != 0;
           });
}

bool is_text_with_nul(const std::uint8_t* data, std::uint32_t length) {
    if (length < 2 || data[length - 1] != 0) {
        return false;
    }
    for (std::uint32_t i = 0; i < length; ++i) {
        const std::uint8_t value = data[i];
        if (value != 0 && (value < 32 || value >= 127)) {
            return false;
        }
    }
    return true;
}

// Port of tools/formats/actors.py `_scan_strings`: walk the body and accept
// every length-prefixed printable string that looks like an asset or a sound.
void scan_body_strings(const std::vector<std::uint8_t>& data,
                       EmployeeProfile& profile) {
    const std::size_t start = 60;
    for (std::size_t off = start; off + 4 <= data.size();) {
        std::uint32_t length = 0;
        std::memcpy(&length, data.data() + off, sizeof(length));
        if (length >= 2 && length <= 400 && off + 4 + length <= data.size() &&
            is_text_with_nul(data.data() + off + 4, length)) {
            std::string text(
                reinterpret_cast<const char*>(data.data() + off + 4));
            const bool asset = text.rfind("DATA\\", 0) == 0;
            const bool known_file =
                ends_with_ci(text, ".mesh") || ends_with_ci(text, ".tga") ||
                ends_with_ci(text, ".ini") || ends_with_ci(text, ".wav");
            if (text.size() >= 4 && (asset || known_file || is_alpha_text(text))) {
                if (asset) {
                    profile.assets.push_back(text);
                    if (ends_with_ci(text, ".mesh")) {
                        profile.meshes.push_back(text);
                    }
                } else {
                    profile.sounds.push_back(text);
                }
                off += 4 + length;
                continue;
            }
        }
        ++off;
    }
}

}  // namespace

EmployeeProfile load_bei(const std::string& path, const std::string& name) {
    const std::vector<std::uint8_t> data = read_binary_file(path);
    if (data.size() < 60) {
        throw std::runtime_error("bei: shorter than header in " + path);
    }
    std::uint32_t words[15] = {};
    std::memcpy(words, data.data(), sizeof(words));
    float floats[15] = {};
    std::memcpy(floats, data.data(), sizeof(floats));

    EmployeeProfile profile;
    profile.path = path;
    profile.name = name.empty()
                       ? std::filesystem::path(path).stem().string()
                       : name;
    profile.kind = static_cast<int>(words[0]);
    profile.height = floats[3];
    profile.walk_speed = floats[4];
    profile.action_speed = floats[5];
    profile.wage = static_cast<int>(words[8]);
    profile.work_duration = floats[10] > 0.0f ? floats[10] : 3.0f;
    scan_body_strings(data, profile);
    return profile;
}

std::vector<EmployeeProfile> load_employee_list(const std::string& dir) {
    const std::filesystem::path root(dir);
    std::ifstream in((root / "EmployeeList.ini").string());
    if (!in) {
        throw std::runtime_error("bei: cannot open EmployeeList.ini in " + dir);
    }
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line.rfind("//", 0) == 0 || line[0] == ';') {
            continue;
        }
        lines.push_back(line);
    }
    if (lines.empty()) {
        throw std::runtime_error("bei: empty EmployeeList.ini in " + dir);
    }
    const int count = std::atoi(lines[0].c_str());
    if (count <= 0 || count > 64 ||
        lines.size() < static_cast<std::size_t>(count) + 1) {
        throw std::runtime_error("bei: bad EmployeeList.ini count in " + dir);
    }

    std::vector<EmployeeProfile> profiles;
    profiles.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        const std::string name = lines[static_cast<std::size_t>(i) + 1];
        std::filesystem::path path = root / (name + ".bei");
        if (!std::filesystem::exists(path)) {
            // Retail file names are case-insensitive; recover the real name.
            for (const auto& entry : std::filesystem::directory_iterator(root)) {
                if (entry.is_regular_file() &&
                    lower(entry.path().stem().string()) == lower(name)) {
                    path = entry.path();
                    break;
                }
            }
        }
        if (!std::filesystem::exists(path)) {
            throw std::runtime_error("bei: missing profile " + path.string());
        }
        profiles.push_back(load_bei(path.string(), name));
    }
    return profiles;
}

}  // namespace dino
