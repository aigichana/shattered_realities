#include "curves.h"
#include "circles.h"
#include "lines.h"
#include <algorithm>
#include <cmath>

Point bezierPoint(const std::vector<Point>& ctrl, double t) {
    std::vector<Point> p = ctrl;  // de Casteljau: lerp neighbours until one point remains
    for (size_t level = p.size(); level > 1; --level)
        for (size_t i = 0; i + 1 < level; ++i)
            p[i] = {(1 - t) * p[i].x + t * p[i + 1].x, (1 - t) * p[i].y + t * p[i + 1].y};
    return p[0];
}

std::vector<Point> sampleBezier(const std::vector<Point>& ctrl, int segments) {
    std::vector<Point> out;
    if (ctrl.empty()) return out;
    for (int i = 0; i <= segments; ++i) out.push_back(bezierPoint(ctrl, (double)i / segments));
    return out;
}

void drawBezier(Framebuffer& fb, const std::vector<Point>& ctrl, Color c, int thickness,
                int segments) {
    drawPolyline(fb, sampleBezier(ctrl, segments), c, thickness);
}

Point catmullRomPoint(const Point& p0, const Point& p1, const Point& p2, const Point& p3,
                      double t) {
    double t2 = t * t, t3 = t2 * t;
    auto f = [&](double a, double b, double c, double d) {
        return 0.5 * ((2 * b) + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t2 +
                      (-a + 3 * b - 3 * c + d) * t3);
    };
    return {f(p0.x, p1.x, p2.x, p3.x), f(p0.y, p1.y, p2.y, p3.y)};
}

std::vector<Point> sampleCatmullRom(const std::vector<Point>& pts, bool closed, int segs) {
    std::vector<Point> out;
    int n = (int)pts.size();
    if (n < 2) return pts;
    int spans = closed ? n : n - 1;
    auto at = [&](int i) {  // clamp at the ends (open) or wrap around (closed)
        if (closed) return pts[((i % n) + n) % n];
        return pts[std::max(0, std::min(n - 1, i))];
    };
    for (int i = 0; i < spans; ++i)
        for (int s = 0; s < segs; ++s)
            out.push_back(catmullRomPoint(at(i - 1), at(i), at(i + 1), at(i + 2), (double)s / segs));
    out.push_back(closed ? pts[0] : pts[n - 1]);
    return out;
}

void drawCatmullRom(Framebuffer& fb, const std::vector<Point>& pts, Color c, bool closed,
                    int thickness, int segs) {
    drawPolyline(fb, sampleCatmullRom(pts, closed, segs), c, thickness);
}

void drawControlPolygon(Framebuffer& fb, const std::vector<Point>& ctrl, Color line, Color dot) {
    drawPolyline(fb, ctrl, line, 1);
    for (const Point& p : ctrl) fillCircleMidpoint(fb, (int)std::lround(p.x), (int)std::lround(p.y), 4, dot);
}
