//
// Created by ivang on 6/10/2026.
//

// curves.h - Bezier curves (de Casteljau) and Catmull-Rom splines
#pragma once
#include "framebuffer.h"
#include "geometry.h"

// Point on a Bezier curve of any degree, using de Casteljau's repeated interpolation.
Point bezierPoint(const std::vector<Point>& ctrl, double t);
std::vector<Point> sampleBezier(const std::vector<Point>& ctrl, int segments = 80);
void drawBezier(Framebuffer& fb, const std::vector<Point>& ctrl, Color c, int thickness = 1,
                int segments = 80);

// Catmull-Rom spline: passes THROUGH every control point (C1 continuous).
Point catmullRomPoint(const Point& p0, const Point& p1, const Point& p2, const Point& p3, double t);
std::vector<Point> sampleCatmullRom(const std::vector<Point>& pts, bool closed = false,
                                    int segsPerSpan = 20);
void drawCatmullRom(Framebuffer& fb, const std::vector<Point>& pts, Color c, bool closed = false,
                    int thickness = 1, int segsPerSpan = 20);

// Draw the control polygon and control points (for demonstrations).
void drawControlPolygon(Framebuffer& fb, const std::vector<Point>& ctrl, Color line, Color dot);