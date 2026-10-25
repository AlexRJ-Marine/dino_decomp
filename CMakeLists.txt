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
