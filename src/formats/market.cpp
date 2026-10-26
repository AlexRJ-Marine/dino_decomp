#include "market.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>

namespace dino {
namespace {

std::string trim(const std::string& text) {
    std::size_t first = 0;
    while (first < text.size() &&
           std::isspace(static_cast<unsigned char>(text[first]))) {
        ++first;
    }
    std::size_t last = text.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(text[last - 1]))) {
        --last;
    }
    return text.substr(first, last - first);
}

std::string unquote(const std::string& text) {
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        return text.substr(1, text.size() - 2);
    }
    return text;
}

bool is_store_key(const std::string& key, int& index) {
    constexpr const char* kPrefix = "DinoStore";
    constexpr std::size_t kPrefixLen = 9;
    if (key.rfind(kPrefix, 0) != 0 || key.size() <= kPrefixLen) {
        return false;
    }
    for (std::size_t i = kPrefixLen; i < key.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(key[i]))) {
            return false;
        }
    }
    index = std::stoi(key.substr(kPrefixLen));
    return true;
}

}  // namespace

Market load_market(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot open market: " + path);
    }
    std::map<int, std::string> stores;
    std::string section;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        line = trim(line);
        if (line.empty() || line.rfind("//", 0) == 0 ||
            line.front() == ';') {
            continue;
        }
        if (line.front() == '[') {
            const std::size_t close = line.find(']');
            section = close == std::string::npos
                          ? std::string()
                          : line.substr(1, close - 1);
            continue;
        }
        if (section != "DINOSTORE") {
            continue;
        }
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }
        const std::string key = trim(line.substr(0, equals));
        const std::string value = unquote(trim(line.substr(equals + 1)));
        int index = 0;
        if (is_store_key(key, index)) {
            stores[index] = value;
        }
    }

    Market market;
    for (const auto& [store, value] : stores) {
        std::istringstream tokens(value);
        std::vector<std::string> parts;
        std::string token;
        while (tokens >> token) {
            parts.push_back(token);
        }
        if (parts.size() % 3 != 0) {
            throw std::runtime_error("market: DinoStore" +
                                     std::to_string(store) +
                                     " is not a triplet list");
        }
        for (std::size_t i = 0; i < parts.size(); i += 3) {
            MarketEntry entry;
            entry.store = store;
            entry.name = parts[i];
            for (const char c : parts[i + 1]) {
                if (!std::isdigit(static_cast<unsigned char>(c))) {
                    throw std::runtime_error("market: bad price " +
                                             parts[i + 1] + " for " +
                                             parts[i]);
                }
            }
            entry.price = std::stoi(parts[i + 1]);
            entry.genotype = parts[i + 2];
            if (entry.genotype.size() < 4 ||
                entry.genotype.compare(entry.genotype.size() - 4, 4, ".gen") !=
                    0) {
                throw std::runtime_error("market: bad genotype " +
                                         entry.genotype);
            }
            market.entries.push_back(std::move(entry));
        }
        market.stores.push_back(store);
    }
    if (market.entries.empty()) {
        throw std::runtime_error("market: no [DINOSTORE] entries in " + path);
    }
    return market;
}

}  // namespace dino
