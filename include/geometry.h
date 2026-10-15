//
// Created by ivang on 6/10/2026.
//

// geometry.h - basic geometric types shared by all modules
#pragma once
#include <vector>

struct Point {
    double x = 0, y = 0;
};

using Polygon = std::vector<Point>;

// Axis-aligned rectangle, used as a clip window
struct Rect {
    double xmin, ymin, xmax, ymax;
};