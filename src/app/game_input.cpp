// Per-frame input handling and the placement/preview update.

#include "game.h"

namespace dino::app {

void Game::apply_injected_input() {
    // --inject-key FRAME,KEYCODE: a real key down/up pair (tests).
    for (const std::string& spec : opt.inject_keys) {
        int at = 0;
        int code = 0;
        if (std::sscanf(spec.c_str(), "%d,%d", &at, &code) != 2 ||
            at != frame) {
            continue;
        }
        for (const Uint32 type : {SDL_EVENT_KEY_DOWN, SDL_EVENT_KEY_UP}) {
            SDL_Event injected{};
            injected.type = type;
            injected.key.windowID = SDL_GetWindowID(window);
            injected.key.key = static_cast<SDL_Keycode>(code);
            injected.key.scancode = SDL_GetScancodeFromKey(
                static_cast<SDL_Keycode>(code), nullptr);
            injected.key.down = type == SDL_EVENT_KEY_DOWN;
            injected.key.repeat = false;
            SDL_PushEvent(&injected);
        }
    }
    // --inject-click FRAME,X,Y[,right]: a real SDL button down/up pair in
    // window pixels, so tests drive the interactive input path.
    for (const std::string& spec : opt.inject_clicks) {
        int at = 0;
        float ix = 0.0f;
        float iy = 0.0f;
        char side[8] = {};
        const int n = std::sscanf(spec.c_str(), "%d,%f,%f,%7s", &at, &ix, &iy, side);
        if (n < 3 || at != frame) {
            continue;
        }
        const Uint8 button =
            (n == 4 && std::string(side) == "right") ? SDL_BUTTON_RIGHT : SDL_BUTTON_LEFT;
        for (const Uint32 type : {SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_EVENT_MOUSE_BUTTON_UP}) {
            SDL_Event injected{};
            injected.type = type;
            injected.button.windowID = SDL_GetWindowID(window);
            injected.button.button = button;
            injected.button.down = type == SDL_EVENT_MOUSE_BUTTON_DOWN;
            injected.button.clicks = 1;
            injected.button.x = ix;
            injected.button.y = iy;
            SDL_PushEvent(&injected);
        }
    }
}

void Game::handle_menu_input(const SDL_Event& event) {
    if (event.type == SDL_EVENT_QUIT) {
        running = false;
    } else if (menu_loaded && opt.use_menu) {
        const float off_x =
            (static_cast<float>(logical_width) - 800.0f) * 0.5f;
        const float off_y =
            (static_cast<float>(logical_height) - 600.0f) * 0.5f;
        if (event.type == SDL_EVENT_MOUSE_MOTION) {
            if (!menu_hover_forced) {
                menu.move(static_cast<int>(event.motion.x - off_x),
                          static_cast<int>(event.motion.y - off_y));
            }
            if (menu.scroll_drag_active &&
                !menu.scroll_drag_list.empty()) {
                menu.list_drag_to(
                    menu.scroll_drag_list,
                    static_cast<int>(event.motion.y - off_y));
            }
            if (menu.slider_drag_active &&
                !menu.slider_drag_name.empty()) {
                menu.slider_drag_to(
                    menu.slider_drag_name,
                    static_cast<int>(event.motion.x - off_x));
                apply_menu_options();
            }
        } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
                   event.button.button == SDL_BUTTON_LEFT) {
            menu.scroll_drag_active = false;
            menu.scroll_drag_list.clear();
            menu.slider_drag_active = false;
            menu.slider_drag_name.clear();
            apply_menu_options();
        } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
            // VSCROLL/HSCROLL wheel: one notch = one line
            // (0x5cbb7f / 0x5be7e0, pos -= delta / 120).
            const int mx = static_cast<int>(
                event.wheel.mouse_x - off_x);
            const int my = static_cast<int>(
                event.wheel.mouse_y - off_y);
            const int hit = menu.hit_test(mx, my);
            if (hit >= 0) {
                const dino::ui::Widget& list =
                    menu.overlay.widgets[static_cast<std::size_t>(
                        hit)];
                int notches =
                    static_cast<int>(event.wheel.y);
                if (notches == 0) {
                    notches = event.wheel.y > 0.0f ? 1 : -1;
                }
                if (list.type == "LIST" &&
                    list.list_scroll_width > 0) {
                    menu.list_scroll(list.name, -notches);
                } else if (list.type == "HSCROLL") {
                    menu.slider_step(list.name, -notches);
                    apply_menu_options();
                }
            }
        } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                   event.button.button == SDL_BUTTON_LEFT) {
            const int mx = static_cast<int>(event.button.x - off_x);
            const int my = static_cast<int>(event.button.y - off_y);
            const int menu_hit = menu.hit_test(mx, my);
            if (menu_hit >= 0) {
                const std::string& name =
                    menu.overlay.widgets[static_cast<std::size_t>(
                        menu_hit)]
                        .name;
                const std::string sound_name =
                    name == "QUIT_BUTTON" ? "exitClic" : "clicDefault";
                const auto sound_it =
                    menu_sound_files.find(sound_name);
                if (sound_it != menu_sound_files.end()) {
                    ui_sound.play(sound_it->second);
                }
            }
            menu.click(mx, my);
            apply_menu_options();
            queue_menu_load();
            if (!pending_menu_load_path.empty()) {
                do_port_load(pending_menu_load_path);
                pending_menu_load_path.clear();
                menu.loading = false;
                menu.active = false;
            }
            queue_menu_cinematic();
            play_pending_cinematic();
            if (menu.quit) {
                running = false;
            }
            if (!menu.active) {
                start_screen = false;
            }
        } else if (event.type == SDL_EVENT_KEY_DOWN &&
                   !event.key.repeat) {
            if (event.key.key == SDLK_ESCAPE) {
                menu.escape();
                if (menu.quit) {
                    running = false;
                }
            } else if (event.key.key == SDLK_RETURN && menu.page == 0) {
                menu.page = 1;
            }
        }
    } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
        if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE) {
            start_screen = false;
        } else if (event.key.key == SDLK_ESCAPE) {
            running = false;
        }
    }
}

void Game::handle_key_input(const SDL_Event& event) {
    const SDL_Keycode key = event.key.key;
    if (park.name_edit_active() && key == SDLK_BACKSPACE) {
        park.backspace_name();
        opt.save_name = park.save_name();
    } else if (key == SDLK_ESCAPE && park.loaded() &&
               park.is_visible("OPTIONS_PANEL")) {
        // Retail: Esc is the overlay-close key. With no tool it
        // is a no-op in the park, but it closes OPTIONS_PANEL
        // (PauseGame is P, not Esc).
        park.overlay.set_visible("OPTIONS_PANEL", false);
        std::printf("options: closed (Esc)\n");
    } else if (key == SDLK_ESCAPE &&
               (build_tool != 0 || pending_store_index >= 0 ||
                fence_corner_x >= 0 || road_has_start ||
                selected_paddock >= 0 || park_sell_mode ||
                park_sell_paddock >= 0)) {
        // `pvwStateFenceConstruction` vtable +0x1c = 0x52be60:
        // Esc leaves the state; +0x08 = 0x5269c0 hides its panel.
        // The road/building/show states use the same common leave.
        if (build_tool == 6 || build_tool == 2) {
            std::printf("fence: tool off (Esc)\n");
        } else if (build_tool == 4) {
            std::printf("road: tool off (Esc)\n");
        } else if (build_tool == 3) {
            std::printf("construction: tool off (Esc)\n");
        } else if (build_tool == 5) {
            std::printf("show: tool off (Esc)\n");
        }
        if (pending_store_index >= 0) {
            std::printf("store: selection off (Esc)\n");
        }
        if (selected_paddock >= 0) {
            selected_paddock = -1;
            std::printf("paddock: deselected (Esc)\n");
            rebuild_build_meshes();
            refresh_fence_info_panel();
        }
        if (park_sell_mode || park_sell_paddock >= 0) {
            park_sell_mode = false;
            park_sell_paddock = -1;
            if (park.dialog_open()) {
                park.close_dialog();
            }
            std::printf("sell: mode off (Esc)\n");
        }
        build_tool = 0;
        fence_corner_x = -1;
        fence_corner_y = -1;
        pending_construct_kind = -1;
        pending_show_index = -1;
        pending_store_index = -1;
        road_has_start = false;
        park.overlay.set_visible(
            "FENCE_CONSTRUCTION_AND_INFO_PANEL", false);
        park.overlay.set_visible("CONSTRUCTION_BUILDINGS_PANEL",
                                 false);
        park.overlay.set_visible(
            "CONSTRUCTION_ROAD_METRO_PANEL", false);
        park.overlay.set_visible("PREDEFINED_SHOW_PANEL", false);
        park.overlay.set_visible("DINO_ACHAT_SAVE_PANEL", false);
    } else if (key == keymap[dino::KeyAction::Quit]) {
        // Retail has no Escape binding in the default park state
        // (the state manager binds it only on the construction /
        // purchase / selected states, which handle it above).
        // Never quit the viewer from the park; quitting goes
        // through the menu's Quit Game / return-to-menu flow.
        if (!park.loaded()) {
            running = false;
        }
    } else if (key == keymap[dino::KeyAction::Reset]) {
        yaw = opt.yaw;
        pitch = opt.pitch;
        distance = auto_distance;
    } else if (key == keymap[dino::KeyAction::Fullscreen]) {
        opt.fullscreen = !opt.fullscreen;
        SDL_SetWindowFullscreen(window, opt.fullscreen);
    } else if (key == keymap[dino::KeyAction::Borderless]) {
        opt.borderless = !opt.borderless;
        SDL_SetWindowBordered(window, !opt.borderless);
    } else if (key == keymap[dino::KeyAction::Mute]) {
        opt.muted = !opt.muted;
        if (audio != nullptr) {
            SDL_SetAudioStreamGain(audio, opt.muted ? 0.0f : 1.0f);
        }
        ui_sound.set_muted(opt.muted);
        music.set_gain(opt.muted
                           ? 0.0f
                           : std::clamp(opt.music_volume / 100.0f,
                                        0.0f, 1.0f));
    } else if (key == keymap[dino::KeyAction::ToggleCamera]) {
        free_camera = !free_camera;
        if (free_camera) {
            enter_free_camera();
        }
    } else if (key == SDLK_F1) {
        hud_visible = !hud_visible;
    } else if (key == SDLK_TAB) {
        show_help = !show_help;
    } else if (key == SDLK_1) {
        build_tool = 1;
        std::printf("build tool: path\n");
    } else if (key == SDLK_2) {
        build_tool = 2;
        std::printf("build tool: fence\n");
    } else if (key == SDLK_0) {
        build_tool = 0;
        pending_construct_kind = -1;
        std::printf("build tool: off\n");
    } else if (key == SDLK_U) {
        if (build_grid.undo()) {
            rebuild_build_meshes();
        }
    }
}

void Game::handle_input() {
    sync_park_layout();
    apply_injected_input();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (opt.no_input) {
            continue;
        }
        if (start_screen) {
            handle_menu_input(event);
            continue;
        }
        if (event.type == SDL_EVENT_QUIT) {
            running = false;
        } else if (event.type == SDL_EVENT_TEXT_INPUT) {
            if (park.loaded() && park.name_edit_active() &&
                park.options_tab() == "save") {
                for (const char* c = event.text.text; *c != '\0'; ++c) {
                    park.append_name_char(*c);
                }
                opt.save_name = park.save_name();
            }
        } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
            handle_key_input(event);
        } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                   event.button.button == SDL_BUTTON_LEFT) {
            if (park.loaded()) {
                int dx = 0, dy = 0;
                park_design(event.button.x, event.button.y, dx, dy);
                if (!park_hover_forced) {
                    park_hover_x = dx;
                    park_hover_y = dy;
                }
                const int hit = park.overlay.hit_test(dx, dy);
                std::string action;
                if (park.click(dx, dy, action)) {
                    if (park.return_to_menu_requested()) {
                        park.clear_return_to_menu_request();
                        activate_main_menu();
                    }
                    if (hit >= 0) {
                        park_active_index = hit;
                    }
                    if (action.empty() && std::getenv("DINO_DEBUG_CLICKS") != nullptr) {
                        std::printf("park click %d,%d -> consumed by %s\n", dx, dy,
                                    hit >= 0 ? park.overlay.widgets[static_cast<std::size_t>(hit)]
                                                   .name.c_str()
                                             : "?");
                    }
                    ui_click_consumed = true;
                    // Clicking the construction panel's background/close
                    // (consumed with no widget action) leaves the retail
                    // FENCE_CONSTRUCTION state, so the next ground click
                    // can select an enclosure.
                    if (hit < 0 && action.empty() && build_tool == 6) {
                        build_tool = 0;
                        fence_corner_x = -1;
                        fence_corner_y = -1;
                        pending_construct_kind = -1;
                        std::printf("fence: tool off (panel close)\n");
                    }
                    if (action == "minimap zoom in") {
                        minimap_zoom_in = true;
                    } else if (action == "minimap zoom out") {
                        minimap_zoom_out = true;
                    } else if (action == "camera pan forward") {
                        nav_pan_forward = true;
                    } else if (action == "camera pan back") {
                        nav_pan_back = true;
                    } else if (action == "camera pan left") {
                        nav_pan_left = true;
                    } else if (action == "camera pan right") {
                        nav_pan_right = true;
                    } else if (action == "camera turn left") {
                        nav_turn_left = true;
                    } else if (action == "camera turn right") {
                        nav_turn_right = true;
                    } else if (action == "camera zoom in") {
                        nav_zoom_in = true;
                    } else if (action == "camera zoom out") {
                        nav_zoom_out = true;
                    } else if (action == "save port" ||
                               action == "load port") {
                        const std::string path =
                            port_save_target(action == "load port");
                        if (action == "save port") {
                            do_port_save(path);
                        } else {
                            do_port_load(path);
                        }
                        park.overlay.set_visible("OPTIONS_PANEL", false);
                    } else if (action == "close campaign end") {
                        advance_campaign();
                    } else if (action.rfind("close ", 0) == 0) {
                        // Leaving a construction/info panel deactivates
                        // its placement tool (retail's info-panel
                        // controller state deactivation). The openers
                        // toggle, so the same icon that armed the tool
                        // closes the panel and frees the next click.
                        const std::string panel = action.substr(6);
                        if (panel ==
                            "FENCE_CONSTRUCTION_AND_INFO_PANEL") {
                            if (build_tool == 6) {
                                build_tool = 0;
                                fence_corner_x = -1;
                                fence_corner_y = -1;
                                pending_construct_kind = -1;
                                std::printf(
                                    "fence: tool off (panel closed)\n");
                            }
                        } else if (
                            panel == "CONSTRUCTION_BUILDINGS_PANEL" ||
                            panel == "CONSTRUCTION_PANEL") {
                            if (build_tool == 3) {
                                build_tool = 0;
                                pending_construct_kind = -1;
                                std::printf(
                                    "construction: tool off (panel "
                                    "closed)\n");
                            }
                        } else if (
                            panel == "CONSTRUCTION_ROAD_METRO_PANEL") {
                            if (build_tool == 4) {
                                build_tool = 0;
                                road_has_start = false;
                                std::printf(
                                    "road: tool off (panel closed)\n");
                            }
                        } else if (
                            panel == "PREDEFINED_SHOW_PANEL" ||
                            panel == "CONSTRUCTION_KIT_PANEL") {
                            if (build_tool == 5) {
                                build_tool = 0;
                                pending_show_index = -1;
                                std::printf(
                                    "show: tool off (panel closed)\n");
                            }
                        } else if (panel == "DINO_ACHAT_SAVE_PANEL") {
                            pending_store_index = -1;
                        }
                    } else if (action == "sell mode") {
                        // HUD Sell switch (`OPEN_SELLING`, state 0x14):
                        // arm the shared object-sell state.
                        pending_sell_mode = true;
                        if (park_sell_mode) {
                            // Toggling the switch leaves the state.
                            park_sell_mode = false;
                            park_sell_paddock = -1;
                            if (park.dialog_open()) {
                                park.close_dialog();
                            }
                            std::printf("sell: mode off\n");
                            pending_sell_mode = false;
                        }
                    } else if (action == "accept request") {
                        if (park_sell_paddock >= 0) {
                            sell_paddock_now(park_sell_paddock);
                        }
                    } else if (action == "cancel request") {
                        if (park_sell_paddock >= 0) {
                            park_sell_paddock = -1;
                            std::printf("sell: cancelled\n");
                        }
                    }
                    if (!action.empty()) {
                        std::printf("park click %d,%d -> %s\n", dx, dy,
                                    action.c_str());
                        const auto sound_it =
                            menu_sound_files.find("clicDefault");
                        if (sound_it != menu_sound_files.end()) {
                            ui_sound.play(sound_it->second);
                        }
                        if (action.rfind("open DINO_LAB", 0) == 0 ||
                            action.rfind("open LAB_", 0) == 0) {
                            const auto lab_music =
                                park_sound_files.find("musLabo");
                            if (lab_music != park_sound_files.end()) {
                                music.play(lab_music->second);
                            }
                        } else if (action == "close lab" &&
                                   !lnd.music.empty()) {
                            const auto island_music =
                                park_sound_files.find(lnd.music);
                            if (island_music != park_sound_files.end()) {
                                music.play(island_music->second);
                            }
                        }
                        // A menu-started free game has no build grid yet;
                        // every construction tool needs one for its
                        // ground clicks.
                        if (action.rfind("construct ", 0) == 0) {
                            ensure_build_ready();
                        }
                        if (action ==
                            "open FENCE_CONSTRUCTION_AND_INFO_PANEL") {
                            // Retail arms the enclosure tool when the
                            // Enclosure panel opens; the ground clicks
                            // then add fence edges.
                            ensure_build_ready();
                            build_tool = 6;
                            fence_corner_x = -1;
                            fence_corner_y = -1;
                            pending_construct_kind = -1;
                            std::printf(
                                "fence: tool armed (click two corners)\n");
                        }
                        if (action == "construct CONSTRUC_ROAD_BUTTON") {
                            arm_road_tool();
                        } else if (action.rfind("construct ", 0) == 0) {
                            const std::string widget = action.substr(10);
                            const int kind =
                                construction_kind_for_widget(widget);
                            if (kind < 0) {
                                std::printf(
                                    "construction: no building for %s\n",
                                    widget.c_str());
                            } else {
                                const dino::BuildingDef& def =
                                    construction_defs[static_cast<
                                        std::size_t>(kind)];
                                if (park_budget <
                                    static_cast<int>(
                                        def.construction_cost)) {
                                    std::printf(
                                        "construction: %s needs %u, "
                                        "budget %d "
                                        "(WARNING_NOT_ENOUGH_MONEY)\n",
                                        def.name.c_str(),
                                        def.construction_cost,
                                        park_budget);
                                } else {
                                    build_tool = 3;
                                    pending_construct_kind = kind;
                                    std::printf(
                                        "construction: %s selected, "
                                        "cost=%u (click the ground)\n",
                                        def.name.c_str(),
                                        def.construction_cost);
                                }
                            }
                        }
                        if (action.rfind("store buy ", 0) == 0) {
                            ensure_build_ready();  // paddock pick click
                            handle_store_buy(
                                std::atoi(action.c_str() + 10));
                        } else if (action == "store no selection") {
                            std::printf("store: no dinosaur selected\n");
                        } else if (action.rfind("show place ", 0) == 0) {
                            handle_show_place(
                                std::atoi(action.c_str() + 11));
                        } else if (action == "show no selection") {
                            std::printf("show: no show selected\n");
                        } else if (action.rfind("attraction ", 0) == 0) {
                            handle_attraction_action(action);
                        }
                    }
                    continue;
                }
            }
            dragging = true;
            drag_distance = 0.0f;
            last_mx = event.button.x;
            last_my = event.button.y;
        } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                   event.button.button == SDL_BUTTON_RIGHT &&
                   build_tool == 0 && selected_paddock >= 0) {
            // Retail leaves the selected-object state on a right click.
            selected_paddock = -1;
            std::printf("paddock: deselected\n");
            rebuild_build_meshes();
            refresh_fence_info_panel();
        } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                   event.button.button == SDL_BUTTON_RIGHT &&
                   build_tool == 4) {
            // 0x52ed6a: cancel the run in progress, else disarm the tool.
            if (road_has_start) {
                road_has_start = false;
                std::printf("road: segment cancelled\n");
            } else {
                build_tool = 0;
                std::printf("road: tool off\n");
            }
        } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
                   event.button.button == SDL_BUTTON_LEFT) {
            park_active_index = -1;
            minimap_zoom_in = false;
            minimap_zoom_out = false;
            nav_pan_forward = false;
            nav_pan_back = false;
            nav_pan_left = false;
            nav_pan_right = false;
            nav_turn_left = false;
            nav_turn_right = false;
            nav_zoom_in = false;
            nav_zoom_out = false;
            dragging = false;
            if (ui_click_consumed) {
                ui_click_consumed = false;
                continue;
            }
            if (std::getenv("DINO_DEBUG_CLICKS") != nullptr) {
                std::printf("ground release %.0f,%.0f drag=%.1f tool=%d build_ready=%d\n",
                            event.button.x, event.button.y, drag_distance,
                            build_tool, build_ready ? 1 : 0);
            }
            if ((build_tool != 0 || pending_store_index >= 0 ||
                 build_ready) &&
                drag_distance < 5.0f) {
                pending_click = true;
                click_x = event.button.x;
                click_y = event.button.y;
            }
        } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
            // Retail edge scroll uses the client-pixel cursor position
            // from WM_MOUSEMOVE. Keep the last position; it persists on
            // focus loss (retail keeps scrolling if left in a band).
            int window_w = 0;
            int window_h = 0;
            SDL_GetWindowSize(window, &window_w, &window_h);
            const float mouse_scale_x =
                window_w > 0 ? static_cast<float>(width) / window_w
                             : 1.0f;
            const float mouse_scale_y =
                window_h > 0 ? static_cast<float>(height) / window_h
                             : 1.0f;
            cam_mouse_x = std::clamp(
                static_cast<int>(event.motion.x * mouse_scale_x), 0,
                std::max(0, width - 1));
            cam_mouse_y = std::clamp(
                static_cast<int>(event.motion.y * mouse_scale_y), 0,
                std::max(0, height - 1));
            cam_mouse_valid = true;
            if (park.loaded()) {
                int dx = 0, dy = 0;
                park_design(event.motion.x, event.motion.y, dx, dy);
                if (!park_hover_forced) {
                    park_hover_x = dx;
                    park_hover_y = dy;
                }
            }
            if (!dragging) {
                continue;
            }
            const float dx = event.motion.x - last_mx;
            const float dy = event.motion.y - last_my;
            drag_distance += std::abs(dx) + std::abs(dy);
            if (free_camera) {
                free_yaw += dx * 0.3f;
                free_pitch = std::clamp(free_pitch - dy * 0.3f, -89.0f, 89.0f);
            } else if (camera_retail_model) {
                // Retail has no mouse-drag camera (KeyMapper.ini has no
                // mouse entries and 0x5e5ff0 has no mouse input path).
            } else {
                yaw += dx * 0.4f;
                pitch = std::clamp(pitch + dy * 0.4f, -89.0f, 89.0f);
            }
            last_mx = event.motion.x;
            last_my = event.motion.y;
        } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
            if (camera_retail_model) {
                // Retail has no mouse wheel binding for the park camera.
            } else {
                distance = std::clamp(
                    distance * std::pow(1.1f, -event.wheel.y),
                    camera_config.distance_min,
                    camera_config.distance_max);
            }
        } else if (event.type == SDL_EVENT_WINDOW_RESIZED) {
            width = event.window.data1;
            height = event.window.data2;
        }
    }
}

void Game::update_fence_cursor() {
    if ((build_tool == 2 || build_tool == 6) && build_ready &&
        !opt.no_input) {
        float mxf = 0.0f;
        float myf = 0.0f;
        SDL_GetMouseState(&mxf, &myf);
        if (!opt.fence_cursor.empty()) {
            std::sscanf(opt.fence_cursor.c_str(), "%f,%f", &mxf, &myf);
        }
        dino::Vec3 hit;
        int cx = 0;
        int cy = 0;
        bool visible = false;
        bool ok = false;
        if (pick_terrain(terrain, eye, view_center, mxf, myf,
                         logical_width, logical_height, hit) &&
            build_grid.road_cells_for_world(hit.x, hit.y, cx, cy)) {
            const float cs = build_grid.cell_size;
            const float wx =
                build_grid.origin_x + (static_cast<float>(cx) + 0.5f) * cs;
            const float wy =
                build_grid.origin_y + (static_cast<float>(cy) + 0.5f) * cs;
            // 0x5f8c70 -> 0x5faaa0: the helper snaps the picked point to
            // the nearest 4-unit anchor (cell corner in map units); the
            // disc/grid sphere sits at anchor + step/2 = the cell centre.
            const int start_x = static_cast<int>(static_cast<float>(cx) * cs);
            const int start_y = static_cast<int>(static_cast<float>(cy) * cs);
            // 0x58a190 runs on the snapped helper position: scan
            // `p .. p+margin-1` (margin 4) for a busy high nibble.
            ok = build_grid.occupancy.free_rect(
                start_x, start_y, start_x + 3, start_y + 3);
            // Retail rejects a fence start strictly inside an existing
            // paddock (`WARNING_INSIDE_PERIMETER`). Use the snapped point,
            // the same point the first-click check uses.
            const float rx = static_cast<float>(start_x);
            const float ry = static_cast<float>(start_y);
            for (const dino::WorldRect& rect : road_perimeters()) {
                if (rx > std::min(rect.x0, rect.x1) &&
                    rx < std::max(rect.x0, rect.x1) &&
                    ry > std::min(rect.y0, rect.y1) &&
                    ry < std::max(rect.y0, rect.y1)) {
                    ok = false;
                    break;
                }
            }
            // Mode 1 shows the start disc; after the first corner the
            // helper switches to mode 2 (the grid) and the disc is gone.
            visible = build_tool != 6 || fence_corner_x < 0;
            if (build_tool == 6 && fence_corner_x >= 0 &&
                fence_corner_y >= 0) {
                // 0x52ad00 shows &MESSAGE_FENCE_PERIMETER_COST while the
                // opposite corner is pending; 0x581b90 uses the sorted
                // max-1 rect and Fence.bwi's 4/30.
                const int px = static_cast<int>(
                    static_cast<float>(cx) * cs);
                const int py = static_cast<int>(
                    static_cast<float>(cy) * cs);
                const int rx0 = std::min(fence_corner_x, px);
                const int ry0 = std::min(fence_corner_y, py);
                const int rx1 = std::max(fence_corner_x, px) - 1;
                const int ry1 = std::max(fence_corner_y, py) - 1;
                // The helper grid spans the two snapped corners (not the
                // max-1 cost rect): (100,100)-(116,116) -> centres
                // 102/106/110/114 with a 4-unit step.
                const int tx0 = std::min(fence_corner_x, px);
                const int ty0 = std::min(fence_corner_y, py);
                const int tx1 = std::max(fence_corner_x, px);
                const int ty1 = std::max(fence_corner_y, py);
                const int cost =
                    ((rx1 + ry1) - (rx0 + ry0)) * 2 / 4 * 30;
                if (cost != fence_preview_cost) {
                    fence_preview_cost = cost;
                    if (park.loaded()) {
                        park.show_message(
                            dino::ui::resolve_loc_text(
                                "&MESSAGE_FENCE_PERIMETER_COST",
                                park.loc) +
                            std::to_string(cost));
                    }
                }
                // Pending-rectangle template: helper mode 2 fills the
                // rectangle with helper_sphere dots on a 4-unit grid
                // (diameter 4 interior, 2 on the border row/column),
                // blue when the retail 0x58a4e0 checks pass, maroon
                // otherwise.
                bool template_ok =
                    rx1 - rx0 >= 23 && ry1 - ry0 >= 23;
                if (template_ok) {
                    for (const dino::WorldRect& rect :
                         road_perimeters()) {
                        if (std::min(rect.x0, rect.x1) < rx0 &&
                            std::min(rect.y0, rect.y1) < ry0 &&
                            rx1 < std::max(rect.x0, rect.x1) &&
                            ry1 < std::max(rect.y0, rect.y1)) {
                            template_ok = false;
                            break;
                        }
                    }
                }
                if (template_ok &&
                    !build_grid.occupancy.free_rect(rx0 + 4, ry0 + 4,
                                                    rx1 - 4, ry1 - 4)) {
                    template_ok = false;
                }
                const std::array<int, 6> tkey = {
                    tx0, ty0, tx1, ty1, template_ok ? 1 : 0, 1};
                if (tkey != fence_template_key) {
                    fence_template_key = tkey;
                    fence_template_ok = template_ok;
                    if (fence_sphere_source.positions.empty()) {
                        try {
                            fence_sphere_source = dino::load_mesh(
                                resolve_asset(
                                    opt.asset_root,
                                    "Data/Graphics/focus/"
                                    "helper_sphere.mesh"));
                        } catch (const std::exception& e) {
                            std::fprintf(stderr,
                                         "fence template mesh: %s\n",
                                         e.what());
                        }
                    }
                    fence_template_gpu.destroy(glfns);
                    if (!fence_sphere_source.positions.empty()) {
                        fence_template_gpu = upload_mesh(
                            dino::fence_template_mesh(
                                fence_sphere_source,
                                {tx0, ty0, tx1, ty1}, terrain, 4.0f),
                            glfns);
                    }
                    fence_template_visible =
                        fence_template_gpu.vao != 0;
                    rebuild_extra_items();
                }
            } else if (fence_template_visible) {
                fence_template_visible = false;
                fence_template_key = {-1, -1, -1, -1, -1, -1};
                rebuild_extra_items();
            }
            if (cx != fence_preview_cell_x ||
                cy != fence_preview_cell_y ||
                ok != fence_preview_ok ||
                fence_preview_gpu.vao == 0) {
                fence_preview_cell_x = cx;
                fence_preview_cell_y = cy;
                fence_preview_gpu.destroy(glfns);
                // Mode 1 draws one helper_sphere of scale 8 on the unit
                // mesh (helper step 8) at the cell centre: radius 4.
                fence_preview_gpu = upload_mesh(
                    dino::fence_preview_disc(
                        wx, wy, terrain.height_at(wx, wy) + 0.15f, cs),
                    glfns);
                std::printf("fence preview: cell=%d,%d ok=%d\n", cx, cy,
                            ok ? 1 : 0);
            }
        }
        if (visible != fence_preview_visible ||
            ok != fence_preview_ok) {
            fence_preview_visible = visible;
            fence_preview_ok = ok;
            rebuild_extra_items();
        }
    } else if (fence_preview_visible || fence_template_visible) {
        // After the first corner the disc is hidden but the mode-2 grid
        // is still drawn; leaving the tool must clear both.
        fence_preview_visible = false;
        fence_preview_cell_x = -1;
        fence_preview_cell_y = -1;
        if (fence_template_visible) {
            fence_template_visible = false;
            fence_template_key = {-1, -1, -1, -1, -1, -1};
        }
        rebuild_extra_items();
    }
}

void Game::place_selected_store_item() {
    // FENCE_INSERTION: the click picks the paddock; a full paddock
    // keeps the insertion state and shows the retail warning.
    dino::Vec3 hit;
    if (pick_terrain(terrain, eye, view_center, click_x, click_y,
                     logical_width, logical_height, hit)) {
        const float half = terrain.world_size * 0.5f;
        const float wx = hit.x + half;
        const float wy = hit.y + half;
        std::printf("store: pick world %.1f %.1f\n", wx, wy);
        int paddock_index = -1;
        for (std::size_t p = 0; p < paddocks.size(); ++p) {
            const dino::WorldRect& r = paddocks[p].rect;
            if (wx >= static_cast<float>(std::min(r.x0, r.x1)) &&
                wx <= static_cast<float>(std::max(r.x0, r.x1)) &&
                wy >= static_cast<float>(std::min(r.y0, r.y1)) &&
                wy <= static_cast<float>(std::max(r.y0, r.y1))) {
                paddock_index = static_cast<int>(p);
                break;
            }
        }
        if (paddock_index < 0) {
            std::printf("store: click inside a paddock\n");
        } else if (pending_store_index >=
                   static_cast<int>(store_catalog.size())) {
            pending_store_index = -1;
            pending_store_price = 0;
        } else {
            const dino::MarketEntry entry =
                store_catalog[static_cast<std::size_t>(
                    pending_store_index)];
            std::string species;
            std::string family;
            int cost_class = 0;
            try {
                const std::string genotype_path = resolve_asset(
                    opt.asset_root,
                    "Data/Design/Scenario/Genotype/" +
                        entry.genotype);
                const int head =
                    dino::read_gen_head_type(genotype_path);
                dino::DinoGenotype genotype{};
                bool have_genotype = false;
                try {
                    genotype =
                        dino::read_gen_genotype(genotype_path);
                    have_genotype = true;
                } catch (const std::exception&) {
                }
                const auto match = head_species.find(head);
                species = match != head_species.end()
                              ? match->second
                              : "allo";
                family = family_table.family_of(species);
                if (family.empty()) {
                    family = "LightBiped";
                }
                cost_class =
                    (have_genotype && phenotype_table.valid)
                        ? phenotype_table.size_class(genotype)
                        : (size_for_species
                               ? size_for_species(species)
                               : 1);
                const int dino_id =
                    1000000 +
                    static_cast<int>(imported_dinos.size());
                dino::Paddock& paddock =
                    paddocks[static_cast<std::size_t>(
                        paddock_index)];
                if (!dino::paddock_can_add(paddock, dino_id,
                                           cost_class)) {
                    std::printf(
                        "store: no room in paddock %d "
                        "(WARNING_NO_ROOM_IN_FENCE)\n",
                        paddock_index);
                    park.show_message(
                        dino::ui::resolve_loc_text(
                            "&WARNING_NO_ROOM_IN_FENCE",
                            park.loc));
                } else {
                    const bool have_attributes =
                        have_genotype && phenotype_table.valid;
                    const dino::DinoAttributes store_attrs =
                        have_attributes
                            ? phenotype_table.attributes(genotype)
                            : dino::DinoAttributes{};
                    if (add_imported_dino(
                            species, family, opt.dino_clip, wx,
                            wy, 0.0f,
                            have_attributes ? &store_attrs
                                            : nullptr)) {
                        dino::paddock_add(paddock, dino_id,
                                          cost_class);
                        park_budget -= pending_store_price;
                        budget_ledger.spend_dinoachat(
                            pending_store_price);
                        store_catalog.erase(
                            store_catalog.begin() +
                            pending_store_index);
                        refresh_store_rows();
                        park_paddocks = paddocks;
                        float total_used = 0.0f;
                        for (const dino::Paddock& p : paddocks) {
                            total_used += p.used;
                        }
                        std::printf(
                            "store: bought %s price=%d budget=%d "
                            "paddock=%d dinos=%zu "
                            "dinoachat=%d/%d used=%.0f "
                            "total=%.0f\n",
                            entry.name.c_str(),
                            pending_store_price, park_budget,
                            paddock_index, imported_dinos.size(),
                            budget_ledger.dinoachat_count,
                            budget_ledger.dinoachat_total,
                            paddock.used, total_used);
                        pending_store_index = -1;
                        pending_store_price = 0;
                        rebuild_extra_items();
                    } else {
                        std::printf(
                            "store: %s model load failed\n",
                            entry.name.c_str());
                    }
                }
            } catch (const std::exception& e) {
                std::fprintf(stderr, "store: %s\n", e.what());
            }
        }
    }
}

void Game::apply_build_tool_click() {
    dino::Vec3 hit;
    const bool picked = pick_terrain(terrain, eye, view_center, click_x,
                                     click_y, logical_width, logical_height, hit);
    if (std::getenv("DINO_DEBUG_CLICKS") != nullptr) {
        std::printf("ground click %.0f,%.0f picked=%d hit=%.1f,%.1f\n", click_x,
                    click_y, picked ? 1 : 0, hit.x, hit.y);
    }
    if (picked) {
        int cx = 0;
        int cy = 0;
        // The road cursor and the fence helper both snap to the
        // nearest 4-unit anchor (0x5faaa0); the other build tools
        // use the floored cell.
        if ((build_tool == 4 || build_tool == 6
                 ? build_grid.road_cells_for_world(hit.x, hit.y,
                                                   cx, cy)
                 : build_grid.cells_for_world(hit.x, hit.y, cx,
                                              cy))) {
            const int before = build_grid.path_count + build_grid.fence_count;
            const float cs = build_grid.cell_size;
            auto cell_buildable = [&](int gx, int gy) {
                return terrain.buildable(
                    build_grid.origin_x +
                        (static_cast<float>(gx) + 0.5f) * cs,
                    build_grid.origin_y +
                        (static_cast<float>(gy) + 0.5f) * cs);
            };
            if (build_tool == 4) {
                road_click(cx, cy);  // rebuilds the meshes itself
            } else if (build_tool == 1) {
                const bool removing = build_grid.path_at(cx, cy);
                if (!removing && !cell_buildable(cx, cy)) {
                    std::printf("placement: blocked cell\n");
                } else {
                    build_grid.set_path(cx, cy, !removing);
                }
            } else if (build_tool == 6) {
                // Retail FENCE_CONSTRUCTION: the helper snaps the
                // click to the nearest anchor (0x5f8c70) and
                // stores that corner; the second click builds the
                // sorted max-1 rectangle.
                const int px = static_cast<int>(
                    static_cast<float>(cx) * cs);
                const int py = static_cast<int>(
                    static_cast<float>(cy) * cs);
                auto inside_paddock = [&](float rx, float ry) {
                    for (const dino::Paddock& paddock : paddocks) {
                        const dino::WorldRect& rect =
                            paddock.rect;
                        if (rx > std::min(rect.x0, rect.x1) &&
                            rx < std::max(rect.x0, rect.x1) &&
                            ry > std::min(rect.y0, rect.y1) &&
                            ry < std::max(rect.y0, rect.y1)) {
                            return true;
                        }
                    }
                    return false;
                };
                if (fence_corner_x < 0) {
                    // 0x58a190: scan `p .. p+margin-1` (margin 4
                    // from Fence.bwi) for a busy high nibble and
                    // the height-slope rule; point strictly
                    // inside an existing perimeter -> code 3.
                    const bool busy =
                        !build_grid.occupancy.free_rect(
                            px, py, px + 3, py + 3);
                    if (busy) {
                        road_message("&WARNING_BUSY_SPACE");
                    } else if (inside_paddock(
                                   static_cast<float>(px),
                                   static_cast<float>(py))) {
                        road_message(
                            "&WARNING_INSIDE_PERIMETER");
                    } else {
                        fence_corner_x = px;
                        fence_corner_y = py;
                        std::printf(
                            "fence: corner %d,%d (click opposite "
                            "corner)\n",
                            px, py);
                    }
                } else {
                    // 0x52bbf0 sorts the two clicked points and
                    // subtracts 1 from the high corner.
                    const int x0 =
                        std::min(fence_corner_x, px);
                    const int y0 =
                        std::min(fence_corner_y, py);
                    const int x1 =
                        std::max(fence_corner_x, px) - 1;
                    const int y1 =
                        std::max(fence_corner_y, py) - 1;
                    std::string warning;
                    int cost = 0;
                    // 0x58a4e0: a side shorter than
                    // 6*margin-1 (margin 4 -> 23 units) is
                    // PERIMETER_TOO_SMALL, then the inside test,
                    // then the inset margin scan.
                    // (The `>= 16` check in 0x527150 is the
                    // stFence LunchBox dish list, max 15 — not a
                    // park enclosure cap; see
                    // notes/fence-panel-sell-recon.md.)
                    if (x1 - x0 < 23 || y1 - y0 < 23) {
                        warning = "&WARNING_PERIMETER_TOO_SMALL";
                    } else if (inside_paddock(
                                   (static_cast<float>(x0) +
                                    static_cast<float>(x1)) *
                                       0.5f,
                                   (static_cast<float>(y0) +
                                    static_cast<float>(y1)) *
                                       0.5f)) {
                        warning = "&WARNING_INSIDE_PERIMETER";
                    } else if (!build_grid.occupancy.free_rect(
                                   x0 + 4, y0 + 4, x1 - 4,
                                   y1 - 4)) {
                        warning = "&WARNING_BUSY_SPACE";
                    } else {
                        // 0x581b90: ((x1+y1)-(x0+y0))*2/
                        // cell*price; Fence.bwi cell 4, price 30.
                        cost = ((x1 + y1) - (x0 + y0)) * 2 / 4 *
                               30;
                        if (park_budget < cost) {
                            warning =
                                "&WARNING_NOT_ENOUGH_MONEY";
                        }
                    }
                    if (!warning.empty()) {
                        road_message(warning.c_str());
                    } else {
                        const int before =
                            build_grid.fence_count;
                        const int cx0 = std::max(
                            0, static_cast<int>(
                                   std::floor(x0 / cs)));
                        const int cx1 = static_cast<int>(
                            std::floor(x1 / cs));
                        const int cy0 = std::max(
                            0, static_cast<int>(
                                   std::floor(y0 / cs)));
                        const int cy1 = static_cast<int>(
                            std::floor(y1 / cs));
                        for (int x = cx0; x <= cx1; ++x) {
                            build_grid.set_fence(x, cy0, 0, true);
                            build_grid.set_fence(x, cy1 + 1, 0,
                                                 true);
                        }
                        for (int y = cy0; y <= cy1; ++y) {
                            build_grid.set_fence(cx0 - 1, y, 1,
                                                 true);
                            build_grid.set_fence(cx1, y, 1,
                                                 true);
                        }
                        // stPerimeter's four wall lines: 1 map
                        // unit thick, inclusive min .. max-1.
                        build_grid.occupancy.stamp_wall_rect(
                            x0, y0, x1, y1, true);
                        dino::Paddock paddock;
                        paddock.rect = {x0, y0, x1, y1};
                        paddock.capacity = dino::paddock_capacity(
                            paddock.rect, cs);
                        // 0x5ec7e0: rand() & 3 picks the ground
                        // atlas quadrant per stFence.
                        paddock.ground_variant = std::rand() & 3;
                        paddock.name = "Enclosure" +
                                       std::to_string(
                                           paddocks.size() + 1);
                        paddocks.push_back(paddock);
                        park_budget -= cost;
                        budget_ledger.spend_construction(cost);
                        std::printf(
                            "fence: built rect %d,%d-%d,%d "
                            "edges=%d paddock=%zu capacity=%.0f "
                            "cost=%d budget=%d "
                            "construction=%d/%d\n",
                            x0, y0, x1, y1,
                            build_grid.fence_count - before,
                            paddocks.size(),
                            static_cast<double>(paddock.capacity),
                            cost, park_budget,
                            budget_ledger.construction_count,
                            budget_ledger.construction_total);
                        rebuild_build_meshes();
                    }
                    fence_corner_x = -1;
                    fence_corner_y = -1;
                }
            } else if (build_tool == 3 &&
                       pending_construct_kind >= 0) {
                const float half = terrain.world_size * 0.5f;
                try_construct(
                    pending_construct_kind,
                    static_cast<int>(std::floor(hit.x + half)),
                    static_cast<int>(std::floor(hit.y + half)), 0);
            } else if (build_tool == 5 && pending_show_index >= 0) {
                // Grid cell -> show record coordinate (0..1024).
                place_show_at(
                    static_cast<int>(std::lround(cx * cs)),
                    static_cast<int>(std::lround(cy * cs)));
            } else {
                const float x0 = build_grid.origin_x + static_cast<float>(cx) * cs;
                const float y0 = build_grid.origin_y + static_cast<float>(cy) * cs;
                float best = 1e30f;
                int ex = cx;
                int ey = cy;
                int edge = 0;
                auto consider = [&](float distance, int gx, int gy, int e) {
                    if (distance < best) {
                        best = distance;
                        ex = gx;
                        ey = gy;
                        edge = e;
                    }
                };
                if (cx > 0) {
                    consider(std::fabs(hit.x - x0), cx - 1, cy, 1);
                }
                consider(std::fabs(hit.x - (x0 + cs)), cx, cy, 1);
                consider(std::fabs(hit.y - y0), cx, cy, 0);
                if (cy + 1 < build_grid.height) {
                    consider(std::fabs(hit.y - (y0 + cs)), cx, cy + 1, 0);
                }
                const bool removing =
                    build_grid.fence_at(ex, ey, edge);
                bool allowed = removing;
                if (!allowed) {
                    const bool inside = cell_buildable(ex, ey);
                    bool neighbour = true;
                    if (edge == 1) {
                        if (ex + 1 < build_grid.width) {
                            neighbour = cell_buildable(ex + 1, ey);
                        }
                    } else if (ey > 0) {
                        neighbour = cell_buildable(ex, ey - 1);
                    }
                    allowed = inside && neighbour;
                }
                if (!allowed) {
                    std::printf("placement: blocked cell\n");
                } else {
                    build_grid.set_fence(ex, ey, edge, !removing);
                }
            }
            if (build_tool != 4 &&
                build_grid.path_count + build_grid.fence_count != before) {
                rebuild_build_meshes();
            }
        }
    }
}

void Game::apply_park_click() {
    // Sell mode (`OPEN_SELLING`, state 0x14) or paddock
    // selection (retail `gatFence` -> pass B). Clicking empty
    // ground/right-click deselects; the Dino_SelectedWall pulse
    // is pass B. The retail info panel is still a separate task.
    dino::Vec3 hit;
    if (pick_terrain(terrain, eye, view_center, click_x, click_y,
                     logical_width, logical_height, hit)) {
        const float half = terrain.world_size * 0.5f;
        const float mx = hit.x + half;
        const float my = hit.y + half;
        const int index = paddock_at_map(mx, my);
        if (park_sell_mode) {
            if (index < 0) {
                std::printf("sell: no enclosure under the cursor\n");
            } else {
                const dino::Paddock& pen =
                    paddocks[static_cast<std::size_t>(index)];
                if (!pen.occupants.empty() || pen.used > 0.0f) {
                    park.show_warning(
                        dino::ui::resolve_loc_text(
                            "&WARNING_SELLING_NON_EMPTY_FENCE",
                            park.loc));
                    std::printf(
                        "sell: enclosure %d refused (occupied)\n",
                        index);
                } else {
                    const int value = dino::fence_sell_value(
                        pen.rect, build_grid.cell_size, 30, 4,
                        park_sellback);
                    park_sell_paddock = index;
                    park.show_request(
                        dino::ui::resolve_loc_text(
                            "&REQUEST_SELL_FENCE", park.loc) +
                        std::to_string(value));
                    std::printf(
                        "sell: enclosure %d value=%d\n", index,
                        value);
                }
            }
        } else if (index != selected_paddock) {
            selected_paddock = index;
            if (index >= 0) {
                std::printf("paddock: selected %d\n", index);
            } else {
                std::printf("paddock: deselected\n");
            }
            rebuild_build_meshes();
            refresh_fence_info_panel();
        }
    }
}

void Game::update_placement() {
    if (pending_fence_tool) {
        ensure_build_ready();
        build_tool = 6;
        fence_corner_x = -1;
        fence_corner_y = -1;
        pending_construct_kind = -1;
        pending_fence_tool = false;
        std::printf("fence: tool armed (click two corners)\n");
    }
    update_fence_cursor();
    if (pending_click) {
        pending_click = false;
        if (pending_store_index >= 0 && build_ready) {
            place_selected_store_item();
        } else if (build_ready && build_tool != 0) {
            apply_build_tool_click();
        } else if (build_ready) {
            apply_park_click();
        }
    }

    if (frame == 0 && !opt.pick_test.empty()) {
        int mx = 0;
        int my = 0;
        if (std::sscanf(opt.pick_test.c_str(), "%d,%d", &mx, &my) == 2) {
            dino::Vec3 hit;
            if (pick_terrain(terrain, eye, view_center, static_cast<float>(mx),
                             static_cast<float>(my), logical_width,
                             logical_height, hit)) {
                const float cs = build_ready ? build_grid.cell_size : 4.0f;
                const float origin = build_ready ? build_grid.origin_x
                                                 : -terrain.world_size * 0.5f;
                const int cx = static_cast<int>(std::floor((hit.x - origin) / cs));
                const int cy = static_cast<int>(std::floor((hit.y - origin) / cs));
                std::printf("pick: %.2f %.2f %.2f cell %d %d\n", hit.x, hit.y,
                            hit.z, cx, cy);
            } else {
                std::printf("pick: none\n");
            }
        }
    }
}

void Game::update_preview() {
    // Road tool preview: the pending run from the start cell to the
    // axis-locked cursor cell, rebuilt only when it changes (0x52e750).
    if (build_ready && build_tool == 4 && road_has_start) {
        float mxf = 0.0f;
        float myf = 0.0f;
        SDL_GetMouseState(&mxf, &myf);
        if (!opt.road_cursor.empty()) {
            std::sscanf(opt.road_cursor.c_str(), "%f,%f", &mxf, &myf);
        }
        dino::Vec3 hit;
        int cx = 0;
        int cy = 0;
        if (pick_terrain(terrain, eye, view_center, mxf, myf, logical_width,
                         logical_height, hit) &&
            build_grid.road_cells_for_world(hit.x, hit.y, cx, cy)) {
            const dino::RoadSegment road =
                dino::road_segment(road_start_x, road_start_y, cx, cy);
            const std::array<int, 4> key = {road.x0, road.y0, road.x1, road.y1};
            if (key != road_preview_key) {
                road_preview_key = key;
                if (road_helper_box.positions.empty()) {
                    try {
                        road_helper_box = dino::load_mesh(resolve_asset(
                            opt.asset_root, "Data/Graphics/focus/helper_road.mesh"));
                    } catch (const std::exception& e) {
                        std::fprintf(stderr, "road helper mesh: %s\n", e.what());
                    }
                }
                road_preview_gpu.destroy(glfns);
                road_preview_gpu = upload_mesh(
                    dino::road_helper_mesh(road_helper_box, road, build_grid, terrain),
                    glfns);
                const int cost = dino::road_cost(road, ensure_road_price());
                road_preview_ok =
                    dino::check_road(build_grid, road, road_perimeters(),
                                     [this](int x, int y) { return road_cell_busy(x, y); }) == dino::RoadCheck::Ok &&
                    park_budget >= cost;
                if (road_preview_texture == 0) {
                    try {
                        road_helper_image = dino::load_tga(resolve_asset(
                            opt.asset_root, "Data/Graphics/Textures/focus/road.tga"));
                    } catch (const std::exception& e) {
                        std::fprintf(stderr, "road helper texture: %s\n", e.what());
                        road_helper_image.width = road_helper_image.height = 1;
                        road_helper_image.rgba = {255, 255, 255, 255};
                    }
                    road_preview_texture = upload_texture(road_helper_image);
                }
                rebuild_extra_items();
                if (cost != road_preview_cost) {
                    road_preview_cost = cost;
                    if (park.loaded()) {
                        park.show_message(dino::ui::resolve_loc_text(
                                              "&MESSAGE_ROAD_COST", park.loc) +
                                          std::to_string(cost));
                    }
                }
            }
        }
        // gscMaterialBank (0x600550): x = trunc((sin(6 t) + 1) * 0.5 * 127);
        // material 2 (valid) = RGB(64, 64, 255 - x), material 3 (invalid)
        // = RGB(255 - x, 64, 64).
        if (road_preview_texture != 0 && road_helper_image.width > 0) {
            const double t = static_cast<double>(SDL_GetTicks()) / 1000.0;
            const int x = static_cast<int>((std::sin(6.0 * t) + 1.0) * 0.5 * 127.0);
            const int color[3] = {road_preview_ok ? 64 : 255 - x, 64,
                                  road_preview_ok ? 255 - x : 64};
            dino::Image tinted = road_helper_image;
            for (std::size_t i = 0; i + 3 < tinted.rgba.size(); i += 4) {
                for (int c = 0; c < 3; ++c) {
                    tinted.rgba[i + c] = static_cast<std::uint8_t>(
                        tinted.rgba[i + c] * color[c] / 255);
                }
            }
            glBindTexture(GL_TEXTURE_2D, road_preview_texture);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tinted.width, tinted.height,
                            GL_RGBA, GL_UNSIGNED_BYTE, tinted.rgba.data());
        }
    } else if (road_preview_gpu.vao != 0) {
        road_preview_gpu.destroy(glfns);
        road_preview_key = {-1, -1, -1, -1};
        road_preview_cost = -1;
        rebuild_extra_items();
    }

    if (park.loaded() && !park.hud_hidden()) {
        const float heading =
            std::atan2(view_center.y - eye.y, view_center.x - eye.x);
        const bool moved =
            std::abs(view_center.x - minimap_focus.x) > 0.5f ||
            std::abs(view_center.y - minimap_focus.y) > 0.5f ||
            std::abs(heading - minimap_heading) > 0.002f ||
            std::abs(minimap_scale - minimap_rendered_scale) > 0.002f;
        if (moved) {
            render_minimap(view_center, heading);
            minimap_focus = view_center;
            minimap_heading = heading;
            minimap_rendered_scale = minimap_scale;
        }
    }
}

}  // namespace dino::app
