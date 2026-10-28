// Screenshot output helpers (see capture.h).

#include "capture.h"

#include <cstdio>
#include <fstream>

void write_bmp(const std::string& path, int width, int height,
               const std::vector<std::uint8_t>& rgba) {
    const int row = width * 3;
    const int padding = (4 - (row % 4)) % 4;
    const std::uint32_t image_size =
        static_cast<std::uint32_t>((row + padding) * height);
    const std::uint32_t file_size = 14 + 40 + image_size;

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        std::fprintf(stderr, "cannot open screenshot %s\n", path.c_str());
        return;
    }
    auto u16 = [&](std::uint16_t v) { out.write(reinterpret_cast<const char*>(&v), 2); };
    auto u32 = [&](std::uint32_t v) { out.write(reinterpret_cast<const char*>(&v), 4); };

    out.put('B');
    out.put('M');
    u32(file_size);
    u16(0);
    u16(0);
    u32(14 + 40);
    u32(40);
    u32(static_cast<std::uint32_t>(width));
    u32(static_cast<std::uint32_t>(height));
    u16(1);
    u16(24);
    u32(0);
    u32(image_size);
    u32(2835);
    u32(2835);
    u32(0);
    u32(0);

    const std::uint8_t pad[3] = {0, 0, 0};
    for (int y = 0; y < height; ++y) {
        const std::uint8_t* src = rgba.data() + static_cast<std::size_t>(y) * width * 4;
        for (int x = 0; x < width; ++x) {
            out.put(static_cast<char>(src[x * 4 + 2]));
            out.put(static_cast<char>(src[x * 4 + 1]));
            out.put(static_cast<char>(src[x * 4 + 0]));
        }
        out.write(reinterpret_cast<const char*>(pad), padding);
    }
    std::printf("wrote screenshot %s (%dx%d)\n", path.c_str(), width, height);
}

