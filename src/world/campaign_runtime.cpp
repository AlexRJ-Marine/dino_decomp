#include "campaign_runtime.h"

namespace dino {

void CampaignRuntime::start(const CampaignMission& mission, int clock_minutes) {
    mission_ = mission;
    load_minutes_ = clock_minutes;
    verdicts_.assign(mission_.objectives.size(), ObjectiveVerdict::Pending);
    timer_ = 1.0;  // evaluate on the first tick
    active_ = true;
    finished_ = false;
    overall_ = ObjectiveVerdict::Pending;
}

void CampaignRuntime::tick(double dt, CampaignState state) {
    if (!active_ || finished_) {
        return;
    }
    timer_ += dt;
    if (timer_ < 1.0) {
        return;
    }
    timer_ = 0.0;
    state.load_minutes = load_minutes_;
    verdicts_ = evaluate_mission(mission_, state);
    overall_ = aggregate_verdict(verdicts_);
    if (overall_ == ObjectiveVerdict::Success ||
        overall_ == ObjectiveVerdict::Failure) {
        finished_ = true;
    }
}

}  // namespace dino
