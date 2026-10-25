#include "tga.h"

#include <fstream>
#include <stdexcept>

namespace dino {
namespace {

std::vector<std::uint8_t> read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot open " + path);
    }
    in.seekg(0, std::ios::end);
    const auto size = in.tellg();
    if (size < 0) {
        throw std::runtime_error("cannot size " + path);
    }
    in.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
    if (!data.empty()) {
        in.read(reinterpret_cast<char*>(data.data()), size);
        if (!in) {
            throw std::runtime_error("short read " + path);
        }
    }
    return data;
}

std::uint16_t le16(const std::vector<std::uint8_t>& d, std::size_t off) {
    return static_cast<std::uint16_t>(d[off]) |
           static_cast<std::uint16_t>(d[off + 1] << 8);
}

struct Pixel {
    std::uint8_t r, g, b, a;
};

Pixel decode_pixel(const std::vector<std::uint8_t>& d, std::size_t off, int type,
                   int bpp) {
    if (type == 2) {
        if (bpp == 3) {
            return {d[off + 2], d[off + 1], d[off], 255};
        }
        if (bpp == 4) {
            return {d[off + 2], d[off + 1], d[off], d[off + 3]};
        }
        if (bpp == 2) {
            const std::uint16_t v = le16(d, off);
            const auto expand = [](std::uint16_t x) {
                const std::uint8_t c = static_cast<std::uint8_t>(x & 0x1F);
                return static_cast<std::uint8_t>((c << 3) | (c >> 2));
            };
            return {expand(static_cast<std::uint16_t>((v >> 10) & 0x1F)),
                    expand(static_cast<std::uint16_t>((v >> 5) & 0x1F)),
                    expand(static_cast<std::uint16_t>(v & 0x1F)),
                    static_cast<std::uint8_t>((v & 0x8000) ? 255 : 0)};
        }
    } else if (type == 3 && bpp == 1) {
        const std::uint8_t v = d[off];
        return {v, v, v, 255};
    }
    throw std::runtime_error("tga: unsupported pixel encoding");
}

}  // namespace

Image load_tga(const std::string& path) {
    const auto d = read_file(path);
    if (d.size() < 18) {
        throw std::runtime_error("tga: shorter than header in " + path);
    }
    const std::uint8_t id_length = d[0];
    const std::uint8_t cmap_type = d[1];
    const std::uint8_t image_type = d[2];
    const int width = le16(d, 12);
    const int height = le16(d, 14);
    const int bpp = d[16] / 8;
    const std::uint8_t descriptor = d[17];

    if (cmap_type != 0) {
        throw std::runtime_error("tga: color maps are not supported in " + path);
    }
    if (image_type != 2 && image_type != 3 && image_type != 10 && image_type != 11) {
        throw std::runtime_error("tga: unsupported image type in " + path);
    }
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("tga: zero dimensions in " + path);
    }
    if (bpp != 1 && bpp != 2 && bpp != 3 && bpp != 4) {
        throw std::runtime_error("tga: unsupported depth in " + path);
    }

    std::size_t off = 18 + id_length;
    if (off > d.size()) {
        throw std::runtime_error("tga: ID past EOF in " + path);
    }
    const std::size_t total = static_cast<std::size_t>(width) * height;
    std::vector<Pixel> pixels(total);
    const bool rle = image_type == 10 || image_type == 11;
    if (!rle) {
        if (off + total * bpp > d.size()) {
            throw std::runtime_error("tga: pixels past EOF in " + path);
        }
        for (std::size_t i = 0; i < total; ++i) {
            pixels[i] = decode_pixel(d, off, image_type, bpp);
            off += bpp;
        }
    } else {
        std::size_t written = 0;
        while (written < total) {
            if (off >= d.size()) {
                throw std::runtime_error("tga: RLE past EOF in " + path);
            }
            const std::uint8_t packet = d[off++];
            const std::size_t count = (packet & 0x7F) + 1;
            if (packet & 0x80) {
                const Pixel p = decode_pixel(d, off, image_type == 10 ? 2 : 3, bpp);
                off += bpp;
                for (std::size_t i = 0; i < count && written < total; ++i) {
                    pixels[written++] = p;
                }
            } else {
                for (std::size_t i = 0; i < count && written < total; ++i) {
                    pixels[written++] = decode_pixel(d, off, image_type == 10 ? 2 : 3, bpp);
                    off += bpp;
                }
            }
        }
    }

    Image image;
    image.width = width;
    image.height = height;
    image.rgba.resize(total * 4);
    const bool flip_v = (descriptor & 0x20) == 0;
    const bool flip_h = (descriptor & 0x10) != 0;
    for (int y = 0; y < height; ++y) {
        const int src_y = flip_v ? (height - 1 - y) : y;
        for (int x = 0; x < width; ++x) {
            const int src_x = flip_h ? (width - 1 - x) : x;
            const Pixel& p = pixels[static_cast<std::size_t>(src_y) * width + src_x];
            const std::size_t dst = (static_cast<std::size_t>(y) * width + x) * 4;
            image.rgba[dst + 0] = p.r;
            image.rgba[dst + 1] = p.g;
            image.rgba[dst + 2] = p.b;
            image.rgba[dst + 3] = p.a;
        }
    }
    return image;
}

}  // namespace dino
