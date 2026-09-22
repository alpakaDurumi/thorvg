#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <vector>
#include <random>
#if defined(THORVG_AVX_SUPPORT)
#include <immintrin.h>
#elif defined(THORVG_NEON_SUPPORT)
#include <arm_neon.h>
#endif

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

#include FILLROW

__attribute__((noinline)) static void oldDirect(uint32_t* dbuffer, uint32_t* sbuffer, size_t w, size_t h, uint32_t dstride, uint32_t sstride, uint32_t color, uint8_t opacity)
{
    for (size_t y = 0; y < h; ++y) {
        auto dst = dbuffer;
        auto src = sbuffer;
        for (size_t x = 0; x < w; ++x, ++dst, ++src) {
            auto a = MULTIPLY(opacity, A(*src));
            auto tmp = ALPHA_BLEND(color, a);
            *dst = tmp + ALPHA_BLEND(*dst, 255 - a);
        }
        dbuffer += dstride;
        sbuffer += sstride;
    }
}

__attribute__((noinline)) static void oldIndirect(uint32_t* dbuffer, size_t w, size_t h, uint32_t stride, uint32_t color, uint8_t opacity)
{
    for (size_t y = 0; y < h; ++y) {
        auto dst = dbuffer;
        for (size_t x = 0; x < w; ++x, ++dst) {
            *dst = ALPHA_BLEND(color, MULTIPLY(opacity, A(*dst)));
        }
        dbuffer += stride;
    }
}

__attribute__((noinline)) static void newDirect(uint32_t* dbuffer, uint32_t* sbuffer, int32_t w, int32_t h, uint32_t dstride, uint32_t sstride, uint32_t color, uint8_t opacity)
{
    #pragma omp parallel for
    for (int32_t y = 0; y < h; ++y)
        _fillRow(dbuffer + y * dstride, sbuffer + y * sstride, w, color, opacity, true);
}

__attribute__((noinline)) static void newIndirect(uint32_t* dbuffer, int32_t w, int32_t h, uint32_t stride, uint32_t color, uint8_t opacity)
{
    #pragma omp parallel for
    for (int32_t y = 0; y < h; ++y) {
        auto row = dbuffer + y * stride;
        _fillRow(row, row, w, color, opacity, false);
    }
}

static uint32_t refPixel(uint32_t d, uint32_t s, uint32_t color, uint8_t opacity, bool direct)
{
    auto a = MULTIPLY(opacity, A(s));
    auto r = ALPHA_BLEND(color, a);
    if (direct) r += ALPHA_BLEND(d, 255 - a);
    return r;
}

static int verify()
{
    long long bad = 0, total = 0;
    uint32_t dst[256], src[256], ref[256];
    for (int c = 0; c < 256; ++c) {
        uint32_t color = c * 0x01010101u;
        for (int d = 0; d < 256; ++d) {
            for (int x = 0; x < 256; ++x) {
                src[x] = (uint32_t(x) << 24) | 0x00123456u;
                dst[x] = d * 0x01010101u;
                ref[x] = refPixel(dst[x], src[x], color, 255, true);
            }
            _fillRow(dst, src, 256, color, 255, true);
            for (int x = 0; x < 256; ++x) { ++total; if (dst[x] != ref[x]) ++bad; }
        }
    }
    std::mt19937 rng(1);
    for (int op = 0; op < 256; ++op) {
        for (int it = 0; it < 64; ++it) {
            uint32_t color = rng() | 0xff000000u;
            for (int direct = 0; direct < 2; ++direct) {
                for (int x = 0; x < 256; ++x) {
                    src[x] = (uint32_t(x) << 24) | (rng() & 0xffffff);
                    dst[x] = direct ? rng() : src[x];
                    ref[x] = refPixel(dst[x], src[x], color, op, direct);
                }
                if (direct) _fillRow(dst, src, 256, color, op, true);
                else _fillRow(dst, dst, 256, color, op, false);
                for (int x = 0; x < 256; ++x) { ++total; if (dst[x] != ref[x]) ++bad; }
            }
        }
    }
    for (int len = 0; len < 40; ++len) {
        for (int off = 0; off < 8; ++off) {
            uint32_t buf[64], sbuf[64], expect[64];
            for (int i = 0; i < 64; ++i) { buf[i] = rng(); sbuf[i] = rng(); expect[i] = buf[i]; }
            for (int i = 0; i < len; ++i) expect[off + i] = refPixel(buf[off + i], sbuf[off + i], 0xff336699u, 200, true);
            _fillRow(buf + off, sbuf + off, len, 0xff336699u, 200, true);
            for (int i = 0; i < 64; ++i) { ++total; if (buf[i] != expect[i]) ++bad; }
        }
    }
    std::printf("verify: %lld / %lld mismatches\n", bad, total);
    return bad != 0;
}

static void bench(int w, int h, int iters)
{
    uint32_t stride = w + 64;
    std::vector<uint32_t> d0(stride * h), d1(stride * h), s(stride * h);
    std::mt19937 rng(7);
    for (auto& v : s) v = rng();
    for (auto& v : d0) v = rng();
    d1 = d0;
    volatile uint32_t vcolor = 0xff336699u;
    volatile uint8_t vop = 180;
    uint32_t color = vcolor;
    uint8_t op = vop;

    auto time = [&](auto fn) {
        fn();
        auto best = 1e30;
        for (int r = 0; r < 5; ++r) {
            auto t0 = std::chrono::steady_clock::now();
            for (int i = 0; i < iters; ++i) fn();
            auto t1 = std::chrono::steady_clock::now();
            auto us = std::chrono::duration<double, std::micro>(t1 - t0).count() / iters;
            if (us < best) best = us;
        }
        return best;
    };

    auto od = time([&] { oldDirect(d0.data(), s.data(), w, h, stride, stride, color, op); });
    auto nd = time([&] { newDirect(d1.data(), s.data(), w, h, stride, stride, color, op); });
    auto oi = time([&] { oldIndirect(d0.data(), w, h, stride, color, op); });
    auto ni = time([&] { newIndirect(d1.data(), w, h, stride, color, op); });
    uint64_t sum0 = 0, sum1 = 0;
    for (auto v : d0) sum0 += v;
    for (auto v : d1) sum1 += v;
    std::printf("%dx%d direct old %.1f us new %.1f us x%.2f | indirect old %.1f us new %.1f us x%.2f | %s\n",
                w, h, od, nd, od / nd, oi, ni, oi / ni, sum0 == sum1 ? "same" : "DIFF");
}

int main(int argc, char** argv)
{
    if (verify()) return 1;
    if (argc > 1) return 0;
    bench(1920, 1080, 20);
    bench(256, 256, 2000);
    bench(64, 16, 20000);
    return 0;
}
