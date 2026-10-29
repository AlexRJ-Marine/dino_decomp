#include "ini.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>

namespace dino {
namespace {

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string unquote(const std::string& value) {
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

void load_into(const std::string& path, IniFile& out, std::set<std::string>& visited) {
    const std::string key = std::filesystem::absolute(path).lexically_normal().string();
    if (!visited.insert(key).second) {
        return;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("ini: cannot open " + path);
    }
    const std::size_t first_section = out.sections.size();
    IniSection* current = nullptr;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const std::string text = trim(line);
        if (text.empty() || text[0] == ';') {
            continue;
        }
        if (text.front() == '[' && text.back() == ']') {
            out.sections.push_back({trim(text.substr(1, text.size() - 2)), {}});
            current = &out.sections.back();
            continue;
        }
        const auto eq = text.find('=');
        if (eq == std::string::npos || current == nullptr) {
            continue;
        }
        const std::string name = trim(text.substr(0, eq));
        current->values[name] = unquote(trim(text.substr(eq + 1)));
    }

    // Collect includes from this file only, then expand them. Appending to
    // out.sections during the loop would invalidate `current`.
    std::vector<std::string> includes;
    for (std::size_t i = first_section; i < out.sections.size(); ++i) {
        if (out.sections[i].name != "#INCLUDE") {
            continue;
        }
        const std::filesystem::path base(path);
        for (int n = 0; n < 64; ++n) {
            const std::string name = "FILE" + std::to_string(n);
            if (!out.sections[i].has(name)) {
                break;
            }
            std::string child = out.sections[i].get(name);
            for (char& c : child) {
                if (c == '\\') {
                    c = '/';
                }
            }
            includes.push_back((base.parent_path() / child).string());
        }
    }
    for (const auto& child : includes) {
        load_into(child, out, visited);
    }
}

}  // namespace

const IniSection* IniFile::find(const std::string& name) const {
    for (const auto& section : sections) {
        if (section.name == name) {
            return &section;
        }
    }
    return nullptr;
}

IniFile load_ini(const std::string& path) {
    IniFile file;
    std::set<std::string> visited;
    load_into(path, file, visited);
    return file;
}

}  // namespace dino
