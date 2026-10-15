#include "clipping.h"
#include "lines.h"
#include <algorithm>
#include <cmath>
#include <functional>

enum { INSIDE = 0, LEFT = 1, RIGHT = 2, BOTTOM = 4, TOP = 8 };

static int outcode(double x, double y, const Rect& w) {
    int code = INSIDE;
    if (x < w.xmin) code |= LEFT;
    else if (x > w.xmax) code |= RIGHT;
    if (y < w.ymin) code |= BOTTOM;  // "bottom" = smaller y (screen space)
    else if (y > w.ymax) code |= TOP;
    return code;
}

bool clipLineCohenSutherland(double& x0, double& y0, double& x1, double& y1, const Rect& w) {
    int c0 = outcode(x0, y0, w), c1 = outcode(x1, y1, w);
    while (true) {
        if (!(c0 | c1)) return true;   // trivially accept: both inside
        if (c0 & c1) return false;     // trivially reject: both share an outside region
        int out = c0 ? c0 : c1;        // pick an endpoint that is outside
        double x = 0, y = 0;
        if (out & TOP) {
            x = x0 + (x1 - x0) * (w.ymax - y0) / (y1 - y0); y = w.ymax;
        } else if (out & BOTTOM) {
            x = x0 + (x1 - x0) * (w.ymin - y0) / (y1 - y0); y = w.ymin;
        } else if (out & RIGHT) {
            y = y0 + (y1 - y0) * (w.xmax - x0) / (x1 - x0); x = w.xmax;
        } else {
            y = y0 + (y1 - y0) * (w.xmin - x0) / (x1 - x0); x = w.xmin;
        }
        if (out == c0) { x0 = x; y0 = y; c0 = outcode(x0, y0, w); }
        else           { x1 = x; y1 = y; c1 = outcode(x1, y1, w); }
    }
}

bool clipLineLiangBarsky(double& x0, double& y0, double& x1, double& y1, const Rect& w) {
    double dx = x1 - x0, dy = y1 - y0;
    double p[4] = {-dx, dx, -dy, dy};
    double q[4] = {x0 - w.xmin, w.xmax - x0, y0 - w.ymin, w.ymax - y0};
    double t0 = 0.0, t1 = 1.0;
    for (int i = 0; i < 4; ++i) {
        if (p[i] == 0) {
            if (q[i] < 0) return false;  // parallel and outside
        } else {
            double t = q[i] / p[i];
            if (p[i] < 0) t0 = std::max(t0, t);  // entering
            else          t1 = std::min(t1, t);  // leaving
        }
    }
    if (t0 > t1) return false;
    double nx0 = x0 + t0 * dx, ny0 = y0 + t0 * dy;
    double nx1 = x0 + t1 * dx, ny1 = y0 + t1 * dy;
    x0 = nx0; y0 = ny0; x1 = nx1; y1 = ny1;
    return true;
}

// Clip against one edge: inside() tests a point, intersect() finds the crossing.
static Polygon clipAgainstEdge(const Polygon& in, const std::function<bool(const Point&)>& inside,
                               const std::function<Point(const Point&, const Point&)>& intersect) {
    Polygon out;
    size_t n = in.size();
    for (size_t i = 0; i < n; ++i) {
        const Point& cur = in[i];
        const Point& prev = in[(i + n - 1) % n];
        bool curIn = inside(cur), prevIn = inside(prev);
        if (curIn) {
            if (!prevIn) out.push_back(intersect(prev, cur));  // entering
            out.push_back(cur);
        } else if (prevIn) {
            out.push_back(intersect(prev, cur));               // leaving
        }
    }
    return out;
}

Polygon clipPolygonSutherlandHodgman(const Polygon& poly, const Rect& w) {
    Polygon p = poly;
    auto xAt = [](const Point& a, const Point& b, double y) {
        return Point{a.x + (b.x - a.x) * (y - a.y) / (b.y - a.y), y};
    };
    auto yAt = [](const Point& a, const Point& b, double x) {
        return Point{x, a.y + (b.y - a.y) * (x - a.x) / (b.x - a.x)};
    };
    if (p.empty()) return p;
    p = clipAgainstEdge(p, [&](const Point& q) { return q.x >= w.xmin; },
                        [&](const Point& a, const Point& b) { return yAt(a, b, w.xmin); });
    if (p.empty()) return p;
    p = clipAgainstEdge(p, [&](const Point& q) { return q.x <= w.xmax; },
                        [&](const Point& a, const Point& b) { return yAt(a, b, w.xmax); });
    if (p.empty()) return p;
    p = clipAgainstEdge(p, [&](const Point& q) { return q.y >= w.ymin; },
                        [&](const Point& a, const Point& b) { return xAt(a, b, w.ymin); });
    if (p.empty()) return p;
    p = clipAgainstEdge(p, [&](const Point& q) { return q.y <= w.ymax; },
                        [&](const Point& a, const Point& b) { return xAt(a, b, w.ymax); });
    return p;
}

void drawRect(Framebuffer& fb, const Rect& r, Color c, int thickness) {
    int x0 = (int)std::lround(r.xmin), y0 = (int)std::lround(r.ymin);
    int x1 = (int)std::lround(r.xmax), y1 = (int)std::lround(r.ymax);
    drawLineThick(fb, x0, y0, x1, y0, thickness, c);
    drawLineThick(fb, x1, y0, x1, y1, thickness, c);
    drawLineThick(fb, x1, y1, x0, y1, thickness, c);
    drawLineThick(fb, x0, y1, x0, y0, thickness, c);
}
