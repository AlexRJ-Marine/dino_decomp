// Screenshot output helpers.

#ifndef DINO_APP_CAPTURE_H
#define DINO_APP_CAPTURE_H

#include <cstdint>
#include <string>
#include <vector>

// Writes a 24-bit BMP. `rgba` holds width*height*4 bytes in OpenGL order
// (bottom row first); the file uses a positive height, so it opens upright.
// Used by --screenshot, --capture-dir and --video-frame.
void write_bmp(const std::string& path, int width, int height,
               const std::vector<std::uint8_t>& rgba);

#endif  // DINO_APP_CAPTURE_H
