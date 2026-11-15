#include "transform.h"
#include <cmath>

Mat3 Mat3::identity() { return {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}}; }

Mat3 Mat3::operator*(const Mat3& o) const {
    Mat3 r{};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            for (int k = 0; k < 3; ++k) r.m[i][j] += m[i][k] * o.m[k][j];
    return r;
}

Point Mat3::apply(Point p) const {
    double x = m[0][0] * p.x + m[0][1] * p.y + m[0][2];
    double y = m[1][0] * p.x + m[1][1] * p.y + m[1][2];
    double w = m[2][0] * p.x + m[2][1] * p.y + m[2][2];  // stays 1 for affine matrices
    return {x / w, y / w};
}

Mat3 translate2D(double tx, double ty) { return {{{1, 0, tx}, {0, 1, ty}, {0, 0, 1}}}; }

Mat3 scale2D(double sx, double sy) { return {{{sx, 0, 0}, {0, sy, 0}, {0, 0, 1}}}; }

Mat3 rotate2D(double a) {
    double c = std::cos(a), s = std::sin(a);
    return {{{c, -s, 0}, {s, c, 0}, {0, 0, 1}}};
}

Mat3 shear2D(double shx, double shy) { return {{{1, shx, 0}, {shy, 1, 0}, {0, 0, 1}}}; }

Mat3 reflect2D(bool acrossXAxis, bool acrossYAxis) {
    // Across the x-axis flips y; across the y-axis flips x.
    return scale2D(acrossYAxis ? -1 : 1, acrossXAxis ? -1 : 1);
}

Mat3 aboutPivot(const Mat3& m, Point p) {
    return translate2D(p.x, p.y) * m * translate2D(-p.x, -p.y);
}

Mat3 rotateAbout(double radians, Point pivot) { return aboutPivot(rotate2D(radians), pivot); }

Mat3 scaleAbout(double sx, double sy, Point pivot) { return aboutPivot(scale2D(sx, sy), pivot); }

Polygon transformPolygon(const Polygon& p, const Mat3& m) {
    Polygon out;
    out.reserve(p.size());
    for (const Point& q : p) out.push_back(m.apply(q));
    return out;
}
