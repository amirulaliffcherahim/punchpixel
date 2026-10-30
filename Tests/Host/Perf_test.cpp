#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <vector>

#include "Game.hpp"
#include "Painter.hpp"
#include "Scene.hpp"

using namespace Pp;

namespace
{
/// Microseconds to render one full frame in 30-row stripes, averaged.
double frameUs(const Game& g, uint32_t clk, int reps = 40)
{
    std::vector<uint16_t> buf(kScreen * 30);
    const auto t0 = std::chrono::steady_clock::now();
    for (int r = 0; r < reps; ++r) {
        for (int y = 0; y < kScreen; y += 30) {
            Painter p(buf.data(), kScreen, 0, y, kScreen, 30);
            drawScene(p, g, clk);
        }
    }
    const auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::micro>(t1 - t0).count() / reps;
}
} // namespace

// Not a pass/fail gate on speed (host timing is noisy); prints the cost so a
// regression is visible, and keeps a generous ceiling against pathologies.
TEST(Perf, FullFrameCostStaysSmall)
{
    Game g(5);
    uint32_t now = 1000;
    g.tick(now);
    const double title = frameUs(g, now);
    g.press(Btn::R1);
    for (int i = 0; i < 300; ++i) { now += 10; g.tick(now); }
    const double fight = frameUs(g, now);
    std::printf("[ perf ] title %.0f us, fight %.0f us per frame (host)\n", title, fight);
    EXPECT_LT(fight, 20000.0);
    EXPECT_LT(title, 20000.0);
}
