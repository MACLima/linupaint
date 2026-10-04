#include "raster/quantize.h"

#include <array>
#include <unordered_map>

namespace lp {

namespace {

int colorDistance(Rgba a, Rgba b)
{
    const int dr = redOf(a) - redOf(b);
    const int dg = greenOf(a) - greenOf(b);
    const int db = blueOf(a) - blueOf(b);
    return dr * dr * 3 + dg * dg * 4 + db * db * 2;
}

std::uint8_t nearest(const std::vector<Rgba>& palette, Rgba c)
{
    int best = 0;
    int bestD = colorDistance(palette[0], c);
    for (int i = 1; i < int(palette.size()) && bestD > 0; ++i) {
        const int d = colorDistance(palette[i], c);
        if (d < bestD) {
            bestD = d;
            best = i;
        }
    }
    return std::uint8_t(best);
}

struct Bucket {
    Rgba color; // representative 5-bit-per-channel color, expanded
    std::uint32_t count;
};

struct Box {
    std::size_t begin;
    std::size_t end; // exclusive, indices into the bucket list
};

int channel(Rgba c, int ch)
{
    return ch == 0 ? redOf(c) : ch == 1 ? greenOf(c) : blueOf(c);
}

std::vector<Rgba> medianCut(const Image& img, int maxColors)
{
    // 15-bit histogram keeps median cut fast on large images.
    std::vector<std::uint32_t> hist(32768, 0);
    const Rgba* p = img.data();
    const std::size_t n = std::size_t(img.width()) * img.height();
    for (std::size_t i = 0; i < n; ++i) {
        const Rgba c = p[i];
        ++hist[((redOf(c) >> 3) << 10) | ((greenOf(c) >> 3) << 5) | (blueOf(c) >> 3)];
    }
    std::vector<Bucket> buckets;
    for (int k = 0; k < 32768; ++k)
        if (hist[k])
            buckets.push_back({rgb(((k >> 10) & 31) << 3 | 4, ((k >> 5) & 31) << 3 | 4, (k & 31) << 3 | 4), hist[k]});

    std::vector<Box> boxes{{0, buckets.size()}};
    while (int(boxes.size()) < maxColors) {
        // Split the box with the widest channel range that can still be split.
        int bestBox = -1, bestCh = 0, bestRange = -1;
        for (int b = 0; b < int(boxes.size()); ++b) {
            if (boxes[b].end - boxes[b].begin < 2)
                continue;
            for (int ch = 0; ch < 3; ++ch) {
                int lo = 255, hi = 0;
                for (std::size_t i = boxes[b].begin; i < boxes[b].end; ++i) {
                    const int v = channel(buckets[i].color, ch);
                    lo = std::min(lo, v);
                    hi = std::max(hi, v);
                }
                if (hi - lo > bestRange) {
                    bestRange = hi - lo;
                    bestBox = b;
                    bestCh = ch;
                }
            }
        }
        if (bestBox < 0)
            break;
        Box box = boxes[bestBox];
        std::sort(buckets.begin() + box.begin, buckets.begin() + box.end, [bestCh](const Bucket& a, const Bucket& b) {
            return channel(a.color, bestCh) < channel(b.color, bestCh);
        });
        std::uint64_t total = 0;
        for (std::size_t i = box.begin; i < box.end; ++i)
            total += buckets[i].count;
        std::uint64_t acc = 0;
        std::size_t mid = box.begin;
        while (mid < box.end - 1 && acc + buckets[mid].count <= total / 2)
            acc += buckets[mid++].count;
        if (mid == box.begin)
            mid = box.begin + 1;
        boxes[bestBox] = {box.begin, mid};
        boxes.push_back({mid, box.end});
    }

    std::vector<Rgba> palette;
    for (const Box& b : boxes) {
        std::uint64_t r = 0, g = 0, bl = 0, cnt = 0;
        for (std::size_t i = b.begin; i < b.end; ++i) {
            r += std::uint64_t(redOf(buckets[i].color)) * buckets[i].count;
            g += std::uint64_t(greenOf(buckets[i].color)) * buckets[i].count;
            bl += std::uint64_t(blueOf(buckets[i].color)) * buckets[i].count;
            cnt += buckets[i].count;
        }
        if (cnt)
            palette.push_back(rgb(int(r / cnt), int(g / cnt), int(bl / cnt)));
    }
    if (palette.empty())
        palette.push_back(kBlack);
    return palette;
}

} // namespace

const std::vector<Rgba>& vga16Palette()
{
    static const std::vector<Rgba> p = {
        rgb(0, 0, 0),       rgb(128, 0, 0),   rgb(0, 128, 0),   rgb(128, 128, 0),
        rgb(0, 0, 128),     rgb(128, 0, 128), rgb(0, 128, 128), rgb(192, 192, 192),
        rgb(128, 128, 128), rgb(255, 0, 0),   rgb(0, 255, 0),   rgb(255, 255, 0),
        rgb(0, 0, 255),     rgb(255, 0, 255), rgb(0, 255, 255), rgb(255, 255, 255),
    };
    return p;
}

IndexedImage mapToPalette(const Image& img, const std::vector<Rgba>& palette)
{
    IndexedImage out{img.width(), img.height(), palette, {}};
    out.indices.resize(std::size_t(img.width()) * img.height());
    std::unordered_map<Rgba, std::uint8_t> cache;
    const Rgba* p = img.data();
    for (std::size_t i = 0; i < out.indices.size(); ++i) {
        const Rgba c = p[i] | 0xFF000000u;
        auto it = cache.find(c);
        if (it == cache.end())
            it = cache.emplace(c, nearest(palette, c)).first;
        out.indices[i] = it->second;
    }
    return out;
}

IndexedImage quantize(const Image& img, int maxColors)
{
    maxColors = std::clamp(maxColors, 2, 256);
    std::unordered_map<Rgba, std::uint8_t> exact;
    const Rgba* p = img.data();
    const std::size_t n = std::size_t(img.width()) * img.height();
    bool fits = true;
    for (std::size_t i = 0; i < n; ++i) {
        const Rgba c = p[i] | 0xFF000000u;
        if (exact.find(c) == exact.end()) {
            if (int(exact.size()) == maxColors) {
                fits = false;
                break;
            }
            exact.emplace(c, std::uint8_t(exact.size()));
        }
    }
    if (fits) {
        IndexedImage out{img.width(), img.height(), std::vector<Rgba>(exact.size()), {}};
        for (const auto& [c, idx] : exact)
            out.palette[idx] = c;
        if (out.palette.empty())
            out.palette.push_back(kBlack);
        out.indices.resize(n);
        for (std::size_t i = 0; i < n; ++i)
            out.indices[i] = exact[p[i] | 0xFF000000u];
        return out;
    }
    return mapToPalette(img, medianCut(img, maxColors));
}

} // namespace lp
