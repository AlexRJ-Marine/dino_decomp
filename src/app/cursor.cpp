#include "cursor.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <exception>

#include "../formats/tga.h"
#include "asset_paths.h"

namespace dino::app {
namespace {

SDL_Cursor* g_main = nullptr;
SDL_Cursor* g_sell = nullptr;
SDL_Cursor* g_fence = nullptr;
CursorMode g_mode = CursorMode::Main;
bool g_initialized = false;
bool g_has_mode = false;

SDL_Cursor* load_cursor(const std::string& path) {
    try {
        dino::Image image = dino::load_tga(path);
        if (image.width <= 0 || image.height <= 0 ||
            image.rgba.size() !=
                static_cast<std::size_t>(image.width) *
                    static_cast<std::size_t>(image.height) * 4) {
            std::fprintf(stderr, "cursor: bad image %s\n", path.c_str());
            return nullptr;
        }
        SDL_Surface* surface = SDL_CreateSurfaceFrom(
            image.width, image.height, SDL_PIXELFORMAT_RGBA32,
            image.rgba.data(), image.width * 4);
        if (surface == nullptr) {
            std::fprintf(stderr, "cursor: surface %s: %s\n", path.c_str(),
                         SDL_GetError());
            return nullptr;
        }
        // The retail .ini has no hotspot field; the pointer art starts at the
        // top-left of the bitmap.
        SDL_Cursor* cursor = SDL_CreateColorCursor(surface, 0, 0);
        SDL_DestroySurface(surface);
        if (cursor == nullptr) {
            std::fprintf(stderr, "cursor: create %s: %s\n", path.c_str(),
                         SDL_GetError());
        }
        return cursor;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "cursor: %s\n", e.what());
        return nullptr;
    }
}

SDL_Cursor* cursor_for(CursorMode mode) {
    switch (mode) {
        case CursorMode::Sell:
            return g_sell != nullptr ? g_sell : g_main;
        case CursorMode::Fence:
            return g_fence != nullptr ? g_fence : g_main;
        case CursorMode::Main:
        default:
            return g_main;
    }
}

}  // namespace

void init_cursors(const std::string& asset_root) {
    shutdown_cursors();
    g_main = load_cursor(resolve_asset(
        asset_root, "Data/Graphics/Cursors/CursorMain_wood.tga"));
    g_sell = load_cursor(
        resolve_asset(asset_root, "Data/Graphics/Cursors/money.tga"));
    g_fence = load_cursor(resolve_asset(
        asset_root, "Data/Graphics/Cursors/put_in_enclos.tga"));
    g_initialized = true;
    g_has_mode = false;
    set_cursor_mode(CursorMode::Main);
}

void set_cursor_mode(CursorMode mode) {
    if (!g_initialized) {
        return;
    }
    if (g_has_mode && mode == g_mode) {
        return;
    }
    g_mode = mode;
    g_has_mode = true;
    SDL_Cursor* cursor = cursor_for(mode);
    if (cursor != nullptr) {
        SDL_SetCursor(cursor);
    }
}

void shutdown_cursors() {
    // Do not leave a destroyed cursor installed on the window.
    SDL_SetCursor(SDL_GetDefaultCursor());
    if (g_main != nullptr) {
        SDL_DestroyCursor(g_main);
        g_main = nullptr;
    }
    if (g_sell != nullptr) {
        SDL_DestroyCursor(g_sell);
        g_sell = nullptr;
    }
    if (g_fence != nullptr) {
        SDL_DestroyCursor(g_fence);
        g_fence = nullptr;
    }
    g_initialized = false;
    g_has_mode = false;
}

}  // namespace dino::app
