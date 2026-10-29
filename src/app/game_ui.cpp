// UI drawing: texture/font lookup, the menu and park HUD quads, and the
// layout helpers they share.

#include "game.h"

namespace dino::app {

UiTexture* Game::ui_texture(const std::string& filename) {
    if (filename.empty()) {
        return nullptr;
    }
    auto it = ui_textures.find(filename);
    if (it != ui_textures.end()) {
        return &it->second;
    }
    try {
        dino::Image img = dino::load_image(resolve_asset(opt.asset_root, filename));
        UiTexture texture;
        texture.id = upload_texture(img);
        texture.width = img.width;
        texture.height = img.height;
        const auto result = ui_textures.emplace(filename, std::move(texture));
        return &result.first->second;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "menu texture: %s\n", e.what());
        return nullptr;
    }

}

const dino::Fnt* Game::ui_font(const std::string& path, int text_height,
                       float& scale_out, GLuint& atlas_out) {
    const std::string font_path =
        path.empty() ? resolve_asset(opt.asset_root, "Data/Fonte/arial12.fnt")
                     : resolve_asset(opt.asset_root, path);
    auto it = ui_fonts.find(font_path);
    if (it == ui_fonts.end()) {
        try {
            dino::Fnt font = dino::load_fnt(font_path);
            it = ui_fonts.emplace(font_path, std::move(font)).first;
        } catch (const std::exception& e) {
            std::fprintf(stderr, "menu font: %s\n", e.what());
            return nullptr;
        }
    }
    const dino::Fnt& font = it->second;
    scale_out = text_height > 0 && font.font_size > 0
                    ? static_cast<float>(text_height) /
                          static_cast<float>(font.font_size)
                    : 1.0f;
    auto at = ui_font_atlases.find(font_path);
    if (at == ui_font_atlases.end()) {
        at = ui_font_atlases.emplace(font_path, upload_font_atlas(font)).first;
    }
    atlas_out = at->second;
    return &font;

}

void Game::draw_ui_mesh(GpuMesh& target, const dino::Mesh& geometry,
                        GLuint texture_id, float alpha, int unlit, int view_w,
                        int view_h) {
    target.destroy(glfns);
    target = upload_mesh(geometry, glfns);
    if (target.vao == 0) {
        return;
    }
    dino::Mat4 ortho;
    ortho.m[0] = 2.0f / static_cast<float>(view_w);
    ortho.m[5] = -2.0f / static_cast<float>(view_h);
    ortho.m[3] = -1.0f;
    ortho.m[7] = 1.0f;
    glfns.UseProgram(program);
    {
        // 2D overlays do not use the 3D pulse tint; reset it so the last
        // fence/selection item cannot darken the HUD.
        const float white[3] = {1.0f, 1.0f, 1.0f};
        glfns.Uniform3fv(glfns.GetUniformLocation(program, "u_tint"), 1,
                         white);
    }
    glfns.UniformMatrix4fv(glfns.GetUniformLocation(program, "u_mvp"), 1, GL_TRUE,
                           ortho.m);
    glfns.Uniform1i(glfns.GetUniformLocation(program, "u_unlit"), unlit);
    glfns.Uniform1i(glfns.GetUniformLocation(program, "u_use_texture"),
                    texture_id != 0 ? 1 : 0);
    glfns.Uniform1i(
        glfns.GetUniformLocation(program, "u_use_texture2"), 0);
    glfns.Uniform1i(
        glfns.GetUniformLocation(program, "u_modulate2x"), 0);
    glfns.Uniform1f(glfns.GetUniformLocation(program, "u_alpha"), alpha);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (texture_id != 0) {
        glfns.ActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture_id);
    }
    glfns.BindVertexArray(target.vao);
    glDrawElements(GL_TRIANGLES, target.index_count, GL_UNSIGNED_INT, nullptr);
    glfns.BindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glfns.UseProgram(0);

}

void Game::draw_menu_sprite(const std::string& name, float x, float y, float w,
                            float h, int sprite_frame, int view_w,
                            int view_h) {
    const dino::ui::Graphic* graphic = menu.overlay.graphic(name);
    if (graphic == nullptr) {
        return;
    }
    UiTexture* sprite_texture = ui_texture(graphic->filename);
    if (sprite_texture == nullptr) {
        return;
    }
    const UiSourceRect source =
        ui_frame_rect(*sprite_texture, *graphic, sprite_frame);
    if (!source.valid) {
        return;
    }
    const float u0 = static_cast<float>(source.x) / sprite_texture->width;
    const float v0 = static_cast<float>(source.y) / sprite_texture->height;
    const float u1 =
        static_cast<float>(source.x + source.w) / sprite_texture->width;
    const float v1 =
        static_cast<float>(source.y + source.h) / sprite_texture->height;
    draw_ui_mesh(menu_gpu,
                 build_ui_quad(x, y, w, h, u0, v0, u1, v1,
                               {255, 255, 255, 255}),
                 sprite_texture->id, 1.0f, 1, view_w, view_h);
}

void Game::draw_menu_hscroll_widget(const dino::ui::Widget& widget, float dpi,
                                    float ox, float oy, int view_w,
                                    int view_h) {
    auto px = [&](int value) { return ox + static_cast<float>(value) * dpi; };
    auto py = [&](int value) { return oy + static_cast<float>(value) * dpi; };
    // Retail HSCROLL children have no INI geometry; the track is
    // the widget texture and the arrows/thumb are derived:
    // U=D=height, thumbW=max(5,W-(U+D+5)-3*range),
    // travel=W-U-D-thumbW, thumbX=U+pos*travel/(range-1).
    const int range = widget.hs_range > 0 ? widget.hs_range : 32;
    const int arrow = widget.height;
    const int thumb_w =
        range <= 1
            ? std::max(0, widget.width - 2 * arrow)
            : std::max(5, widget.width - (2 * arrow + 5) -
                              3 * range);
    const int travel =
        std::max(0, widget.width - 2 * arrow - thumb_w);
    const int pos =
        std::clamp(widget.hs_position, 0, range - 1);
    const float thumb_x =
        px(widget.abs_x + arrow) +
        (range > 1 && travel > 0
             ? static_cast<float>(pos * travel) /
                   static_cast<float>(range - 1) * dpi
             : 0.0f);
    auto draw_scroll_child = [&](const char* suffix, float x, float w) {
        for (const int child_index : widget.children) {
            const dino::ui::Widget& child =
                menu.overlay.widgets[static_cast<std::size_t>(child_index)];
            const std::string suffix_text(suffix);
            if (child.name.size() >= suffix_text.size() &&
                child.name.compare(child.name.size() - suffix_text.size(),
                                   suffix_text.size(), suffix_text) == 0) {
                if (!child.texture.empty()) {
                    // Pick the first frame not in SKIP_LIST; the thumb
                    // graphic skips 0..3 and 7.
                    int child_frame = 0;
                    if (const dino::ui::Graphic* graphic =
                            menu.overlay.graphic(child.texture)) {
                        child_frame = 0;
                        for (int f = 0; f < graphic->objects; ++f) {
                            bool skipped = false;
                            for (const int skip : graphic->skip) {
                                if (skip == f) {
                                    skipped = true;
                                    break;
                                }
                            }
                            if (!skipped) {
                                child_frame = f;
                                break;
                            }
                        }
                    }
                    draw_menu_sprite(
                        child.texture.c_str(), x, py(widget.abs_y), w,
                        static_cast<float>(widget.height) * dpi, child_frame,
                        view_w, view_h);
                }
                return;
            }
        }
    };
    draw_scroll_child("_BTN_LEFT", px(widget.abs_x),
                      static_cast<float>(arrow) * dpi);
    draw_scroll_child("_SCROLLRIGHT",
                      px(widget.abs_x + widget.width - arrow),
                      static_cast<float>(arrow) * dpi);
    draw_scroll_child("_BTN_THUMB", thumb_x,
                      static_cast<float>(thumb_w) * dpi);
}

void Game::draw_menu_list_widget(const dino::ui::Widget& widget, float dpi,
                                 float ox, float oy, int view_w,
                                 int view_h) {
    auto px = [&](int value) { return ox + static_cast<float>(value) * dpi; };
    auto py = [&](int value) { return oy + static_cast<float>(value) * dpi; };
    const int line_h =
        widget.list_line_height > 0 ? widget.list_line_height : 34;
    const int row_w =
        std::max(0, widget.width - widget.list_scroll_width);
    const int visible_rows =
        line_h > 0 ? widget.height / line_h : 0;
    // Retail overwrites LB_COLUMN0_WIDTH with (list.w - vs.w) for
    // one-column lists (0x5bda70); the first visible row is the
    // VSCROLL position.
    const int first_row = menu.list_first_row(widget.name);
    const int total_rows = menu.list_total_rows(widget.name);
    const bool campaign_list = widget.name == "CAMPAIGN_LIST";
    const bool campaign_info =
        widget.name == "CAMPAIGN_INFO_DESCRIPTION";
    const bool load_list =
        widget.name == "LOAD_GAME_NAMES_LIST";
    const bool cinematics_list =
        widget.name == "CINEMATICS_LIST";
    int selected_row = menu.island;
    if (load_list) {
        selected_row = menu.save_slot;
    } else if (campaign_list) {
        selected_row = menu.campaign;
    } else if (cinematics_list) {
        selected_row = menu.cinematic;
    } else if (campaign_info) {
        selected_row = 0;
    }
    const int row_count = std::max(
        0, std::min(visible_rows, total_rows - first_row));
    const dino::ui::Graphic* row_graphic =
        menu.overlay.graphic("ISLAND_LIST_BUTTON");
    UiTexture* row_texture =
        row_graphic != nullptr ? ui_texture(row_graphic->filename)
                               : nullptr;
    const int selected_frame = 7;
    const int normal_frame = 4;
    float list_scale = 1.0f;
    GLuint list_atlas = 0;
    const dino::Fnt* list_font = ui_font(
        widget.font, widget.text_height, list_scale, list_atlas);
    const std::string list_justify =
        widget.justify.empty() ? "CENTER" : widget.justify;
    if (campaign_info) {
        // Page 3 is the campaign briefing: the LIST holds the
        // selected campaign's description wrapped at the list
        // column (235 - 22 = 213 px). The retail break positions
        // match a pixel wrap, not a character count; the VSCROLL
        // scrolls the wrapped lines with a proportional thumb.
        const dino::Campaign* selected = menu.selected_campaign();
        if (selected != nullptr && list_font != nullptr) {
            const std::vector<std::string> lines =
                wrap_lines_pixels(
                    *list_font, list_scale * dpi,
                    dino::ui::resolve_loc_text(
                        selected->description_id, menu.loc),
                    static_cast<float>(row_w) * dpi);
            menu.campaign_info_lines =
                static_cast<int>(lines.size());
            const int first = menu.list_first_row(
                "CAMPAIGN_INFO_DESCRIPTION");
            std::string visible;
            for (int i = first;
                 i < static_cast<int>(lines.size()) &&
                 i < first + visible_rows;
                 ++i) {
                visible += lines[static_cast<std::size_t>(i)];
                visible += '\n';
            }
            const dino::Mesh block = build_text_block(
                *list_font, visible,
                px(widget.abs_x),
                py(widget.abs_y) + 2.0f * dpi, list_scale * dpi,
                widget.text_color, "LEFT",
                static_cast<float>(row_w - 4) * dpi,
                static_cast<float>(widget.height) * dpi,
                static_cast<float>(line_h) * dpi);
            draw_ui_mesh(menu_gpu, block, list_atlas, 1.0f, 2,
                         view_w, view_h);
        }
    } else {
        for (int slot = 0; slot < row_count; ++slot) {
            const int row = first_row + slot;
            const float row_y = py(widget.abs_y) +
                                static_cast<float>(slot) * line_h * dpi;
            const bool locked =
                (campaign_list && !menu.campaign_unlocked(row)) ||
                (cinematics_list && !menu.cinematic_unlocked(row));
            if (row_texture != nullptr && row_graphic != nullptr) {
                const int frame =
                    locked ? 0
                           : (row == selected_row ? selected_frame
                                                  : normal_frame);
                const UiSourceRect source =
                    ui_frame_rect(*row_texture, *row_graphic, frame);
                if (source.valid) {
                    const float u0 =
                        static_cast<float>(source.x) /
                        row_texture->width;
                    const float v0 =
                        static_cast<float>(source.y) /
                        row_texture->height;
                    const float u1 =
                        static_cast<float>(source.x + source.w) /
                        row_texture->width;
                    const float v1 =
                        static_cast<float>(source.y + source.h) /
                        row_texture->height;
                    draw_ui_mesh(
                        menu_gpu,
                        build_ui_quad(
                            px(widget.abs_x), row_y,
                            static_cast<float>(row_w) * dpi,
                            static_cast<float>(line_h) * dpi, u0,
                            v0, u1, v1, {255, 255, 255, 255}),
                        row_texture->id, 1.0f, 1, view_w, view_h);
                }
            }
            if (list_font != nullptr) {
                std::string name;
                if (load_list) {
                    if (row < static_cast<int>(
                                  menu_save_slots.size())) {
                        name = menu_save_slots
                                   [static_cast<std::size_t>(row)]
                                       .second;
                    }
                } else if (campaign_list) {
                    if (row < static_cast<int>(
                                  menu.campaigns.size())) {
                        name = dino::ui::resolve_loc_text(
                            menu.campaigns[static_cast<std::size_t>(
                                               row)]
                                .title_id,
                            menu.loc);
                    }
                } else if (cinematics_list) {
                    if (row < static_cast<int>(
                                  menu.campaigns.size())) {
                        name = dino::ui::resolve_loc_text(
                            menu.campaigns[static_cast<std::size_t>(
                                               row)]
                                .ending_video_name,
                            menu.loc);
                    }
                } else {
                    char field[32];
                    std::snprintf(field, sizeof(field),
                                  "&Isl%02d_Name", row + 1);
                    name = dino::ui::resolve_loc_text(field,
                                                     menu.loc);
                }
                if (!name.empty()) {
                    const dino::Color text_color =
                        locked ? dino::Color{120, 120, 120, 255}
                               : widget.text_color;
                    const dino::Mesh block = build_text_block(
                        *list_font, name, px(widget.abs_x), row_y,
                        list_scale * dpi, text_color,
                        list_justify,
                        static_cast<float>(row_w) * dpi,
                        static_cast<float>(line_h) * dpi);
                    draw_ui_mesh(menu_gpu, block, list_atlas, 1.0f,
                                 2, view_w, view_h);
                }
            }
        }
    }
    if (widget.list_scroll_width > 0) {
        const dino::ui::ListScrollGeometry scroll =
            menu.list_scroll_geometry(widget.name);
        // Retail hides the auto-created VSCROLL when the list
        // fits (range == 1): the campaign list (7 rows in an
        // 8-row viewport) shows bare panel metal, while the
        // island list (10 rows) shows the bar.
        if (scroll.width > 0 && scroll.range > 1) {
            const float scroll_x = px(scroll.x);
            const float scroll_y = py(scroll.y);
            const float scroll_w =
                static_cast<float>(scroll.width) * dpi;
            const float scroll_h =
                static_cast<float>(scroll.height) * dpi;
            const float arrow =
                static_cast<float>(scroll.arrow) * dpi;
            // State frames are universal (docs/formats/overlay-ini.md):
            // 0 = disabled, 4 = normal, 5 = hover, 6 = pressed,
            // 7 = selected. SKIP_LIST only removes animation
            // frames 1-3, so normal is frame 4 even though the
            // art skips 1 2 3.
            draw_menu_sprite("CAMPAIGN_TEXT_LIFT", scroll_x, scroll_y,
                             scroll_w, scroll_h, 0, view_w, view_h);
            draw_menu_sprite("CAMPAIGN_TEXT_SCROLLUP", scroll_x,
                             scroll_y, scroll_w, arrow, 4, view_w,
                             view_h);
            draw_menu_sprite("CAMPAIGN_TEXT_SCROLLDOWN", scroll_x,
                             scroll_y + scroll_h - arrow, scroll_w,
                             arrow, 4, view_w, view_h);
            draw_menu_sprite(
                "CAMPAIGN_TEXT_THUMB", scroll_x,
                py(scroll.thumb_y), scroll_w,
                static_cast<float>(scroll.thumb_h) * dpi, 4, view_w,
                view_h);
        }
    }
}

void Game::draw_menu_widget(int widget_index, float dpi, float ox,
                            float oy, int view_w, int view_h) {
    auto px = [&](int value) { return ox + static_cast<float>(value) * dpi; };
    auto py = [&](int value) { return oy + static_cast<float>(value) * dpi; };
    const dino::ui::Widget& widget =
        menu.overlay.widgets[static_cast<std::size_t>(widget_index)];
    bool visible = menu.widget_visible(widget_index);
    const std::string island_prefix = "FREE_GAME_ISLAND_PIC_isl";
    if (widget.name.rfind(island_prefix, 0) == 0) {
        const int index = std::atoi(widget.name.c_str() + island_prefix.size());
        visible = menu.page == 1 &&
                  menu.overlay.page_root(widget_index) ==
                      static_cast<int>(menu.overlay.pages[1]) &&
                  index == menu.island + 1;
    }
    const std::string info_island_prefix =
        "CAMPAIGN_INFO_ISLAND_PIC_isl";
    if (widget.name.rfind(info_island_prefix, 0) == 0) {
        const int index = std::atoi(widget.name.c_str() +
                                    info_island_prefix.size());
        visible = false;
        const dino::Campaign* campaign = menu.selected_campaign();
        if (campaign != nullptr && menu.page == 3 &&
            menu.overlay.page_root(widget_index) ==
                static_cast<int>(menu.overlay.pages[3])) {
            char island_name[16];
            std::snprintf(island_name, sizeof(island_name), "Isl%02d",
                          index);
            visible = campaign->island == island_name;
        }
    }
    if (!visible) {
        return;
    }

    int sprite_frame = widget_index == menu.hover ? 5 : 4;
    if ((widget.name == "FREE_EASY_BUTTON" && menu.difficulty == 0) ||
        (widget.name == "FREE_MIDDLE_BUTTON" && menu.difficulty == 1) ||
        (widget.name == "FREE_HARD_BUTTON" && menu.difficulty == 2)) {
        sprite_frame = 7;
    }
    // Retail starts the campaign list with no row selected; START is
    // disabled (frame 0) until an unlocked campaign is chosen.
    if (widget.name == "CAMPAIGN_START_BUTTON" &&
        (menu.selected_campaign() == nullptr ||
         !menu.campaign_unlocked(menu.campaign))) {
        sprite_frame = 0;
    }

    if (!widget.texture.empty()) {
        if (const dino::ui::Graphic* graphic =
                menu.overlay.graphic(widget.texture)) {
            if (UiTexture* texture = ui_texture(graphic->filename)) {
                const int chosen_frame =
                    graphic->objects <= 1 ? 0 : sprite_frame;
                const UiSourceRect source =
                    ui_frame_rect(*texture, *graphic, chosen_frame);
                if (source.valid) {
                    const float u0 = static_cast<float>(source.x) / texture->width;
                    const float v0 = static_cast<float>(source.y) / texture->height;
                    const float u1 = static_cast<float>(source.x + source.w) /
                                     texture->width;
                    const float v1 = static_cast<float>(source.y + source.h) /
                                     texture->height;
                    draw_ui_mesh(
                        menu_gpu,
                        build_ui_quad(px(widget.abs_x), py(widget.abs_y),
                                      static_cast<float>(widget.width) * dpi,
                                      static_cast<float>(widget.height) * dpi,
                                      u0, v0, u1, v1, {255, 255, 255, 255}),
                        texture->id, 1.0f, 1, view_w, view_h);
                }
            }
        }
    } else if (widget.has_back_color) {
        draw_ui_mesh(menu_gpu,
                     build_screen_rect(px(widget.abs_x), py(widget.abs_y),
                                       static_cast<float>(widget.width) * dpi,
                                       static_cast<float>(widget.height) * dpi,
                                       widget.back_color),
                     0, 1.0f, 1, view_w, view_h);
    }

    if (widget.type == "HSCROLL") {
        draw_menu_hscroll_widget(widget, dpi, ox, oy, view_w, view_h);
    }

    if (widget.type == "LIST") {
        draw_menu_list_widget(widget, dpi, ox, oy, view_w, view_h);
    }

    const std::string text = menu.text_for(widget);
    // Retail only draws widget text when TEXT_HEIGHT is set (the
    // glyph scale is TEXT_HEIGHT / font_size, so an absent height is
    // a zero scale). Icon buttons such as START_BUTTON/CANCEL_BUTTON
    // carry a TEXT key but no TEXT_HEIGHT and render as bare art.
    if (!text.empty() && widget.text_height > 0) {
        float scale = 1.0f;
        GLuint atlas = 0;
        if (const dino::Fnt* font =
                ui_font(widget.font, widget.text_height, scale, atlas)) {
            const dino::Mesh block = build_text_block(
                *font, text, px(widget.abs_x), py(widget.abs_y), scale * dpi,
                widget.text_color, widget.justify,
                static_cast<float>(widget.width) * dpi,
                static_cast<float>(widget.height) * dpi);
            draw_ui_mesh(menu_gpu, block, atlas, 1.0f, 2, view_w, view_h);
        }
    }
}

void Game::draw_menu(int view_w, int view_h, int logical_w, int logical_h) {
    (void)logical_h;
    if (!menu_loaded) {
        return;
    }
    const float dpi = logical_w > 0
                          ? static_cast<float>(view_w) / static_cast<float>(logical_w)
                          : 1.0f;
    const float ox = (static_cast<float>(view_w) - 800.0f * dpi) * 0.5f;
    const float oy = (static_cast<float>(view_h) - 600.0f * dpi) * 0.5f;

    if (const dino::ui::Graphic* background =
            menu.overlay.graphic(menu.background())) {
        if (UiTexture* texture = ui_texture(background->filename)) {
            draw_ui_mesh(menu_gpu,
                         build_ui_quad(ox, oy, 800.0f * dpi, 600.0f * dpi, 0.0f,
                                       0.0f, 1.0f, 1.0f, {255, 255, 255, 255}),
                         texture->id, 1.0f, 1, view_w, view_h);
        }
    }

    for (int i = 0; i < static_cast<int>(menu.overlay.widgets.size()); ++i) {
        draw_menu_widget(i, dpi, ox, oy, view_w, view_h);
    }

}

void Game::draw_park_hud_campaign_text(float dpi, int view_w, int view_h) {
    auto px = [&](int value) { return static_cast<float>(value) * dpi; };
    auto py = [&](int value) { return static_cast<float>(value) * dpi; };
    // The mission description/tips are LIST widgets whose back area has a
    // higher ZPRIORITY than the list in the overlay data; drawing them
    // per-widget would put the back area over the text. Draw these two
    // after the normal widget pass so they stay readable.
    if (campaign_runtime) {
        const dino::CampaignMission& mission = campaign_runtime->mission();
        auto draw_mission_text = [&](const char* widget_name,
                                     const std::string& text) {
            if (text.empty()) {
                return;
            }
            const int index = park.overlay.find(widget_name);
            if (index < 0 || !park.overlay.visible_in_tree(index)) {
                return;
            }
            const dino::ui::Widget& widget =
                park.overlay.widgets[static_cast<std::size_t>(index)];
            float scale = 1.0f;
            GLuint atlas = 0;
            if (const dino::Fnt* font = ui_font(
                    widget.font, widget.text_height, scale, atlas)) {
                const dino::Mesh block = build_text_block(
                    *font, text, px(widget.abs_x), py(widget.abs_y),
                    scale * dpi, widget.text_color, widget.justify,
                    static_cast<float>(widget.width) * dpi,
                    static_cast<float>(widget.height) * dpi);
                draw_ui_mesh(menu_gpu, block, atlas, 1.0f, 2, view_w,
                             view_h);
            }
        };
        draw_mission_text(
            "MISSION_PANEL_EXPLICATION",
            limit_lines(wrap_text(dino::ui::resolve_loc_text(
                                      mission.description_id, park.loc),
                                  58),
                        6));
        draw_mission_text(
            "MISSION_PANEL_TIPS",
            limit_lines(wrap_text(dino::ui::resolve_loc_text(
                                      mission.hint_id, park.loc),
                                  58),
                        6));
    }
}

void Game::draw_park_hud_frame(const dino::ui::Widget& widget, int i,
                               int hovered, float dpi, int view_w, int view_h) {
    auto px = [&](int value) { return static_cast<float>(value) * dpi; };
    auto py = [&](int value) { return static_cast<float>(value) * dpi; };
    if (!widget.texture.empty()) {
        if (const dino::ui::Graphic* graphic =
                park.overlay.graphic(widget.texture)) {
            if (UiTexture* texture = ui_texture(graphic->filename)) {
                int frame = graphic->objects <= 1 ? 0 : 4;
                if (widget.type == "BUTTON" && graphic->objects > 1) {
                    int wanted = -1;
                    if (i == park_active_index) {
                        wanted = 6;
                    } else if (i == hovered) {
                        wanted = 5;
                    }
                    if (wanted >= 0 &&
                        ui_frame_rect(*texture, *graphic, wanted).valid) {
                        frame = wanted;
                    }
                }
                const UiSourceRect source =
                    ui_frame_rect(*texture, *graphic, frame);
                if (source.valid) {
                    const float u0 =
                        static_cast<float>(source.x) / texture->width;
                    const float v0 =
                        static_cast<float>(source.y) / texture->height;
                    const float u1 = static_cast<float>(source.x + source.w) /
                                     texture->width;
                    const float v1 = static_cast<float>(source.y + source.h) /
                                     texture->height;
                    draw_ui_mesh(
                        menu_gpu,
                        build_ui_quad(px(widget.abs_x), py(widget.abs_y),
                                      static_cast<float>(widget.width) * dpi,
                                      static_cast<float>(widget.height) * dpi,
                                      u0, v0, u1, v1,
                                      {255, 255, 255, 255}),
                        texture->id, 1.0f, 1, view_w, view_h);
                }
            }
        }
    } else if (widget.has_back_color) {
        draw_ui_mesh(menu_gpu,
                     build_screen_rect(px(widget.abs_x), py(widget.abs_y),
                                       static_cast<float>(widget.width) * dpi,
                                       static_cast<float>(widget.height) * dpi,
                                       widget.back_color),
                     0, 1.0f, 1, view_w, view_h);
    }
}

void Game::draw_park_hud_store_preview(const dino::ui::Widget& widget,
                                       float dpi, int view_w, int view_h) {
    auto px = [&](int value) { return static_cast<float>(value) * dpi; };
    auto py = [&](int value) { return static_cast<float>(value) * dpi; };
    // DINO_ACHAT_SAVE_RENDER_VIEW: draw the selected store entry's
    // dinosaur's standing LOD1 mesh directly into the well. The
    // retail widget is a 3D preview; the port fits the mesh to the
    // widget rect with a pixel-space MVP and scissor clipping.
    if (widget.name == "DINO_ACHAT_SAVE_RENDER_VIEW" &&
        !park.store_rows().empty()) {
        const int selected = park.store_selected();
        if (selected >= 0 &&
            selected < static_cast<int>(park.store_rows().size())) {
            if (StorePreview* preview =
                    store_preview_for(
                        park.store_rows()[static_cast<std::size_t>(
                            selected)])) {
                const float w =
                    static_cast<float>(widget.width) * dpi;
                const float h =
                    static_cast<float>(widget.height) * dpi;
                const float size =
                    std::max(0.001f, preview->size);
                const dino::Vec3 center = preview->center;
                // Side/three-quarter camera; the scene renderer
                // already uses this projection convention for the
                // imported dinosaurs.
                const dino::Vec3 eye{
                    center.x - size * 1.4f,
                    center.y - size * 0.25f,
                    center.z + size * 0.35f};
                const dino::Mat4 view =
                    look_at(eye, center, {0.0f, 0.0f, 1.0f});
                const dino::Mat4 proj = perspective(
                    32.0f, 1.0f, 0.1f, size * 10.0f);
                const dino::Mat4 mvp =
                    dino::multiply(proj, view);
                const int vx =
                    static_cast<int>(px(widget.abs_x));
                const int vy = static_cast<int>(
                    view_h - (py(widget.abs_y) + h));
                glEnable(GL_SCISSOR_TEST);
                glScissor(vx, vy, static_cast<int>(w),
                          static_cast<int>(h));
                glViewport(vx, vy, static_cast<int>(w),
                           static_cast<int>(h));
                glEnable(GL_DEPTH_TEST);
                glDepthFunc(GL_LEQUAL);
                glDisable(GL_CULL_FACE);
                glClear(GL_DEPTH_BUFFER_BIT);
                glfns.UseProgram(program);
                {
                    const float white[3] = {1.0f, 1.0f, 1.0f};
                    glfns.Uniform3fv(
                        glfns.GetUniformLocation(program, "u_tint"), 1,
                        white);
                }
                glfns.UniformMatrix4fv(
                    glfns.GetUniformLocation(program, "u_mvp"), 1,
                    GL_TRUE, mvp.m);
                glfns.Uniform1i(
                    glfns.GetUniformLocation(program, "u_unlit"), 1);
                glfns.Uniform1i(
                    glfns.GetUniformLocation(program,
                                             "u_use_texture"),
                    preview->texture != 0 ? 1 : 0);
                glfns.Uniform1i(
                    glfns.GetUniformLocation(program,
                                             "u_use_texture2"),
                    0);
                glfns.Uniform1i(
                    glfns.GetUniformLocation(program,
                                             "u_modulate2x"),
                    0);
                glfns.Uniform1i(
                    glfns.GetUniformLocation(program, "u_texture"), 0);
                glfns.Uniform1f(
                    glfns.GetUniformLocation(program, "u_alpha"),
                    1.0f);
                glfns.Uniform1i(
                    glfns.GetUniformLocation(program,
                                             "u_fog_enable"),
                    0);
                const float light[3] = {-0.35f, -0.68f, 0.72f};
                const float light_color[3] = {0.92f, 0.93f, 0.95f};
                const float ambient[3] = {0.62f, 0.64f, 0.68f};
                const float camera[3] = {0.0f, 0.0f, 4.0f};
                glfns.Uniform3fv(
                    glfns.GetUniformLocation(program,
                                             "u_light_dir"),
                    1, light);
                glfns.Uniform3fv(
                    glfns.GetUniformLocation(program,
                                             "u_light_color"),
                    1, light_color);
                glfns.Uniform3fv(
                    glfns.GetUniformLocation(program, "u_ambient"),
                    1, ambient);
                glfns.Uniform3fv(
                    glfns.GetUniformLocation(program, "u_camera"),
                    1, camera);
                if (preview->texture != 0) {
                    glfns.ActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, preview->texture);
                }
                glfns.BindVertexArray(preview->gpu.vao);
                glDrawElements(GL_TRIANGLES,
                               preview->gpu.index_count,
                               GL_UNSIGNED_INT, nullptr);
                glViewport(0, 0, view_w, view_h);
                glDisable(GL_SCISSOR_TEST);
                glDisable(GL_DEPTH_TEST);
            }
        }
    }
}

void Game::draw_park_hud_progress(const dino::ui::Widget& widget, float dpi,
                                  int view_w, int view_h) {
    auto px = [&](int value) { return static_cast<float>(value) * dpi; };
    auto py = [&](int value) { return static_cast<float>(value) * dpi; };
    // PROGRESS widgets: fill the gauge track proportionally. The
    // retail panel texture supplies the empty track; the port draws a
    // solid light-blue fill over it (the exact retail gauge art is a
    // `moProgress` draw we have not ported yet).
    if (widget.type == "PROGRESS" && widget.progress_max > 0) {
        const float fraction = std::clamp(
            static_cast<float>(widget.progress_current) /
                static_cast<float>(widget.progress_max),
            0.0f, 1.0f);
        if (fraction > 0.0f) {
            draw_ui_mesh(
                menu_gpu,
                build_screen_rect(
                    px(widget.abs_x), py(widget.abs_y),
                    static_cast<float>(widget.width) * fraction * dpi,
                    static_cast<float>(widget.height) * dpi,
                    {120, 200, 255, 255}),
                0, 1.0f, 1, view_w, view_h);
        }
    }
}

void Game::draw_park_hud_dialog_text(const dino::ui::Widget& widget, float dpi,
                                     int view_w, int view_h) {
    auto px = [&](int value) { return static_cast<float>(value) * dpi; };
    auto py = [&](int value) { return static_cast<float>(value) * dpi; };
    if (widget.name == hud_dialog_zone() && park.dialog_open()) {
        const int line_h =
            widget.list_line_height > 0 ? widget.list_line_height : 15;
        float list_scale = 1.0f;
        GLuint list_atlas = 0;
        const dino::Fnt* list_font =
            ui_font(widget.font, widget.text_height, list_scale,
                    list_atlas);
        if (list_font != nullptr) {
            const float max_w = static_cast<float>(
                widget.width - widget.list_scroll_width);
            std::vector<std::string> lines;
            std::string current;
            std::string word;
            auto flush_line = [&]() {
                lines.push_back(current);
                current.clear();
            };
            auto flush_word = [&]() {
                if (word.empty()) {
                    return;
                }
                const std::string candidate =
                    current.empty() ? word : current + " " + word;
                if (!current.empty() &&
                    text_width(*list_font, candidate, list_scale) > max_w) {
                    flush_line();
                    current = word;
                } else {
                    current = candidate;
                }
                word.clear();
            };
            for (const char ch : park.dialog_text()) {
                if (ch == '\n') {
                    flush_word();
                    flush_line();
                } else if (ch == ' ') {
                    flush_word();
                } else {
                    word.push_back(ch);
                }
            }
            flush_word();
            if (!current.empty()) {
                flush_line();
            }
            const int rows = std::min(
                static_cast<int>(lines.size()),
                line_h > 0 ? widget.height / line_h : 0);
            for (int row = 0; row < rows; ++row) {
                const dino::Mesh block = build_text_block(
                    *list_font, lines[static_cast<std::size_t>(row)],
                    px(widget.abs_x), py(widget.abs_y + row * line_h),
                    list_scale * dpi, widget.text_color, "LEFT",
                    static_cast<float>(max_w) * dpi,
                    static_cast<float>(line_h) * dpi);
                draw_ui_mesh(menu_gpu, block, list_atlas, 1.0f, 2,
                             view_w, view_h);
            }
        }
    }
}

void Game::draw_park_hud_save_list(const dino::ui::Widget& widget, float dpi,
                                   int view_w, int view_h) {
    auto px = [&](int value) { return static_cast<float>(value) * dpi; };
    auto py = [&](int value) { return static_cast<float>(value) * dpi; };
    if (widget.name == "OPTIONS_SAVE_NAMES_LIST" && !save_slots.empty()) {
        const int line_h =
            widget.list_line_height > 0 ? widget.list_line_height : 12;
        const int row_w =
            widget.list_column0_width > 0
                ? widget.list_column0_width
                : widget.width - widget.list_scroll_width;
        float list_scale = 1.0f;
        GLuint list_atlas = 0;
        const dino::Fnt* list_font =
            ui_font(widget.font, widget.text_height, list_scale,
                    list_atlas);
        if (list_font != nullptr) {
            const int rows = std::min(
                static_cast<int>(save_slots.size()),
                line_h > 0 ? widget.height / line_h : 0);
            for (int row = 0; row < rows; ++row) {
                const dino::Mesh block = build_text_block(
                    *list_font, save_slots[static_cast<std::size_t>(row)].second,
                    px(widget.abs_x), py(widget.abs_y + row * line_h),
                    list_scale * dpi, widget.text_color, widget.justify,
                    static_cast<float>(row_w) * dpi,
                    static_cast<float>(line_h) * dpi);
                draw_ui_mesh(menu_gpu, block, list_atlas, 1.0f, 2,
                             view_w, view_h);
            }
        }
    }
}

void Game::draw_park_hud_store_list(const dino::ui::Widget& widget, float dpi,
                                    int view_w, int view_h) {
    auto px = [&](int value) { return static_cast<float>(value) * dpi; };
    auto py = [&](int value) { return static_cast<float>(value) * dpi; };
    if (widget.name == "DINO_ACHAT_SAVE_NAMES_LIST" &&
        !park.store_rows().empty()) {
        const int line_h = widget.list_line_height > 0
                               ? widget.list_line_height
                               : 12;
        const int pitch = std::max(
            1, widget.height /
                   dino::ui::ParkHud::store_visible_rows());
        const int row_w =
            widget.list_column0_width > 0
                ? widget.list_column0_width
                : widget.width - widget.list_scroll_width;
        float list_scale = 1.0f;
        GLuint list_atlas = 0;
        const dino::Fnt* list_font =
            ui_font(widget.font, widget.text_height, list_scale,
                    list_atlas);
        if (list_font != nullptr) {
            for (int slot = 0;
                 slot < dino::ui::ParkHud::store_visible_rows();
                 ++slot) {
                const dino::ui::StoreRow* row =
                    park.store_visible_row(slot);
                if (row == nullptr) {
                    break;
                }
                const bool selected =
                    park.store_scroll() + slot ==
                    park.store_selected();
                const int row_y = widget.abs_y + slot * pitch;
                const dino::Mesh name_block = build_text_block(
                    *list_font, row->name, px(widget.abs_x),
                    py(row_y), list_scale * dpi,
                    selected ? dino::Color{255, 255, 0, 255}
                             : widget.text_color,
                    "LEFT", static_cast<float>(row_w) * dpi,
                    static_cast<float>(line_h) * dpi);
                draw_ui_mesh(menu_gpu, name_block, list_atlas, 1.0f,
                             2, view_w, view_h);
                const dino::Mesh price_block = build_text_block(
                    *list_font, std::to_string(row->price),
                    px(widget.abs_x), py(row_y + line_h),
                    list_scale * dpi,
                    dino::Color{220, 200, 120, 255}, "RIGHT",
                    static_cast<float>(row_w) * dpi,
                    static_cast<float>(line_h) * dpi);
                draw_ui_mesh(menu_gpu, price_block, list_atlas, 1.0f,
                             2, view_w, view_h);
            }
        }
    }
}

void Game::draw_park_hud_show_list(const dino::ui::Widget& widget, float dpi,
                                   int view_w, int view_h) {
    auto px = [&](int value) { return static_cast<float>(value) * dpi; };
    auto py = [&](int value) { return static_cast<float>(value) * dpi; };
    if (widget.name == "PREDEFINED_SHOW_LIST" &&
        !park.show_rows().empty()) {
        const int pitch = std::max(
            1, widget.height /
                   dino::ui::ParkHud::show_visible_rows());
        const int column_widths[7] = {
            widget.list_column0_width > 0
                ? widget.list_column0_width
                : 150,
            widget.list_column1_width > 0
                ? widget.list_column1_width
                : 28,
            widget.list_column2_width > 0
                ? widget.list_column2_width
                : 60,
            widget.list_column3_width > 0
                ? widget.list_column3_width
                : 28,
            widget.list_column4_width > 0
                ? widget.list_column4_width
                : 28,
            widget.list_column5_width > 0
                ? widget.list_column5_width
                : 28,
            widget.list_column6_width > 0
                ? widget.list_column6_width
                : 28,
        };
        float list_scale = 1.0f;
        GLuint list_atlas = 0;
        const dino::Fnt* list_font =
            ui_font(widget.font, widget.text_height, list_scale,
                    list_atlas);
        if (list_font != nullptr) {
            for (int slot = 0;
                 slot < dino::ui::ParkHud::show_visible_rows();
                 ++slot) {
                const dino::ui::ShowRow* row =
                    park.show_visible_row(slot);
                if (row == nullptr) {
                    break;
                }
                const bool selected =
                    park.show_scroll() + slot ==
                    park.show_selected();
                const int row_y = widget.abs_y + slot * pitch;
                int column_x = widget.abs_x;
                auto draw_cell = [&](int column,
                                     const std::string& value,
                                     dino::Color color,
                                     const char* justify) {
                    const dino::Mesh block = build_text_block(
                        *list_font, value, px(column_x), py(row_y),
                        list_scale * dpi, color, justify,
                        static_cast<float>(column_widths[column]) *
                            dpi,
                        static_cast<float>(pitch) * dpi);
                    draw_ui_mesh(menu_gpu, block, list_atlas, 1.0f,
                                 2, view_w, view_h);
                    column_x += column_widths[column];
                };
                draw_cell(0, row->title,
                          selected ? dino::Color{255, 255, 0, 255}
                                   : widget.text_color,
                          "LEFT");
                draw_cell(1, std::to_string(row->judge_kind),
                          dino::Color{220, 220, 220, 255}, "CENTER");
                draw_cell(2, std::to_string(row->price),
                          dino::Color{220, 200, 120, 255}, "RIGHT");
                for (int cage = 0; cage < 4; ++cage) {
                    draw_cell(
                        3 + cage,
                        row->cage_size[static_cast<std::size_t>(cage)] >
                                0
                            ? std::to_string(cage + 1)
                            : std::string(),
                        dino::Color{180, 220, 180, 255}, "CENTER");
                }
            }
        }
    }
}

void Game::draw_park_hud_text(const dino::ui::Widget& widget, float dpi,
                              int view_w, int view_h) {
    auto px = [&](int value) { return static_cast<float>(value) * dpi; };
    auto py = [&](int value) { return static_cast<float>(value) * dpi; };
    std::string text = hud_widget_text(widget.name);
    if (text.empty()) {
        // `FUN_00516dd0` clears an empty attraction row to the
        // zero-filled `DAT_006743c8`; the INI `TEXT` ("10:00 AM") is
        // only the pre-refresh template. The hour itself is drawn by
        // the separate `ATTRACTION_LIST_TIME<n>` widget.
        if (widget.name.rfind("ATTRACTION_LIST_NAME", 0) == 0 ||
            widget.name.rfind("ATTRACTION_LIST_ITERATION_INDIC", 0) == 0) {
            return;
        }
        text = dino::ui::resolve_loc_text(widget.text, park.loc);
    }
    if (text.empty()) {
        return;
    }
    float scale = 1.0f;
    GLuint atlas = 0;
    if (const dino::Fnt* font =
            ui_font(widget.font, widget.text_height, scale, atlas)) {
        const dino::Mesh block = build_text_block(
            *font, text, px(widget.abs_x), py(widget.abs_y), scale * dpi,
            widget.text_color, widget.justify,
            static_cast<float>(widget.width) * dpi,
            static_cast<float>(widget.height) * dpi);
        draw_ui_mesh(menu_gpu, block, atlas, 1.0f, 2, view_w, view_h);
    }
}

void Game::draw_park_hud_widget(int i, int hovered, float dpi, int view_w,
                                int view_h) {
    const dino::ui::Widget& widget =
        park.overlay.widgets[static_cast<std::size_t>(i)];

    draw_park_hud_frame(widget, i, hovered, dpi, view_w, view_h);
    draw_park_hud_store_preview(widget, dpi, view_w, view_h);
    draw_park_hud_progress(widget, dpi, view_w, view_h);
    draw_park_hud_dialog_text(widget, dpi, view_w, view_h);
    draw_park_hud_save_list(widget, dpi, view_w, view_h);
    draw_park_hud_store_list(widget, dpi, view_w, view_h);
    draw_park_hud_show_list(widget, dpi, view_w, view_h);
    draw_park_hud_text(widget, dpi, view_w, view_h);
}


std::string Game::hud_widget_text(const std::string& name) {
    if (name == "MISSION_PANEL_TITLE" &&
        !mission_panel_title.empty()) {
        return mission_panel_title;
    }
    if (campaign_runtime) {
        const dino::CampaignMission& mission =
            campaign_runtime->mission();
        static const std::string suffix = "_PANEL_EXPLICATION";
        if (name.rfind("OBJECTIF", 0) == 0 &&
            name.size() > suffix.size() &&
            name.compare(name.size() - suffix.size(), suffix.size(),
                         suffix) == 0) {
            const int index = std::atoi(name.c_str() + 8) - 1;
            if (index >= 0 &&
                index < static_cast<int>(mission.objectives.size())) {
                return dino::ui::resolve_loc_text(
                    mission.objectives[static_cast<std::size_t>(index)]
                        .description_id,
                    park.loc);
            }
        }
    }
    if (name == "BUDGET_AMOUNT") {
        return std::to_string(park_session.budget);
    }
    if (name == "OPTIONS_NAME_SLOT") {
        return park.save_name();
    }
    if (name == "VISITOR_AMOUNT") {
        return std::to_string(park_session.visitor_system.visitors.size());
    }
    if (name == "DINO_AMOUNT") {
        const std::size_t count =
            imported_dinos.size() + (place_dino ? 1 : 0);
        return std::to_string(count);
    }
    if (name == "SHOW_AMOUNT") {
        return std::to_string(show_runtime.count());
    }
    if (name == "PADDOCK_AMOUNT") {
        return std::to_string(park_paddocks.size());
    }
    const auto slot_index = [](const std::string& widget_name,
                               const char* prefix) -> int {
        const std::size_t prefix_len = std::strlen(prefix);
        if (widget_name.rfind(prefix, 0) != 0) {
            return -1;
        }
        return std::atoi(widget_name.c_str() + prefix_len) - 1;
    };
    if (name.rfind("DINO_NAME_SLOT", 0) == 0) {
        const int slot = slot_index(name, "DINO_NAME_SLOT");
        if (slot >= 0 && slot < static_cast<int>(imported_dinos.size())) {
            return imported_dinos[static_cast<std::size_t>(slot)].species;
        }
        return "";
    }
    if (name.rfind("FENCE_NAME_SLOT", 0) == 0) {
        const int slot = slot_index(name, "FENCE_NAME_SLOT");
        if (slot >= 0 && slot < static_cast<int>(park_paddocks.size())) {
            return "Paddock " + std::to_string(slot + 1);
        }
        return "";
    }
    if (name.rfind("FENCE_LIST_QUANTITY_SLOT", 0) == 0) {
        const int slot = slot_index(name, "FENCE_LIST_QUANTITY_SLOT");
        if (slot >= 0 && slot < static_cast<int>(park_paddocks.size())) {
            return std::to_string(
                park_paddocks[static_cast<std::size_t>(slot)]
                    .occupants.size());
        }
        return "";
    }
    if (name.rfind("FENCE_LIST_POURCENT_SLOT", 0) == 0) {
        const int slot = slot_index(name, "FENCE_LIST_POURCENT_SLOT");
        if (slot >= 0 && slot < static_cast<int>(park_paddocks.size())) {
            const float percent =
                park_paddocks[static_cast<std::size_t>(slot)].fill() *
                100.0f;
            return std::to_string(
                static_cast<int>(percent + 0.5f));
        }
        return "";
    }
    // Attraction list: one row per hour slot. The retail NAME field
    // defaults to the hour label and holds the show name once a show
    // is scheduled (`FUN_00516dd0`); ITERATION_INDIC is the
    // performances-per-day count `show+0x8c` (`%u`).
    if (name.rfind("ATTRACTION_LIST_NAME", 0) == 0) {
        const int slot = slot_index(name, "ATTRACTION_LIST_NAME") + 1;
        std::string text;
        int iterations = 0;
        int locked = 0;
        attraction_row_text(slot, text, iterations, locked);
        return text;
    }
    if (name.rfind("ATTRACTION_LIST_ITERATION_INDIC", 0) == 0) {
        const int slot =
            slot_index(name, "ATTRACTION_LIST_ITERATION_INDIC") + 1;
        std::string text;
        int iterations = 0;
        int locked = 0;
        attraction_row_text(slot, text, iterations, locked);
        return iterations > 0 ? std::to_string(iterations) : "";
    }
    if (name == "PARK_CURRENT_TIME") {
        return park_session.clock.time_text(false);
    }
    if (name == "PARK_CURRENT_DAY") {
        return park_session.clock.day_text();
    }
    // Budget-panel fields are separate number widgets refreshed by
    // 0x510430. The panel shows the yesterday block (docs/decomp/
    // economy.md); only the categories the port currently changes
    // are non-zero.
    if (name.rfind("BUDGET_", 0) == 0 &&
        name.size() > 7 &&
        name.compare(name.size() - 7, 7, "_NUMBER") == 0) {
        if (name == "BUDGET_DINOACHAT_QUANTITY_NUMBER") {
            return std::to_string(park_session.ledger.y_dinoachat_count);
        }
        if (name == "BUDGET_DINOACHAT_QUANTITY_COST_NUMBER") {
            return std::to_string(park_session.ledger.y_dinoachat_total);
        }
        if (name == "BUDGET_CONSTRUCTION_QUANTITY_NUMBER") {
            return std::to_string(park_session.ledger.y_construction_count);
        }
        if (name == "BUDGET_CONSTRUCTION_QUANTITY_COST_NUMBER") {
            return std::to_string(park_session.ledger.y_construction_total);
        }
        if (name == "BUDGET_LABOACTION_QUANTITY_NUMBER") {
            return std::to_string(park_session.ledger.y_laboaction_count);
        }
        if (name == "BUDGET_LABOACTION_QUANTITY_COST_NUMBER") {
            return std::to_string(park_session.ledger.y_laboaction_total);
        }
        if (name == "BUDGET_ENTREES_QUANTITY_NUMBER") {
            return std::to_string(park_session.ledger.y_ticket_count);
        }
        if (name == "BUDGET_ENTREES_QUANTITY_COST_NUMBER") {
            return std::to_string(park_session.ledger.y_ticket_total);
        }
        if (name == "BUDGET_VENTES_QUANTITY_NUMBER") {
            return std::to_string(park_session.ledger.y_ventes_count);
        }
        if (name == "BUDGET_VENTES_QUANTITY_COST_NUMBER") {
            return std::to_string(park_session.ledger.y_ventes_total);
        }
        if (name == "BUDGET_FOOD_QUANTITY_NUMBER") {
            return std::to_string(park_session.ledger.y_food_count);
        }
        if (name == "BUDGET_FOOD_QUANTITY_COST_NUMBER") {
            return std::to_string(park_session.ledger.y_food_total);
        }
        if (name == "BUDGET_EFFECTIFS_SECU_QUANTITY_NUMBER") {
            return std::to_string(park_session.ledger.y_secu_count);
        }
        if (name == "BUDGET_EFFECTIFS_SECU_QUANTITY_COST_NUMBER") {
            return std::to_string(park_session.ledger.y_secu_total);
        }
        if (name == "BUDGET_EFFECTIFS_FOOD_QUANTITY_NUMBER") {
            return std::to_string(
                employee_system.count_role(employee_profiles, 0));
        }
        if (name == "BUDGET_EFFECTIFS_FOOD_QUANTITY_COST_NUMBER") {
            return std::to_string(park_session.ledger.y_staff_food_total);
        }
        const struct ShopRow {
            const char* prefix;
            int kind;
        } shop_rows[] = {
            {"BUDGET_RESTOS_", 1},     {"BUDGET_BARAQUES_", 2},
            {"BUDGET_SOUVENIRS_", 3},  {"BUDGET_BOITNOIRE_", 4},
            {"BUDGET_POINTDEVUE_", 5}, {"BUDGET_GRADINS_", 6},
        };
        for (const ShopRow& row : shop_rows) {
            if (name.rfind(row.prefix, 0) != 0) {
                continue;
            }
            const std::size_t index =
                static_cast<std::size_t>(row.kind);
            if (name.find("_COST_NUMBER") != std::string::npos) {
                return std::to_string(
                    park_session.ledger.y_shop_revenue[index]);
            }
            return std::to_string(
                park_session.ledger.y_shop_sales[index]);
        }
        if (name == "BUDGET_TOTAL_LEFT_COST_NUMBER") {
            return std::to_string(park_session.ledger.expenses());
        }
        if (name == "BUDGET_TOTAL_RIGHT_COST_NUMBER") {
            return std::to_string(park_session.ledger.income());
        }
        return "0";
    }
    if (name == "BUDGET_TOTAL_PRICE_PANEL_PRICE_ZONE") {
        return std::to_string(park_session.ledger.net());
    }
    return "";
}

const char* Game::hud_dialog_zone() {
    switch (park.dialog_kind()) {
        case dino::ui::ParkHud::DialogKind::warning:
            return "WARNING_TEXT_ZONE";
        case dino::ui::ParkHud::DialogKind::request:
            return "REQUEST_TEXT_ZONE";
        case dino::ui::ParkHud::DialogKind::campaign_end:
            return "CPGEND_TEXT_ZONE";
        default:
            return "";
    }
}

void Game::draw_park_hud(int view_w, int view_h, int logical_w, int logical_h) {
    (void)logical_h;
    if (!park.loaded()) {
        return;
    }
    const int hovered =
        park_hover_x >= 0 ? park.overlay.hit_test(park_hover_x, park_hover_y) : -1;
    auto is_text_zone = [](const std::string& name) {
        static const std::string suffix = "_TEXT_ZONE";
        return name.size() >= suffix.size() &&
               name.compare(name.size() - suffix.size(), suffix.size(),
                            suffix) == 0;
    };
    const float dpi = logical_w > 0
                          ? static_cast<float>(view_w) / static_cast<float>(logical_w)
                          : 1.0f;

    std::vector<int> order;
    auto draw_less = [&](int a, int b) {            const auto& wa = park.overlay.widgets[static_cast<std::size_t>(a)];
        const auto& wb = park.overlay.widgets[static_cast<std::size_t>(b)];
        if (wa.priority != wb.priority) {
            return wa.priority < wb.priority;
        }
        if (wa.zpriority != wb.zpriority) {
            // Lower ZPRIORITY is nearer the viewer in the retail overlay;
            // draw the far widgets first and the near ones last.
            return wa.zpriority > wb.zpriority;
        }
        return a < b;
    };
    // moWidget::Draw (0x005c5bf0) draws the widget, then its child list in
    // priority order. A flat ZPRIORITY sort loses that nesting: a list
    // slot's snapshot would cover its indicator strip. Walk the tree.
    std::function<void(int)> append_widget = [&](int i) {
        const dino::ui::Widget& widget =
            park.overlay.widgets[static_cast<std::size_t>(i)];
        if (widget.name == "RENDERVIEW" ||
            !park.overlay.visible_in_tree(i)) {
            return;
        }
        order.push_back(i);
        std::vector<int> children = widget.children;
        std::sort(children.begin(), children.end(), draw_less);
        for (int child : children) {
            append_widget(child);
        }
    };
    std::vector<int> roots = park.overlay.roots;
    std::sort(roots.begin(), roots.end(), draw_less);
    for (int root : roots) {
        append_widget(root);
    }
    // Dialog text must draw above its *_TEXT_BACK_ZONE and the wood frame.
    std::stable_partition(order.begin(), order.end(), [&](int i) {
        return !is_text_zone(
            park.overlay.widgets[static_cast<std::size_t>(i)].name);
    });

    for (const int i : order) {
        draw_park_hud_widget(i, hovered, dpi, view_w, view_h);
    }

    draw_park_hud_campaign_text(dpi, view_w, view_h);
}

void Game::park_design(float window_x, float window_y, int& design_x,
                          int& design_y) {
    map_park_pointer(window_x, window_y, design_x, design_y);

}

void Game::sync_park_layout() {
    SDL_GetWindowSize(window, &logical_width, &logical_height);
    if (park.loaded() &&
        (logical_width != park_layout_w || logical_height != park_layout_h)) {
        park.overlay.layout_for_size(logical_width, logical_height);
        park_layout_w = logical_width;
        park_layout_h = logical_height;
    }

}

void Game::activate_main_menu() {
    park_return_to_menu = false;
    if (!menu_loaded || !opt.use_menu) {
        // No menu in this run mode: quitting the park still exits.
        running = false;
        return;
    }
    menu.active = true;
    menu.page = 0;
    menu.loading = false;
    menu.start_campaign = false;
    menu.loading_timer = 0.0f;
    start_screen = true;
    park_hover_x = -1;
    park_hover_y = -1;
    park.end_session();
    // Retail returns to the menu score, not the island music.
    if (opt.music) {
        const auto menu_music = menu_sound_files.find("music");
        if (menu_music != menu_sound_files.end()) {
            music.play(menu_music->second);
        }
    }

}

void Game::refresh_fence_info_panel() {
    const bool selected =
        selected_paddock >= 0 &&
        selected_paddock < static_cast<int>(park_session.paddocks.size());
    if (!selected) {
        if (build_tool != 6) {
            park.overlay.set_visible("FENCE_CONSTRUCTION_AND_INFO_PANEL",
                                     false);
        }
        return;
    }
    const dino::Paddock& pen =
        park_session.paddocks[static_cast<std::size_t>(selected_paddock)];
    park.overlay.set_visible("FENCE_CONSTRUCTION_AND_INFO_PANEL", true);
    auto set_text = [&](const char* widget_name, const std::string& text) {
        dino::ui::Widget* widget = park.overlay.get(widget_name);
        if (widget == nullptr) {
            return;
        }
        widget->text = text;
        widget->font = "DATA/FONTE/FONT.FNT";
        widget->text_height = 12;
        widget->justify = "CENTER";
        widget->text_color = dino::Color{255, 255, 255, 255};
    };
    const std::string name =
        pen.name.empty()
            ? "Enclosure" + std::to_string(selected_paddock + 1)
            : pen.name;
    set_text("FENCE_NAME_INDICS", name);
    set_text("FENCE_STATS_DINO_QUANTITY_INDICS_BACK_CHIFFRE",
             std::to_string(pen.occupants.size()));
    set_text("FENCE_STATS_OCCUPATION_INDICS_BACK_CHIFFRE",
             std::to_string(static_cast<int>(pen.fill() * 100.0f)) + "%");
    // 0x57e720 per-diet demand: imported dinos are carnivores or
    // herbivores; a diet with no animals reads 100 % (retail rule).
    // With animals and no LunchBox dish the food side is 0; the dish
    // ratio additionally needs the schedule scale (stats audit).
    int carn = 0;
    int herb = 0;
    for (const int id : pen.occupants) {
        if (id < 0 || id >= static_cast<int>(imported_dinos.size())) {
            continue;
        }
        if (imported_dinos[static_cast<std::size_t>(id)].carnivore) {
            ++carn;
        } else {
            ++herb;
        }
    }
    set_text("FENCE_STATS_CARNI_DISTRIBUTION_INDICS_BACK_CHIFFRE",
             std::to_string(carn == 0 ? 100 : 0) + "%");
    set_text("FENCE_STATS_HERBI_DISTRIBUTION_INDICS_BACK_CHIFFRE",
             std::to_string(herb == 0 ? 100 : 0) + "%");
    std::printf(
        "fence info: paddock=%d name=%s dinos=%zu fill=%d%% carn=%d%% "
        "herb=%d%%\n",
        selected_paddock, name.c_str(), pen.occupants.size(),
        static_cast<int>(pen.fill() * 100.0f), carn == 0 ? 100 : 0,
        herb == 0 ? 100 : 0);

}

void Game::attraction_row_text(int slot, std::string& name, int& iterations,
                                   int& locked) {
    name.clear();
    iterations = 0;
    locked = 0;
    for (const dino::ParkShow& show : show_runtime.shows()) {
        if (show.slot != slot) {
            continue;
        }
        name = show.entry.title.empty() ? show.entry.stem
                                        : show.entry.title;
        iterations = show.iterations;
        locked = show.locked ? 1 : 0;
        break;
    }

}

}  // namespace dino::app
