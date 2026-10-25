#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "../formats/loc.h"
#include "../formats/mesh.h"  // dino::Color

namespace dino::ui {

struct GraphicRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct Graphic {
    std::string name;
    std::string filename;
    GraphicRect rect;
    int objects = 1;
    std::vector<int> skip;
};

struct Widget {
    std::string name;
    std::string type;   // WIDGET/BUTTON/LIST/...
    std::string parent;
    int parent_index = -1;
    std::vector<int> children;

    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int priority = 0;
    float zpriority = 0.0f;

    bool visible = true;
    bool enable = true;
    bool has_back_color = false;
    bool has_border_color = false;
    dino::Color back_color{0, 0, 0, 0};
    dino::Color border_color{0, 0, 0, 0};
    dino::Color text_color{255, 255, 255, 255};

    std::string texture;
    std::string text;
    std::string hint;
    std::string font;
    std::string justify;
    std::string button_sub_type;
    std::string button_start_state;
    int text_height = 0;
    int list_line_height = 0;
    int list_scroll_width = 0;
    int list_column0_width = 0;

    // ALIGN_* bits: 1 left, 2 up, 4 right, 8 down, 16 center.
    int align = 0;

    int abs_x = 0;
    int abs_y = 0;
};

struct Overlay {
    std::vector<Widget> widgets;
    std::unordered_map<std::string, int> index;
    std::unordered_map<std::string, Graphic> graphics;
    std::vector<int> pages;  // menu state order (0..8)
    std::vector<int> roots;  // all PARENT=ROOT widgets

    int find(const std::string& name) const;
    const Graphic* graphic(const std::string& name) const;
    Widget* get(const std::string& name);
    const Widget* get(const std::string& name) const;

    // Reads MenuInterface.Ini + MenuGraphicDB.ini. `asset_root` is the game
    // root used to resolve texture filenames.
    bool load_menu(const std::string& interface_path, const std::string& db_path,
                   std::string& error);
    // Reads ParkInterface.Ini + ParkGraphicDB.ini (all roots, no pages).
    bool load_park(const std::string& interface_path, const std::string& db_path,
                   std::string& error);
    bool load_interface(const std::string& interface_path,
                        const std::string& db_path, bool menu, std::string& error);
    void layout();

    // True when `widget` is on the current page and every ancestor is visible.
    bool visible_on_page(int widget_index, int page_index) const;
    bool visible_in_tree(int widget_index) const;
    int page_root(int widget_index) const;
};

std::string resolve_loc_text(const std::string& text, const dino::Loc& loc);

}  // namespace dino::ui
