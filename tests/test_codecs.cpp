#include "raster/codecs.h"
#include "raster/quantize.h"

#include <catch2/catch_test_macros.hpp>

using namespace lp;

namespace {

std::uint32_t le32(const std::vector<std::uint8_t>& b, std::size_t off)
{
    return b[off] | (b[off + 1] << 8) | (b[off + 2] << 16) | (std::uint32_t(b[off + 3]) << 24);
}

} // namespace

TEST_CASE("quantize keeps exact palettes")
{
    Image img(4, 1);
    img.setPixel(1, 0, kBlack);
    img.setPixel(2, 0, rgb(255, 0, 0));
    const IndexedImage q = quantize(img, 256);
    REQUIRE(q.palette.size() == 3);
    for (int x = 0; x < 4; ++x)
        REQUIRE(q.palette[q.indices[x]] == img.pixel(x, 0));
}

TEST_CASE("median cut limits colors")
{
    Image img(64, 64);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x)
            img.setPixel(x, y, rgb(x * 4, y * 4, (x + y) * 2));
    const IndexedImage q = quantize(img, 16);
    REQUIRE(q.palette.size() <= 16);
    REQUIRE(q.indices.size() == 64 * 64);
}

TEST_CASE("bmp headers for every depth")
{
    Image img(5, 3, rgb(10, 200, 30));
    for (int bpp : {1, 4, 8, 24}) {
        const auto b = encodeBmp(img, bpp);
        REQUIRE(b[0] == 'B');
        REQUIRE(b[1] == 'M');
        REQUIRE(le32(b, 2) == b.size());
        REQUIRE(le32(b, 18) == 5);
        REQUIRE(le32(b, 22) == 3);
        REQUIRE((b[28] | (b[29] << 8)) == bpp);
        const std::uint32_t row = ((5 * bpp + 31) / 32) * 4;
        const std::uint32_t palette = bpp == 24 ? 0 : (1u << bpp) * 4;
        REQUIRE(le32(b, 10) == 54 + palette);
        REQUIRE(b.size() == 54 + palette + row * 3);
    }
}

TEST_CASE("24-bit bmp stores BGR bottom-up")
{
    Image img(1, 2);
    img.setPixel(0, 0, rgb(1, 2, 3));
    img.setPixel(0, 1, rgb(4, 5, 6));
    const auto b = encodeBmp(img, 24);
    REQUIRE(b[54] == 6);
    REQUIRE(b[55] == 5);
    REQUIRE(b[56] == 4);
    REQUIRE(b[58] == 3);
}

TEST_CASE("gif structure")
{
    const auto g = encodeGif(Image(7, 3, kBlack));
    REQUIRE(std::string(g.begin(), g.begin() + 6) == "GIF89a");
    REQUIRE((g[6] | (g[7] << 8)) == 7);
    REQUIRE((g[8] | (g[9] << 8)) == 3);
    REQUIRE(g.back() == 0x3B);
}
