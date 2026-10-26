#pragma once

#include <string>
#include <vector>

namespace dino {

// One `Market.ini` dinosaur-store entry (a triplet in a DinoStoreN value).
struct MarketEntry {
    int store = 0;
    std::string name;     // underscores for spaces, e.g. Tyrannosaurus_Rex
    int price = 0;
    std::string genotype;  // .gen file under Scenario/Genotype
};

struct Market {
    std::vector<MarketEntry> entries;  // store order, then file order
    std::vector<int> stores;           // the DinoStoreN indices present
};

// Reads Data/Design/Scenario/Market.ini. The file includes MissionBase.ini,
// but all [DINOSTORE] keys live in Market.ini itself, so includes are ignored.
Market load_market(const std::string& path);

}  // namespace dino
