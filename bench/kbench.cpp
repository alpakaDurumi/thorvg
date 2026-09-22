#include <cstdint>
#include <cstdlib>
#include <vector>
#include <arm_neon.h>

static inline uint8_t MULTIPLY(uint8_t c, uint8_t a)
{
    return (((c) * (a) + 0xff) >> 8);
}

static inline uint32_t ALPHA_BLEND(uint32_t c, uint32_t a)
{
    ++a;
    return (((((c >> 8) & 0x00ff00ff) * a) & 0xff00ff00) + ((((c & 0x00ff00ff) * a) >> 8) & 0x00ff00ff));
}

static inline uint8_t A(uint32_t c)
{
    return ((c) >> 24);
}

#ifdef FILLROW
#include FILLROW
#endif

__attribute__((noinline)) static void rows(uint32_t* d, uint32_t* s, uint32_t w, uint32_t h, uint32_t color, uint8_t opacity, bool direct)
{
#ifdef FILLROW
    for (uint32_t y = 0; y < h; ++y) {
        if (direct) _fillRow(d + y * w, s + y * w, w, color, opacity, true);
        else _fillRow(d + y * w, d + y * w, w, color, opacity, false);
    }
#else
    if (direct) {
        for (uint32_t y = 0; y < h; ++y) {
            auto dst = d + y * w;
            auto src = s + y * w;
            for (uint32_t x = 0; x < w; ++x, ++dst, ++src) {
                auto a = MULTIPLY(opacity, A(*src));
                auto tmp = ALPHA_BLEND(color, a);
                *dst = tmp + ALPHA_BLEND(*dst, 255 - a);
            }
        }
    } else {
        for (uint32_t y = 0; y < h; ++y) {
            auto dst = d + y * w;
            for (uint32_t x = 0; x < w; ++x, ++dst) {
                *dst = ALPHA_BLEND(color, MULTIPLY(opacity, A(*dst)));
            }
        }
    }
#endif
}

int main(int argc, char** argv)
{
    if (argc < 3) return 1;
    auto direct = atoi(argv[1]) != 0;
    auto iterations = atoi(argv[2]);
    const uint32_t w = 1920, h = 1080;
    std::vector<uint32_t> d(w * h), s(w * h);
    uint32_t r = 0x12345678;
    for (uint32_t i = 0; i < w * h; ++i) {
        r ^= r << 13; r ^= r >> 17; r ^= r << 5;
        s[i] = r;
        r ^= r << 13; r ^= r >> 17; r ^= r << 5;
        d[i] = r | 0xff000000;
    }
    volatile uint32_t vcolor = 0xffc86432;
    volatile uint8_t vopacity = 200;
    for (int i = 0; i < iterations; ++i) {
        rows(d.data(), s.data(), w, h, vcolor, vopacity, direct);
        asm volatile("" : : "r"(d.data()) : "memory");
    }
    return 0;
}
