#pragma once

#include <map>
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

// Builds the catalog from flattened campaign-mission values as produced by
// `CampaignMission::values` (`DINOSTORE.DINOSTORE<N>`, uppercase). Campaign
// missions carry their own `[DINOSTORE]` block, so each mission lists only the
// dinosaurs available in that mission. Empty store values (Cpg04_Mission4) are
// kept as empty pages and yield an empty catalog instead of an error.
Market market_from_values(const std::map<std::string, std::string>& values);

}  // namespace dino
