// raster.h - raster operations: bit-blit (block transfer), logical raster ops,
// binary masks, alpha blending and sprite sheets.
#pragma once
#include "framebuffer.h"
#include <cstdint>
#include <vector>

// Integer pixel rectangle: top-left corner plus width and height.
struct IRect {
    int x, y, w, h;
};

// How a source pixel S is combined with the destination pixel D it lands on.
// The logical ops work bit-by-bit on each 8-bit channel.
enum class RasterOp {
    Copy,      // D = S            (overwrite)
    KeepDest,  // D = D            (destination retained)
    And,       // D = S & D
    Or,        // D = S | D
    Xor        // D = S ^ D        (applying it twice restores D)
};
Color applyRop(Color src, Color dst, RasterOp op);
const char* ropName(RasterOp op);

// ---- Bit-blit: copy the block srcRect of src to (dx, dy) in dst using op.
// Pixels falling outside either buffer are skipped (clipped).
void blit(Framebuffer& dst, int dx, int dy, const Framebuffer& src, IRect srcRect,
          RasterOp op = RasterOp::Copy);

// Blit with a different destination size: nearest-neighbour scaling.
void blitScaled(Framebuffer& dst, IRect dstRect, const Framebuffer& src, IRect srcRect);

// ---- Binary mask: 1 = pixel belongs to the object (copy it), 0 = transparent (skip it).
struct Mask {
    int w = 0, h = 0;
    std::vector<uint8_t> bits;
    bool at(int x, int y) const {
        return x >= 0 && y >= 0 && x < w && y < h && bits[(size_t)y * w + x];
    }
};
// Builds a mask from a "colour key": every pixel equal to key becomes transparent.
Mask makeColorKeyMask(const Framebuffer& src, Color key);

// Classic two-pass sprite technique built only from logical raster ops:
//   pass 1: D = D AND andMask   (andMask is black on the object, white elsewhere -> cuts a hole)
//   pass 2: D = D OR  orSprite  (orSprite is the object on a black background -> fills the hole)
Framebuffer makeAndMask(const Mask& m);
Framebuffer makeOrSprite(const Framebuffer& src, const Mask& m);

// ---- Alpha blending: final = src * alpha + dst * (1 - alpha), alpha in [0, 1].
Color blendAlpha(Color src, Color dst, double alpha);

// General sprite blit: optional mask (skip transparent pixels), global alpha,
// and optional horizontal flip (so one walk cycle can face both directions).
void blitAlpha(Framebuffer& dst, int dx, int dy, const Framebuffer& src, IRect srcRect,
               double alpha, const Mask* mask = nullptr, bool flipX = false);

void fillRect(Framebuffer& fb, IRect r, Color c);
void fillRectAlpha(Framebuffer& fb, IRect r, Color c, double alpha);

// ---- Sprite sheet: equally sized frames laid out left to right in one image.
struct SpriteSheet {
    Framebuffer pixels;
    Mask mask;
    int frameW, frameH, frames;
    IRect frame(int i) const { return {(i % frames) * frameW, 0, frameW, frameH}; }
};
// Draws frame i with its top-left corner at (x, y).
void drawSprite(Framebuffer& dst, const SpriteSheet& s, int frame, int x, int y,
                double alpha = 1.0, bool flipX = false);
