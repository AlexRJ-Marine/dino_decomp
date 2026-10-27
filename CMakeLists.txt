cmake_minimum_required(VERSION 3.25)
project(dino_port LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(dino_tools src/main.cpp)

target_compile_features(dino_tools PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(dino_tools PRIVATE /W4)
else()
    target_compile_options(dino_tools PRIVATE -Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# Portable viewer (Phase 2): SDL3 + OpenGL
# ---------------------------------------------------------------------------

set(SDL3_ROOT "" CACHE PATH "SDL3 MinGW prefix (contains include/SDL3 and lib)")
if(NOT SDL3_ROOT)
    message(FATAL_ERROR
        "SDL3_ROOT is not set. Run scripts/setup-sdl3.ps1, then configure with "
        "-DSDL3_ROOT=C:/Tools/SDL3/SDL3-3.4.18/x86_64-w64-mingw32")
endif()

find_package(SDL3 CONFIG REQUIRED PATHS "${SDL3_ROOT}" NO_DEFAULT_PATH)

add_executable(dino_viewer
    src/viewer/main.cpp
    src/viewer/keymap.cpp
    src/viewer/camera_config.cpp
    src/formats/mesh.cpp
    src/formats/tga.cpp
    src/formats/image.cpp
    src/formats/fnt.cpp
    src/formats/loc.cpp
    src/ui/ini.cpp
    src/ui/overlay.cpp
    src/ui/menu.cpp
    src/ui/park.cpp
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
    src/world/port_save.cpp
    src/world/economy.cpp
)

target_compile_features(dino_viewer PRIVATE cxx_std_20)
target_compile_definitions(dino_viewer PRIVATE SDL_MAIN_HANDLED)
target_link_libraries(dino_viewer PRIVATE SDL3::SDL3 opengl32)

if(MSVC)
    target_compile_options(dino_viewer PRIVATE /W4)
else()
    target_compile_options(dino_viewer PRIVATE -Wall -Wextra)
endif()

add_custom_command(TARGET dino_viewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:dino_viewer>
    COMMENT "Copying SDL3 runtime next to dino_viewer"
)
