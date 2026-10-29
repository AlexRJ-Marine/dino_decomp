// Menu and campaign actions, options, and video playback.

#include "game.h"

namespace dino::app {

void Game::apply_menu_options() {
    if (!menu_loaded) {
        return;
    }
    opt.music_volume = static_cast<float>(menu.music_volume);
    opt.sfx_volume = static_cast<float>(menu.sfx_volume);
    opt.game_speed = menu.game_speed;
    opt.menu_lod_quality = menu.lod_quality;
    opt.texture_quality = menu.texture_quality;
    opt.texture_depth = menu.texture_depth;
    opt.hardware_tnl = menu.hardware_tnl;
    opt.hardware_sound = menu.hardware_sound;
    music.set_gain(opt.muted
                       ? 0.0f
                       : std::clamp(menu.music_volume / 100.0f, 0.0f,
                                    1.0f));
    ui_sound.set_gain(opt.muted
                          ? 0.0f
                          : std::clamp(menu.sfx_volume / 100.0f, 0.0f,
                                       1.0f));
    if (menu.game_speed_dirty && opt.park_time_scale <= 0.0f) {
        park_session.clock.time_scale = 6.0 * static_cast<double>(
                                          std::max(1, menu.game_speed));
        menu.game_speed_dirty = false;
    }
    if (menu.screen_mode_changed && !menu.screen_modes.empty()) {
        const std::string& mode =
            menu.screen_modes[static_cast<std::size_t>(std::clamp(
                menu.screen_mode, 0,
                static_cast<int>(menu.screen_modes.size()) - 1))];
        int mode_w = 0;
        int mode_h = 0;
        int mode_bpp = 0;
        if (std::sscanf(mode.c_str(), "%dx%dx%d", &mode_w, &mode_h,
                        &mode_bpp) == 3 &&
            mode_w >= 320 && mode_h >= 240) {
            // Retail Dino.ini stores the chosen mode and applies it at
            // startup; the current window stays until the next launch.
            opt.width = mode_w;
            opt.height = mode_h;
        }
    }

}

void Game::update_campaign_info_lines() {
    menu.campaign_info_lines = 0;
    const dino::Campaign* selected = menu.selected_campaign();
    const dino::ui::Widget* list =
        menu.overlay.get("CAMPAIGN_INFO_DESCRIPTION");
    if (selected == nullptr || list == nullptr) {
        return;
    }
    const int row_w =
        std::max(0, list->width - list->list_scroll_width);
    try {
        const std::string font_path = list->font.empty()
                                          ? "Data/Fonte/FONT14.fnt"
                                          : list->font;
        const dino::Fnt font =
            dino::load_fnt(resolve_asset(opt.asset_root, font_path));
        const float scale =
            list->text_height > 0 && font.font_size > 0
                ? static_cast<float>(list->text_height) /
                      static_cast<float>(font.font_size)
                : 1.0f;
        menu.campaign_info_lines = static_cast<int>(wrap_lines_pixels(
            font, scale,
            dino::ui::resolve_loc_text(selected->description_id, menu.loc),
            static_cast<float>(row_w)).size());
    } catch (const std::exception&) {
        menu.campaign_info_lines = 0;
    }

}

void Game::queue_menu_load() {
    if (!menu.load_requested) {
        return;
    }
    menu.load_requested = false;
    if (menu.save_count <= 0) {
        return;
    }
    const int slot = std::clamp(menu.save_slot, 0, menu.save_count - 1);
    pending_menu_load_path =
        menu_save_slots[static_cast<std::size_t>(slot)].first;

}

void Game::queue_menu_cinematic() {
    if (!menu.cinematic_requested) {
        return;
    }
    menu.cinematic_requested = false;
    const std::string video = menu.cinematic_video();
    if (!video.empty()) {
        pending_menu_cinematic =
            resolve_asset(opt.asset_root, "Data/Video/" + video);
    }

}

bool Game::play_video(const std::string& path, int start_frame,
                          int max_frames) {
    dino::MpgStream video;
    std::string video_error;
    if (!video.open(path, video_error)) {
        std::fprintf(stderr, "play video: %s\n", video_error.c_str());
        return false;
    }
    std::printf("video play: %s %dx%d %.3f fps\n", path.c_str(),
                video.width(), video.height(), video.frame_rate());
    for (int i = 0; i < start_frame; ++i) {
        dino::MpgFrame skipped;
        if (!video.next(skipped)) {
            break;
        }
    }
    // Retail starts the player from the menu click and skips on the next
    // mouse button press (0x517930 polls DIMOUSESTATE L/R/M; there is no
    // key path). Drain the launching click/up events first so the video
    // is not skipped instantly.
    {
        SDL_Event drain;
        while (SDL_PollEvent(&drain)) {
            if (drain.type == SDL_EVENT_QUIT) {
                return false;
            }
        }
    }
    // The cinematics carry an MP2 track (`plm_audio_*`). Retail's movie
    // player plays it while the menu/island music is suspended; do the
    // same, with the video paced by its frame PTS so the audio stays in
    // sync.
    const bool music_was_playing = music.stream != nullptr;
    if (music_was_playing) {
        SDL_PauseAudioStreamDevice(music.stream);
    }
    SDL_AudioStream* video_audio = nullptr;
    if (video.has_audio()) {
        SDL_AudioSpec spec{};
        spec.format = SDL_AUDIO_F32;
        spec.channels = 2;
        spec.freq = video.audio_samplerate();
        video_audio = SDL_OpenAudioDeviceStream(
            SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        if (video_audio != nullptr) {
            SDL_SetAudioStreamGain(
                video_audio,
                opt.muted ? 0.0f
                          : std::clamp(opt.sfx_volume / 100.0f, 0.0f,
                                       1.0f));
            SDL_ResumeAudioStreamDevice(video_audio);
            std::printf("video audio: %d Hz stereo\n",
                        video.audio_samplerate());
        } else {
            std::fprintf(stderr, "video audio device: %s\n",
                         SDL_GetError());
        }
    }
    auto finish_video = [&]() {
        if (video_audio != nullptr) {
            SDL_DestroyAudioStream(video_audio);
            video_audio = nullptr;
        }
        if (music_was_playing && music.stream != nullptr) {
            SDL_ResumeAudioStreamDevice(music.stream);
        }
    };
    bool video_running = true;
    int played = 0;
    dino::MpgFrame last_frame;
    dino::Mesh video_quad_mesh;
    GpuMesh video_quad;
    GLuint video_texture = 0;
    const Uint64 play_start = SDL_GetTicks();
    double first_time = -1.0;
    while (video_running) {
        dino::MpgFrame frame;
        if (!video.next(frame)) {
            break;
        }
        last_frame = frame;
        if (first_time < 0.0) {
            first_time = frame.time;
        }
        if (video_audio != nullptr) {
            const std::vector<float> pcm = video.take_audio();
            if (!pcm.empty()) {
                SDL_PutAudioStreamData(
                    video_audio, pcm.data(),
                    static_cast<int>(pcm.size() * sizeof(float)));
            }
        }
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                video_running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN &&
                       event.key.key == SDLK_ESCAPE) {
                // Port-only extra; retail uses a mouse click.
                video_running = false;
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                // Retail skips on any mouse button (left/right/middle).
                video_running = false;
            }
        }
        if (!video_running) {
            break;
        }
        int view_w = 0;
        int view_h = 0;
        SDL_GetWindowSizeInPixels(window, &view_w, &view_h);
        if (view_w <= 0 || view_h <= 0) {
            break;
        }
        dino::Image image;
        image.width = frame.width;
        image.height = frame.height;
        image.rgba.resize(
            static_cast<std::size_t>(frame.width) * frame.height * 4);
        for (std::size_t i = 0;
             i < static_cast<std::size_t>(frame.width) * frame.height;
             ++i) {
            image.rgba[i * 4 + 0] = frame.rgb[i * 3 + 0];
            image.rgba[i * 4 + 1] = frame.rgb[i * 3 + 1];
            image.rgba[i * 4 + 2] = frame.rgb[i * 3 + 2];
            image.rgba[i * 4 + 3] = 255;
        }
        const GLuint texture = upload_texture(image);
        if (video_texture != 0) {
            glDeleteTextures(1, &video_texture);
        }
        video_texture = texture;
        const float scale = std::min(
            static_cast<float>(view_w) / frame.width,
            static_cast<float>(view_h) / frame.height);
        const float quad_w = frame.width * scale;
        const float quad_h = frame.height * scale;
        video_quad_mesh = build_ui_quad(
            (view_w - quad_w) * 0.5f, (view_h - quad_h) * 0.5f, quad_w,
            quad_h, 0.0f, 0.0f, 1.0f, 1.0f, {255, 255, 255, 255});
        glViewport(0, 0, view_w, view_h);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        draw_ui_mesh(video_quad, video_quad_mesh, video_texture, 1.0f, 1,
                     view_w, view_h);
        SDL_GL_SwapWindow(window);
        ++played;
        if (max_frames > 0 && played >= max_frames) {
            break;
        }
        // Pace on the frame PTS (retail's movie clock); the decoded MP2
        // samples are queued ahead of it in the SDL audio buffer.
        const double target = frame.time - first_time;
        const double elapsed =
            static_cast<double>(SDL_GetTicks() - play_start) / 1000.0;
        if (target > elapsed) {
            SDL_Delay(static_cast<Uint32>((target - elapsed) * 1000.0 +
                                          0.5));
        }
    }
    finish_video();
    if (!opt.screenshot.empty() && last_frame.width > 0) {
        std::vector<std::uint8_t> rgba(
            static_cast<std::size_t>(last_frame.width) *
                last_frame.height * 4,
            255);
        for (int y = 0; y < last_frame.height; ++y) {
            const std::uint8_t* src =
                last_frame.rgb.data() +
                static_cast<std::size_t>(last_frame.height - 1 - y) *
                    last_frame.width * 3;
            std::uint8_t* dst =
                rgba.data() +
                static_cast<std::size_t>(y) * last_frame.width * 4;
            for (int x = 0; x < last_frame.width; ++x) {
                dst[x * 4 + 0] = src[x * 3 + 0];
                dst[x * 4 + 1] = src[x * 3 + 1];
                dst[x * 4 + 2] = src[x * 3 + 2];
            }
        }
        write_bmp(opt.screenshot, last_frame.width, last_frame.height,
                  rgba);
    }
    std::printf("video played: %d frames\n", played);
    video_quad.destroy(glfns);
    if (video_texture != 0) {
        glDeleteTextures(1, &video_texture);
    }
    return true;

}

void Game::play_pending_cinematic() {
    if (pending_menu_cinematic.empty()) {
        return;
    }
    std::printf("menu cinematic: %s\n", pending_menu_cinematic.c_str());
    play_video(pending_menu_cinematic, 0, opt.video_play_frames);
    pending_menu_cinematic.clear();

}

void Game::start_campaign_mission(const dino::CampaignMission& mission) {
    campaign_runtime.emplace();
    campaign_runtime->start(mission, park_session.clock.minutes());
    show_runtime.clear();
    show_runtime.set_duration_minutes(opt.show_duration_minutes);
    show_catalog.clear();
    show_placements_applied = false;
    pending_show_index = -1;
    try {
        show_catalog = dino::load_show_catalog(opt.asset_root, mission,
                                               park.loc);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "show catalog: %s\n", e.what());
    }
    if (!show_catalog.empty()) {
        std::printf("show catalog: %zu entries\n", show_catalog.size());
    }
    std::vector<dino::ui::ShowRow> show_rows;
    show_rows.reserve(show_catalog.size());
    for (const dino::ShowCatalogEntry& entry : show_catalog) {
        dino::ui::ShowRow row;
        row.title = entry.title;
        row.stem = entry.stem;
        row.judge_kind = entry.judge_kind;
        row.price = entry.cost;
        row.cage_size = entry.cage_size;
        show_rows.push_back(std::move(row));
        std::printf(
            "show catalog: %s title=%s judge=%d cost=%d cage=%d%d%d%d\n",
            entry.stem.c_str(), entry.title.c_str(), entry.judge_kind,
            entry.cost, entry.cage_size[0], entry.cage_size[1],
            entry.cage_size[2], entry.cage_size[3]);
    }
    park.set_show_rows(std::move(show_rows));
    last_mission_verdicts.assign(mission.objectives.size(),
                                 dino::ObjectiveVerdict::Pending);
    park.set_mission_verdicts(
        std::vector<int>(mission.objectives.size(), 0));
    park.show_mission_panel();
    std::printf("mission: %s objectives=%zu\n", mission.name.c_str(),
                mission.objectives.size());
    for (std::size_t i = 0; i < mission.objectives.size(); ++i) {
        const dino::CampaignObjective& objective = mission.objectives[i];
        std::printf("mission objective %zu %s: %s\n", i,
                    objective.verb.c_str(),
                    dino::ui::resolve_loc_text(objective.description_id,
                                               park.loc)
                        .c_str());
    }
    const std::string campaign_name =
        std::filesystem::path(mission.path)
            .parent_path()
            .filename()
            .string();
    if (!campaign_name.empty()) {
        campaign_progress[campaign_name] = mission.name;
        try {
            dino::save_campaign_progress(campaign_progress,
                                         campaign_progress_path);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "campaign progress: %s\n", e.what());
        }
    }
    mission_panel_title.clear();
    const std::optional<dino::Campaign> parent =
        dino::load_parent_campaign(mission);
    if (parent) {
        for (std::size_t i = 0; i < parent->missions.size(); ++i) {
            if (parent->missions[i].name == mission.name) {
                mission_panel_title =
                    std::to_string(i + 1) + "/" +
                    std::to_string(parent->missions.size());
                break;
            }
        }
    }
    if (!mission_panel_title.empty()) {
        std::printf("mission title: %s\n",
                    mission_panel_title.c_str());
    }
    // Campaign store: the mission's own [DINOSTORE] block. Empty blocks
    // (Cpg04_Mission4) produce an empty catalog, matching the retail
    // per-mission store vector.
    apply_store_catalog(dino::market_from_values(mission.values),
                        mission.name);

}

void Game::advance_campaign() {
    if (!pending_next_mission) {
        if (!pending_ending_video.empty()) {
            const std::string video = pending_ending_video;
            pending_ending_video.clear();
            std::printf("campaign video: %s\n", video.c_str());
            play_video(video, 0, opt.video_play_frames);
        }
        // The campaign is over: the mission panel would otherwise stay
        // open with stale objective text behind the dismissed CPGEND.
        park.clear_mission();
        return;
    }
    const dino::CampaignMission next = *pending_next_mission;
    pending_next_mission.reset();
    const auto delta = next.values.find("CONTEXT.BUDGET");
    if (delta != next.values.end()) {
        park_session.budget += std::atoi(delta->second.c_str());
    }
    park.close_dialog();
    std::printf("campaign: next mission %s budget=%d\n",
                next.name.c_str(), park_session.budget);
    start_campaign_mission(next);

}

}  // namespace dino::app
