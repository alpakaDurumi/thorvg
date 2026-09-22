#include <thorvg.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <chrono>
#include <vector>
#include <memory>

using namespace tvg;

#ifdef INSTRUMENT
extern double g_fillNs[2];
extern uint64_t g_fillCalls[2];
#endif

static constexpr uint32_t W = 1920, H = 1080, HW = W / 2;

static uint32_t rng(uint32_t& s)
{
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

static Picture* picture(const std::vector<uint32_t>& data, float x)
{
    auto p = Picture::gen();
    p->load(data.data(), HW, H, ColorSpace::ARGB8888, true);
    p->translate(x, 0);
    return p;
}

int main(int argc, char** argv)
{
    if (argc < 3) return 1;
    auto frames = atoi(argv[1]);
    auto threads = atoi(argv[2]);

    Initializer::init(threads);

    std::vector<uint32_t> fg(HW * H), out(W * H);
    uint32_t s = 0x12345678;
    for (uint32_t y = 0; y < H; ++y) {
        for (uint32_t x = 0; x < HW; ++x) {
            uint32_t a = (x + y) & 0xff;
            uint32_t r = (rng(s) & 0xff) * a / 255, g = (rng(s) & 0xff) * a / 255, b = (rng(s) & 0xff) * a / 255;
            fg[y * HW + x] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }

    std::unique_ptr<SwCanvas> canvas(SwCanvas::gen(EngineOption::None));
    canvas->target(out.data(), W, W, H, ColorSpace::ARGB8888);

    auto back = Shape::gen();
    back->appendRect(0, 0, W, H);
    back->fill(40, 80, 120, 255);
    canvas->add(back);

    auto direct = Scene::gen();
    direct->add(picture(fg, 0));
    direct->add(SceneEffect::Fill, 255, 0, 0, 200);
    canvas->add(direct);

    auto composite = Scene::gen();
    composite->add(picture(fg, HW));
    composite->add(SceneEffect::Fill, 0, 0, 255, 128);
    composite->add(SceneEffect::Fill, 0, 255, 0, 200);
    canvas->add(composite);

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < frames; ++i) {
        canvas->update();
        canvas->draw(true);
        canvas->sync();
    }
    auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    uint64_t h = 1469598103934665603ull;
    auto p = reinterpret_cast<const uint8_t*>(out.data());
    for (size_t i = 0; i < out.size() * 4; ++i) {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    fprintf(stderr, "%016llx\n", (unsigned long long)h);
    fprintf(stderr, "fps %.1f\n", frames / seconds);
#ifdef INSTRUMENT
    for (int d = 1; d >= 0; --d) fprintf(stderr, "%s %.1f us/call  ", d ? "direct" : "non-direct", g_fillNs[d] / 1000.0 / g_fillCalls[d]);
    fprintf(stderr, "\n");
#endif

    Initializer::term();
    return 0;
}
