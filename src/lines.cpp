#include "lines.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

void drawLineDDA(Framebuffer& fb, int x0, int y0, int x1, int y1, Color c) {
    int dx = x1 - x0, dy = y1 - y0;
    int steps = std::max(std::abs(dx), std::abs(dy));
    if (steps == 0) {
        fb.setPixel(x0, y0, c);
        return;
    }
    double xInc = (double)dx / steps;  // one of these is exactly +-1
    double yInc = (double)dy / steps;
    double x = x0, y = y0;
    for (int i = 0; i <= steps; ++i) {
        fb.setPixel((int)std::lround(x), (int)std::lround(y), c);
        x += xInc;
        y += yInc;
    }
}

// Generalised integer Bresenham (handles every slope and direction).
// err accumulates the error term; the two comparisons decide whether to
// step in x, y or both - equivalent to the textbook decision parameter p_k.

void drawLineBresenham(Framebuffer& fb, int x0, int y0, int x1, int y1, Color c) {
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
        fb.setPixel(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void drawLineThick(Framebuffer& fb, int x0, int y0, int x1, int y1, int thickness, Color c) {
    if (thickness <= 1) {
        drawLineBresenham(fb, x0, y0, x1, y1, c);
        return;
    }
    int r = thickness / 2;
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
        for (int j = -r; j <= r; ++j)
            for (int i = -r; i <= r; ++i) fb.setPixel(x0 + i, y0 + j, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void drawPolyline(Framebuffer& fb, const std::vector<Point>& p, Color c, int thickness) {
    for (size_t i = 0; i + 1 < p.size(); ++i)
        drawLineThick(fb, (int)std::lround(p[i].x), (int)std::lround(p[i].y),
                      (int)std::lround(p[i + 1].x), (int)std::lround(p[i + 1].y), thickness, c);
}
