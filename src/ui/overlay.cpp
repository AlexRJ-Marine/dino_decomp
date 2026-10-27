#include "overlay.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <sstream>

#include "ini.h"

namespace dino::ui {
namespace {

const char* kPageNames[9] = {
    "MAIN_MENU_PAGE",       "FREE_GAME_PAGE",      "CAMPAIGN_SELECT_PAGE",
    "CAMPAIGN_INFO_PAGE",   "LOAD_GAME_PAGE",      "OPTIONS_PAGE",
    "CINEMATICS_PAGE",      "CREDITS_PAGE",        "LOADING_PAGE",
};

int to_int(const std::string& value, int fallback = 0) {
    return value.empty() ? fallback : std::atoi(value.c_str());
}

float to_float(const std::string& value, float fallback = 0.0f) {
    return value.empty() ? fallback : static_cast<float>(std::atof(value.c_str()));
}

dino::Color to_color(const std::string& value, dino::Color fallback) {
    int r = 0;
    int g = 0;
    int b = 0;
    int a = 255;
    const int count = std::sscanf(value.c_str(), "%d %d %d %d", &r, &g, &b, &a);
    if (count == 3) {
        a = 255;
    }
    if (count < 3) {
        return fallback;
    }
    return {static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g),
            static_cast<std::uint8_t>(b), static_cast<std::uint8_t>(a)};
}

}  // namespace

int Overlay::find(const std::string& name) const {
    const auto it = index.find(name);
    return it == index.end() ? -1 : it->second;
}

const Graphic* Overlay::graphic(const std::string& name) const {
    const auto it = graphics.find(name);
    return it == graphics.end() ? nullptr : &it->second;
}

Widget* Overlay::get(const std::string& name) {
    const int i = find(name);
    return i < 0 ? nullptr : &widgets[static_cast<std::size_t>(i)];
}

const Widget* Overlay::get(const std::string& name) const {
    const int i = find(name);
    return i < 0 ? nullptr : &widgets[static_cast<std::size_t>(i)];
}

bool Overlay::load_menu(const std::string& interface_path,
                        const std::string& db_path, std::string& error) {
    return load_interface(interface_path, db_path, true, error);
}

bool Overlay::load_park(const std::string& interface_path,
                        const std::string& db_path, std::string& error) {
    return load_interface(interface_path, db_path, false, error);
}

bool Overlay::load_interface(const std::string& interface_path,
                             const std::string& db_path, bool menu,
                             std::string& error) {
    try {
        IniFile ini = load_ini(interface_path);
        IniFile db = load_ini(db_path);

        for (const auto& section : ini.sections) {
            if (section.name == "#INCLUDE" || !section.has("TYPE")) {
                continue;
            }
            Widget widget;
            widget.name = section.name;
            widget.type = section.get("TYPE");
            widget.parent = section.get("PARENT");
            widget.x = to_int(section.get("X"));
            widget.y = to_int(section.get("Y"));
            widget.width = to_int(section.get("WIDTH"));
            widget.height = to_int(section.get("HEIGHT"));
            widget.priority = to_int(section.get("PRIORITY"));
            widget.zpriority = to_float(section.get("ZPRIORITY"));
            widget.has_zpriority = section.has("ZPRIORITY");
            widget.has_priority = section.has("PRIORITY");
            widget.visible = section.get("VISIBLE") != "0";
            widget.enable = section.get("ENABLE") != "0";
            widget.texture = section.get("TEXTURE");
            widget.text = section.get("TEXT");
            widget.hint = section.get("HINT");
            widget.font = section.get("TEXT_FONT");
            widget.justify = section.get("TEXT_JUSTIFY");
            widget.text_height = to_int(section.get("TEXT_HEIGHT"));
            widget.button_sub_type = section.get("BTN_SUB_TYPE");
            widget.button_start_state = section.get("BTN_START_STATE");
            widget.list_line_height = to_int(section.get("LB_LINE_HEIGHT"));
            widget.list_scroll_width = to_int(section.get("LB_VSCROLL_WIDTH"));
            widget.list_column0_width = to_int(section.get("LB_COLUMN0_WIDTH"));
            const int progress_max = to_int(section.get("PB_MAX_PROGRESS"));
            if (progress_max > 0) {
                widget.progress_max = progress_max;
            }
            widget.progress_current = to_int(section.get("PB_CURRENT_PROGRESS"));
            if (section.has("TEXT_COLOR")) {
                widget.text_color = to_color(section.get("TEXT_COLOR"),
                                             {255, 255, 255, 255});
            }
            if (section.has("BACK_COLOR")) {
                widget.has_back_color = true;
                widget.back_color = to_color(section.get("BACK_COLOR"),
                                             {0, 0, 0, 0});
            }
            if (section.has("BORDER_COLOR")) {
                widget.has_border_color = true;
                widget.border_color = to_color(section.get("BORDER_COLOR"),
                                               {0, 0, 0, 0});
            }
            if (section.get("ALIGN_LEFT") == "1") widget.align |= 1;
            if (section.get("ALIGN_UP") == "1") widget.align |= 2;
            if (section.get("ALIGN_RIGHT") == "1") widget.align |= 4;
            if (section.get("ALIGN_DOWN") == "1") widget.align |= 8;
            if (section.get("ALIGN_CENTER") == "1") widget.align |= 16;
            widget.base_x = widget.x;
            widget.base_y = widget.y;
            widget.base_width = widget.width;
            widget.base_height = widget.height;

            index[widget.name] = static_cast<int>(widgets.size());
            widgets.push_back(std::move(widget));
        }

        for (auto& widget : widgets) {
            if (widget.parent.empty() || widget.parent == "ROOT") {
                widget.parent_index = -1;
                continue;
            }
            widget.parent_index = find(widget.parent);
            if (widget.parent_index >= 0) {
                widgets[static_cast<std::size_t>(widget.parent_index)]
                    .children.push_back(
                        static_cast<int>(&widget - widgets.data()));
            }
        }

        // moWidget::Draw walks the tree parent-first. The flat draw list sorts
        // by PRIORITY then ZPRIORITY, so a child without its own values must
        // inherit the nearest ancestor's; otherwise z=0 children (list slots,
        // names) draw under the z=0.49 panel art, and P=0 buttons (the road
        // panel's CONSTRUC_ROAD/METRO_BUTTON) draw under P=10 weave panels
        // declared before them. Inferred: retail Load (0x5bca20) only applies
        // PRIORITY/ZPRIORITY when present (ctor 0x5bc330 defaults -1 / 0.0),
        // and the exact retail resolution is not decoded
        // (notes/ui-recon.md, draw order).
        for (Widget& widget : widgets) {
            const bool need_z = !widget.has_zpriority;
            const bool need_p = !widget.has_priority;
            bool got_z = !need_z;
            bool got_p = !need_p;
            int parent = widget.parent_index;
            while (parent >= 0 && (!got_z || !got_p)) {
                const Widget& ancestor = widgets[static_cast<std::size_t>(parent)];
                if (!got_z && ancestor.has_zpriority) {
                    widget.zpriority = ancestor.zpriority;
                    got_z = true;
                }
                if (!got_p && ancestor.has_priority) {
                    widget.priority = ancestor.priority;
                    got_p = true;
                }
                parent = ancestor.parent_index;
            }
        }

        // Text-only children (the list counters, names) sit on top of their
        // parent's panel art even when the INI gives them a lower ZPRIORITY.
        // Lower ZPRIORITY is nearer, so "on top" means parent - 0.001.
        for (Widget& widget : widgets) {
            if (!widget.texture.empty() || widget.font.empty() ||
                widget.parent_index < 0) {
                continue;
            }
            const Widget& parent = widgets[static_cast<std::size_t>(
                widget.parent_index)];
            if (widget.zpriority <= parent.zpriority) {
                widget.zpriority = parent.zpriority - 0.001f;
            }
        }

        roots.clear();
        for (int i = 0; i < static_cast<int>(widgets.size()); ++i) {
            if (widgets[static_cast<std::size_t>(i)].parent_index < 0) {
                roots.push_back(i);
            }
        }
        if (menu) {
            for (const char* name : kPageNames) {
                const int i = find(name);
                if (i >= 0) {
                    pages.push_back(i);
                }
            }
            if (pages.size() != 9) {
                error = "menu: expected 9 pages, got " +
                        std::to_string(pages.size());
                return false;
            }
        }

        for (const auto& section : db.sections) {
            if (section.name == "#INCLUDE" || !section.has("FILENAME")) {
                continue;
            }
            Graphic graphic;
            graphic.name = section.name;
            graphic.filename = section.get("FILENAME");
            graphic.rect.x = to_int(section.get("X"));
            graphic.rect.y = to_int(section.get("Y"));
            graphic.rect.width = to_int(section.get("WIDTH"));
            graphic.rect.height = to_int(section.get("HEIGHT"));
            graphic.objects = to_int(section.get("NB_OBJECTS"), 1);
            std::istringstream skip(section.get("SKIP_LIST"));
            int frame = 0;
            while (skip >> frame) {
                graphic.skip.push_back(frame);
            }
            graphics[graphic.name] = std::move(graphic);
        }

        layout();
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}

void Overlay::layout() {
    layout_for_size(800, 600);
}

void Overlay::layout_for_size(int view_w, int view_h) {
    // Mirrors moWidget::OnResize (FUN_005bc000). Positions and sizes are
    // relative to the parent; base_* hold the 800x600 authoring values.
    std::function<void(int, int, int, int, int, int, int)> place =
        [&](int index, int parent_x, int parent_y, int parent_new_w,
            int parent_new_h, int parent_old_w, int parent_old_h) {
            Widget& w = widgets[static_cast<std::size_t>(index)];
            w.x = w.base_x;
            w.y = w.base_y;
            w.width = w.base_width;
            w.height = w.base_height;

            if ((w.align & 16) != 0) {
                w.x = (parent_new_w - w.base_width) / 2;
                w.y = (parent_new_h - w.base_height) / 2;
            } else {
                const int dw = parent_new_w - parent_old_w;
                const int dh = parent_new_h - parent_old_h;
                if ((w.align & 1) != 0 && (w.align & 4) != 0) {
                    w.width = std::max(0, w.base_width + dw);
                } else if ((w.align & 4) != 0) {
                    w.x = w.base_x + dw;
                }
                if ((w.align & 2) != 0 && (w.align & 8) != 0) {
                    w.height = std::max(0, w.base_height + dh);
                } else if ((w.align & 8) != 0) {
                    w.y = w.base_y + dh;
                }
            }

            w.abs_x = parent_x + w.x;
            w.abs_y = parent_y + w.y;
            for (const int child : w.children) {
                place(child, w.abs_x, w.abs_y, w.width, w.height, w.base_width,
                      w.base_height);
            }
        };

    for (const int root_index : roots) {
        place(root_index, 0, 0, view_w, view_h, 800, 600);
    }
}

int Overlay::page_root(int widget_index) const {
    int i = widget_index;
    while (i >= 0 && widgets[static_cast<std::size_t>(i)].parent_index >= 0) {
        i = widgets[static_cast<std::size_t>(i)].parent_index;
    }
    return i;
}

bool Overlay::visible_on_page(int widget_index, int page_index) const {
    if (page_index < 0 || page_index >= static_cast<int>(pages.size())) {
        return false;
    }
    const int active_root = pages[static_cast<std::size_t>(page_index)];
    if (page_root(widget_index) != active_root) {
        return false;
    }
    int i = widget_index;
    while (i >= 0) {
        if (i != active_root && !widgets[static_cast<std::size_t>(i)].visible) {
            return false;
        }
        i = widgets[static_cast<std::size_t>(i)].parent_index;
    }
    return true;
}

bool Overlay::visible_in_tree(int widget_index) const {
    int i = widget_index;
    while (i >= 0) {
        if (!widgets[static_cast<std::size_t>(i)].visible) {
            return false;
        }
        i = widgets[static_cast<std::size_t>(i)].parent_index;
    }
    return true;
}

int Overlay::hit_test(int x, int y) const {
    for (int i = static_cast<int>(widgets.size()) - 1; i >= 0; --i) {
        const Widget& widget = widgets[static_cast<std::size_t>(i)];
        if (widget.type != "BUTTON" && widget.type != "LIST" &&
            widget.type != "EDIT") {
            continue;
        }
        if (!widget.enable || !visible_in_tree(i)) {
            continue;
        }
        if (x >= widget.abs_x && x < widget.abs_x + widget.width &&
            y >= widget.abs_y && y < widget.abs_y + widget.height) {
            return i;
        }
    }
    return -1;
}

bool Overlay::set_visible(const std::string& name, bool visible) {
    const int i = find(name);
    if (i < 0) {
        return false;
    }
    widgets[static_cast<std::size_t>(i)].visible = visible;
    return true;
}

std::string resolve_loc_text(const std::string& text, const dino::Loc& loc) {
    if (text.empty() || text.front() != '&') {
        return text;
    }
    const std::string* value = loc.find(text);
    return value != nullptr ? *value : text;
}

}  // namespace dino::ui
