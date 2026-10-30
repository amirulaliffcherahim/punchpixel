#include "Scene.hpp"

namespace Pp
{

namespace
{
constexpr uint32_t BLK = 0x000000, WHT = 0xFFFFFF, RED = 0xFF0000, YEL = 0xFFFF00, ORG = 0xFFAA00;
constexpr uint32_t CYN = 0x00FFFF, BLU = 0x0055FF, GRN = 0x00FF00, MAG = 0xFF00FF, GRY = 0xAAAAAA;
constexpr uint32_t DGR = 0x555555, NAV = 0x000055, SKN = 0xFFAA55;

int lerp(int a, int b, int pct) { return a + (b - a) * pct / 100; }
int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

uint32_t shade(uint32_t c)
{
    uint32_t out = 0;
    for (int s = 0; s <= 16; s += 8) {
        const uint32_t v = (c >> s) & 0xFF;
        const uint32_t n = v >= 0xC0 ? 0xAA : (v >= 0x80 ? 0x55 : 0x00);
        out |= n << s;
    }
    return out;
}

/// -amp .. +amp .. -amp over one period.
int tri(uint32_t x, uint32_t period, int amp)
{
    x %= period;
    const uint32_t h = period / 2;
    const int xi = static_cast<int>(x), hi = static_cast<int>(h), ai = amp;
    return xi < hi ? -ai + 2 * ai * xi / hi : ai - 2 * ai * (xi - hi) / hi;
}

/// 0..100..0 triangle.
int peak(uint32_t x, uint32_t period)
{
    return (tri(x, period, 50) + 50);
}

int weaveAmp(uint32_t a)
{
    if (a < 110) return static_cast<int>(a * 100 / 110);
    if (a < 210) return 100;
    if (a < kWeaveMs) return static_cast<int>((kWeaveMs - a) * 100 / (kWeaveMs - 210));
    return 0;
}

int jabExt(uint32_t a)
{
    if (a < 110) return static_cast<int>(a * 100 / 110);
    if (a < kJabMs) return static_cast<int>((kJabMs - a) * 100 / (kJabMs - 110));
    return 0;
}

void outlined(Painter& p, int x, int y, const char* s, int u, uint32_t c)
{
    const int o = u >= 4 ? 2 : 1;
    p.text(x + 2 * o, y + 2 * o, s, u, BLK);
    for (int dy = -o; dy <= o; dy += o) {
        for (int dx = -o; dx <= o; dx += o) {
            if (dx || dy) {
                p.text(x + dx, y + dy, s, u, BLK);
            }
        }
    }
    p.text(x, y, s, u, c);
}

void outlinedC(Painter& p, int cx, int y, const char* s, int u, uint32_t c)
{
    outlined(p, cx - Painter::textWidth(s, u) / 2, y, s, u, c);
}

// ---------------------------------------------------------------------------
// Camera: the whole world slides when the player weaves or is hit.
// ---------------------------------------------------------------------------
struct Cam { int dx = 0, dy = 0; };

Cam camera(const Game& g)
{
    Cam c;
    if (g.ps == PlState::WEAVE) {
        const int e = weaveAmp(g.t - g.pSince);
        c.dx = -g.pSide * (e * 9 / 100);
        c.dy = e * 3 / 100;
    } else if (g.ps == PlState::DEMPSEY) {
        const uint32_t a = g.t - g.pSince;
        if (a < kDempseyMs - 100) {
            c.dx = tri(a, 4 * kDempseyStepMs, 9);
            c.dy = 1 + (peak(a, 2 * kDempseyStepMs) > 50 ? 1 : 0);
        }
    } else if (g.ps == PlState::HIT) {
        const int a = static_cast<int>(g.t - g.pSince);
        c.dx = -g.pSide * 5 * clampi(static_cast<int>(kHitStunMs) - a, 0, 380) / 380;
    }
    if (g.t < g.shakeUntil) {
        c.dx += ((g.t / 35) & 1) ? 2 : -2;
        c.dy += ((g.t / 50) & 1) ? 1 : -1;
    }
    return c;
}

// ---------------------------------------------------------------------------
// Backdrop
// ---------------------------------------------------------------------------
void halftone(Painter& p, int y0, int y1, int shift, uint32_t base, uint32_t dot)
{
    p.r(0, y0, 80, y1 - y0, base);
    const int sx = ((shift % 4) + 4) % 4;
    for (int gy = y0; gy < y1; gy += 4) {
        const int sz = gy < 16 ? 1 : (gy < 32 ? 2 : 3);
        if (!p.rowVisible(gy) && !p.rowVisible(gy + sz - 1)) {
            continue;
        }
        const int off = ((gy >> 2) & 1) ? 2 : 0;
        for (int gx = -4 + sx + off; gx < 80; gx += 4) {
            p.r(gx, gy, sz, sz, dot);
        }
    }
}

void drawBackdrop(Painter& p, const Game& g, const Cam& cam, uint32_t clk)
{
    const FoeDef& d = foeDef(g.foeIdx);
    const bool dmp = g.ps == PlState::DEMPSEY;
    halftone(p, 0, 50, cam.dx / 2, dmp && ((g.t / 85) & 1) ? YEL : d.bgA, d.bgB);

    if (dmp) {
        // Speed rays out of the foe.
        for (int i = 0; i < 14; ++i) {
            const int ex = -10 + (i * 100) / 13;
            p.line(40 + cam.dx, 22, ex, i & 1 ? -6 : 50, 1, ((i + g.t / 85) & 1) ? WHT : YEL);
        }
    }
    (void)clk;

    // Ropes, posts.
    for (int k = 0; k < 2; ++k) {
        const int y = 34 + k * 7;
        p.r(0, y - 1, 80, 4, BLK);
        p.r(0, y, 80, 2, k ? WHT : RED);
    }
    const int px = cam.dx / 2;
    p.box(2 + px, 26, 3, 26, DGR);
    p.box(75 + px, 26, 3, 26, DGR);
    p.box(1 + px, 31, 5, 7, RED);
    p.box(74 + px, 31, 5, 7, RED);

    // Far canvas.
    p.r(0, 49, 80, 1, BLK);
    p.r(0, 50, 80, 6, GRY);
}

void drawApron(Painter& p)
{
    p.r(0, 55, 80, 1, BLK);
    p.r(0, 56, 80, 24, NAV);
    for (int k = -5; k <= 5; ++k) {
        p.line(40 + k * 3, 56, 40 + k * 15, 80, 1, 0x0000AA);
    }
}

// ---------------------------------------------------------------------------
// Gloves and boxers
// ---------------------------------------------------------------------------
void glove(Painter& p, int cx, int cy, int size, uint32_t col, uint32_t edge = BLK)
{
    const int x = cx - size / 2, y = cy - size / 2;
    p.box(x, y, size, size, col, edge);
    p.r(x + 1, y + 1, size / 3 + 1, 2, WHT);               // shine
    p.r(x, y + size - 3, size, 3, WHT);                    // cuff
    p.r(x, y + size - 3, size, 1, GRY);
}

void arm(Painter& p, int x0, int y0, int x1, int y1, uint32_t skin)
{
    p.line(x0 - 1, y0 - 1, x1 - 1, y1 - 1, 6, BLK);
    p.line(x0, y0, x1, y1, 4, skin);
}

void drawFoe(Painter& p, const Game& g, const Cam& cam)
{
    const FoeDef& d = foeDef(g.foeIdx);
    const int big = d.big;
    const FoeState s = g.fs;
    const int age = static_cast<int>(g.t - g.fSince);
    const int bob = (g.t / 230) & 1;

    int sink = 0, lean = 0;
    if (s == FoeState::KO) sink = clampi(age * 34 / 700, 0, 34);
    if (s == FoeState::WINDUP) lean = -g.fSide * 2;

    const bool flash = g.t < g.hitFlashUntil && ((g.t / 40) & 1) == 0;
    const uint32_t skin   = flash ? WHT : d.skin;
    const uint32_t hair   = flash ? WHT : d.hair;
    const uint32_t trunks = flash ? WHT : d.trunks;
    const uint32_t glv    = flash ? WHT : d.glove;

    const int th = 8 + 2 * big;
    const int hw = 5 + big;
    const int x  = 40 + cam.dx + lean;
    const int hy = 13 + bob + cam.dy + sink;
    const int ty = hy + 11;
    const int tr = ty + 14;
    const int ly = tr + 6;

    // Legs.
    p.box(x - th + 2, ly, 6, 6, skin);
    p.box(x + th - 8, ly, 6, 6, skin);
    p.r(x - th + 1, ly + 4, 8, 3, WHT);       // boots
    p.r(x + th - 9, ly + 4, 8, 3, WHT);

    // Glove centres per side: guard by default.
    int gcx[2], gcy[2], gsz[2] = { 8, 8 };
    for (int i = 0; i < 2; ++i) {
        const int sd = i == 0 ? -1 : 1;
        gcx[i] = x + sd * (hw + 4);
        gcy[i] = hy + 9 + ((bob && sd > 0) ? 1 : 0);
        if (s == FoeState::GUARD) {
            gcx[i] = x + sd * 3;
            gcy[i] = hy + 6;
        } else if (s == FoeState::STUN || s == FoeState::KO) {
            gcx[i] = x + sd * (th + 3);
            gcy[i] = ty + 10;
        }
    }
    bool hot = false;
    if (s == FoeState::WINDUP || s == FoeState::SWING) {
        const int i = g.fSide < 0 ? 0 : 1;
        if (s == FoeState::WINDUP) {
            const int dur = static_cast<int>(g.fUntil - g.fSince);
            const int ph = clampi(age * 100 / (dur > 0 ? dur : 1), 0, 100);
            gcx[i] = x + g.fSide * lerp(hw + 4, th + 8, ph);
            gcy[i] = lerp(hy + 9, hy + 3, ph);
            gsz[i] = 9;
            hot = true;
        } else {
            gcx[i] = x + g.fSide * 7;
            gcy[i] = hy + 24;
            gsz[i] = 20;
        }
    }

    // Arms (under the torso).
    for (int i = 0; i < 2; ++i) {
        const int sd = i == 0 ? -1 : 1;
        arm(p, x + sd * (th - 1), ty + 3, gcx[i], gcy[i], skin);
    }

    // Torso, trunks.
    p.box(x - th, ty, 2 * th, 14, skin);
    p.r(x - th + 2, ty + 4, 2 * th - 4, 1, shade(skin));       // pecs
    p.r(x - 1, ty + 5, 1, 8, shade(skin));                     // abs line
    p.r(x - 3, ty + 8, 7, 1, shade(skin));
    p.r(x - 3, ty + 11, 7, 1, shade(skin));
    p.box(x - th, tr, 2 * th, 6, trunks);
    p.r(x - th, tr, 2 * th, 1, WHT);

    // Head.
    p.box(x - hw, hy, 2 * hw, 11, skin);
    p.r(x - hw, hy, 2 * hw, 3, hair);
    p.r(x - hw, hy + 3, 1, 3, hair);
    p.r(x + hw - 1, hy + 3, 1, 3, hair);
    if (g.foeIdx == 1) {                                        // mohawk
        p.r(x - 1, hy - 4, 3, 4, hair);
        p.r(x - 1, hy - 4, 3, 1, BLK);
    } else if (g.foeIdx == 2) {                                 // headband + scar
        p.r(x - hw, hy + 3, 2 * hw, 2, RED);
        p.r(x + 2, hy + 6, 1, 4, WHT);
    } else if (g.foeIdx == 3) {                                 // crown
        p.r(x - 5, hy - 3, 11, 3, YEL);
        p.r(x - 5, hy - 5, 2, 2, YEL);
        p.r(x - 1, hy - 5, 3, 2, YEL);
        p.r(x + 4, hy - 5, 2, 2, YEL);
        p.r(x - 5, hy - 3, 11, 1, ORG);
    }

    const bool hurt = g.t < g.fHurtUntil;
    const int ey = hy + 5;
    if (s == FoeState::STUN || s == FoeState::KO) {
        for (int e = 0; e < 2; ++e) {
            const int ex = x + (e ? 2 : -4);
            p.r(ex, ey, 1, 1, BLK); p.r(ex + 2, ey, 1, 1, BLK); p.r(ex + 1, ey + 1, 1, 1, BLK);
            p.r(ex, ey + 2, 1, 1, BLK); p.r(ex + 2, ey + 2, 1, 1, BLK);
        }
    } else if (hurt || s == FoeState::GUARD) {
        p.r(x - 4, ey + 1, 3, 1, BLK);
        p.r(x + 2, ey + 1, 3, 1, BLK);
    } else {
        const bool rage = g.enraged();
        const uint32_t ec = (hot || rage) ? RED : WHT;
        p.r(x - 4, ey, 3, 3, ec);
        p.r(x + 2, ey, 3, 3, ec);
        const int look = g.fSide;
        p.r(x - 3 + (hot ? look : 0), ey + 1, 1, 2, BLK);
        p.r(x + 3 + (hot ? look : 0), ey + 1, 1, 2, BLK);
        if (hot || rage) {                                      // angry brows
            p.r(x - 5, ey - 2, 3, 1, BLK); p.r(x - 3, ey - 1, 1, 1, BLK);
            p.r(x + 3, ey - 2, 3, 1, BLK); p.r(x + 2, ey - 1, 1, 1, BLK);
        }
    }
    p.r(x, hy + 7, 1, 1, shade(skin));
    if (s == FoeState::WINDUP || s == FoeState::SWING) {
        p.r(x - 2, hy + 9, 5, 2, BLK);
        p.r(x - 1, hy + 9, 3, 1, WHT);
    } else if (hurt || s == FoeState::STUN || s == FoeState::KO) {
        p.r(x - 2, hy + 9, 5, 2, WHT);
        p.r(x - 2, hy + 9, 5, 1, BLK);
    } else {
        p.r(x - 2, hy + 9, 4, 1, BLK);
    }

    // Gloves.
    for (int i = 0; i < 2; ++i) {
        uint32_t edge = BLK;
        if (hot && i == (g.fSide < 0 ? 0 : 1)) {
            edge = ((g.t / 70) & 1) ? YEL : WHT;
            p.box(gcx[i] - gsz[i] / 2 - 2, gcy[i] - gsz[i] / 2 - 2, gsz[i] + 4, gsz[i] + 4, edge, BLK);
        }
        glove(p, gcx[i], gcy[i], gsz[i], glv, BLK);
    }
    if (s == FoeState::SWING) {                                 // speed streaks
        const int sd = g.fSide;
        for (int k = 0; k < 3; ++k) {
            p.r(x + sd * (14 + k * 2), hy + 16 + k * 5, 8, 1, WHT);
        }
    }
    if (s == FoeState::STUN) {                                  // dizzy stars
        for (int k = 0; k < 3; ++k) {
            const int a = static_cast<int>(g.t / 90) + k * 3;
            const int sx = x + tri(static_cast<uint32_t>(a * 90), 540, 9);
            p.r(sx, hy - 5 + (k & 1) * 2, 2, 2, YEL);
            p.r(sx - 1, hy - 4 + (k & 1) * 2, 4, 1, YEL);
        }
    }
    if (s == FoeState::WINDUP) {                                // tell
        const int sx = x + g.fSide * (th + 10);
        if ((g.t / 70) & 1) {
            p.r(sx - 1, hy - 6, 3, 6, RED);
            p.r(sx - 1, hy + 1, 3, 3, RED);
        }
    }
}

void drawPlayer(Painter& p, const Game& g, const Cam& cam)
{
    const int bob = (g.t / 260) & 1;
    const int sway = -cam.dx / 3;
    const int dyy = cam.dy;
    const uint32_t skin = SKN;
    const uint32_t col = RED;

    const int restX[2] = { 19, 61 };
    const int restY = 64 + bob + dyy;

    const bool dmp = g.ps == PlState::DEMPSEY;
    const int dmpT = dmp ? static_cast<int>(g.t - g.pSince) - 165 : -1;

    for (int i = 0; i < 2; ++i) {
        const int sd = i == 0 ? -1 : 1;
        int ext = 0;
        if (g.ps == PlState::JAB && g.pSide == sd) {
            ext = jabExt(g.t - g.pSince);
        } else if (dmp && dmpT >= 0) {
            const int idx = dmpT / static_cast<int>(kDempseyStepMs);
            const int frac = dmpT % static_cast<int>(kDempseyStepMs);
            if ((idx & 1) == i && idx < static_cast<int>(kDempseyHits)) {
                const int half = static_cast<int>(kDempseyStepMs) / 2;
                ext = frac < half ? frac * 100 / half : (static_cast<int>(kDempseyStepMs) - frac) * 100 / half;
            }
        }
        int cx = restX[i] + sway;
        int cy = restY;
        int size = 15;
        if (g.ps == PlState::HIT) {
            cy += 4;
        }
        if (ext > 0) {
            cx = lerp(cx, 40 + sd * 5, ext);
            cy = lerp(cy, 26, ext);
            size = lerp(15, 11, ext);
        }
        arm(p, restX[i] + sway + sd * 2, 82, cx, cy + 4, skin);
        glove(p, cx, cy, size, col);
    }
}

// ---------------------------------------------------------------------------
// Pop words
// ---------------------------------------------------------------------------
bool frontPop(Pop p) { return p == Pop::DODGE || p == Pop::OUCH; }

void drawPop(Painter& p, const Game& g, const Cam& cam, bool front)
{
    if (g.pop == Pop::NONE || frontPop(g.pop) != front) return;
    const int age = static_cast<int>(g.t - g.popAt);
    if (age < 0 || age >= static_cast<int>(kPopMs)) return;

    int rad = clampi(6 + age / 12, 6, 15);
    if (age > 330) rad -= (age - 330) / 20;
    int cx = 40, cy = 24, u = 3;
    const char* s = "";
    uint32_t f = YEL, c = RED;
    switch (g.pop) {
        case Pop::POW:     s = "POW!";  f = YEL; c = RED; cx = 40 + g.popSide * 9 + cam.dx; cy = 13 + cam.dy; break;
        case Pop::BAM:     s = "BAM!";  f = CYN; c = BLU; cx = 40 + g.popSide * 9 + cam.dx; cy = 13 + cam.dy; break;
        case Pop::WHAM:    s = "WHAM!"; f = RED; c = YEL; u = 2; cx = 40 + cam.dx; cy = 14 + cam.dy; rad = clampi(rad + 2, 6, 17); break;
        case Pop::BLOCK:   s = "BLOCK"; f = GRY; c = BLK; u = 2; cx = 40 + g.popSide * 9 + cam.dx; cy = 13 + cam.dy; break;
        case Pop::COUNTER: s = "CRIT!"; f = ORG; c = BLK; u = 2; cx = 40 + g.popSide * 9 + cam.dx; cy = 13 + cam.dy; rad = clampi(rad + 1, 6, 16); break;
        case Pop::DODGE:   s = "DODGE!"; f = GRN; c = WHT; u = 2; cx = 40 - g.popSide * 15; cy = 40; break;
        case Pop::OUCH:    s = "OUCH!"; f = MAG; c = WHT; u = 2; cx = 40 + g.popSide * 14; cy = 44; break;
        case Pop::NONE:    return;
    }
    p.burst(cx, cy, rad, f);
    if (rad >= 11) {
        const int ty = cy * kCell - (7 * u) / 2;
        p.shadowText(cx * kCell - Painter::textWidth(s, u) / 2, ty, s, u, c, f == YEL || f == CYN ? shade(f) : BLK);
    }
}

// ---------------------------------------------------------------------------
// HUD
// ---------------------------------------------------------------------------
void bar(Painter& p, int x, int y, int w, int h, int val, int maxv, uint32_t col, uint32_t track)
{
    p.fill(x - 1, y - 1, w + 2, h + 2, BLK);
    p.fill(x, y, w, h, track);
    const int fw = maxv > 0 ? clampi(w * val / maxv, 0, w) : 0;
    if (fw > 0) {
        p.fill(x, y, fw, h, col);
        p.fill(x, y, fw, 1, WHT);
    }
}

// The HUD lives on the bezel, the way UNA's own faces draw their gauges: thick
// arcs round the edge of the round display, grey track under a coloured fill.
constexpr int kRi = 103, kRo = 114;          // gauge ring
constexpr int kHpA0 = 205, kHpA1 = 335;      // player: left side, fills upward
constexpr int kFoeA0 = 25, kFoeA1 = 155;     // foe: right side, fills upward
constexpr int kRollA0 = 161;                 // Dempsey meter: bottom, 4 x 9 degrees

void gauge(Painter& p, int a0, int a1, int fill, bool fromEnd, uint32_t col)
{
    p.ring(120, 120, kRi - 2, kRo + 2, a0 - 2, a1 + 2, BLK, 1);
    p.ring(120, 120, kRi, kRo, a0, a1, DGR);
    if (fill <= 0) return;
    if (fromEnd) p.ring(120, 120, kRi, kRo, a1 - fill, a1, col);
    else         p.ring(120, 120, kRi, kRo, a0, a0 + fill, col);
    // Highlight on the outer edge: the pop-art shine.
    if (fromEnd) p.ring(120, 120, kRo - 3, kRo, a1 - fill, a1, WHT);
    else         p.ring(120, 120, kRo - 3, kRo, a0, a0 + fill, WHT);
}

void drawHud(Painter& p, const Game& g, uint32_t clk)
{
    const FoeDef& d = foeDef(g.foeIdx);
    const bool low = g.plHp < 30;
    uint32_t pc = g.plHp > 55 ? CYN : (g.plHp > 30 ? YEL : RED);
    if (low && ((clk / 120) & 1)) pc = WHT;
    const int span = kHpA1 - kHpA0;
    gauge(p, kHpA0, kHpA1, span * g.plHp / kPlayerHp, false, pc);
    const uint32_t fc = g.enraged() && ((clk / 120) & 1) ? RED : ORG;
    gauge(p, kFoeA0, kFoeA1, span * g.foeHp / d.hp, true, fc);

    // Dempsey meter: four chunky segments across the bottom of the bezel.
    for (int i = 0; i < kRollNeed; ++i) {
        const int a = kRollA0 + i * 10;
        uint32_t c = DGR;
        if (i < g.roll) {
            c = g.rollReady() ? (((clk / 90) & 1) ? RED : YEL) : YEL;
        }
        p.ring(120, 120, kRi - 2, kRo + 2, a - 1, a + 9, BLK, 1);
        p.ring(120, 120, kRi, kRo, a, a + 8, c);
    }
    if (g.rollReady() && g.ps != PlState::DEMPSEY) {
        const uint32_t rc = ((clk / 100) % 3 == 0) ? YEL : (((clk / 100) % 3 == 1) ? CYN : MAG);
        p.centerText(120, 171, "DEMPSEY!", 2, rc);
    }

    // Round chip at 12 o'clock.
    char rb[4] = { 'R', static_cast<char>('1' + g.foeIdx), 0, 0 };
    p.fill(103, 3, 34, 19, BLK);
    p.fill(105, 5, 30, 15, WHT);
    p.text(110, 9, rb, 2, BLK);

    char buf[16];
    utoa10(g.score, buf);
    p.centerText(120, 196, buf, 2, WHT);
}
void drawKoBanner(Painter& p, const Game& g, uint32_t clk)
{
    if (g.fs != FoeState::KO || g.screen != Screen::FIGHT) return;
    const uint32_t a = g.t - g.fSince;
    const int u = a < 120 ? 3 + static_cast<int>(a / 40) : 6;
    outlinedC(p, 120, 96 - u * 3, "K.O.!", u, ((clk / 90) & 1) ? RED : YEL);
}

void drawEdgeWarning(Painter& p, const Game& g)
{
    // Incoming punch: the bezel on its side flashes, outside the gauge.
    if (g.fs == FoeState::WINDUP || g.fs == FoeState::SWING) {
        const uint32_t c = ((g.t / 70) & 1) ? YEL : RED;
        if (g.fSide > 0) p.ring(120, 120, kRo + 2, 121, 30, 150, c, 1);
        else             p.ring(120, 120, kRo + 2, 121, 210, 330, c, 1);
    }
    if (g.t < g.hurtFlashUntil) {
        p.ring(120, 120, kRo + 2, 121, 0, 360, ((g.t / 55) & 1) ? RED : MAG, 1);
    }
}
void drawHints(Painter& p, const Game& g)
{
    if (g.foeIdx != 0 || g.screen != Screen::FIGHT || g.roll > 0 || g.fs == FoeState::KO) return;
    const uint32_t a = g.t - g.screenSince;
    if (a > 7500) return;
    static const char* const tips[] = { "TOP = PUNCH!", "BOT = DODGE!", "DODGE AWAY!" };
    p.centerText(120, 152, tips[(a / 1500) % 3], 2, WHT);
}

// ---------------------------------------------------------------------------
// Overlays and full screens
// ---------------------------------------------------------------------------
void panel(Painter& p, int y0, int h, uint32_t edge)
{
    p.fill(0, y0, 240, h, BLK);
    p.fill(0, y0 + 3, 240, 3, edge);
    p.fill(0, y0 + h - 6, 240, 3, edge);
}

// The SDK's button arcs (SDK::LVGL::Buttons): same radius, width and angles,
// same meaning per colour. 0 = no arc.
constexpr uint32_t BTN_START = 0xFFAA00, BTN_BACK = WHT, BTN_GO = GRN, BTN_QUIT = RED;

void buttonArcs(Painter& p, uint32_t l1, uint32_t l2, uint32_t r1, uint32_t r2)
{
    static const int arcs[4][2] = { { 291, 308 }, { 232, 249 }, { 52, 69 }, { 112, 128 } };
    const uint32_t col[4] = { l1, l2, r1, r2 };
    for (int i = 0; i < 4; ++i) {
        if (!col[i]) continue;
        p.ring(120, 120, 108, 118, arcs[i][0] - 1, arcs[i][1] + 1, BLK, 1);
        p.ring(120, 120, 110, 116, arcs[i][0], arcs[i][1], col[i], 1);
    }
}

uint32_t rainbow(uint32_t clk, int i = 0)
{
    static const uint32_t pal[] = { RED, ORG, YEL, GRN, CYN, BLU, MAG };
    return pal[((clk / 90) + static_cast<uint32_t>(i)) % 7];
}

void drawFight(Painter& p, const Game& g, uint32_t clk)
{
    const Cam cam = camera(g);
    drawBackdrop(p, g, cam, clk);
    drawFoe(p, g, cam);
    drawPop(p, g, cam, false);
    drawApron(p);
    drawPlayer(p, g, cam);
    drawPop(p, g, cam, true);
    drawKoBanner(p, g, clk);
    drawEdgeWarning(p, g);
    drawHud(p, g, clk);
    drawHints(p, g);
}

void drawTitle(Painter& p, uint32_t clk)
{
    halftone(p, 0, 80, static_cast<int>(clk / 120), YEL, RED);
    p.burst(40, 33, 26, WHT);
    p.burst(40, 33, 20, RED);
    p.burst(40, 33, 17, YEL);
    outlinedC(p, 120, 38, "PUNCH", 5, RED);
    outlinedC(p, 120, 78, "PIXEL", 5, BLU);

    p.fill(0, 124, 240, 116, BLK);
    p.fill(0, 126, 240, 3, WHT);
    if ((clk / 450) & 1) p.centerText(120, 134, "PRESS TOP!", 2, rainbow(clk));
    else p.centerText(120, 134, "PRESS TOP!", 2, YEL);
    p.centerText(120, 154, "TOP = PUNCH", 2, RED);
    p.centerText(120, 172, "BOT = WEAVE", 2, GRN);
    p.centerText(120, 190, "L-R-L-R = ROLL", 2, CYN);
    buttonArcs(p, BTN_START, 0, BTN_START, BTN_BACK);
}

void drawIntro(Painter& p, const Game& g, uint32_t clk)
{
    drawFight(p, g, clk);
    panel(p, 92, 70, rainbow(clk));
    const uint32_t a = g.t - g.screenSince;
    char buf[16] = "ROUND ";
    if (a + kFightBannerMs < kIntroMs) {
        buf[6] = static_cast<char>('1' + g.foeIdx);
        buf[7] = 0;
        outlinedC(p, 120, 104, buf, 4, YEL);
        outlinedC(p, 120, 138, foeDef(g.foeIdx).name, 2, WHT);
    } else {
        outlinedC(p, 120, 108, "FIGHT!", 6, ((clk / 80) & 1) ? RED : YEL);
    }
}

void drawWon(Painter& p, const Game& g, uint32_t clk)
{
    drawFight(p, g, clk);
    panel(p, 58, 150, YEL);
    outlinedC(p, 120, 70, "K.O.!", 6, ((clk / 120) & 1) ? RED : YEL);
    char buf[24] = "SCORE ";
    utoa10(g.score, buf + 6);
    p.centerText(120, 118, buf, 2, WHT);
    p.centerText(120, 140, "NEXT:", 2, GRY);
    p.centerText(120, 160, foeDef(g.foeIdx + 1).name, 2, CYN);
    if ((clk / 400) & 1) p.centerText(120, 184, "TOP = GO", 2, GRN);
    buttonArcs(p, BTN_GO, BTN_BACK, BTN_GO, BTN_BACK);
}

void drawLost(Painter& p, const Game& g, uint32_t clk)
{
    drawFight(p, g, clk);
    panel(p, 56, 160, RED);
    outlinedC(p, 120, 68, "KNOCKED", 4, RED);
    outlinedC(p, 120, 102, "OUT!", 6, YEL);
    char buf[16] = "ROUND ";
    buf[6] = static_cast<char>('1' + g.foeIdx);
    buf[7] = 0;
    p.centerText(120, 150, buf, 2, GRY);
    p.centerText(120, 170, "TOP = RETRY", 2, GRN);
    p.centerText(120, 190, "BOT = MENU", 2, WHT);
    buttonArcs(p, BTN_START, BTN_BACK, BTN_START, BTN_BACK);
}

void drawChamp(Painter& p, const Game& g, uint32_t clk)
{
    halftone(p, 0, 80, static_cast<int>(clk / 100), MAG, YEL);
    for (uint32_t i = 0; i < 40; ++i) {                          // confetti
        const uint32_t h = i * 2654435761u;
        const int x = static_cast<int>((h >> 8) % 240);
        const int y = static_cast<int>(((h >> 16) % 240 + clk / (6 + i % 5)) % 240);
        p.fill(x, y, 4, 6, rainbow(h >> 4, static_cast<int>(i)));
    }
    p.fill(0, 36, 240, 190, BLK);
    p.fill(0, 39, 240, 3, YEL);
    p.fill(0, 220, 240, 3, YEL);
    outlinedC(p, 120, 52, "CHAMPION", 3, rainbow(clk));
    static const char* const ranks[] = { "S", "A", "B", "C" };
    static const uint32_t rcol[] = { YEL, GRN, CYN, GRY };
    p.centerText(120, 92, "RANK", 2, WHT);
    outlinedC(p, 120, 112, ranks[g.rank()], 8, rcol[g.rank()]);
    char buf[24] = "SCORE ";
    utoa10(g.score, buf + 6);
    p.centerText(120, 178, buf, 2, WHT);
    if ((clk / 400) & 1) p.centerText(120, 198, "ANY = MENU", 2, GRN);
    buttonArcs(p, BTN_BACK, BTN_BACK, BTN_BACK, BTN_BACK);
}

void drawPause(Painter& p, const Game& g, uint32_t clk)
{
    drawFight(p, g, clk);
    panel(p, 76, 130, YEL);
    outlinedC(p, 120, 88, "PAUSED", 5, YEL);
    p.centerText(120, 134, "TOP = RESUME", 2, GRN);
    p.centerText(120, 156, "L2 = MENU", 2, WHT);
    p.centerText(120, 178, "HOLD R2 = QUIT", 2, GRY);
    buttonArcs(p, BTN_GO, BTN_BACK, BTN_GO, BTN_QUIT);
}
} // namespace

void drawScene(Painter& p, const Game& g, uint32_t clockMs)
{
    switch (g.screen) {
        case Screen::TITLE: drawTitle(p, clockMs);            break;
        case Screen::INTRO: drawIntro(p, g, clockMs);         break;
        case Screen::FIGHT: drawFight(p, g, clockMs);         break;
        case Screen::PAUSE: drawPause(p, g, clockMs);         break;
        case Screen::WON:   drawWon(p, g, clockMs);           break;
        case Screen::LOST:  drawLost(p, g, clockMs);          break;
        case Screen::CHAMP: drawChamp(p, g, clockMs);         break;
    }
}

} // namespace Pp
