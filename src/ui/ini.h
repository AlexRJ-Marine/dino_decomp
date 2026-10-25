#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace dino::ui {

struct IniSection {
    std::string name;
    std::unordered_map<std::string, std::string> values;

    bool has(const std::string& key) const { return values.find(key) != values.end(); }
    std::string get(const std::string& key) const {
        const auto it = values.find(key);
        return it == values.end() ? std::string() : it->second;
    }
};

struct IniFile {
    std::vector<IniSection> sections;  // source order, includes expanded in place
    const IniSection* find(const std::string& name) const;
};

// Reads a retail overlay .Ini (Windows-1252) and expands `[#INCLUDE]`
// sections relative to the including file.
IniFile load_ini(const std::string& path);

}  // namespace dino::ui
