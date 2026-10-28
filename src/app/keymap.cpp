#include "keymap.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace dino {
namespace {

struct KeyName {
    const char* name;
    SDL_Keycode code;
};

const KeyName kNamedKeys[] = {
    {"LEFT", SDLK_LEFT},       {"RIGHT", SDLK_RIGHT},
    {"UP", SDLK_UP},           {"DOWN", SDLK_DOWN},
    {"ESC", SDLK_ESCAPE},      {"ESCAPE", SDLK_ESCAPE},
    {"SPACE", SDLK_SPACE},     {"TAB", SDLK_TAB},
    {"ENTER", SDLK_RETURN},    {"RETURN", SDLK_RETURN},
    {"BACKSPACE", SDLK_BACKSPACE}, {"DELETE", SDLK_DELETE},
    {"INSERT", SDLK_INSERT},   {"HOME", SDLK_HOME},
    {"END", SDLK_END},         {"PAGEUP", SDLK_PAGEUP},
    {"PGUP", SDLK_PAGEUP},     {"PAGEDOWN", SDLK_PAGEDOWN},
    {"PGDN", SDLK_PAGEDOWN},   {"PLUS", SDLK_PLUS},
    {"MINUS", SDLK_MINUS},     {"EQUALS", SDLK_EQUALS},
    {"KP_PLUS", SDLK_KP_PLUS}, {"KP_MINUS", SDLK_KP_MINUS},
    {"LCTRL", SDLK_LCTRL},     {"RCTRL", SDLK_RCTRL},
    {"LSHIFT", SDLK_LSHIFT},   {"RSHIFT", SDLK_RSHIFT},
    {"LALT", SDLK_LALT},       {"RALT", SDLK_RALT},
    {"F1", SDLK_F1}, {"F2", SDLK_F2}, {"F3", SDLK_F3}, {"F4", SDLK_F4},
    {"F5", SDLK_F5}, {"F6", SDLK_F6}, {"F7", SDLK_F7}, {"F8", SDLK_F8},
    {"F9", SDLK_F9}, {"F10", SDLK_F10}, {"F11", SDLK_F11}, {"F12", SDLK_F12},
};

std::string trim(const std::string& s) {
    const auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) {
        return "";
    }
    const auto b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::string upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

const KeyName action_names[] = {
    {"orbit_left", static_cast<SDL_Keycode>(0)},
    {"orbit_right", static_cast<SDL_Keycode>(0)},
    {"orbit_up", static_cast<SDL_Keycode>(0)},
    {"orbit_down", static_cast<SDL_Keycode>(0)},
    {"zoom_in", static_cast<SDL_Keycode>(0)},
    {"zoom_out", static_cast<SDL_Keycode>(0)},
    {"reset", static_cast<SDL_Keycode>(0)},
    {"fullscreen", static_cast<SDL_Keycode>(0)},
    {"borderless", static_cast<SDL_Keycode>(0)},
    {"mute", static_cast<SDL_Keycode>(0)},
    {"move_forward", static_cast<SDL_Keycode>(0)},
    {"move_back", static_cast<SDL_Keycode>(0)},
    {"move_left", static_cast<SDL_Keycode>(0)},
    {"move_right", static_cast<SDL_Keycode>(0)},
    {"move_up", static_cast<SDL_Keycode>(0)},
    {"move_down", static_cast<SDL_Keycode>(0)},
    {"sprint", static_cast<SDL_Keycode>(0)},
    {"toggle_camera", static_cast<SDL_Keycode>(0)},
    {"quit", static_cast<SDL_Keycode>(0)},
};

}  // namespace

KeyMap::KeyMap() {
    (*this)[KeyAction::OrbitLeft] = SDLK_LEFT;
    (*this)[KeyAction::OrbitRight] = SDLK_RIGHT;
    (*this)[KeyAction::OrbitUp] = SDLK_UP;
    (*this)[KeyAction::OrbitDown] = SDLK_DOWN;
    (*this)[KeyAction::ZoomIn] = SDLK_PLUS;
    (*this)[KeyAction::ZoomOut] = SDLK_MINUS;
    (*this)[KeyAction::Reset] = SDLK_R;
    (*this)[KeyAction::Fullscreen] = SDLK_F11;
    (*this)[KeyAction::Borderless] = SDLK_F10;
    (*this)[KeyAction::Mute] = SDLK_M;
    (*this)[KeyAction::MoveForward] = SDLK_W;
    (*this)[KeyAction::MoveBack] = SDLK_S;
    (*this)[KeyAction::MoveLeft] = SDLK_A;
    (*this)[KeyAction::MoveRight] = SDLK_D;
    (*this)[KeyAction::MoveUp] = SDLK_E;
    (*this)[KeyAction::MoveDown] = SDLK_Q;
    (*this)[KeyAction::Sprint] = SDLK_LSHIFT;
    (*this)[KeyAction::ToggleCamera] = SDLK_C;
    (*this)[KeyAction::Quit] = SDLK_ESCAPE;
}

const char* key_action_name(KeyAction action) {
    const auto index = static_cast<std::size_t>(action);
    return index < static_cast<std::size_t>(KeyAction::Count)
        ? action_names[index].name : "?";
}

bool parse_key_action(const std::string& name, KeyAction& action) {
    for (int i = 0; i < kKeyActionCount; ++i) {
        if (name == action_names[i].name) {
            action = static_cast<KeyAction>(i);
            return true;
        }
    }
    return false;
}

bool parse_keycode(const std::string& name, SDL_Keycode& code) {
    if (name.size() == 1) {
        const unsigned char c = static_cast<unsigned char>(name[0]);
        if (std::isalnum(c)) {
            code = static_cast<SDL_Keycode>(std::tolower(c));
            return true;
        }
    }
    const std::string key = upper(name);
    for (const auto& entry : kNamedKeys) {
        if (key == entry.name) {
            code = entry.code;
            return true;
        }
    }
    return false;
}

std::string keycode_name(SDL_Keycode code) {
    for (const auto& entry : kNamedKeys) {
        if (code == entry.code) {
            return entry.name;
        }
    }
    if (code >= 32 && code < 127) {
        return std::string(1, static_cast<char>(std::toupper(static_cast<unsigned char>(code))));
    }
    std::ostringstream out;
    out << "0x" << std::hex << static_cast<unsigned int>(code);
    return out.str();
}

bool rebind_key(KeyMap& map, const std::string& assignment, std::string& error) {
    const std::size_t eq = assignment.find('=');
    if (eq == std::string::npos) {
        error = "expected ACTION=KEY: " + assignment;
        return false;
    }
    const std::string action_name = trim(assignment.substr(0, eq));
    const std::string key_name = trim(assignment.substr(eq + 1));
    KeyAction action;
    if (!parse_key_action(action_name, action)) {
        error = "unknown action: " + action_name;
        return false;
    }
    SDL_Keycode code;
    if (!parse_keycode(key_name, code)) {
        error = "unknown key: " + key_name;
        return false;
    }
    map[action] = code;
    return true;
}

void load_keymap(const std::string& path, KeyMap& map) {
    std::ifstream in(path);
    if (!in) {
        return;
    }
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::string error;
        (void)rebind_key(map, line, error);
    }
}

void save_keymap(const std::string& path, const KeyMap& map) {
    std::ofstream out(path);
    if (!out) {
        return;
    }
    for (int i = 0; i < kKeyActionCount; ++i) {
        const auto action = static_cast<KeyAction>(i);
        out << key_action_name(action) << "=" << keycode_name(map[action]) << "\n";
    }
}

void print_keymap(const KeyMap& map) {
    for (int i = 0; i < kKeyActionCount; ++i) {
        const auto action = static_cast<KeyAction>(i);
        std::printf("%-12s %s\n", key_action_name(action),
                    keycode_name(map[action]).c_str());
    }
}

}  // namespace dino
