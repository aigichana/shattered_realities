#include "circles.h"

// ---- Midpoint circle: compute one octant, mirror to the other seven ----
void drawCircleMidpoint(Framebuffer& fb, int cx, int cy, int r, Color c) {
    if (r < 0) return;
    int x = 0, y = r;
    int p = 1 - r;  // initial decision parameter
    auto plot8 = [&](int x, int y) {
        fb.setPixel(cx + x, cy + y, c); fb.setPixel(cx - x, cy + y, c);
        fb.setPixel(cx + x, cy - y, c); fb.setPixel(cx - x, cy - y, c);
        fb.setPixel(cx + y, cy + x, c); fb.setPixel(cx - y, cy + x, c);
        fb.setPixel(cx + y, cy - x, c); fb.setPixel(cx - y, cy - x, c);
    };
    plot8(x, y);
    while (x < y) {
        ++x;
        if (p < 0) {
            p += 2 * x + 1;
        } else {
            --y;
            p += 2 * (x - y) + 1;
        }
        plot8(x, y);
    }
}

// Same traversal, but each step fills horizontal spans instead of plotting points.
void fillCircleMidpoint(Framebuffer& fb, int cx, int cy, int r, Color c) {
    if (r < 0) return;
    int x = 0, y = r;
    int p = 1 - r;
    auto spans = [&](int x, int y) {
        fb.hline(cx - x, cx + x, cy + y, c); fb.hline(cx - x, cx + x, cy - y, c);
        fb.hline(cx - y, cx + y, cy + x, c); fb.hline(cx - y, cx + y, cy - x, c);
    };
    spans(x, y);
    while (x < y) {
        ++x;
        if (p < 0) {
            p += 2 * x + 1;
        } else {
            --y;
            p += 2 * (x - y) + 1;
        }
        spans(x, y);
    }
}

// ---- Midpoint ellipse: region 1 (slope > -1) then region 2 (slope < -1) ----
static void ellipseCore(Framebuffer& fb, int cx, int cy, int rx, int ry, Color c, bool fill) {
    if (rx < 0 || ry < 0) return;
    long rx2 = (long)rx * rx, ry2 = (long)ry * ry;
    long x = 0, y = ry;
    long px = 0, py = 2 * rx2 * y;
    auto plot4 = [&](int x, int y) {
        if (fill) {
            fb.hline(cx - x, cx + x, cy + y, c);
            fb.hline(cx - x, cx + x, cy - y, c);
        } else {
            fb.setPixel(cx + x, cy + y, c); fb.setPixel(cx - x, cy + y, c);
            fb.setPixel(cx + x, cy - y, c); fb.setPixel(cx - x, cy - y, c);
        }
    };
    // Region 1
    double p = ry2 - rx2 * ry + 0.25 * rx2;
    plot4((int)x, (int)y);
    while (px < py) {
        ++x;
        px += 2 * ry2;
        if (p < 0) {
            p += ry2 + px;
        } else {
            --y;
            py -= 2 * rx2;
            p += ry2 + px - py;
        }
        plot4((int)x, (int)y);
    }
    // Region 2
    p = ry2 * (x + 0.5) * (x + 0.5) + rx2 * (double)(y - 1) * (y - 1) - (double)rx2 * ry2;
    while (y > 0) {
        --y;
        py -= 2 * rx2;
        if (p > 0) {
            p += rx2 - py;
        } else {
            ++x;
            px += 2 * ry2;
            p += rx2 - py + px;
        }
        plot4((int)x, (int)y);
    }
}

void drawEllipseMidpoint(Framebuffer& fb, int cx, int cy, int rx, int ry, Color c) {
    ellipseCore(fb, cx, cy, rx, ry, c, false);
}
void fillEllipseMidpoint(Framebuffer& fb, int cx, int cy, int rx, int ry, Color c) {
    ellipseCore(fb, cx, cy, rx, ry, c, true);
}