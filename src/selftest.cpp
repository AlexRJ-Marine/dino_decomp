// Self-test implementation (see selftest.h). Moved verbatim out of
// src/app/main.cpp during the app-shell split; it must stay free of SDL and
// OpenGL so `dino_tools --selftest` runs headless.

#include "selftest.h"

#include "formats/campaign.h"
#include "formats/dino_data.h"
#include "formats/image.h"
#include "formats/mesh.h"
#include "world/build.h"
#include "world/campaign_objectives.h"
#include "world/campaign_runtime.h"
#include "world/danger_grid.h"
#include "world/dino_motion.h"
#include "world/show_runtime.h"
#include "world/terrain.h"
#include "world/visitors.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

int run_selftest() {
    int checks = 0;
    int failures = 0;
    auto check = [&](bool ok, const char* what) {
        ++checks;
        if (!ok) {
            std::printf("selftest: FAIL %s\n", what);
            ++failures;
        }
    };

    dino::BuildGrid grid;
    grid.init(4.0f, 1024.0f);
    check(grid.width == 256 && grid.height == 256, "grid size");
    check(grid.origin_x == -512.0f && grid.origin_y == -512.0f, "grid origin");
    check(!grid.path_at(10, 10), "grid empty");

    grid.set_path(10, 10, true);
    check(grid.path_count == 1 && grid.path_at(10, 10), "path set");
    grid.set_path(10, 10, true);
    check(grid.path_count == 1, "path idempotent");
    grid.set_fence(10, 10, 0, true);
    check(grid.fence_count == 1 && grid.fence_at(10, 10, 0), "fence south");
    grid.set_fence(10, 10, 1, true);
    check(grid.fence_count == 2 && grid.fence_at(10, 10, 1), "fence east");
    grid.set_fence(10, 10, 0, false);
    check(grid.fence_count == 1 && !grid.fence_at(10, 10, 0), "fence clear");
    check(grid.undo() && grid.fence_count == 2 && grid.fence_at(10, 10, 0),
          "undo restores south fence");
    check(grid.undo() && grid.fence_count == 1, "undo restores east fence");

    int cx = 0;
    int cy = 0;
    check(grid.cells_for_world(-512.0f + 10.5f * 4.0f,
                               -512.0f + 10.5f * 4.0f, cx, cy) &&
              cx == 10 && cy == 10, "world to cell");

    // Road cursor snap (`0x5faaa0` nearest anchor, half-down). The floored
    // cell is off by one for the upper half of a cell, which moves the
    // axis-lock flip at 45 degrees.
    check(grid.road_cells_for_world(-512.0f + 305.0f, -512.0f + 200.0f,
                                    cx, cy) && cx == 76 && cy == 50,
          "road snap 305 -> 76");
    check(grid.road_cells_for_world(-512.0f + 306.0f, -512.0f + 200.0f,
                                    cx, cy) && cx == 76,
          "road snap half-cell keeps lower");
    check(grid.road_cells_for_world(-512.0f + 307.0f, -512.0f + 200.0f,
                                    cx, cy) && cx == 77,
          "road snap 307 -> 77");
    check(grid.cells_for_world(-512.0f + 307.0f, -512.0f + 200.0f, cx, cy) &&
              cx == 76,
          "floored tool cell stays 76");
    check(dino::road_segment(75, 50, 76, 51).x1 == 75 &&
              dino::road_segment(75, 50, 76, 51).y1 == 51,
          "road 1:1 flip is vertical");
    check(dino::road_segment(75, 50, 77, 51).y1 == 50,
          "road 2:1 flips horizontal");

    // Retail road tool (notes/road-construction-recon.md).
    {
        const dino::RoadSegment h = dino::road_segment(20, 20, 26, 22);
        check(h.x1 == 26 && h.y1 == 20 && h.cells() == 7, "road axis lock x");
        const dino::RoadSegment v = dino::road_segment(20, 20, 22, 22);
        check(v.x1 == 20 && v.y1 == 22, "road axis tie goes vertical");
        check(dino::road_cost(h, 10) == 60, "road cost 10 per extra cell");
        check(dino::road_cost(dino::road_segment(26, 20, 20, 20), 10) == 60,
              "road cost toward -x");
        dino::BuildGrid roads;
        roads.init(4.0f, 1024.0f);
        const std::vector<dino::WorldRect> pens = {{100, 100, 140, 140}};
        auto none = [](int, int) { return false; };
        check(dino::check_road(roads, dino::road_segment(5, 5, 5, 5), {}, none) ==
                  dino::RoadCheck::TooShort,
              "road too short");
        check(dino::check_road(roads, dino::road_segment(27, 27, 30, 27), pens,
                               none) == dino::RoadCheck::InsidePerimeter,
              "road inside perimeter");
        check(dino::check_road_start(roads, 30, 30, pens, none) ==
                  dino::RoadCheck::InsidePerimeter,
              "road start inside perimeter");
        auto blocked = [](int x, int y) { return x == 42 && y == 40; };
        check(dino::check_road(roads, dino::road_segment(40, 40, 45, 40), {},
                               blocked) == dino::RoadCheck::BusySpace,
              "road busy cell");
        roads.set_fence(42, 41, 0, true);  // edge between (42,40) and (42,41)
        check(dino::check_road(roads, dino::road_segment(42, 38, 42, 44), {},
                               none) == dino::RoadCheck::BusySpace,
              "road crosses fence");
        const dino::RoadSegment ok = dino::road_segment(60, 60, 60, 64);
        check(dino::check_road(roads, ok, pens, none) == dino::RoadCheck::Ok &&
                  dino::place_road(roads, ok) == 5 && roads.path_count == 5,
              "road placed");
        auto everything_busy = [](int, int) { return true; };
        check(dino::check_road(roads, dino::road_segment(60, 60, 60, 64), {},
                               everything_busy) == dino::RoadCheck::Ok,
              "road may run over roads");

        // spRoadNetwork occupancy (0x5849a0): u16 per map unit, high nibble
        // type; roads may overlap roads, every other occupant blocks.
        {
            dino::OccupancyGrid occ;
            occ.init(8, 8);
            check(occ.road_free(0, 0, 7, 7), "occupancy empty");
            occ.stamp(2, 2, 5, 4, dino::OccupancyGrid::kRoad);
            check(occ.road_free(0, 0, 7, 7), "road over road ok");
            check(!occ.free_rect(2, 2, 2, 2), "road cell occupied");
            occ.stamp(3, 3, 3, 3, dino::OccupancyGrid::kWall);
            check(!occ.road_free(2, 2, 5, 4), "wall blocks road");
            occ.clear(3, 3, 3, 3, dino::OccupancyGrid::kWall);
            check(occ.road_free(2, 2, 5, 4), "wall clear restores road");
            const dino::WorldRect units =
                dino::road_units(dino::road_segment(3, 4, 7, 4), 4);
            check(units.x0 == 12 && units.y0 == 16 && units.x1 == 31 &&
                      units.y1 == 19,
                  "road units");
        }

        // Road layer 0x602380: cells (10,10)+(11,10) = units x 40..47,
        // y 40..43 -> top-down rows 980..983.
        dino::BuildGrid painted;
        painted.init(4.0f, 1024.0f);
        painted.set_path(10, 10, true);
        painted.set_path(11, 10, true);
        dino::Image map;
        map.width = map.height = 1024;
        map.rgba.assign(1024u * 1024u * 4u, 0);
        dino::paint_roads(map, painted, 1024.0f);
        auto rgb = [&map](int x, int y) {
            const std::uint8_t* p = &map.rgba[(static_cast<std::size_t>(y) * 1024 + x) * 4];
            return std::array<int, 3>{p[0], p[1], p[2]};
        };
        const std::array<int, 3> fill = {253, 198, 137};
        const std::array<int, 3> edge = {170, 130, 90};
        check(rgb(41, 981) == fill && rgb(43, 981) == fill && rgb(44, 981) == fill,
              "road paint fill joins cells");
        check(rgb(40, 981) == edge && rgb(47, 981) == edge && rgb(41, 980) == edge &&
                  rgb(41, 983) == edge,
              "road paint border on open sides");
        check(rgb(41, 979) == std::array<int, 3>{0, 0, 0} &&
                  rgb(41, 41) == std::array<int, 3>{0, 0, 0},
              "road paint y flipped");
        dino::Image small;
        small.width = small.height = 512;
        small.rgba.assign(512u * 512u * 4u, 0);
        dino::paint_roads(small, painted, 1024.0f);
        const std::uint8_t* sp = &small.rgba[(static_cast<std::size_t>(490) * 512 + 20) * 4];
        check(sp[0] == 253 && sp[1] == 198 && sp[2] == 137, "road paint small map fill only");

        // spRoadNetwork (notes/road-network-recon.md).
        dino::BuildGrid net;
        net.init(4.0f, 1024.0f);
        dino::add_road(net, {100, 100, 100, 140});
        dino::add_road(net, {100, 144, 100, 160});
        check(net.roads.size() == 1 && net.roads[0].y0 == 100 &&
                  net.roads[0].y1 == 160,
              "road network merges touching collinear roads (0x58d890)");
        dino::add_road(net, {104, 120, 140, 120});
        check(net.roads.size() == 2 && net.roads[0].x0 == 100 &&
                  net.roads[1].x0 == 100 && net.roads[1].y0 == 120 &&
                  net.roads[1].x1 == 140,
              "road network snaps a T-junction onto the side road (0x58d9d0)");
        dino::add_road(net, {120, 100, 120, 116});
        check(net.roads.size() == 3 && net.roads[2].x0 == 120 &&
                  net.roads[2].y1 == 120,
              "road network extends a road that stops short of another");
        dino::BuildGrid one;
        one.init(4.0f, 1024.0f);
        dino::add_road(one, {40, 40, 40, 44});
        dino::Image rect_map;
        rect_map.width = rect_map.height = 1024;
        rect_map.rgba.assign(1024u * 1024u * 4u, 0);
        dino::paint_roads(rect_map, one, 1024.0f);
        auto texel = [&](int x, int row) {
            return &rect_map.rgba[(static_cast<std::size_t>(row) * 1024 + x) * 4];
        };
        // (40,40)-(40,44) covers x 40..43, rows 1023-47..1023-40 = 976..983.
        check(texel(40, 980)[0] == 170 && texel(43, 980)[0] == 170 &&
                  texel(41, 976)[0] == 170 && texel(41, 983)[0] == 170,
              "road segment painted with a full 1-texel border (0x602380)");
        check(texel(41, 977)[0] == 253 && texel(42, 982)[0] == 253 &&
                  texel(44, 980)[3] == 0 && texel(41, 975)[3] == 0,
              "road segment fill is inset and ends at x1+3 / y1+3");
        // Crossing: vertical x 100..103 and horizontal y 120..123. The
        // vertical road's border column x=100 inside the horizontal road's
        // fill rows (y 121..122 -> rows 901..902) stays sand (fill z 0.996
        // beats border z 0.997 under ShapeColor2D LessEqual).
        dino::BuildGrid cross;
        cross.init(4.0f, 1024.0f);
        dino::add_road(cross, {80, 120, 140, 120});
        dino::add_road(cross, {100, 100, 100, 140});
        std::fill(rect_map.rgba.begin(), rect_map.rgba.end(), 0);
        dino::paint_roads(rect_map, cross, 1024.0f);
        check(texel(100, 901)[0] == 253 && texel(103, 902)[0] == 253 &&
                  texel(101, 900)[0] == 253 && texel(99, 901)[0] == 253,
              "road crossing joins without border lines (fill over border)");
        check(texel(100, 890)[0] == 170 && texel(90, 900)[0] == 170,
              "road crossing keeps the outer borders");
    }

    dino::Paddock paddock;
    paddock.rect = {0, 0, 31, 31};
    paddock.capacity = dino::paddock_capacity(paddock.rect, 4.0f);
    check(paddock.capacity == 64.0f, "paddock capacity");
    check(dino::paddock_cost(0) == 8.0f && dino::paddock_cost(1) == 16.0f &&
              dino::paddock_cost(3) == 64.0f, "paddock cost table");
    check(dino::paddock_size_class(200) == 0 &&
              dino::paddock_size_class(620) == 1 &&
              dino::paddock_size_class(900) == 2 &&
              dino::paddock_size_class(901) == 3, "paddock size class");
    check(dino::paddock_add(paddock, 1, 0) && paddock.used == 8.0f,
          "paddock add");
    check(!dino::paddock_add(paddock, 1, 0), "paddock duplicate");
    for (int id = 2; id <= 8; ++id) {
        dino::paddock_add(paddock, id, 0);
    }
    check(paddock.used == 64.0f && paddock.fill() == 1.0f, "paddock full");
    check(!dino::paddock_add(paddock, 9, 0), "paddock no room");
    // stFence::GetSellValue: the build-870 enclosure (30x30 units) sells for
    // 630 because the value uses the 4-unit-inset perimeter (live retail).
    check(dino::fence_sell_value({540, 460, 569, 489}, 4.0f, 30, 4) == 630,
          "fence sell value 870 -> 630");
    check(dino::fence_sell_value({540, 460, 569, 489}, 4.0f, 30, 4, 0.5f) ==
              315,
          "fence sellback 0.5");

    // `saDinosaur+0x2c8` mover: walk speed 2 m/s, 45-degree turn steps,
    // exact arrival and the paddock clamp (`FUN_0055af70`).
    dino::DinoMover mover(0.0f, 0.0f, 0.0f, 2.0f, 5.0f);
    mover.start(1, 10.0f, 0.0f, 1);
    mover.tick(1.0);
    check(std::fabs(mover.x() - 2.0f) < 0.001f && mover.y() == 0.0f,
          "dino mover walk 2 m/s");
    mover.tick(4.0);
    check(!mover.active() && std::fabs(mover.x() - 10.0f) < 0.001f,
          "dino mover arrives");
    dino::DinoMover turn(0.0f, 0.0f, 0.0f, 2.0f, 5.0f);
    turn.start(1, 0.0f, 10.0f, 1);
    turn.tick(1.0);
    check(turn.state() == 2 && std::fabs(turn.angle_deg() - 45.0f) < 0.001f,
          "dino mover turn step 45");
    turn.tick(1.0);
    check(turn.state() == 1 && std::fabs(turn.angle_deg() - 90.0f) < 0.001f,
          "dino mover turn 90");
    turn.tick(1.0);
    check(std::fabs(turn.x()) < 0.001f && std::fabs(turn.y() - 2.0f) < 0.001f,
          "dino mover walks after turn");
    dino::DinoMover u_turn(0.0f, 0.0f, 0.0f, 2.0f, 5.0f);
    u_turn.start(1, -10.0f, 0.0f, 1);
    u_turn.tick(1.0);
    check(std::fabs(u_turn.angle_deg() - 180.0f) < 0.001f &&
              u_turn.state() == 1,
          "dino mover 180-degree record");
    // Per-family Actions.bad durations: LightBiped kind 3/4 = 1.2 s,
    // kind 5 = 1.5 s.
    dino::DinoMover species_turn(0.0f, 0.0f, 0.0f, 2.0f, 5.0f);
    species_turn.set_turn_durations(1.2f, 1.5f);
    species_turn.start(1, 0.0f, 10.0f, 1);
    species_turn.tick(1.2f);
    check(species_turn.state() == 2 &&
              std::fabs(species_turn.angle_deg() - 45.0f) < 0.001f,
          "dino mover species turn 1.2 s");
    species_turn.tick(1.2f);
    check(species_turn.state() == 1 &&
              std::fabs(species_turn.angle_deg() - 90.0f) < 0.001f,
          "dino mover species turn 90");
    dino::DinoMover species_u(0.0f, 0.0f, 0.0f, 2.0f, 5.0f);
    species_u.set_turn_durations(1.2f, 1.5f);
    species_u.start(1, -10.0f, 0.0f, 1);
    species_u.tick(1.5f);
    check(species_u.state() == 1 &&
              std::fabs(species_u.angle_deg() - 180.0f) < 0.001f,
          "dino mover species 180 at 1.5 s");
    dino::DinoMover bounded(2.0f, 2.0f, 0.0f, 2.0f, 5.0f);
    bounded.set_bounds(0.0f, 0.0f, 5.0f, 5.0f, 1.0f);
    bounded.start(1, 10.0f, 2.0f, 1);
    bounded.tick(2.0);
    check(bounded.blocked() && std::fabs(bounded.x() - 2.0f) < 0.001f,
          "dino mover paddock clamp");
    // Gesture kinds do not translate; a mode-2 self record waits its full
    // duration (Sleep 0x1d/0x1e, Victory 0x27/0x20).
    dino::DinoMover gesture(0.0f, 0.0f, 0.0f, 2.0f, 5.0f);
    gesture.start(0x1d, 0.0f, 0.0f, 2, 1.5f);
    gesture.tick(1.0);
    check(gesture.active() && std::fabs(gesture.x()) < 0.001f,
          "dino gesture waits");
    gesture.tick(0.6);
    check(!gesture.active() && std::fabs(gesture.x()) < 0.001f,
          "dino gesture completes at its duration");
    dino::DinoMover marker(0.0f, 0.0f, 0.0f, 2.0f, 5.0f);
    marker.start(0x34, 10.0f, 0.0f, 1);
    marker.tick(1.0);
    check(std::fabs(marker.x() - 2.0f) < 0.001f,
          "dino 0x34 marker walks");

    // gscMaterialBank pulse (`FUN_00600550`) worked values from
    // notes/fence-selected-state-recon.md §1.4. Comparisons are in 8-bit
    // units: the lamp colour *255, the selected-wall factor *255 (= 2*grey).
    auto near8 = [](float value, int expected) {
        return std::abs(value * 255.0f - static_cast<float>(expected)) < 1.0f;
    };
    float pulse_lamp[3] = {};
    float pulse_selected[3] = {};
    dino::fence_pulse_colors(0.0, pulse_lamp, pulse_selected);
    check(near8(pulse_lamp[0], 53) && near8(pulse_lamp[1], 53) &&
              near8(pulse_lamp[2], 170) && near8(pulse_selected[0], 382),
          "fence pulse t=0");
    dino::fence_pulse_colors(0.25, pulse_lamp, pulse_selected);
    check(near8(pulse_lamp[0], 31) && near8(pulse_lamp[1], 31) &&
              near8(pulse_lamp[2], 99) && near8(pulse_selected[0], 458),
          "fence pulse t=0.25");
    dino::fence_pulse_colors(0.5, pulse_lamp, pulse_selected);
    check(near8(pulse_lamp[0], 29) && near8(pulse_lamp[1], 29) &&
              near8(pulse_lamp[2], 93) && near8(pulse_selected[0], 260),
          "fence pulse t=0.5");

    dino::Terrain basis_terrain;
    basis_terrain.width = 16;
    basis_terrain.height = 16;
    basis_terrain.world_size = 1024.0f;
    basis_terrain.height_scale = 1.0f;
    basis_terrain.heights.assign(16 * 16, 0);
    const dino::TerrainBasis flat_basis =
        dino::slope_basis(basis_terrain, 0.0f, 0.0f, 0.0f);
    check(flat_basis.normal.z > 0.999f && flat_basis.right.x > 0.999f &&
              flat_basis.up.y > 0.999f, "slope basis flat");
    dino::Terrain sloped = basis_terrain;
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            sloped.heights[static_cast<std::size_t>(y) * 16 + x] =
                static_cast<std::uint8_t>(x * 8);
        }
    }
    const dino::TerrainBasis sloped_basis =
        dino::slope_basis(sloped, 0.0f, 0.0f, 0.0f);
    check(sloped_basis.normal.x < -0.05f &&
              sloped_basis.normal.z > 0.9f, "slope basis tilted");

    dino::Terrain terrain;
    terrain.width = 16;
    terrain.height = 16;
    terrain.world_size = 1024.0f;
    terrain.height_scale = 1.0f;
    terrain.heights.assign(16 * 16, 0);
    terrain.occupation_width = 1024;
    terrain.occupation_height = 1024;
    terrain.occupation.assign(1024 * 1024, 255);
    terrain.occupation[512 * 1024 + 512] = 0;  // world (0, 0)
    terrain.occupation[100 * 1024 + 100] = 0;  // world (-412, -412)
    check(terrain.buildable(0.0f, 0.0f) &&
              terrain.buildable(-412.0f, -412.0f) &&
              !terrain.buildable(10.0f, 0.0f) &&
              !terrain.buildable(-1000.0f, 0.0f),
          "occupation mask");

    for (int i = 100; i <= 110; ++i) {
        grid.set_path(i, 100, true);
        grid.set_path(i, 110, true);
        grid.set_path(100, i, true);
        grid.set_path(110, i, true);
    }
    check(grid.path_count == 41, "loop cells (40 + one earlier)");

    dino::VisitorSystem visitors;
    check(visitors.spawn(grid, 12) == 12, "visitor spawn count");
    const float start_x = visitors.visitors.empty() ? 0.0f : visitors.visitors[0].x;
    const float start_y = visitors.visitors.empty() ? 0.0f : visitors.visitors[0].y;
    for (int i = 0; i < 900; ++i) {
        visitors.update(grid, terrain, 1.0f / 60.0f);
    }
    bool moved = false;
    bool on_path = true;
    for (const auto& visitor : visitors.visitors) {
        if (std::fabs(visitor.x - start_x) > 0.1f ||
            std::fabs(visitor.y - start_y) > 0.1f) {
            moved = true;
        }
        if (!grid.path_at(visitor.cell_x, visitor.cell_y)) {
            on_path = false;
        }
    }
    check(moved, "visitors moved");
    check(on_path, "visitors stay on path cells");

    // SIM-2b: exact retail score and the choose -> travel -> use loop.
    dino::VisitorProfile ai_profile;
    ai_profile.weights = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    ai_profile.walk_speed = 10.0f;
    dino::Attraction ai_attraction;
    ai_attraction.world_x = -512.0f + 105.5f * 4.0f;
    ai_attraction.world_y = -512.0f + 100.5f * 4.0f;
    ai_attraction.radius = 1000.0f;
    ai_attraction.appeal_scale = 1.0f;
    ai_attraction.appeal_weights = {10.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    ai_attraction.need_delta1 = 10;
    ai_attraction.duration = 0.1f;
    ai_attraction.category = 1;
    check(dino::attraction_score(ai_attraction, ai_profile, 99, 50, 99, 10,
                                 0) == 21010,
          "attraction score");
    dino::BuildGrid ai_grid;
    ai_grid.init(4.0f, 1024.0f);
    for (int x = 100; x <= 110; ++x) {
        ai_grid.set_path(x, 100, true);
    }
    dino::VisitorSystem ai_visitors;
    check(ai_visitors.spawn(ai_grid, 1, {ai_profile}, -1, -1, 7) == 1,
          "ai visitor spawn");
    dino::Visitor& ai_visitor = ai_visitors.visitors[0];
    ai_visitor.cell_x = 100;
    ai_visitor.cell_y = 100;
    ai_visitor.target_x = 100;
    ai_visitor.target_y = 100;
    ai_visitor.x = -512.0f + 100.5f * 4.0f;
    ai_visitor.y = -512.0f + 100.5f * 4.0f;
    ai_visitor.need0 = 50;
    ai_visitor.need1 = 50;
    ai_visitor.need2 = 99;
    ai_visitor.state = dino::VisitorState::Choose;
    ai_visitor.rng = 1;
    for (int i = 0; i < 30; ++i) {
        ai_visitors.update(ai_grid, terrain, 0.1f, {ai_attraction}, -1, -1,
                           {ai_profile});
    }
    check(!ai_visitors.visitors.empty(), "ai visitor survives");
    check(ai_visitors.visitors[0].need1 > 50, "ai need1 gained");
    check(ai_visitors.visitors[0].need0 <= 50, "ai need0 adjusted");

    const dino::Mesh empty_pose;
    check(visitors.build_mesh(grid, terrain, empty_pose, empty_pose)
              .positions.empty(), "empty pose builds empty mesh");

    dino::Mesh fence_source;
    fence_source.positions = {{0, 0, 0}, {0, 1, 0}, {0, 1, 1}, {0, 0, 1}};
    fence_source.indices = {0, 1, 2, 0, 2, 3};
    grid.set_fence(10, 10, 0, true);
    const dino::BuildMeshes meshes =
        dino::build_placement_meshes(grid, terrain, fence_source);
    check(meshes.path_tiles == grid.path_count, "path mesh tiles");
    check(meshes.fence_segments == grid.fence_count, "fence mesh segments");

    // SECU danger grid (`FUN_005774F0` / `FUN_00571B50` / `FUN_00571440`).
    dino::DangerGrid danger;
    danger.clear();
    danger.stamp(100.0f, 100.0f, 4, 50);
    check(danger.at(100.0f, 100.0f) == 50, "danger grid stamp");
    check(danger.at(108.0f, 100.0f) == 50, "danger grid radius");
    check(danger.at(112.0f, 100.0f) == 0, "danger grid edge");
    check(dino::secu_from_grid(danger, 100.0f, 100.0f, 50, 100, 100) == 99,
          "secu full");
    check(dino::secu_from_grid(danger, 100.0f, 100.0f, 0, 100, 100) == 94,
          "secu threat");
    check(dino::secu_from_grid(danger, 100.0f, 100.0f, 0, 50, 100) == 44,
          "secu health");
    check(dino::secu_from_grid(danger, 100.0f, 100.0f, 0, 25, 100) == 19,
          "secu damage");
    check(dino::secu_from_grid(danger, 100.0f, 100.0f, 0, 0, 100) == 0,
          "secu clamp");
    check(dino::secu_from_grid(danger, 100.0f, 100.0f, 0, 0, 0) == 94,
          "secu no max health");

    // Campaign objective evaluator (`snoObjective` 0x5654f0 / verb 0x5658d0).
    auto objective = [](const char* command) {
        return dino::parse_objective_command(command);
    };
    dino::CampaignState objective_state;
    objective_state.treasury = 25000;
    check(dino::evaluate_objective(objective("treasury 25000 1"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective treasury success");
    objective_state.treasury = 100;
    check(dino::evaluate_objective(objective("treasury 25000 1"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Pending,
          "objective treasury pending");
    check(dino::evaluate_objective(objective("treasury 25000 0"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Failure,
          "objective treasury failure");
    objective_state.merchandise_income = 50001;
    check(dino::evaluate_objective(objective("marchandizing 50000"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective marchandizing");
    objective_state.deaths = 3;
    check(dino::evaluate_objective(objective("dyingdinos 5"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective dyingdinos success");
    objective_state.deaths = 6;
    check(dino::evaluate_objective(objective("dyingdinos 5"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Failure,
          "objective dyingdinos failure");

    dino::CampaignVisitorState kid;
    kid.profile = "kid";
    kid.satisfaction = 80;
    dino::CampaignVisitorState adult;
    adult.profile = "adult";
    adult.satisfaction = 90;
    objective_state.visitors = {kid, adult};
    check(dino::evaluate_objective(objective("visitors all 2"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective visitors all");
    check(dino::evaluate_objective(objective("visitors kid 1"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective visitors profile");
    check(dino::evaluate_objective(objective("visitors all 3"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Pending,
          "objective visitors pending");
    check(dino::evaluate_objective(objective("happyvisitors all 80"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective happy visitors");
    objective_state.employee_roles = {"nutritionist", "nutritionist"};
    check(dino::evaluate_objective(objective("employees all 2"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective employees");
    objective_state.camera_altitude = 120;
    check(dino::evaluate_objective(objective("cameraaltitude 120"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective camera altitude");
    objective_state.clock_minutes = 5;
    objective_state.load_minutes = 0;
    check(dino::evaluate_objective(objective("delaylimit 10"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Pending,
          "objective delay limit pending");
    objective_state.clock_minutes = 11;
    check(dino::evaluate_objective(objective("delaylimit 10"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Failure,
          "objective delay limit failure");
    objective_state.store_empty = true;
    check(dino::evaluate_objective(objective("buyalldinos"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective buy all dinos");
    objective_state.bankrupt = true;
    check(dino::evaluate_objective(objective("bankrupt"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Failure,
          "objective bankrupt");

    dino::DinoTraitState trait;
    trait.species = "Hypsilophodon";
    trait.cuteness = 60;
    trait.aggression = 90;
    trait.hybrid = 95;
    trait.speed = 30;
    trait.stamina = 70;
    trait.agility = 50;
    trait.intelligence = 80;
    trait.size_class = 2;
    trait.food = 70;
    trait.beauty = 80;
    trait.strangeness = 10;
    trait.ornament_count = 1;
    trait.isolated = true;
    trait.carnivore = true;
    trait.biped = true;
    trait.age = 10;
    dino::DinoTraitState hungry = trait;
    hungry.food = 50;
    hungry.aggression = 0;
    hungry.ornament_count = 0;
    hungry.hybrid = 0;
    objective_state.dinos = {trait, hungry};
    check(dino::evaluate_objective(objective("aggressivedinos 1 80"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective aggressive dinos");
    check(dino::evaluate_objective(objective("aggressivedinos 2 80"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Pending,
          "objective aggressive count");
    check(dino::evaluate_objective(objective("cutedinos 1 50"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective cute dinos");
    check(dino::evaluate_objective(objective("hybriddinos 1 90"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective hybrid dinos");
    check(dino::evaluate_objective(objective("hybriddinos 2 90"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Pending,
          "objective hybrid count");
    check(dino::evaluate_objective(objective("speeddinos 1 25"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective speed dinos");
    check(dino::evaluate_objective(objective("staminadinos 1 60"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective stamina dinos");
    check(dino::evaluate_objective(objective("cleverdinos 1 70"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective clever dinos");
    check(dino::evaluate_objective(objective("hungrydinos 1 60"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective hungry one");
    check(dino::evaluate_objective(objective("hungrydinos -1 60"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Pending,
          "objective hungry all");
    objective_state.buildings = {{8, true}, {11, false}, {6, true}};
    objective_state.fence_count = 2;
    check(dino::evaluate_objective(objective("connectedbuildings farm 1"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective connected farm");
    check(dino::evaluate_objective(
              objective("connectedbuildings lunchdoor 1"), objective_state) ==
              dino::ObjectiveVerdict::Pending,
          "objective connected lunchdoor");
    check(dino::evaluate_objective(
              objective("notconnectedbuildings lunchdoor 1"),
              objective_state) == dino::ObjectiveVerdict::Success,
          "objective notconnected lunchdoor");
    check(dino::evaluate_objective(objective("connectedbuildings buildings 1"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective connected buildings");
    check(dino::evaluate_objective(objective("connectedbuildings buildings 2"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Pending,
          "objective connected buildings count");
    check(dino::evaluate_objective(objective("connectedbuildings show 1"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective connected show");
    check(dino::evaluate_objective(objective("connectedbuildings fence 1"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective connected fence");
    check(dino::evaluate_objective(objective("isolateddinos 71"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective isolated dinos");
    dino::CampaignState crowded = objective_state;
    crowded.dinos[0].isolated = false;
    check(dino::evaluate_objective(objective("isolateddinos 71"), crowded) ==
              dino::ObjectiveVerdict::Pending,
          "objective isolated crowded");
    check(dino::evaluate_objective(objective("sizedagilitydinos 1 40"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective size agility");
    check(dino::evaluate_objective(objective("sizedagilitydinos 3 40"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Pending,
          "objective size class threshold");
    check(dino::evaluate_objective(objective("franckstrangedinos 20 30"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective franck strange");
    check(dino::evaluate_objective(objective("franckstrangedinos 5 30"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Pending,
          "objective franck age");
    check(dino::evaluate_objective(objective("ornateddinos 1"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective ornated dinos");
    check(dino::evaluate_objective(objective("ornateddinos 2"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Pending,
          "objective ornated count");
    check(dino::evaluate_objective(objective("prettystrangedino 70 20"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective pretty strange");
    check(dino::evaluate_objective(objective("genotypedinos hypsilo 1"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective genotype species");

    // `FUN_00618020` five show needs from the retail constants.
    {
        dino::DinoPhenotypeTable needs_table;
        dino::DinoAttributes attrs;
        attrs.beauty = 50;
        attrs.attack_head = 40;
        attrs.aggression = 20;
        attrs.attack_total = 100;
        attrs.length_cm = 1000;
        attrs.width_cm = 500;
        attrs.height_cm = 200;
        attrs.conventional_appeal = 60;
        attrs.intelligence = 50;
        const std::array<float, 5> needs =
            dino::dino_show_needs(attrs, needs_table);
        const auto close_to = [](float value, float expected) {
            return std::fabs(value - expected) < 0.05f;
        };
        check(close_to(needs[0], 30.0f), "show need beauty");
        check(close_to(needs[1], 13.5f), "show need danger");
        check(close_to(needs[2], 20.69f), "show need spectacle");
        check(close_to(needs[3], 30.0f), "show need conventional");
        check(close_to(needs[4], 34.5f), "show need taming");
    }

    // Show objectives (`0x567190`, `0x5678b0`, `0x5692a0`).
    {
        dino::CampaignShowState show;
        show.type = 1;
        show.participants = 3;
        show.add_score(0, 65.0f);
        show.add_score(1, 70.0f);
        show.add_score(2, 55.0f);
        show.participant_ids = {0, 1, 2};
        show.participant_hybrid = {50, 90, 91};
        dino::CampaignState show_state;
        show_state.shows = {show};
        check(dino::evaluate_objective(objective("happyshow one 60"),
                                       show_state) ==
                  dino::ObjectiveVerdict::Success,
              "objective happyshow one");
        check(dino::evaluate_objective(objective("happyshow all 60"),
                                       show_state) ==
                  dino::ObjectiveVerdict::Pending,
              "objective happyshow all");
        check(dino::evaluate_objective(objective("happyshow all 50"),
                                       show_state) ==
                  dino::ObjectiveVerdict::Success,
              "objective happyshow all pass");
        check(dino::evaluate_objective(objective("happyshow holligan 65"),
                                       show_state) ==
                  dino::ObjectiveVerdict::Success,
              "objective happyshow holligan");
        check(dino::evaluate_objective(objective("happyshow hooligan 65"),
                                       show_state) ==
                  dino::ObjectiveVerdict::Success,
              "objective happyshow hooligan alias");
        check(dino::evaluate_objective(objective("happyshow unknown 65"),
                                       show_state) ==
                  dino::ObjectiveVerdict::Success,
              "objective happyshow unknown surfer");
        check(dino::evaluate_objective(objective("happyshow one 71"),
                                       show_state) ==
                  dino::ObjectiveVerdict::Pending,
              "objective happyshow threshold");
        check(dino::evaluate_objective(objective("happydinoshow 50 2"),
                                       show_state) ==
                  dino::ObjectiveVerdict::Success,
              "objective happydinoshow");
        check(dino::evaluate_objective(objective("happydinoshow 60 2"),
                                       show_state) ==
                  dino::ObjectiveVerdict::Pending,
              "objective happydinoshow score");
        check(dino::evaluate_objective(objective("happydinoshow 50 4"),
                                       show_state) ==
                  dino::ObjectiveVerdict::Pending,
              "objective happydinoshow count");
        check(dino::evaluate_objective(objective("fightshow 65"), show_state) ==
                  dino::ObjectiveVerdict::Pending,
              "objective fightshow type");
        dino::CampaignState fight = show_state;
        fight.shows[0].type = 3;
        check(dino::evaluate_objective(objective("fightshow 65"), fight) ==
                  dino::ObjectiveVerdict::Success,
              "objective fightshow");
        dino::CampaignState scientific = show_state;
        scientific.shows[0].add_score(6, 65.0f);
        scientific.shows[0].participant_hybrid = {50, 90};
        check(dino::evaluate_objective(objective("scientificshow 60 90"),
                                       scientific) ==
                  dino::ObjectiveVerdict::Success,
              "objective scientificshow");
        scientific.shows[0].participant_hybrid = {50, 91};
        check(dino::evaluate_objective(objective("scientificshow 60 90"),
                                       scientific) ==
                  dino::ObjectiveVerdict::Pending,
              "objective scientificshow cap");
        scientific.shows[0].participant_hybrid = {50, 90};
        scientific.shows[0].profile_scores.back().score = 60.0f;
        check(dino::evaluate_objective(objective("scientificshow 60 90"),
                                       scientific) ==
                  dino::ObjectiveVerdict::Pending,
              "objective scientificshow strict");
        dino::CampaignState exclusive = show_state;
        exclusive.shows.push_back(show);
        check(dino::evaluate_objective(objective("dinoexclusiveshow"),
                                       exclusive) ==
                  dino::ObjectiveVerdict::Pending,
              "objective exclusive shared");
        dino::CampaignState exclusive_ok = show_state;
        dino::CampaignShowState other;
        other.participants = 1;
        other.participant_ids = {5};
        exclusive_ok.shows.push_back(other);
        check(dino::evaluate_objective(objective("dinoexclusiveshow"),
                                       exclusive_ok) ==
                  dino::ObjectiveVerdict::Success,
              "objective exclusive disjoint");
        dino::CampaignState exclusive_dup = show_state;
        exclusive_dup.shows[0].participant_ids = {0, 0};
        check(dino::evaluate_objective(objective("dinoexclusiveshow"),
                                       exclusive_dup) ==
                  dino::ObjectiveVerdict::Success,
              "objective exclusive same show");
        dino::CampaignState exclusive_empty;
        check(dino::evaluate_objective(objective("dinoexclusiveshow"),
                                       exclusive_empty) ==
                  dino::ObjectiveVerdict::Success,
              "objective exclusive empty");
    }

    // Show runtime (`spScheduleManager` hour slots + show state machine).
    {
        dino::ShowCatalogEntry entry;
        entry.stem = "Cpg4_Derby_1";
        entry.name_id = "&cpg4_derby_1";
        entry.judge_kind = 4;
        entry.width = 71;
        entry.height = 35;

        std::vector<dino::ShowCatalogEntry> catalog = {entry};
        check(dino::find_show_catalog_index(catalog, "Cpg4_Derby_1") == 0,
              "show catalog stem");
        check(dino::find_show_catalog_index(catalog, "cpg4_derby_1") == 0,
              "show catalog name id bare");
        check(dino::find_show_catalog_index(catalog, "&CPG4_DERBY_1") == 0,
              "show catalog name id");
        check(dino::find_show_catalog_index(catalog, "missing") == -1,
              "show catalog miss");
        check(dino::show_catalog_entry_matches(entry, "&cpg4_derby_1"),
              "show catalog matches");

        dino::ShowRuntime runtime;
        const std::vector<int> profile_ids = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        const int first = runtime.place(entry, 10, 20, profile_ids);
        check(first == 0, "show place index");
        check(runtime.count() == 1, "show count");
        const dino::ParkShow* placed = runtime.at(first);
        check(placed != nullptr && placed->slot == 1, "show first free slot");
        check(placed != nullptr && placed->cell_x == 10 &&
                  placed->cell_y == 20,
              "show placement cell");
        check(placed != nullptr && placed->record.profile_scores.size() == 10,
              "show zero-filled score vector");
        check(placed != nullptr && placed->record.profile_scores[0].score ==
                                       0.0f,
              "show zero-filled score value");
        check(placed != nullptr && placed->record.type == 4,
              "show record type");

        const int second = runtime.place(entry, 40, 20, profile_ids);
        check(second == 1 && runtime.at(second)->slot == 2,
              "show second slot");

        check(runtime.set_slot(0, 5), "show set slot");
        check(runtime.at(0)->slot == 5, "show slot value");
        check(!runtime.set_slot(0, 11), "show slot range");
        check(!runtime.set_iterations(0, 0), "show iteration range");
        check(runtime.set_iterations(0, 2), "show set iterations");
        check(runtime.set_participants(0, {3, 4}),
              "show set participants");
        check(runtime.at(0)->record.participants == 2,
              "show participant count");

        int judged = 0;
        int completed = 0;
        auto judge = [&](dino::ParkShow&) { ++judged; };
        runtime.set_duration_minutes(5.0f);
        // Hour 0 (9:00) has no show; hour 5 is the first scheduled one.
        completed = runtime.tick(0, 0, false, 0.0, 60.0, judge);
        check(completed == 0 && runtime.at(0)->state == 0,
              "show not started before slot");
        completed = runtime.tick(5, 0, false, 0.0, 60.0, judge);
        check(completed == 0 && runtime.at(0)->state == 2,
              "show started on slot hour");
        check(judged == 0, "show judge waits for performance");
        check(runtime.at(1)->state == 0, "show other slot idle");
        // 2.5 park minutes at time_scale 60 -> half the 5-minute performance.
        completed = runtime.tick(5, 0, false, 2.5, 60.0, judge);
        check(completed == 0 && judged == 0, "show performance in progress");
        completed = runtime.tick(5, 0, false, 2.5, 60.0, judge);
        check(completed == 1 && judged == 1, "show performance judged");
        check(runtime.at(0)->done == 1 && runtime.at(0)->state == 2,
              "show first of two rounds");
        completed = runtime.tick(5, 0, false, 5.0, 60.0, judge);
        check(completed == 1 && runtime.at(0)->done == 2 &&
                  runtime.at(0)->state == 0,
              "show iterations complete");
        // Next day: the schedule restarts from zero.
        runtime.tick(5, 0, true, 0.0, 60.0, judge);
        check(runtime.at(0)->done == 0 && runtime.at(0)->state == 0,
              "show day reset");
        completed = runtime.tick(5, 1, false, 0.0, 60.0, judge);
        check(completed == 0 && runtime.at(0)->state == 2,
              "show next day start");

        dino::ShowRuntime full;
        for (int i = 0; i < 10; ++i) {
            check(full.place(entry, i, 0, profile_ids) == i,
                  "show fill ten slots");
        }
        check(full.place(entry, 0, 0, profile_ids) == -1,
              "show slots exhausted");
        full.clear();
        check(full.count() == 0, "show clear");

        runtime.set_iterations(0, 1);
        const std::vector<dino::CampaignShowState> states =
            runtime.campaign_states();
        check(states.size() == 2, "show campaign states");
        check(states[0].participants == 2, "show campaign participants");
    }

    check(dino::aggregate_verdict({dino::ObjectiveVerdict::Success,
                                   dino::ObjectiveVerdict::Success}) ==
              dino::ObjectiveVerdict::Success,
          "objective aggregate success");
    check(dino::aggregate_verdict({dino::ObjectiveVerdict::Success,
                                   dino::ObjectiveVerdict::Failure}) ==
              dino::ObjectiveVerdict::Failure,
          "objective aggregate failure");

    if (failures == 0) {
        std::printf("selftest: PASS (%d checks)\n", checks);
    }
    return failures == 0 ? 0 : 1;
}
