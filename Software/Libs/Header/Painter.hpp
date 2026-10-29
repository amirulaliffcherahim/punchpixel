/**
 ******************************************************************************
 * @file    Painter.hpp
 * @brief   Tiny clipped pixel painter into an RGB565 buffer, plus a 5x7 font.
 *
 * The buffer holds one rectangular piece of the 240 x 240 screen (LVGL renders
 * it in stripes). All coordinates are screen pixels; the painter clips to the
 * piece. No LVGL, no heap: usable on the host to test or to dump frames.
 ******************************************************************************
 */

#ifndef PP_PAINTER_HPP
#define PP_PAINTER_HPP

#include <cstdint>

namespace Pp
{

constexpr int kScreen = 240;
constexpr int kGrid   = 80;     ///< logical pixels per side
constexpr int kCell   = 3;      ///< screen pixels per logical pixel

class Painter
{
public:
    /// @p buf covers screen pixels [ox, ox+w) x [oy, oy+h); @p stride in pixels.
    Painter(uint16_t* buf, int stride, int ox, int oy, int w, int h)
        : mBuf(buf), mStride(stride), mOx(ox), mOy(oy), mW(w), mH(h) {}

    static uint16_t pack(uint32_t rgb)
    {
        return static_cast<uint16_t>(((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x001F));
    }

    /// Filled rectangle in screen pixels.
    void fill(int x, int y, int w, int h, uint32_t rgb);
    /// Filled rectangle in logical (3x3) pixels.
    void r(int x, int y, int w, int h, uint32_t rgb) { fill(x * kCell, y * kCell, w * kCell, h * kCell, rgb); }
    /// Rectangle with a one-logical-pixel black outline.
    void box(int x, int y, int w, int h, uint32_t rgb, uint32_t outline = 0x000000)
    {
        r(x - 1, y - 1, w + 2, h + 2, outline);
        r(x, y, w, h, rgb);
    }
    /// Is any part of logical row @p ly inside the buffer? Lets callers skip work.
    bool rowVisible(int ly) const
    {
        const int y0 = ly * kCell;
        return y0 + kCell > mOy && y0 < mOy + mH;
    }

    /// Ring sector in screen pixels: radii [ri, ro) round (cx, cy), angles a0..a1 in
    /// TouchGFX degrees (0 = 12 o'clock, clockwise), the SDK's bezel convention.
    /// Samples snap to a @p grid px grid so arcs stay as chunky as the art.
    void ring(int cx, int cy, int ri, int ro, int a0, int a1, uint32_t rgb, int grid = kCell);

    /// Comic burst: octagon-star in logical pixels, black outline.
    void burst(int cx, int cy, int rad, uint32_t fill, uint32_t outline = 0x000000);
    /// Straight line made of logical dots @p thick wide.
    void line(int x0, int y0, int x1, int y1, int thick, uint32_t rgb);

    /// Text in screen pixels; @p u is the size of one font pixel. Returns the x after the text.
    int  text(int x, int y, const char* s, int u, uint32_t rgb);
    /// Text with a hard drop shadow (pop-art look).
    void shadowText(int x, int y, const char* s, int u, uint32_t rgb, uint32_t shadow);
    /// Same, centred on @p cx.
    void centerText(int cx, int y, const char* s, int u, uint32_t rgb, uint32_t shadow = 0x000000);
    static int textWidth(const char* s, int u);

private:
    uint16_t* mBuf;
    int mStride, mOx, mOy, mW, mH;
};

/// Unsigned decimal into @p out (at least 11 bytes).
void utoa10(uint32_t v, char* out);

} // namespace Pp

#endif
