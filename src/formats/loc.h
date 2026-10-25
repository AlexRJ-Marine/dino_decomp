#pragma once

#include <string>
#include <unordered_map>

namespace dino {

// Reads a retail `.loc` file (UTF-16LE, KEY="value" lines) and exposes
// FIELD_ID -> TEXT. See docs/formats/loc.md.
struct Loc {
    std::unordered_map<std::string, std::string> texts;

    const std::string* find(const std::string& field) const {
        const auto it = texts.find(field);
        return it == texts.end() ? nullptr : &it->second;
    }
};

Loc load_loc(const std::string& path);

}  // namespace dino
