// Asset lookup and small design-file helpers used by the app shell.
//
// These are the "where is the file / what does this design table say" helpers
// that used to live in main.cpp. They are SDL- and GL-free on purpose: the
// headless report modes and the self-test call them too.

#pragma once

#include <string>
#include <vector>

#include "../formats/bvi.h"
#include "../formats/image.h"

namespace dino::app {

// One `--show NAME[:I,J]` test hook. The retail `.shw` "Judge Kind" is the
// show-type id read by `FUN_00567190` (1 race, 2 obstacle, 3 fight,
// 4 destruction, 5 team, 6 capture, 7 fence). `participants` are indices into
// the imported-dino vector.
struct PortShowSpec {
    std::string name;
    int type = 0;
    std::vector<int> participants;
};

// `NAME:V1,V2` test-hook argument used by --place-show / --show-participants
// / --show-hour / --show-iterations.
struct PortNameSpec {
    std::string name;
    std::vector<int> values;
};

// Join the retail asset root with a `Data/...` relative path.
std::string resolve_asset(const std::string& root, const std::string& path);

// Optional texture load shared by the standalone and park build paths. A
// missing texture is reported on stderr; `image` is left untouched.
void load_asset_tga(const std::string& root, const char* relative,
                    dino::Image& image, const char* what);

// Action -> animation clip. The retail link is still open (the `.bai` files
// carry no clip name); these are the name-based candidates documented in
// `notes/dino-action-chooser-recon.md` and are marked inferred, not verified.
std::string dino_clip_for_action(const std::string& action);

std::vector<PortShowSpec> parse_show_specs(
    const std::vector<std::string>& specs, const std::string& asset_root);

PortNameSpec parse_name_spec(const std::string& spec);

// Calendar label for a show hour slot: 1 -> 10:00 AM, 10 -> 07:00 PM.
// The retail day starts at 9:00 (spScheduleManager ctor), and slot 0 is the
// 9:00 hour, which has no show (the show window starts one hour later).
std::string show_slot_text(int slot);

// Free-game easy profile table (`FreeGame.ini` `[PROFILES]`):
// retraite, surfer, parent, enfant, holligan, scientific - 30 each.
std::vector<dino::VisitorProfile> load_easy_visitor_profiles(
    const std::string& asset_root);

// Full 10-profile `.bvi` table, indexed by the global personality id
// (`VisitorList.ini` order 0..9). Used for the per-visitor ticket scale of
// saved visitors, whose `drsVisitor.field_20` is this global id.
std::vector<dino::VisitorProfile> load_all_visitor_profiles(
    const std::string& asset_root);

// `Data/Design/landscape/IslandList.ini`: count then one folder name per line.
// The original reads this in gdtIslandDataBase (FUN_00506530) and builds
// `Data\Design\Landscape\<name>\<name>.lnd` from the selected record.
std::vector<std::string> read_island_list(const std::string& path);

}  // namespace dino::app
