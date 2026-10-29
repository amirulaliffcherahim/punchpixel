#include "Painter.hpp"

namespace Pp
{

namespace
{
constexpr const char* kChars = " !.-+:/?<>*=0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

// 5 columns x 7 rows per glyph, '#' = lit.
const char* const kGlyphs[][7] = {
    { ".....", ".....", ".....", ".....", ".....", ".....", "....." },   // space
    { "..#..", "..#..", "..#..", "..#..", "..#..", ".....", "..#.." },   // !
    { ".....", ".....", ".....", ".....", ".....", ".....", "..#.." },   // .
    { ".....", ".....", ".....", "#####", ".....", ".....", "....." },   // -
    { ".....", "..#..", "..#..", "#####", "..#..", "..#..", "....." },   // +
    { ".....", "..#..", ".....", ".....", ".....", "..#..", "....." },   // :
    { "....#", "....#", "...#.", "..#..", ".#...", "#....", "#...." },   // /
    { ".###.", "#...#", "....#", "...#.", "..#..", ".....", "..#.." },   // ?
    { "...#.", "..#..", ".#...", "#....", ".#...", "..#..", "...#." },   // <
    { ".#...", "..#..", "...#.", "....#", "...#.", "..#..", ".#..." },   // >
    { ".....", "#.#.#", ".###.", "#####", ".###.", "#.#.#", "....." },   // *
    { ".....", ".....", "#####", ".....", "#####", ".....", "....." },   // =
    { ".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###." },   // 0
    { "..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###." },   // 1
    { ".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####" },   // 2
    { "####.", "....#", "....#", ".###.", "....#", "....#", "####." },   // 3
    { "...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#." },   // 4
    { "#####", "#....", "####.", "....#", "....#", "#...#", ".###." },   // 5
    { ".###.", "#....", "#....", "####.", "#...#", "#...#", ".###." },   // 6
    { "#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..." },   // 7
    { ".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###." },   // 8
    { ".###.", "#...#", "#...#", ".####", "....#", "....#", ".###." },   // 9
    { ".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#" },   // A
    { "####.", "#...#", "#...#", "####.", "#...#", "#...#", "####." },   // B
    { ".###.", "#...#", "#....", "#....", "#....", "#...#", ".###." },   // C
    { "####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####." },   // D
    { "#####", "#....", "#....", "####.", "#....", "#....", "#####" },   // E
    { "#####", "#....", "#....", "####.", "#....", "#....", "#...." },   // F
    { ".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".###." },   // G
    { "#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#" },   // H
    { "#####", "..#..", "..#..", "..#..", "..#..", "..#..", "#####" },   // I
    { "..###", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##.." },   // J
    { "#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#" },   // K
    { "#....", "#....", "#....", "#....", "#....", "#....", "#####" },   // L
    { "#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#" },   // M
    { "#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#", "#...#" },   // N
    { ".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###." },   // O
    { "####.", "#...#", "#...#", "####.", "#....", "#....", "#...." },   // P
    { ".###.", "#...#", "#...#", "#...#", "#.#.#", "#..#.", ".##.#" },   // Q
    { "####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#" },   // R
    { ".####", "#....", "#....", ".###.", "....#", "....#", "####." },   // S
    { "#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.." },   // T
    { "#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###." },   // U
    { "#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.." },   // V
    { "#...#", "#...#", "#...#", "#.#.#", "#.#.#", "##.##", "#...#" },   // W
    { "#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#" },   // X
    { "#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.." },   // Y
    { "#####", "....#", "...#.", "..#..", ".#...", "#....", "#####" },   // Z
};

// sin(0..90 degrees) * 1024.
const int16_t kSin[91] = {
    0, 18, 36, 54, 71, 89, 107, 125, 143, 160, 178, 195, 213, 230, 248, 265, 282, 299, 316, 333,
    350, 367, 384, 400, 416, 433, 449, 465, 481, 496, 512, 527, 543, 558, 573, 587, 602, 616, 630, 644,
    658, 672, 685, 698, 711, 724, 737, 749, 761, 773, 784, 796, 807, 818, 828, 839, 849, 859, 868, 878,
    887, 896, 904, 912, 920, 928, 935, 943, 949, 956, 962, 968, 974, 979, 984, 989, 994, 998, 1002, 1005,
    1008, 1011, 1014, 1016, 1018, 1020, 1022, 1023, 1023, 1024, 1024,
};

int isin(int d)
{
    d %= 360;
    if (d < 0) d += 360;
    if (d <= 90) return kSin[d];
    if (d <= 180) return kSin[180 - d];
    if (d <= 270) return -kSin[d - 180];
    return -kSin[360 - d];
}

int glyphIndex(char c)
{
    if (c >= 'a' && c <= 'z') {
        c = static_cast<char>(c - 'a' + 'A');
    }
    for (int i = 0; kChars[i]; ++i) {
        if (kChars[i] == c) {
            return i;
        }
    }
    return 7;   // '?'
}
} // namespace

void Painter::fill(int x, int y, int w, int h, uint32_t rgb)
{
    int x0 = x - mOx, y0 = y - mOy, x1 = x0 + w, y1 = y0 + h;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > mW) x1 = mW;
    if (y1 > mH) y1 = mH;
    if (x0 >= x1 || y0 >= y1) {
        return;
    }
    const uint16_t c = pack(rgb);
    for (int yy = y0; yy < y1; ++yy) {
        uint16_t* row = mBuf + yy * mStride;
        for (int xx = x0; xx < x1; ++xx) {
            row[xx] = c;
        }
    }
}

void Painter::ring(int cx, int cy, int ri, int ro, int a0, int a1, uint32_t rgb, int grid)
{
    if (a1 <= a0 || ro <= ri) {
        return;
    }
    if (a1 - a0 > 360) {
        a1 = a0 + 360;
    }
    if (grid < 1) {
        grid = 1;
    }
    // Up to five sectors of at most 90 degrees: inside a sector a point is
    // between its edges when both cross products are non-negative.
    int ax[5], ay[5], bx[5], by[5], n = 0;
    for (int s = a0; s < a1 && n < 5; s += 90, ++n) {
        const int e = s + 90 < a1 ? s + 90 : a1;
        ax[n] = isin(s);  ay[n] = -isin(s + 90);     // direction (sin, -cos): y grows down
        bx[n] = isin(e);  by[n] = -isin(e + 90);
    }

    int y0 = cy - ro, y1 = cy + ro, x0 = cx - ro, x1 = cx + ro;
    if (y0 < mOy) y0 = mOy;
    if (y1 > mOy + mH - 1) y1 = mOy + mH - 1;
    if (x0 < mOx) x0 = mOx;
    if (x1 > mOx + mW - 1) x1 = mOx + mW - 1;

    // Doubled coordinates so a cell centre is an integer.
    const int ri2 = 4 * ri * ri, ro2 = 4 * ro * ro;
    const uint16_t c = pack(rgb);
    for (int y = y0; y <= y1; ++y) {
        const int py = 2 * ((y / grid) * grid) + grid - 2 * cy;
        const int py2 = py * py;
        if (py2 >= ro2) {
            continue;
        }
        uint16_t* row = mBuf + (y - mOy) * mStride;
        for (int x = x0; x <= x1; ++x) {
            const int px = 2 * ((x / grid) * grid) + grid - 2 * cx;
            const int d2 = px * px + py2;
            if (d2 < ri2 || d2 >= ro2) {
                continue;
            }
            for (int k = 0; k < n; ++k) {
                if (ax[k] * py - ay[k] * px >= 0 && px * by[k] - py * bx[k] >= 0) {
                    row[x - mOx] = c;
                    break;
                }
            }
        }
    }
}

void Painter::burst(int cx, int cy, int rad, uint32_t fillC, uint32_t outline)
{
    for (int pass = 0; pass < 2; ++pass) {
        const int rr = pass == 0 ? rad + 1 : rad;
        const int sq = (rr * 3) / 4;
        const uint32_t c = pass == 0 ? outline : fillC;
        for (int dy = -rr; dy <= rr; ++dy) {
            const int ady = dy < 0 ? -dy : dy;
            int half = rr - ady;                 // diamond
            if (ady <= sq && sq > half) {
                half = sq;                       // square
            }
            r(cx - half, cy + dy, half * 2 + 1, 1, c);
        }
    }
}

void Painter::line(int x0, int y0, int x1, int y1, int thick, uint32_t rgb)
{
    int dx = x1 - x0, dy = y1 - y0;
    const int adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
    const int n = adx > ady ? adx : ady;
    if (n == 0) {
        r(x0, y0, thick, thick, rgb);
        return;
    }
    for (int i = 0; i <= n; ++i) {
        r(x0 + dx * i / n, y0 + dy * i / n, thick, thick, rgb);
    }
}

int Painter::textWidth(const char* s, int u)
{
    int n = 0;
    while (s[n]) ++n;
    return n ? n * 6 * u - u : 0;
}

int Painter::text(int x, int y, const char* s, int u, uint32_t rgb)
{
    for (; *s; ++s) {
        if (x + 5 * u > mOx && x < mOx + mW && y + 7 * u > mOy && y < mOy + mH) {
            const char* const* g = kGlyphs[glyphIndex(*s)];
            for (int row = 0; row < 7; ++row) {
                for (int col = 0; col < 5; ++col) {
                    if (g[row][col] == '#') {
                        fill(x + col * u, y + row * u, u, u, rgb);
                    }
                }
            }
        }
        x += 6 * u;
    }
    return x - u;
}

void Painter::shadowText(int x, int y, const char* s, int u, uint32_t rgb, uint32_t shadow)
{
    const int d = u > 2 ? u - 1 : u;
    text(x + d, y + d, s, u, shadow);
    text(x, y, s, u, rgb);
}

void Painter::centerText(int cx, int y, const char* s, int u, uint32_t rgb, uint32_t shadow)
{
    shadowText(cx - textWidth(s, u) / 2, y, s, u, rgb, shadow);
}

void utoa10(uint32_t v, char* out)
{
    char tmp[11];
    int n = 0;
    do {
        tmp[n++] = static_cast<char>('0' + v % 10);
        v /= 10;
    } while (v);
    for (int i = 0; i < n; ++i) {
        out[i] = tmp[n - 1 - i];
    }
    out[n] = 0;
}

} // namespace Pp
