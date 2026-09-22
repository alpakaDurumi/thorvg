#include <thorvg.h>
#include <cstdlib>
#include <cstdint>
#include <vector>
#include <memory>

using namespace tvg;

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

    for (int i = 0; i < frames; ++i) {
        canvas->update();
        canvas->draw(true);
        canvas->sync();
    }

    Initializer::term();
    return 0;
}
