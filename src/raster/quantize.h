#pragma once

#include "raster/image.h"

#include <cstdint>
#include <vector>

namespace lp {

struct IndexedImage {
    int width = 0;
    int height = 0;
    std::vector<Rgba> palette;
    std::vector<std::uint8_t> indices; // row-major, top-down
};

// Exact palette when the image has at most `maxColors` colors, median cut otherwise.
IndexedImage quantize(const Image& img, int maxColors);

// Maps every pixel to the nearest palette entry.
IndexedImage mapToPalette(const Image& img, const std::vector<Rgba>& palette);

// The standard 16-color VGA palette used by 16-color bitmaps.
const std::vector<Rgba>& vga16Palette();

} // namespace lp
