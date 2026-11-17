#include "raster.h"
#include <algorithm>
#include <cmath>

Color applyRop(Color s, Color d, RasterOp op) {
    switch (op) {
        case RasterOp::Copy: return s;
        case RasterOp::KeepDest: return d;
        case RasterOp::And: return {(uint8_t)(s.r & d.r), (uint8_t)(s.g & d.g), (uint8_t)(s.b & d.b)};
        case RasterOp::Or: return {(uint8_t)(s.r | d.r), (uint8_t)(s.g | d.g), (uint8_t)(s.b | d.b)};
        case RasterOp::Xor: return {(uint8_t)(s.r ^ d.r), (uint8_t)(s.g ^ d.g), (uint8_t)(s.b ^ d.b)};
    }
    return s;
}

const char* ropName(RasterOp op) {
    switch (op) {
        case RasterOp::Copy: return "COPY";
        case RasterOp::KeepDest: return "KEEP DEST";
        case RasterOp::And: return "AND";
        case RasterOp::Or: return "OR";
        case RasterOp::Xor: return "XOR";
    }
    return "?";
}

// Shrinks a block transfer so it stays inside both buffers (rectangle clipping).
// Returns false if nothing is left to copy.
static bool clipBlit(const Framebuffer& dst, int& dx, int& dy, const Framebuffer& src, IRect& r) {
    if (r.x < 0) { dx -= r.x; r.w += r.x; r.x = 0; }   // source left / top
    if (r.y < 0) { dy -= r.y; r.h += r.y; r.y = 0; }
    if (dx < 0) { r.x -= dx; r.w += dx; dx = 0; }      // destination left / top
    if (dy < 0) { r.y -= dy; r.h += dy; dy = 0; }
    r.w = std::min({r.w, src.width() - r.x, dst.width() - dx});    // right edges
    r.h = std::min({r.h, src.height() - r.y, dst.height() - dy});  // bottom edges
    return r.w > 0 && r.h > 0;
}

// The four steps from the lecture: source region -> destination region ->
// choose the operation -> transfer the block row by row.
void blit(Framebuffer& dst, int dx, int dy, const Framebuffer& src, IRect r, RasterOp op) {
    if (op == RasterOp::KeepDest || !clipBlit(dst, dx, dy, src, r)) return;
    for (int j = 0; j < r.h; ++j) {
        const Color* s = src.row(r.y + j) + r.x;
        Color* d = dst.row(dy + j) + dx;
        if (op == RasterOp::Copy) std::copy(s, s + r.w, d);  // a plain copy moves whole rows of memory
        else
            for (int i = 0; i < r.w; ++i) d[i] = applyRop(s[i], d[i], op);
    }
}

void blitScaled(Framebuffer& dst, IRect d, const Framebuffer& src, IRect s) {
    if (d.w <= 0 || d.h <= 0) return;
    for (int j = 0; j < d.h; ++j)
        for (int i = 0; i < d.w; ++i) {
            // map the destination pixel centre back into the source block
            int sx = s.x + (int)((i + 0.5) * s.w / d.w);
            int sy = s.y + (int)((j + 0.5) * s.h / d.h);
            dst.setPixel(d.x + i, d.y + j, src.getPixel(sx, sy));
        }
}

static bool same(Color a, Color b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

Mask makeColorKeyMask(const Framebuffer& src, Color key) {
    Mask m;
    m.w = src.width();
    m.h = src.height();
    m.bits.resize((size_t)m.w * m.h);
    for (int y = 0; y < m.h; ++y)
        for (int x = 0; x < m.w; ++x) m.bits[(size_t)y * m.w + x] = !same(src.getPixel(x, y), key);
    return m;
}

Framebuffer makeAndMask(const Mask& m) {
    Framebuffer out(m.w, m.h, {255, 255, 255});
    for (int y = 0; y < m.h; ++y)
        for (int x = 0; x < m.w; ++x)
            if (m.at(x, y)) out.setPixel(x, y, {0, 0, 0});
    return out;
}

Framebuffer makeOrSprite(const Framebuffer& src, const Mask& m) {
    Framebuffer out(m.w, m.h, {0, 0, 0});
    for (int y = 0; y < m.h; ++y)
        for (int x = 0; x < m.w; ++x)
            if (m.at(x, y)) out.setPixel(x, y, src.getPixel(x, y));
    return out;
}

Color blendAlpha(Color s, Color d, double a) {
    // fixed-point version of src * a + dst * (1 - a): alpha becomes an integer in [0, 256]
    const int w = (int)(std::max(0.0, std::min(1.0, a)) * 256 + 0.5);
    auto ch = [w](int sv, int dv) { return (uint8_t)((sv * w + dv * (256 - w) + 128) >> 8); };
    return {ch(s.r, d.r), ch(s.g, d.g), ch(s.b, d.b)};
}

void blitAlpha(Framebuffer& dst, int dx, int dy, const Framebuffer& src, IRect r, double alpha,
               const Mask* mask, bool flipX) {
    if (alpha <= 0) return;
    for (int j = 0; j < r.h; ++j)
        for (int i = 0; i < r.w; ++i) {
            int sx = flipX ? r.x + r.w - 1 - i : r.x + i, sy = r.y + j;
            int x = dx + i, y = dy + j;
            if (!dst.inBounds(x, y) || !src.inBounds(sx, sy)) continue;
            if (mask && !mask->at(sx, sy)) continue;  // transparent pixel: keep background
            Color s = src.getPixel(sx, sy);
            dst.setPixel(x, y, alpha >= 1.0 ? s : blendAlpha(s, dst.getPixel(x, y), alpha));
        }
}

void fillRect(Framebuffer& fb, IRect r, Color c) {
    for (int y = r.y; y < r.y + r.h; ++y) fb.hline(r.x, r.x + r.w - 1, y, c);
}

void fillRectAlpha(Framebuffer& fb, IRect r, Color c, double alpha) {
    if (alpha <= 0) return;  // fully transparent: nothing to do
    int x0 = std::max(r.x, 0), x1 = std::min(r.x + r.w, fb.width());
    int y0 = std::max(r.y, 0), y1 = std::min(r.y + r.h, fb.height());
    for (int y = y0; y < y1; ++y) {
        Color* row = fb.row(y);
        for (int x = x0; x < x1; ++x) row[x] = blendAlpha(c, row[x], alpha);
    }
}

void drawSprite(Framebuffer& dst, const SpriteSheet& s, int frame, int x, int y, double alpha,
                bool flipX) {
    blitAlpha(dst, x, y, s.pixels, s.frame(frame), alpha, &s.mask, flipX);
}
