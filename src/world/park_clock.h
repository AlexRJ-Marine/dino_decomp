// Park clock: the retail `spScheduleManager` time base.
//
// Started life inside the app shell. It is pure simulation state (no SDL, no
// GL), so it belongs in `world`: the HUD, the campaign objectives and the
// daily settlement all read the same clock.
//
// Mirrors spScheduleManager's time manager (0x0058c1c0 / 0x0058c2f0 /
// 0x0058c350). A day is 12 hours; the 11th hour ends it. The constructor
// pushes 9 (09:00) and GeneralParams `+0x1a0` is **40.0** at runtime
// (`FUN_0058c1c0(40, 9)`; the old 60.0 came from the 4x4 matrix start at
// `+0x1a8`). See `notes/show-budget-runtime-probe.md`.

#ifndef DINO_WORLD_PARK_CLOCK_H
#define DINO_WORLD_PARK_CLOCK_H

#include <cstdio>
#include <string>

struct ParkClock {
    int base_hour = 9;  // hardcoded PUSH 9 in spScheduleManager ctor
    int day = 0;
    double elapsed = 0.0;
    // GeneralParams.bgd `+0x1a0` = 40.0 at runtime (Cpg04 probe,
    // `FUN_0058c1c0(40, 9)`); the 60.0 in older docs was `+0x1a8`, the
    // matrix start. `--park-time-scale` / the GameSpeed slider override it.
    double time_scale = 40.0;
    bool new_hour = false;
    bool end_of_day = false;

    double hours_per_second() const { return time_scale / 3600.0; }
    double minutes_per_second() const { return time_scale / 60.0; }

    int hour() const {
        return static_cast<int>(elapsed * hours_per_second()) + base_hour;
    }
    int minute() const {
        return static_cast<int>(elapsed * minutes_per_second()) % 60;
    }
    // Absolute park minute, used by campaign objectives (`delaylimit`).
    int minutes() const { return day * 720 + hour() * 60 + minute(); }

    void advance(double dt) {
        new_hour = false;
        end_of_day = false;
        const double next = elapsed + dt;
        if (static_cast<int>(next * hours_per_second()) >= 11) {
            end_of_day = true;
            return;
        }
        if (static_cast<int>(next * hours_per_second()) !=
            static_cast<int>(elapsed * hours_per_second())) {
            new_hour = true;
        }
        elapsed = next;
    }

    void next_day() {
        ++day;
        new_hour = false;
        end_of_day = false;
        elapsed = 0.0;
    }

    std::string time_text(bool ampm) const {
        int h = hour();
        const int m = minute();
        const char* suffix = "";
        if (ampm) {
            suffix = h < 12 ? "AM" : "PM";
            h %= 12;
            if (h == 0) {
                h = 12;
            }
        }
        char text[16];
        std::snprintf(text, sizeof(text), "%02d:%02d%s", h, m, suffix);
        return text;
    }

    std::string day_text() const { return std::to_string(day + 1); }
};

#endif  // DINO_WORLD_PARK_CLOCK_H
