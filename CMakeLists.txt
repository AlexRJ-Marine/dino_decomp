cmake_minimum_required(VERSION 3.25)
project(dino_port LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# dino_core: formats, world simulation and UI state.
#
# No SDL, no OpenGL: the readers can be linked into a headless tool. Keep this
# true - `scripts/check-env.ps1` relies on it and `dino_tools --selftest`
# depends on it.
# ---------------------------------------------------------------------------

add_library(dino_core STATIC
    src/formats/mesh.cpp
    src/formats/tga.cpp
    src/formats/image.cpp
    src/formats/fnt.cpp
    src/formats/loc.cpp
    src/formats/skel.cpp
    src/formats/skin.cpp
    src/formats/anim.cpp
    src/formats/wav.cpp
    src/formats/lnd.cpp
    src/formats/imp.cpp
    src/formats/blf.cpp
    src/formats/sav.cpp
    src/formats/bbi.cpp
    src/formats/bvi.cpp
    src/formats/dino_data.cpp
    src/formats/wld.cpp
    src/formats/bai.cpp
    src/formats/market.cpp
    src/formats/bsi.cpp
    src/formats/campaign.cpp
    src/formats/bei.cpp
    src/formats/shw.cpp
    src/formats/bfi.cpp
    src/formats/mpg.cpp
    src/formats/general_params.cpp
    src/ui/ini.cpp
    src/ui/overlay.cpp
    src/ui/menu.cpp
    src/ui/park.cpp
    src/world/terrain.cpp
    src/world/vegetation.cpp
    src/world/build.cpp
    src/world/food.cpp
    src/world/employees.cpp
    src/world/visitors.cpp
    src/world/dino_actions.cpp
    src/world/danger_grid.cpp
    src/world/campaign_objectives.cpp
    src/world/campaign_runtime.cpp
    src/world/show_runtime.cpp
    src/world/port_save.cpp
    src/world/economy.cpp
)

target_compile_features(dino_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(dino_core PRIVATE /W4)
else()
    target_compile_options(dino_core PRIVATE -Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# dino_tools: headless CLI. Runs the self-test without opening a window.
# ---------------------------------------------------------------------------

add_executable(dino_tools
    src/main.cpp
    src/selftest.cpp
)

target_link_libraries(dino_tools PRIVATE dino_core)

if(MSVC)
    target_compile_options(dino_tools PRIVATE /W4)
else()
    target_compile_options(dino_tools PRIVATE -Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# Portable game shell: SDL3 + OpenGL
# ---------------------------------------------------------------------------

set(SDL3_ROOT "" CACHE PATH "SDL3 MinGW prefix (contains include/SDL3 and lib)")
if(NOT SDL3_ROOT)
    message(FATAL_ERROR
        "SDL3_ROOT is not set. Use scripts/build.ps1 (it finds SDL3 under DINO_TOOLS), "
        "or configure with -DSDL3_ROOT=<tools>/SDL3/SDL3-3.4.18/x86_64-w64-mingw32")
endif()

find_package(SDL3 CONFIG REQUIRED PATHS "${SDL3_ROOT}" NO_DEFAULT_PATH)

add_executable(dino_port
    src/app/main.cpp
    src/app/keymap.cpp
    src/app/camera_config.cpp
    src/app/capture.cpp
    src/selftest.cpp
)

target_compile_features(dino_port PRIVATE cxx_std_20)
target_compile_definitions(dino_port PRIVATE SDL_MAIN_HANDLED)
target_link_libraries(dino_port PRIVATE dino_core SDL3::SDL3 opengl32)

if(MSVC)
    target_compile_options(dino_port PRIVATE /W4)
else()
    target_compile_options(dino_port PRIVATE -Wall -Wextra)
endif()

add_custom_command(TARGET dino_port POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:dino_port>
    COMMENT "Copying SDL3 runtime next to dino_port"
)
