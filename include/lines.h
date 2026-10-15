//
// Created by ivang on 6/10/2026.
//

// lines.h - line drawing algorithms
#pragma once
#include "framebuffer.h"
#include "geometry.h"

// DDA: steps along the longer axis, adding a constant increment to the other.
void drawLineDDA(Framebuffer& fb, int x0, int y0, int x1, int y1, Color c);

// Bresenham: integer-only decision parameter, works in all octants.
void drawLineBresenham(Framebuffer& fb, int x0, int y0, int x1, int y1, Color c);

// Bresenham with a square brush, for thicker strokes (thickness in pixels).
void drawLineThick(Framebuffer& fb, int x0, int y0, int x1, int y1, int thickness, Color c);

// Connect a list of points with Bresenham segments.
void drawPolyline(Framebuffer& fb, const std::vector<Point>& pts, Color c, int thickness = 1);