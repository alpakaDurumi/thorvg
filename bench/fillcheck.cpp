#include <thorvg.h>
#include <cstdio>
#include <cstdint>
#include <vector>
#include <memory>

using namespace tvg;

static constexpr uint32_t W = 256, H = 256;
static constexpr uint32_t FW = 253, FH = 200;

static uint32_t rng(uint32_t& s)
{
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

static uint32_t premul(uint32_t a, uint32_t r, uint32_t g, uint32_t b)
{
    return (a << 24) | ((r * a / 255) << 16) | ((g * a / 255) << 8) | (b * a / 255);
}

static uint64_t fnv(const std::vector<uint32_t>& buf)
{
    uint64_t h = 1469598103934665603ull;
    auto p = reinterpret_cast<const uint8_t*>(buf.data());
    for (size_t i = 0; i < buf.size() * 4; ++i) {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    return h;
}

int main()
{
    Initializer::init(4);

    std::vector<uint32_t> bg(W * H), fg(FW * FH), out(W * H);
    uint32_t s = 0x12345678;
    for (auto& p : bg) p = premul(rng(s) & 0xff, rng(s) & 0xff, rng(s) & 0xff, rng(s) & 0xff);
    for (uint32_t y = 0; y < FH; ++y)
        for (uint32_t x = 0; x < FW; ++x)
            fg[y * FW + x] = premul((x + y) & 0xff, rng(s) & 0xff, rng(s) & 0xff, rng(s) & 0xff);

    auto run = [&](int effects, uint8_t r, uint8_t g, uint8_t b, uint8_t o, uint8_t so) {
        std::unique_ptr<SwCanvas> canvas(SwCanvas::gen());
        canvas->target(out.data(), W, W, H, ColorSpace::ARGB8888);

        auto back = Picture::gen();
        back->load(bg.data(), W, H, ColorSpace::ARGB8888, true);
        canvas->add(back);

        auto front = Picture::gen();
        front->load(fg.data(), FW, FH, ColorSpace::ARGB8888, true);
        front->translate(1, 7);

        auto scene = Scene::gen();
        scene->add(front);
        scene->opacity(so);
        scene->add(SceneEffect::Fill, r, g, b, o);
        if (effects == 2) scene->add(SceneEffect::Fill, b, r, g, 255 - o);
        canvas->add(scene);

        canvas->draw(true);
        canvas->sync();
        printf("%d %3d %3d %3d %3d %3d %016llx\n", effects, r, g, b, o, so, (unsigned long long)fnv(out));
    };

    const uint8_t sceneOpacities[] = {255, 200, 128, 1};
    uint32_t c = 0x9e3779b9;
    for (auto so : sceneOpacities) {
        for (int o = 0; o < 256; ++o) {
            run(1, rng(c) & 0xff, rng(c) & 0xff, rng(c) & 0xff, o, so);
        }
    }
    for (int o = 0; o < 256; ++o) {
        run(2, rng(c) & 0xff, rng(c) & 0xff, rng(c) & 0xff, o, 255);
    }

    Initializer::term();
    return 0;
}
