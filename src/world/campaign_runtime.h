#pragma once

#include <vector>

#include "../formats/campaign.h"
#include "campaign_objectives.h"

namespace dino {

// Drives one mission: evaluate its objectives once per second against a live
// `CampaignState` and expose the tri-state result.
class CampaignRuntime {
public:
    void start(const CampaignMission& mission, int clock_minutes);
    void tick(double dt, CampaignState state);
    void stop() { active_ = false; }

    bool active() const { return active_; }
    bool finished() const { return finished_; }
    const CampaignMission& mission() const { return mission_; }
    const std::vector<ObjectiveVerdict>& verdicts() const { return verdicts_; }
    ObjectiveVerdict overall() const { return overall_; }
    int load_minutes() const { return load_minutes_; }

private:
    CampaignMission mission_;
    std::vector<ObjectiveVerdict> verdicts_;
    int load_minutes_ = 0;
    double timer_ = 1.0;
    bool active_ = false;
    bool finished_ = false;
    ObjectiveVerdict overall_ = ObjectiveVerdict::Pending;
};

}  // namespace dino
