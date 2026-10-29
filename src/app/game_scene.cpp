// Scene resources and park contents: vegetation, minimap, extra items,
// dinosaur species/import and the paddock queries.

#include "game.h"

namespace dino::app {

void Game::upload_vegetation() {
    std::map<std::string, GLuint> cache;
    veg_gpu.reserve(vegetation.groups.size());
    veg_textures.reserve(vegetation.groups.size());
    for (const auto& group : vegetation.groups) {
        veg_gpu.push_back(upload_mesh(group.mesh, glfns));
        GLuint group_texture = 0;
        const auto cached = cache.find(group.texture_path);
        if (cached != cache.end()) {
            group_texture = cached->second;
        } else if (!group.texture_path.empty() &&
                   std::filesystem::exists(group.texture_path)) {
            try {
                group_texture = upload_texture(dino::load_tga(group.texture_path));
            } catch (const std::exception& e) {
                std::fprintf(stderr, "vegetation texture: %s\n", e.what());
            }
            cache[group.texture_path] = group_texture;
        }
        veg_textures.push_back(group_texture);
    }
    veg_visible.assign(vegetation.groups.size(), 1);
    veg_bounds.clear();
    for (const auto& group : vegetation.groups) {
        Bounds box;
        for (const auto& p : group.mesh.positions) {
            box.add(p);
        }
        const float cx = (box.min.x + box.max.x) * 0.5f;
        const float cy = (box.min.y + box.max.y) * 0.5f;
        const float cz = (box.min.z + box.max.z) * 0.5f;
        const float rx = box.max.x - cx;
        const float ry = box.max.y - cy;
        const float rz = box.max.z - cz;
        veg_bounds.push_back({cx, cy, cz, std::sqrt(rx * rx + ry * ry + rz * rz)});
    }

}

void Game::release_vegetation() {
    for (auto& mesh : veg_gpu) {
        mesh.destroy(glfns);
    }
    std::vector<GLuint> ids = veg_textures;
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    for (const GLuint id : ids) {
        if (id != 0) {
            glDeleteTextures(1, &id);
        }
    }
    veg_gpu.clear();
    veg_textures.clear();
    veg_visible.clear();
    veg_bounds.clear();

}

void Game::minimap_click_zoom(int delta) {
    minimap_scale =
        std::clamp(minimap_scale + 0.05f * static_cast<float>(delta),
                   0.1f, 2.0f);
    std::printf("minimap scale=%.2f\n", minimap_scale);

}

void Game::render_minimap(const dino::Vec3& focus, float heading_rad) {
    if (!minimap_ready || terrain.world_size <= 0.0f) {
        return;
    }
    const float half = terrain.world_size * 2.0f * minimap_scale;
    const dino::Vec3 minimap_eye{focus.x, focus.y, 4000.0f};
    // Put the camera's forward direction at the top of the map so the
    // island rotates with the park camera (FUN_005fb120).
    const dino::Vec3 up{std::cos(heading_rad), std::sin(heading_rad), 0.0f};

    glfns.BindFramebuffer(GL_FRAMEBUFFER, minimap.fbo);
    glViewport(0, 0, minimap.size, minimap.size);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glClearColor(0.10f, 0.29f, 0.43f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const dino::Mat4 view =
        look_at(minimap_eye, {focus.x, focus.y, 0.0f}, up);
    const dino::Mat4 proj =
        ortho(-half, half, -half, half, 0.1f, 8000.0f);
    const dino::Mat4 mvp = dino::multiply(proj, view);

    glfns.UseProgram(program);
    {
        const float white[3] = {1.0f, 1.0f, 1.0f};
        glfns.Uniform3fv(glfns.GetUniformLocation(program, "u_tint"), 1,
                         white);
    }
    glfns.UniformMatrix4fv(glfns.GetUniformLocation(program, "u_mvp"), 1,
                           GL_TRUE, mvp.m);
    glfns.Uniform1i(glfns.GetUniformLocation(program, "u_unlit"), 1);
    glfns.Uniform1i(glfns.GetUniformLocation(program, "u_use_texture"),
                    texture != 0 ? 1 : 0);
    glfns.Uniform1i(
        glfns.GetUniformLocation(program, "u_use_texture2"), 0);
    glfns.Uniform1i(
        glfns.GetUniformLocation(program, "u_modulate2x"), 0);
    glfns.Uniform1f(glfns.GetUniformLocation(program, "u_alpha"), 1.0f);
    if (texture != 0) {
        glfns.ActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
    }
    glfns.BindVertexArray(gpu.vao);
    glDrawElements(GL_TRIANGLES, gpu.index_count, GL_UNSIGNED_INT, nullptr);

    glfns.BindFramebuffer(GL_FRAMEBUFFER, 0);

}

void Game::rebuild_extra_items() {
    extra_items.clear();
    for (std::size_t i = 0; i < veg_gpu.size(); ++i) {
        DrawItem item{&veg_gpu[i], veg_textures[i]};
        item.visible = &veg_visible[i];
        item.alpha_test = true;
        std::copy(std::begin(veg_bounds[i]), std::end(veg_bounds[i]),
                  std::begin(item.bound));
        extra_items.push_back(item);
    }
    // Roads are painted into the terrain texture when the island has a
    // basemap; the tile mesh is only the fallback for bare terrain.
    if (path_gpu.vao != 0 && (image.width <= 0 || !opt.texture_path.empty())) {
        extra_items.push_back({&path_gpu, path_texture});
    }
    if (road_preview_gpu.vao != 0 && road_preview_texture != 0) {
        // Retail helper (Data/States/Helper.ini): focus/road.tga times
        // the pulsing gscMaterialBank colour, blended One/One.
        DrawItem item{&road_preview_gpu, road_preview_texture};
        // The captured retail fill is exactly the pulsing material
        // colour (64,64,135 at the captured phase), not a blend with the
        // grass. Draw the tinted helper unlit and opaque, with depth so
        // only the nearest box face contributes.
        item.unlit = true;
        item.additive = false;
        item.alpha = 1.0f;
        item.depth_write = true;
        extra_items.push_back(item);
    }
    if (fence_preview_visible && fence_preview_gpu.vao != 0) {
        DrawItem item{&fence_preview_gpu,
                      fence_preview_ok ? fence_preview_ok_texture
                                       : fence_preview_bad_texture};
        item.unlit = true;       // exact retail disc colour
        item.depth_write = true; // occluded by fence posts
        extra_items.push_back(item);
    }
    if (fence_template_visible && fence_template_gpu.vao != 0) {
        // helper_sphere grid between the pending corners (mode 2).
        DrawItem item{&fence_template_gpu,
                      fence_template_ok ? fence_template_ok_texture
                                        : fence_template_bad_texture};
        item.unlit = true;
        item.depth_write = true;
        extra_items.push_back(item);
    }
    if (fence_ground_gpu.vao != 0 && paddock_dirt_texture != 0) {
        // gatAttraction ground: stage 0 boue_pad, stage 1 the occupancy
        // mottle sampled from the next 128x128 quadrant. ZWrite off and
        // SrcAlpha/InvSrcAlpha, so the ragged texture edge blends.
        DrawItem item{&fence_ground_gpu, paddock_dirt_texture};
        item.texture2 = paddock_occup_texture;
        item.uv2_scale[0] = 1.0f;
        item.uv2_scale[1] = 1.0f;
        item.uv2_offset[0] = 0.5f;
        item.uv2_offset[1] = 0.5f;
        item.unlit = true;
        item.alpha_blend = true;
        item.depth_write = false;
        extra_items.push_back(item);
    }
    if (fence_gpu.vao != 0) {
        extra_items.push_back({&fence_gpu, fence_texture});
    }
    if (fence_selected_gpu.vao != 0) {
        // Selected paddock body: bank 1 + Dino_SelectedWall (stage 1
        // Modulate2x), so the texture brightens 1.0x..2.0x at 0.628 s.
        DrawItem item{&fence_selected_gpu, fence_texture};
        item.tint = selected_tint.data();
        extra_items.push_back(item);
    }
    if (fence_beams_gpu.vao != 0 && fence_laser_texture != 0) {
        // Material 1: laser2.tga lamp billboards, dino_additif (One/One,
        // ZWrite off, fog off). Static; the retail capture shows a slow
        // brightness pulse whose source is not proven.
        DrawItem item{&fence_beams_gpu, fence_laser_texture};
        item.unlit = true;
        item.additive = true;
        item.depth_write = false;
        // Bank 4 (sin 4t): blue-white additive ramp 1.571 s.
        item.tint = lamp_tint.data();
        extra_items.push_back(item);
    }
    if (visitor_gpu.vao != 0) {
        extra_items.push_back({&visitor_gpu, visitor_texture_id});
    }
    if (employee_gpu.vao != 0) {
        extra_items.push_back({&employee_gpu, employee_texture_id});
    }
    if (dino_gpu.vao != 0) {
        extra_items.push_back({&dino_gpu, dino_texture});
        if (shadow_gpu.vao != 0) {
            extra_items.push_back({&shadow_gpu, 0, 0.35f});
        }
    }
    for (const ImportedDino& dinosaur : imported_dinos) {
        if (dinosaur.gpu.vao != 0) {
            extra_items.push_back(
                {&dinosaur.gpu, dinosaur.assets->texture});
            if (dinosaur.shadow.vao != 0) {
                extra_items.push_back({&dinosaur.shadow, 0, 0.35f});
            }
        }
    }
    if (food_gpu.vao != 0 && !food_sources.empty() &&
        food_sources.front().remaining > 0.0f) {
        extra_items.push_back({&food_gpu, food_texture});
    }
    for (const ShowFeatureDraw& feature : show_feature_draws) {
        if (feature.gpu.vao != 0) {
            extra_items.push_back({&feature.gpu, feature.texture});
        }
    }
    for (const BuildingDraw& building : building_draws) {
        if (building.gpu.vao != 0) {
            extra_items.push_back({&building.gpu, building.texture});
        }
    }
    // Transparent pass: water last so it blends over all opaque actors.
    // WaterSurface.ini is additive (src One, dest One, add) and uses a
    // second texture stage: Modulate2x on coordinate set 1 (same TGA,
    // 28/30 tiling + 0.25 offset, opposite scroll in retail).
    if (water_gpu.vao != 0) {
        DrawItem water;
        water.mesh = &water_gpu;
        water.texture = water_texture;
        water.alpha = 1.0f;
        water.additive = true;
        water.fog = 1;
        if (opt.water2 && water_texture2 != 0) {
            water.texture2 = water_texture2;
            water.modulate2x = true;
            water.uv2_scale[0] = 28.0f / 30.0f;
            water.uv2_scale[1] = 28.0f / 30.0f;
            water.uv2_offset[0] = 0.25f / 30.0f;
        }
        extra_items.push_back(water);
    }

}

void Game::refresh_terrain_texture() {
    if (!opt.texture_path.empty() || image.width <= 0) {
        return;
    }
    dino::Image painted = image;
    if (build_ready) {
        dino::paint_roads(painted, park_session.build_grid, terrain.world_size);
    }
    if (texture != 0) {
        glDeleteTextures(1, &texture);
    }
    texture = upload_texture(painted);

}

void Game::load_dino_family_tables() {
    if (!family_table.species.empty()) {
        return;
    }
    // family_table is shared with the interactive store path.
    try {
        family_table = dino::read_dino_family_table(
            resolve_asset(opt.asset_root,
                          "Data/Design/Genotype/anim_trans.dic"),
            resolve_asset(opt.asset_root,
                          "Data/Design/Genotype/simu_trans.dic"));
    } catch (const std::exception& e) {
        std::fprintf(stderr, "dino family table: %s\n", e.what());
    }
    // head_species is shared with the interactive store path.
    for (const std::string& species : family_table.species) {
        try {
            const int head = dino::read_gen_head_type(resolve_asset(
                opt.asset_root,
                "Data/Design/Scenario/Genotype/" + species + ".gen"));
            if (head >= 0) {
                head_species[head] = species;
            }
        } catch (const std::exception& e) {
            std::fprintf(stderr, "genotype %s: %s\n", species.c_str(),
                         e.what());
        }
    }

}

void Game::spawn_world_dinos() {
    if (!opt.import_dinos || wld_dinosaurs.empty() ||
        terrain.world_size <= 0.0f) {
        return;
    }
    int spawned = 0;
    std::map<std::string, int> species_counts;
    // Each `.wld` fence rectangle is a paddock with the retail capacity
    // model (stFence, 0x57dc60/0x57eb50). Imported dinosaurs have no
    // stored position, so they fill paddocks in record order.
    std::vector<dino::WorldRect> paddock_rects = wld_fences;
    if (paddock_rects.empty()) {
        for (const dino::WorldFence& fence : sav_fences) {
            paddock_rects.push_back({fence.start_x, fence.start_y,
                                     fence.end_x, fence.end_y});
        }
    }
    park_session.paddocks.clear();
    park_session.paddocks.reserve(paddock_rects.size());
    for (const dino::WorldRect& rect : paddock_rects) {
        dino::Paddock paddock;
        paddock.rect = rect;
        paddock.capacity =
            dino::paddock_capacity(rect, park_session.build_grid.cell_size);
        // 0x5ec7e0 picks the ground atlas quadrant at creation time.
        paddock.ground_variant = std::rand() & 3;
        // FUN_00590ac0 names the stFence "EnclosureN" from the bank; the
        // name is regenerated on load and never saved.
        paddock.name = "Enclosure" + std::to_string(park_session.paddocks.size() + 1);
        park_session.paddocks.push_back(paddock);
    }
    // `import_park_layout` runs before the paddock list exists, so its
    // rebuild produced no gatAttraction ground quads. Loaded parks need
    // the sandy interiors immediately, not only after the next build.
    rebuild_build_meshes();
    // `dinosaur+0x1e0` is a size class (8/16/32/64). The exact class comes
    // from the individual's phenotype; the port derives it from the
    // species rest-mesh bbox until the genotype translator is ported.
    // species_size_cm is shared with the interactive store path.
    size_for_species = [&](const std::string& species) -> int {
        const auto found = species_size_cm.find(species);
        if (found != species_size_cm.end()) {
            return found->second;
        }
        int size_cm = 300;
        try {
            const dino::Mesh rest = dino::load_mesh(resolve_asset(
                opt.asset_root, "Data/Graphics/Actors/Dinos/" + species +
                                    "/" + species + "_lod1.mesh"));
            dino::Vec3 lo{1e30f, 1e30f, 1e30f};
            dino::Vec3 hi{-1e30f, -1e30f, -1e30f};
            for (const dino::Vec3& p : rest.positions) {
                lo.x = std::min(lo.x, p.x);
                lo.y = std::min(lo.y, p.y);
                lo.z = std::min(lo.z, p.z);
                hi.x = std::max(hi.x, p.x);
                hi.y = std::max(hi.y, p.y);
                hi.z = std::max(hi.z, p.z);
            }
            if (hi.x > lo.x) {
                size_cm = static_cast<int>(
                    std::max({hi.x - lo.x, hi.y - lo.y, hi.z - lo.z}) *
                    100.0f);
            }
        } catch (const std::exception&) {
        }
        species_size_cm[species] = size_cm;
        return size_cm;
    };
    for (std::size_t i = 0; i < wld_dinosaurs.size(); ++i) {
        const dino::WorldDinosaur& record = wld_dinosaurs[i];
        const int head = static_cast<int>(std::min(
            record.genotype[0].first, record.genotype[0].second));
        const auto match = head_species.find(head);
        const std::string species =
            match != head_species.end() ? match->second : "allo";
        std::string family = family_table.family_of(species);
        if (family.empty()) {
            family = "LightBiped";
        }
        float x = terrain.world_size * 0.5f;
        float y = terrain.world_size * 0.5f;
        float angle = static_cast<float>(i) * 137.5f;
        const dino::DinoAttributes attrs =
            phenotype_table.valid
                ? phenotype_table.attributes(record.genotype)
                : dino::DinoAttributes{};
        const dino::DinoAttributes* attrs_ptr =
            phenotype_table.valid ? &attrs : nullptr;
        if (!park_session.paddocks.empty()) {
            const int cost_class =
                phenotype_table.valid
                    ? phenotype_table.size_class(record.genotype)
                    : dino::paddock_size_class(size_for_species(species));
            if (std::getenv("DINO_DEBUG_PHENO") != nullptr) {
                std::fprintf(
                    stderr,
                    "pheno %zu g0=(%u,%u) g2=(%u,%u) g3=(%u,%u) len=%d "
                    "class=%d wid=%d hei=%d P=%d I=%d B=%d Agi=%d Arm=%d "
                    "Atk=%d Ag=%d O=%d CA=%d Orn=%d St=%d Co=%d walk=%.3f run=%.3f "
                    "jump=%d wt=%d pl=%d,%d,%d,%d,%d "
                    "Ah=%d Ab=%d Aa=%d Al=%d At=%d food=%.3f "
                    "g15=(%u,%u) g16=(%u,%u) g17=(%u,%u)\n",
                    i, record.genotype[0].first,
                    record.genotype[0].second,
                    record.genotype[2].first,
                    record.genotype[2].second,
                    record.genotype[3].first,
                    record.genotype[3].second,
                    phenotype_table.length_cm(record.genotype), cost_class,
                    attrs.width_cm, attrs.height_cm, attrs.perception,
                    attrs.intelligence, attrs.beauty, attrs.agility,
                    attrs.armor, attrs.attack_total, attrs.aggression,
                    attrs.originality, attrs.conventional_appeal,
                    attrs.ornament_count, attrs.stamina, attrs.courage,
                    attrs.walk_speed,
                    attrs.run_speed, attrs.jump, attrs.weight_kg,
                    attrs.part_length[0], attrs.part_length[1],
                    attrs.part_length[2], attrs.part_length[3],
                    attrs.part_length[4], attrs.attack_head,
                    attrs.attack_body, attrs.attack_arm, attrs.attack_leg,
                    attrs.attack_tail, static_cast<double>(attrs.food_consumption),
                    record.genotype[15].first, record.genotype[15].second,
                    record.genotype[16].first, record.genotype[16].second,
                    record.genotype[17].first, record.genotype[17].second);
            }
            int paddock_index = -1;
            for (std::size_t p = 0; p < park_session.paddocks.size(); ++p) {
                if (dino::paddock_can_add(
                        park_session.paddocks[p], static_cast<int>(i), cost_class)) {
                    paddock_index = static_cast<int>(p);
                    break;
                }
            }
            if (paddock_index >= 0) {
                dino::Paddock& paddock =
                    park_session.paddocks[static_cast<std::size_t>(paddock_index)];
                dino::paddock_add(paddock, static_cast<int>(i),
                                  cost_class);
                const dino::WorldRect& pen = paddock.rect;
                const float x0 =
                    static_cast<float>(std::min(pen.x0, pen.x1));
                const float x1 =
                    static_cast<float>(std::max(pen.x0, pen.x1));
                const float y0 =
                    static_cast<float>(std::min(pen.y0, pen.y1));
                const float y1 =
                    static_cast<float>(std::max(pen.y0, pen.y1));
                const float radius =
                    std::max(1.0f, std::min(x1 - x0, y1 - y0) * 0.28f);
                const float theta =
                    static_cast<float>(paddock.occupants.size() - 1) *
                        2.39996323f +
                    static_cast<float>(paddock_index) * 0.7f;
                x = (x0 + x1) * 0.5f + std::cos(theta) * radius;
                y = (y0 + y1) * 0.5f + std::sin(theta) * radius;
                angle = theta * 180.0f / kPi + 90.0f;
            } else {
                std::printf("dino %u (%s): no paddock capacity\n",
                            record.object_id, species.c_str());
            }
        }
        try {
            if (add_imported_dino(species, family, opt.dino_clip, x, y,
                                  angle, attrs_ptr)) {
                imported_dinos.back().carnivore =
                    dino::genotype_is_carnivore(record.genotype);
                ++spawned;
                ++species_counts[species];
            }
        } catch (const std::exception& e) {
            std::fprintf(stderr, "dino %u (%s): %s\n",
                         record.object_id, species.c_str(), e.what());
        }
    }
    if (spawned > 0) {
        std::printf("dinos: %d/%zu spawned", spawned,
                    wld_dinosaurs.size());
        for (const auto& [species, count] : species_counts) {
            std::printf(" %s=%d", species.c_str(), count);
        }
        std::printf("\n");
        rebuild_extra_items();
    }
    park_paddocks = park_session.paddocks;

}

dino::Mesh Game::pose_dino(double time) {
    const dino::Animation* anim_ptr = anim.bones.empty() ? nullptr : &anim;
    dino::Mesh pose = build_skinned_mesh(skin.mesh, skin, skeleton, anim_ptr, time,
                                         vertex_bone, bone_skel,
                                         opt.skin_bind_from_skin);
    if (mesh.colors.size() == pose.positions.size()) {
        pose.colors = mesh.colors;
    }
    return place_on_terrain(std::move(pose), terrain, spawn_x, spawn_y,
                            opt.dino_angle);

}

float Game::camera_radius() {
    return camera_config.distance_min +
           (camera_config.distance_max - camera_config.distance_min) *
               camera_progress;

}

float Game::camera_height() {
    return camera_config.elevation_min +
           (camera_config.elevation_max - camera_config.elevation_min) *
               camera_progress;

}

dino::Vec3 Game::orbit_eye() {
    const float cy = std::cos(yaw * kPi / 180.0f);
    const float sy = std::sin(yaw * kPi / 180.0f);
    if (camera_retail_model) {
        return retail_camera.eye_cur;
    }
    const float cp = std::cos(pitch * kPi / 180.0f);
    const float sp = std::sin(pitch * kPi / 180.0f);
    return dino::Vec3{center.x + distance * cp * cy,
                      center.y + distance * cp * sy,
                      center.z + distance * sp};

}

void Game::enter_free_camera() {
    free_pos = orbit_eye();
    const dino::Vec3 dir = dino::normalize({center.x - free_pos.x,
                                            center.y - free_pos.y,
                                            center.z - free_pos.z});
    free_yaw = std::atan2(dir.y, dir.x) * 180.0f / kPi;
    free_pitch = std::asin(std::clamp(dir.z, -1.0f, 1.0f)) * 180.0f / kPi;

}

int Game::paddock_at_map(float mx, float my) {
    int best = -1;
    float best_d = 1e30f;
    for (std::size_t p = 0; p < park_session.paddocks.size(); ++p) {
        const dino::WorldRect& r = park_session.paddocks[p].rect;
        const float x0 = static_cast<float>(std::min(r.x0, r.x1));
        const float x1 = static_cast<float>(std::max(r.x0, r.x1));
        const float y0 = static_cast<float>(std::min(r.y0, r.y1));
        const float y1 = static_cast<float>(std::max(r.y0, r.y1));
        const float margin = 3.0f;
        if (mx < x0 - margin || mx > x1 + margin || my < y0 - margin ||
            my > y1 + margin) {
            continue;
        }
        const float dx = mx - (x0 + x1) * 0.5f;
        const float dy = my - (y0 + y1) * 0.5f;
        const float d = dx * dx + dy * dy;
        if (d < best_d) {
            best_d = d;
            best = static_cast<int>(p);
        }
    }
    return best;

}

const dino::WorldRect* Game::selected_paddock_rect() {
    if (selected_paddock < 0 ||
        selected_paddock >= static_cast<int>(park_session.paddocks.size())) {
        return nullptr;
    }
    return &park_session.paddocks[static_cast<std::size_t>(selected_paddock)].rect;

}

int Game::paddock_at(float x, float y) {
    for (std::size_t p = 0; p < park_session.paddocks.size(); ++p) {
        const dino::WorldRect& r = park_session.paddocks[p].rect;
        if (x >= static_cast<float>(std::min(r.x0, r.x1)) &&
            x <= static_cast<float>(std::max(r.x0, r.x1)) &&
            y >= static_cast<float>(std::min(r.y0, r.y1)) &&
            y <= static_cast<float>(std::max(r.y0, r.y1))) {
            return static_cast<int>(p);
        }
    }
    return -1;

}

dino::FoodSource* Game::paddock_food(int paddock, bool carnivore) {
    if (paddock < 0 ||
        paddock >= static_cast<int>(park_session.paddocks.size())) {
        return nullptr;
    }
    const dino::WorldRect& r =
        park_session.paddocks[static_cast<std::size_t>(paddock)].rect;
    for (dino::FoodSource& source : food_sources) {
        if (source.remaining <= 0.0f || source.def < 0 ||
            source.def >= static_cast<int>(food_defs.size())) {
            continue;
        }
        if (dino::food_is_carnivore(
                food_defs[static_cast<std::size_t>(source.def)].kind) !=
            carnivore) {
            continue;
        }
        if (source.x >= static_cast<float>(std::min(r.x0, r.x1)) &&
            source.x <= static_cast<float>(std::max(r.x0, r.x1)) &&
            source.y >= static_cast<float>(std::min(r.y0, r.y1)) &&
            source.y <= static_cast<float>(std::max(r.y0, r.y1))) {
            return &source;
        }
    }
    return nullptr;

}

void Game::print_paddock_totals() {
    if (park_session.paddocks.empty()) {
        return;
    }
    float capacity = 0.0f;
    float used = 0.0f;
    for (const dino::Paddock& paddock : park_session.paddocks) {
        capacity += paddock.capacity;
        used += paddock.used;
    }
    std::printf("paddocks: %zu capacity=%.0f used=%.0f\n",
                park_session.paddocks.size(), capacity, used);

}

void Game::log_dino_food() {
    for (const ImportedDino& dinosaur : imported_dinos) {
        const int pen = paddock_at(dinosaur.x, dinosaur.y);
        const dino::FoodSource* source =
            paddock_food(pen, dinosaur.carnivore);
        std::printf("dino food: %s paddock=%d diet=%s source=%s\n",
                    dinosaur.species.c_str(), pen,
                    dinosaur.carnivore ? "carnivore" : "herbivore",
                    source != nullptr
                        ? food_defs[static_cast<std::size_t>(
                                        source->def)]
                              .name.c_str()
                        : "none");
    }

}

std::shared_ptr<Game::ImportedSpecies> Game::load_imported_species(const std::string& species,
                                     const std::string& family,
                                     const std::string& clip) {
    const std::string key = species + "|" + family + "|" + clip;
    const auto found = imported_species.find(key);
    if (found != imported_species.end()) {
        return found->second;
    }
    auto assets = std::make_shared<ImportedSpecies>();
    const std::string dir =
        "Data/Graphics/Actors/Dinos/" + species + "/" + species;
    assets->mesh = dino::load_mesh(
        resolve_asset(opt.asset_root, dir + "_lod1.mesh"));
    assets->skeleton = dino::load_skel(
        resolve_asset(opt.asset_root, dir + ".skel"));
    assets->skin = dino::load_skin(
        resolve_asset(opt.asset_root, dir + "_lod1.skin"),
        assets->mesh.name);
    // Same bind rule as the single-dinosaur path: the `.skin` mesh is a
    // crouched bind pose, so rebuild it from the standing `.mesh` while
    // keeping the skin weights.
    if (assets->skin.mesh.positions.size() ==
            assets->mesh.positions.size() &&
        assets->skin.mesh.indices == assets->mesh.indices) {
        for (std::size_t i = 0; i < assets->mesh.positions.size(); ++i) {
            assets->skin.mesh.positions[i] = {
                assets->mesh.positions[i].x * 100.0f,
                assets->mesh.positions[i].y * 100.0f,
                assets->mesh.positions[i].z * 100.0f};
        }
        if (assets->mesh.uvs.size() == assets->mesh.positions.size()) {
            assets->skin.mesh.uvs = assets->mesh.uvs;
        }
        if (assets->mesh.normals.size() == assets->mesh.positions.size()) {
            assets->skin.mesh.normals = assets->mesh.normals;
        }
        if (assets->mesh.colors.size() == assets->mesh.positions.size()) {
            assets->skin.mesh.colors = assets->mesh.colors;
        }
    }
    build_skin_mapping(assets->skin, assets->skeleton, assets->vertex_bone,
                       assets->bone_skel);
    if (!clip.empty() && !family.empty()) {
        assets->anim = dino::load_anim(resolve_asset(
            opt.asset_root, "Data/Graphics/Actors/Dinos/animations/" +
                                family + "/" + family + "_" + clip +
                                ".anim"));
    }
    try {
        assets->texture = upload_texture(dino::load_tga(resolve_asset(
            opt.asset_root,
            "Data/Graphics/Textures/actors/dinos/" + species + ".tga")));
    } catch (const std::exception& e) {
        std::fprintf(stderr, "dino texture %s: %s\n", species.c_str(),
                     e.what());
    }
    imported_species[key] = assets;
    return assets;

}

bool Game::add_imported_dino(const std::string& species,
                                 const std::string& family,
                                 const std::string& clip, float x, float y,
                                 float angle,
                                 const dino::DinoAttributes* attributes ) {
    auto assets = load_imported_species(species, family, clip);
    if (!assets) {
        return false;
    }
    ImportedDino dinosaur;
    dinosaur.assets = assets;
    dinosaur.species = species;
    dinosaur.family = family;
    dinosaur.clip = clip;
    dinosaur.x = x;
    dinosaur.y = y;
    dinosaur.angle = angle;
    dinosaur.phase =
        static_cast<double>(imported_dinos.size()) * 617.0;
    dinosaur.needs.value = dino_initial_needs;
    dinosaur.needs.threat = std::max(0, opt.dino_threat);
    dinosaur.needs.threat_radius = std::max(0, opt.dino_radius);
    dinosaur.needs.health = std::max(0, opt.dino_health);
    dinosaur.needs.max_health = std::max(0, opt.dino_max_health);
    // The `.bai` Eat gate reads attribute 4 (size in cm); derive a rough
    // body length from the rest mesh so quadrupeds/bipeds pick the right
    // eater action.
    dino::Vec3 lo{1e30f, 1e30f, 1e30f};
    dino::Vec3 hi{-1e30f, -1e30f, -1e30f};
    for (const dino::Vec3& p : assets->mesh.positions) {
        lo.x = std::min(lo.x, p.x);
        lo.y = std::min(lo.y, p.y);
        lo.z = std::min(lo.z, p.z);
        hi.x = std::max(hi.x, p.x);
        hi.y = std::max(hi.y, p.y);
        hi.z = std::max(hi.z, p.z);
    }
    if (hi.x > lo.x) {
        dinosaur.needs.size_cm = static_cast<int>(
            std::max({hi.x - lo.x, hi.y - lo.y, hi.z - lo.z}) * 100.0f);
    }
    if (attributes != nullptr && attributes->length_cm > 0) {
        dinosaur.attributes = *attributes;
        // Retail attribute 4 (`actor+0x50`) is the phenotype length, not
        // the rest-mesh bbox; the other values feed the `.bai` gates and
        // the GetCourage/ObstAttackSize1 completion writers.
        dinosaur.needs.size_cm = attributes->length_cm;
        dinosaur.needs.aggression = attributes->aggression;
        dinosaur.needs.stamina = attributes->stamina;
        dinosaur.needs.agility = attributes->agility;
        dinosaur.needs.combined_attack = attributes->combined_attack;
        dinosaur.needs.obstacle_radius = attributes->obstacle_radius;
    }
    if (opt.dino_action_loop && !dino_action_table.empty()) {
        dino::DinoActionLoopConfig config;
        config.action_scale =
            1.0 / std::max(0.05f, opt.dino_action_speed);
        dinosaur.action_loop.emplace(&dino_action_table, dinosaur.needs,
                                     config);
    }
    const dino::Animation* anim_ptr =
        assets->anim.bones.empty() ? nullptr : &assets->anim;
    dino::Mesh pose = build_skinned_mesh(
        assets->skin.mesh, assets->skin, assets->skeleton, anim_ptr, 0.0,
        assets->vertex_bone, assets->bone_skel, false);
    if (assets->mesh.colors.size() == pose.positions.size()) {
        pose.colors = assets->mesh.colors;
    }
    pose = place_on_terrain(std::move(pose), terrain, x, y, angle);
    dinosaur.gpu = upload_mesh(pose, glfns);
    const float half = terrain.world_size * 0.5f;
    dinosaur.shadow = upload_mesh(
        make_contact_shadow(x - half, y - half,
                            terrain.height_at(x - half, y - half) + 0.06f,
                            2.2f),
        glfns);
    imported_dinos.push_back(std::move(dinosaur));
    const std::size_t dino_index = imported_dinos.size() - 1;
    if (imported_dinos[dino_index].action_loop) {
        // `node+0x3c` target policy (`GetTarget` +0x14). The park target
        // factory is not decoded yet, so Eat/EatForSmallies are modelled
        // as the nearest stocked food source (box target); every other
        // action has no target (-1, threat term skipped).
        imported_dinos[dino_index].action_loop->set_threat_provider(
            [this, dino_index](const dino::BaiAction& action) {
                const ImportedDino& self = imported_dinos[dino_index];
                if (action.name != "Eat" &&
                    action.name != "EatForSmallies") {
                    return 0;
                }
                const dino::FoodSource* nearest = nullptr;
                float best = 0.0f;
                for (const dino::FoodSource& source : food_sources) {
                    if (source.remaining <= 0.0f) {
                        continue;
                    }
                    const float dx = source.x - self.x;
                    const float dy = source.y - self.y;
                    const float d2 = dx * dx + dy * dy;
                    if (nearest == nullptr || d2 < best) {
                        nearest = &source;
                        best = d2;
                    }
                }
                if (nearest == nullptr) {
                    return 0;
                }
                dino::ActionTarget target;
                target.kind = dino::ActionTarget::Kind::Box;
                target.x = static_cast<int>(nearest->x);
                target.y = static_cast<int>(nearest->y);
                // The loop owns the live needs; the outer copy is only
                // the spawn snapshot.
                const dino::DinoNeeds& needs =
                    self.action_loop ? self.action_loop->needs()
                                     : self.needs;
                return dino::chooser_threat_score(
                    target, static_cast<int>(self.x),
                    static_cast<int>(self.y), needs.threat,
                    needs.threat_radius, danger_grid);
            });
    }
    return true;

}

void Game::clear_imported_dinos() {
    for (ImportedDino& dinosaur : imported_dinos) {
        dinosaur.gpu.destroy(glfns);
        dinosaur.shadow.destroy(glfns);
    }
    imported_dinos.clear();

}

}  // namespace dino::app
