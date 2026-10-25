#pragma once

#include <string>

#include "tga.h"

namespace dino {

// Loads a retail image. PNG goes through stb_image; TGA through our validated
// reader. Returned pixels are top-left origin RGBA8.
Image load_image(const std::string& path);

}  // namespace dino
