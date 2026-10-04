#include "raster/codecs.h"

#include "raster/quantize.h"
#include "raster/transform.h"

#include <unordered_map>

namespace lp {

namespace {

void put16(std::vector<std::uint8_t>& out, std::uint32_t v)
{
    out.push_back(std::uint8_t(v));
    out.push_back(std::uint8_t(v >> 8));
}

void put32(std::vector<std::uint8_t>& out, std::uint32_t v)
{
    put16(out, v & 0xFFFF);
    put16(out, v >> 16);
}

class BitWriter {
public:
    explicit BitWriter(std::vector<std::uint8_t>& bytes) : bytes_(bytes) {}
    void write(int code, int bits)
    {
        acc_ |= std::uint32_t(code) << nbits_;
        nbits_ += bits;
        while (nbits_ >= 8) {
            bytes_.push_back(std::uint8_t(acc_));
            acc_ >>= 8;
            nbits_ -= 8;
        }
    }
    void flush()
    {
        if (nbits_ > 0)
            bytes_.push_back(std::uint8_t(acc_));
        acc_ = 0;
        nbits_ = 0;
    }

private:
    std::vector<std::uint8_t>& bytes_;
    std::uint32_t acc_ = 0;
    int nbits_ = 0;
};

std::vector<std::uint8_t> lzwEncode(const std::vector<std::uint8_t>& indices, int minCodeSize)
{
    std::vector<std::uint8_t> data;
    BitWriter bw(data);
    const int clearCode = 1 << minCodeSize;
    const int endCode = clearCode + 1;
    int codeSize = minCodeSize + 1;
    int nextCode = endCode + 1;
    std::unordered_map<std::uint32_t, int> dict;
    dict.reserve(8192);

    bw.write(clearCode, codeSize);
    if (indices.empty()) {
        bw.write(endCode, codeSize);
        bw.flush();
        return data;
    }
    int prefix = indices[0];
    for (std::size_t i = 1; i < indices.size(); ++i) {
        const int k = indices[i];
        const std::uint32_t key = (std::uint32_t(prefix) << 8) | std::uint32_t(k);
        const auto it = dict.find(key);
        if (it != dict.end()) {
            prefix = it->second;
            continue;
        }
        bw.write(prefix, codeSize);
        if (nextCode < 4096) {
            dict.emplace(key, nextCode);
            if (nextCode == (1 << codeSize) && codeSize < 12)
                ++codeSize;
            ++nextCode;
        } else {
            bw.write(clearCode, codeSize);
            dict.clear();
            codeSize = minCodeSize + 1;
            nextCode = endCode + 1;
        }
        prefix = k;
    }
    bw.write(prefix, codeSize);
    bw.write(endCode, codeSize);
    bw.flush();
    return data;
}

} // namespace

std::vector<std::uint8_t> encodeBmp(const Image& img, int bpp)
{
    if (bpp != 1 && bpp != 4 && bpp != 8)
        bpp = 24;
    const int w = img.width();
    const int h = img.height();

    IndexedImage idx;
    if (bpp == 1) {
        idx = mapToPalette(toMonochrome(img), {kBlack, kWhite});
    } else if (bpp == 4) {
        idx = quantize(img, 16);
        if (idx.palette.size() > 16)
            idx = mapToPalette(img, vga16Palette());
    } else if (bpp == 8) {
        idx = quantize(img, 256);
    }
    const int paletteSize = bpp == 24 ? 0 : (1 << bpp);
    const int rowBytes = ((w * bpp + 31) / 32) * 4;
    const std::uint32_t pixelOffset = 14 + 40 + paletteSize * 4;
    const std::uint32_t imageSize = std::uint32_t(rowBytes) * h;

    std::vector<std::uint8_t> out;
    out.reserve(pixelOffset + imageSize);
    out.push_back('B');
    out.push_back('M');
    put32(out, pixelOffset + imageSize);
    put32(out, 0);
    put32(out, pixelOffset);
    put32(out, 40);
    put32(out, std::uint32_t(w));
    put32(out, std::uint32_t(h));
    put16(out, 1);
    put16(out, std::uint32_t(bpp));
    put32(out, 0); // BI_RGB
    put32(out, imageSize);
    put32(out, 3780); // 96 dpi
    put32(out, 3780);
    put32(out, std::uint32_t(paletteSize));
    put32(out, 0);
    for (int i = 0; i < paletteSize; ++i) {
        const Rgba c = i < int(idx.palette.size()) ? idx.palette[i] : kBlack;
        out.push_back(std::uint8_t(blueOf(c)));
        out.push_back(std::uint8_t(greenOf(c)));
        out.push_back(std::uint8_t(redOf(c)));
        out.push_back(0);
    }
    std::vector<std::uint8_t> row(rowBytes);
    for (int y = h - 1; y >= 0; --y) {
        std::fill(row.begin(), row.end(), 0);
        if (bpp == 24) {
            const Rgba* s = img.scanLine(y);
            for (int x = 0; x < w; ++x) {
                row[x * 3] = std::uint8_t(blueOf(s[x]));
                row[x * 3 + 1] = std::uint8_t(greenOf(s[x]));
                row[x * 3 + 2] = std::uint8_t(redOf(s[x]));
            }
        } else {
            const std::uint8_t* s = idx.indices.data() + std::size_t(y) * w;
            for (int x = 0; x < w; ++x) {
                const int bit = x * bpp;
                row[bit / 8] |= std::uint8_t(s[x] << (8 - bpp - bit % 8));
            }
        }
        out.insert(out.end(), row.begin(), row.end());
    }
    return out;
}

std::vector<std::uint8_t> encodeGif(const Image& img)
{
    const IndexedImage idx = quantize(img, 256);
    int bits = 1;
    while ((1 << bits) < int(idx.palette.size()))
        ++bits;
    const int tableSize = 1 << bits;

    std::vector<std::uint8_t> out = {'G', 'I', 'F', '8', '9', 'a'};
    put16(out, std::uint32_t(img.width()));
    put16(out, std::uint32_t(img.height()));
    out.push_back(std::uint8_t(0x80 | (7 << 4) | (bits - 1)));
    out.push_back(0); // background color index
    out.push_back(0); // pixel aspect ratio
    for (int i = 0; i < tableSize; ++i) {
        const Rgba c = i < int(idx.palette.size()) ? idx.palette[i] : kBlack;
        out.push_back(std::uint8_t(redOf(c)));
        out.push_back(std::uint8_t(greenOf(c)));
        out.push_back(std::uint8_t(blueOf(c)));
    }
    out.push_back(0x2C);
    put16(out, 0);
    put16(out, 0);
    put16(out, std::uint32_t(img.width()));
    put16(out, std::uint32_t(img.height()));
    out.push_back(0);

    const int minCodeSize = std::max(2, bits);
    out.push_back(std::uint8_t(minCodeSize));
    const std::vector<std::uint8_t> data = lzwEncode(idx.indices, minCodeSize);
    for (std::size_t i = 0; i < data.size(); i += 255) {
        const std::size_t len = std::min<std::size_t>(255, data.size() - i);
        out.push_back(std::uint8_t(len));
        out.insert(out.end(), data.begin() + i, data.begin() + i + len);
    }
    out.push_back(0);
    out.push_back(0x3B);
    return out;
}

} // namespace lp
