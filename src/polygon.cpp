#include "polygon.h"
#include "lines.h"
#include <algorithm>
#include <cmath>
#include <stack>

void drawPolygonOutline(Framebuffer& fb, const Polygon& poly, Color c, int thickness) {
    size_t n = poly.size();
    for (size_t i = 0; i < n; ++i) {
        const Point& a = poly[i];
        const Point& b = poly[(i + 1) % n];
        drawLineThick(fb, (int)std::lround(a.x), (int)std::lround(a.y),
                      (int)std::lround(b.x), (int)std::lround(b.y), thickness, c);
    }
}

// Scanline fill: for each y, find where the edges cross the scanline, sort the
// x values, and fill between pairs (even-odd rule). Edges use a half-open
// interval [ymin, ymax) so a vertex shared by two edges is counted correctly.
void fillPolygonScanline(Framebuffer& fb, const Polygon& poly, Color fill) {
    size_t n = poly.size();
    if (n < 3) return;
    double ymin = poly[0].y, ymax = poly[0].y;
    for (const Point& p : poly) {
        ymin = std::min(ymin, p.y);
        ymax = std::max(ymax, p.y);
    }
    int yStart = std::max(0, (int)std::floor(ymin));
    int yEnd = std::min(fb.height() - 1, (int)std::ceil(ymax));
    std::vector<double> xs;
    for (int y = yStart; y <= yEnd; ++y) {
        double sy = y + 0.5;  // sample at pixel centre
        xs.clear();
        for (size_t i = 0; i < n; ++i) {
            const Point& a = poly[i];
            const Point& b = poly[(i + 1) % n];
            if (a.y == b.y) continue;  // horizontal edges never cross
            double lo = std::min(a.y, b.y), hi = std::max(a.y, b.y);
            if (sy >= lo && sy < hi) xs.push_back(a.x + (sy - a.y) * (b.x - a.x) / (b.y - a.y));
        }
        std::sort(xs.begin(), xs.end());
        for (size_t k = 0; k + 1 < xs.size(); k += 2)
            fb.hline((int)std::lround(xs[k]), (int)std::lround(xs[k + 1]), y, fill);
    }
}

static bool sameColor(Color a, Color b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

void boundaryFill(Framebuffer& fb, int x, int y, Color fill, Color boundary) {
    std::stack<std::pair<int, int>> st;  // explicit stack avoids deep recursion
    st.push({x, y});
    while (!st.empty()) {
        auto [px, py] = st.top();
        st.pop();
        if (!fb.inBounds(px, py)) continue;
        Color cur = fb.getPixel(px, py);
        if (sameColor(cur, boundary) || sameColor(cur, fill)) continue;
        fb.setPixel(px, py, fill);
        st.push({px + 1, py}); st.push({px - 1, py});
        st.push({px, py + 1}); st.push({px, py - 1});
    }
}

void floodFill(Framebuffer& fb, int x, int y, Color newColor) {
    if (!fb.inBounds(x, y)) return;
    Color target = fb.getPixel(x, y);
    if (sameColor(target, newColor)) return;
    std::stack<std::pair<int, int>> st;
    st.push({x, y});
    while (!st.empty()) {
        auto [px, py] = st.top();
        st.pop();
        if (!fb.inBounds(px, py) || !sameColor(fb.getPixel(px, py), target)) continue;
        fb.setPixel(px, py, newColor);
        st.push({px + 1, py}); st.push({px - 1, py});
        st.push({px, py + 1}); st.push({px, py - 1});
    }
}

Polygon rectPolygon(double x0, double y0, double x1, double y1) {
    return {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
}

Polygon regularPolygon(double cx, double cy, double r, int n, double startAngle) {
    Polygon p;
    for (int i = 0; i < n; ++i) {
        double a = startAngle + 2.0 * M_PI * i / n;
        p.push_back({cx + r * std::cos(a), cy + r * std::sin(a)});
    }
    return p;
}