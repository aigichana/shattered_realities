//
// Created by ivang on 6/10/2026.
//

// polygon.h - polygon representation, outline drawing and filling algorithms
#pragma once
#include "framebuffer.h"
#include "geometry.h"

void drawPolygonOutline(Framebuffer& fb, const Polygon& poly, Color c, int thickness = 1);

// Scanline fill using the even-odd rule (handles concave and self-intersecting polygons).
void fillPolygonScanline(Framebuffer& fb, const Polygon& poly, Color fill);

// 4-connected boundary fill: spreads until it reaches the boundary colour.
void boundaryFill(Framebuffer& fb, int x, int y, Color fill, Color boundary);

// 4-connected flood fill: replaces the colour found at the seed pixel.
void floodFill(Framebuffer& fb, int x, int y, Color newColor);

// Polygon helpers
Polygon rectPolygon(double x0, double y0, double x1, double y1);
Polygon regularPolygon(double cx, double cy, double r, int n, double startAngle = 0);