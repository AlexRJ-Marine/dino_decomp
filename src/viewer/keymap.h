#pragma once

#include <SDL3/SDL.h>

#include <array>
#include <string>

namespace dino {

enum class KeyAction {
    OrbitLeft,
    OrbitRight,
    OrbitUp,
    OrbitDown,
    ZoomIn,
    ZoomOut,
    Reset,
    Fullscreen,
    Borderless,
    Mute,
    Quit,
    Count
};

constexpr int kKeyActionCount = static_cast<int>(KeyAction::Count);

const char* key_action_name(KeyAction action);
bool parse_key_action(const std::string& name, KeyAction& action);
bool parse_keycode(const std::string& name, SDL_Keycode& code);
std::string keycode_name(SDL_Keycode code);

struct KeyMap {
    std::array<SDL_Keycode, kKeyActionCount> keys{};

    KeyMap();
    SDL_Keycode operator[](KeyAction action) const {
        return keys[static_cast<std::size_t>(action)];
    }
    SDL_Keycode& operator[](KeyAction action) {
        return keys[static_cast<std::size_t>(action)];
    }
};

void load_keymap(const std::string& path, KeyMap& map);
void save_keymap(const std::string& path, const KeyMap& map);
bool rebind_key(KeyMap& map, const std::string& assignment, std::string& error);
void print_keymap(const KeyMap& map);

}  // namespace dino
