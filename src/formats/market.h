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
// `CampaignMission::values` (`DINOSTORE.DINOSTORE<N>`, uppercase). The block
// is the mission's own `[DINOSTORE]` or the one inherited from
// `MissionBase.ini`; many missions (Cpg01/Cpg02 and several later ones)
// intentionally have all-empty pages. Empty values are kept as empty pages and
// yield an empty catalog instead of an error.
Market market_from_values(const std::map<std::string, std::string>& values);

}  // namespace dino
