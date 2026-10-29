#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "Game.hpp"
#include "Painter.hpp"
#include "Scene.hpp"

using namespace Pp;

namespace
{
/// Renders a frame in 30-row stripes, the way LVGL hands the buffer over.
std::vector<uint16_t> render(const Game& g, uint32_t clk, int stripe = 30)
{
    std::vector<uint16_t> fb(kScreen * kScreen, 0xDEAD);
    std::vector<uint16_t> buf(kScreen * stripe);
    for (int y = 0; y < kScreen; y += stripe) {
        const int h = (y + stripe > kScreen) ? kScreen - y : stripe;
        std::fill(buf.begin(), buf.end(), 0xBEEF);
        Painter p(buf.data(), kScreen, 0, y, kScreen, h);
        drawScene(p, g, clk);
        for (int r = 0; r < h; ++r) {
            std::copy(buf.begin() + r * kScreen, buf.begin() + (r + 1) * kScreen, fb.begin() + (y + r) * kScreen);
        }
    }
    return fb;
}

/// Optional: PUNCHPIXEL_DUMP=dir writes each frame as a PPM to look at.
void dump(const std::vector<uint16_t>& fb, const char* name)
{
    const char* dir = std::getenv("PUNCHPIXEL_DUMP");
    if (!dir) return;
    const std::string path = std::string(dir) + "/" + name + ".ppm";
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return;
    std::fprintf(f, "P6 %d %d 255\n", kScreen, kScreen);
    for (uint16_t px : fb) {
        const unsigned char rgb[3] = { static_cast<unsigned char>(((px >> 11) & 0x1F) << 3),
                                       static_cast<unsigned char>(((px >> 5) & 0x3F) << 2),
                                       static_cast<unsigned char>((px & 0x1F) << 3) };
        std::fwrite(rgb, 1, 3, f);
    }
    std::fclose(f);
}

int distinctColours(const std::vector<uint16_t>& fb)
{
    std::vector<bool> seen(65536, false);
    int n = 0;
    for (uint16_t px : fb) if (!seen[px]) { seen[px] = true; ++n; }
    return n;
}

void expectFullyPainted(const std::vector<uint16_t>& fb)
{
    for (size_t i = 0; i < fb.size(); ++i) {
        ASSERT_NE(fb[i], 0xBEEF) << "pixel " << i % kScreen << "," << i / kScreen << " left unpainted";
    }
}

/// Every colour is on the display's 2-bit-per-channel grid.
void expectPaletteOnly(const std::vector<uint16_t>& fb)
{
    // Packed RGB565 channel values of 00, 55, AA, FF.
    auto ok5 = [](int v) { return v == 0x00 || v == 0x0A || v == 0x15 || v == 0x1F; };
    auto ok6 = [](int v) { return v == 0x00 || v == 0x15 || v == 0x2A || v == 0x3F; };
    for (uint16_t px : fb) {
        ASSERT_TRUE(ok5((px >> 11) & 0x1F) && ok6((px >> 5) & 0x3F) && ok5(px & 0x1F))
            << "off-palette pixel 0x" << std::hex << px;
    }
}

struct Driver
{
    Game g{77};
    uint32_t now = 1000;
    Driver() { g.tick(now); }
    void run(uint32_t ms) { for (uint32_t i = 0; i < ms; i += 10) { now += 10; g.tick(now); } }
};
} // namespace

TEST(Painter, FillClipsToItsStripe)
{
    std::vector<uint16_t> buf(10 * 4, 0);
    Painter p(buf.data(), 10, 100, 50, 10, 4);     // screen x 100..109, y 50..53
    p.fill(95, 48, 8, 3, 0xFFFFFF);                // overlaps the top-left corner only
    EXPECT_EQ(buf[0], 0xFFFF);                     // (100,50)
    EXPECT_EQ(buf[2], 0xFFFF);                     // (102,50)
    EXPECT_EQ(buf[3], 0);                          // (103,50) outside the rect
    EXPECT_EQ(buf[10], 0);                         // (100,51) outside: rect ends at y 50
    p.fill(-500, -500, 10, 10, 0xFFFFFF);          // fully outside: no write, no crash
    p.fill(0, 0, 1000, 1000, 0x0000FF);
    for (uint16_t px : buf) EXPECT_EQ(px, 0x001F);
}

TEST(Painter, PacksPaletteColours)
{
    EXPECT_EQ(Painter::pack(0xFF0000), 0xF800);
    EXPECT_EQ(Painter::pack(0x00FF00), 0x07E0);
    EXPECT_EQ(Painter::pack(0x0000FF), 0x001F);
    EXPECT_EQ(Painter::pack(0x000000), 0x0000);
}

TEST(Painter, TextWidthAndDigits)
{
    EXPECT_EQ(Painter::textWidth("A", 2), 10);
    EXPECT_EQ(Painter::textWidth("AB", 2), 22);
    EXPECT_EQ(Painter::textWidth("", 3), 0);
    char b[12];
    utoa10(0, b);          EXPECT_STREQ(b, "0");
    utoa10(4294967295u, b); EXPECT_STREQ(b, "4294967295");
}

TEST(Scene, EveryScreenPaintsEveryPixelOnPalette)
{
    Driver d;
    struct Shot { const char* name; };
    auto check = [&](const char* name) {
        const auto fb = render(d.g, d.now);
        dump(fb, name);
        expectFullyPainted(fb);
        expectPaletteOnly(fb);
        EXPECT_GE(distinctColours(fb), 5) << name << " looks flat";
    };
    check("01_title");
    d.g.press(Btn::R1);
    d.run(400);
    check("02_round");
    d.run(kIntroMs - 300);
    check("03_fight_go");
    d.run(100);
    d.g.fs = FoeState::WINDUP; d.g.fSide = 1; d.g.fSince = d.g.t; d.g.fUntil = d.g.t + 400; d.g.comboLeft = 1;
    d.run(250);
    check("04_tell");
    d.g.press(Btn::L2);
    d.run(150 + kSwingMs - 40);
    check("05_dodge");
    d.run(400);
    d.g.press(Btn::R1);
    d.run(110);
    check("06_counter");
    d.run(400);
    d.g.fs = FoeState::IDLE; d.g.fUntil = d.g.t + 30000;
    for (Btn b : { Btn::L2, Btn::R2, Btn::L2, Btn::R2 }) { d.g.press(b); d.run(200); }
    check("07_roll_ready");
    d.g.press(Btn::L1);
    d.run(560);
    check("08_dempsey");
    d.run(kDempseyMs);
    check("09_ko");
    d.run(kKoMs);
    check("10_won");
    d.run(900);
    d.g.press(Btn::R1);
    d.run(kIntroMs + 50);
    d.g.foeIdx = 3; d.g.foeHp = foeDef(3).hp;
    d.g.fs = FoeState::WINDUP; d.g.fSide = -1; d.g.fSince = d.g.t; d.g.fUntil = d.g.t + 1000;
    d.run(500);
    check("11_boss");
    d.g.fs = FoeState::WINDUP; d.g.fSide = -1; d.g.fSince = d.g.t; d.g.fUntil = d.g.t + 20; d.g.comboLeft = 1;
    d.run(20 + kSwingMs + 40);
    check("12_ouch");
    d.g.press(Btn::CHORD);
    check("13_pause");
    d.g.press(Btn::R1);
    d.g.plHp = 1;
    d.g.fs = FoeState::WINDUP; d.g.fSide = 1; d.g.fSince = d.g.t; d.g.fUntil = d.g.t + 20; d.g.comboLeft = 1;
    d.run(20 + kSwingMs + 600);
    ASSERT_EQ(d.g.screen, Screen::LOST);
    check("14_lost");
    d.run(200);                     // past the input lock
    d.g.press(Btn::L2);
    ASSERT_EQ(d.g.screen, Screen::TITLE);
    d.g.press(Btn::R1);
    d.run(kIntroMs + 50);
    d.g.foeIdx = 3; d.g.foeHp = 1; d.g.fs = FoeState::IDLE; d.g.fUntil = d.g.t + 30000;
    d.g.press(Btn::L1);
    d.run(kJabHitMs + kKoMs + 100);
    ASSERT_EQ(d.g.screen, Screen::CHAMP);
    check("15_champ");
}

TEST(Scene, StripeSizeDoesNotChangeThePicture)
{
    Driver d;
    d.g.press(Btn::R1);
    d.run(kIntroMs + 600);
    const auto a = render(d.g, d.now, 30);
    const auto b = render(d.g, d.now, 7);
    const auto c = render(d.g, d.now, 240);
    EXPECT_EQ(a, b);
    EXPECT_EQ(a, c);
}
