// transform.h - 2D transformations using 3x3 homogeneous matrices.
//
// A point (x, y) is written as the column vector [x y 1]^T so that translation,
// which is not linear in 2D, becomes a matrix multiplication like the others:
//
//     [x']   [a b tx] [x]
//     [y'] = [c d ty] [y]
//     [1 ]   [0 0 1 ] [1]
//
// Composition: M = A * B applies B first, then A (right-to-left).
#pragma once
#include "geometry.h"

struct Mat3 {
    double m[3][3];

    static Mat3 identity();
    Mat3 operator*(const Mat3& o) const;  // matrix product
    Point apply(Point p) const;           // M * [x y 1]^T
};

Mat3 translate2D(double tx, double ty);
Mat3 scale2D(double sx, double sy);       // about the origin
Mat3 rotate2D(double radians);            // about the origin (screen y points down -> clockwise)
Mat3 shear2D(double shx, double shy);     // x' = x + shx*y,  y' = y + shy*x
Mat3 reflect2D(bool acrossXAxis, bool acrossYAxis);

// Applies M about a pivot instead of the origin: T(pivot) * M * T(-pivot)
Mat3 aboutPivot(const Mat3& m, Point pivot);
Mat3 rotateAbout(double radians, Point pivot);
Mat3 scaleAbout(double sx, double sy, Point pivot);

Polygon transformPolygon(const Polygon& p, const Mat3& m);
