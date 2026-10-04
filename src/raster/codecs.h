#pragma once

#include "raster/image.h"

#include <cstdint>
#include <vector>

namespace lp {

// Windows bitmap with 1, 4, 8 or 24 bits per pixel (BITMAPINFOHEADER, 96 dpi).
std::vector<std::uint8_t> encodeBmp(const Image& img, int bitsPerPixel);

// GIF89a, single frame, up to 256 colors (median cut when needed).
std::vector<std::uint8_t> encodeGif(const Image& img);

} // namespace lp
