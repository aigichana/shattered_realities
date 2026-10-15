//
// Created by ivang on 6/10/2026.
//

// clipping.h - line and polygon clipping against a rectangular window
#pragma once
#include "framebuffer.h"
#include "geometry.h"

// Cohen-Sutherland: region outcodes. Returns false if the line is fully outside.
bool clipLineCohenSutherland(double& x0, double& y0, double& x1, double& y1, const Rect& w);

// Liang-Barsky: parametric clipping. Returns false if the line is fully outside.
bool clipLineLiangBarsky(double& x0, double& y0, double& x1, double& y1, const Rect& w);

// Sutherland-Hodgman: clips a polygon against each window edge in turn.
Polygon clipPolygonSutherlandHodgman(const Polygon& poly, const Rect& w);

void drawRect(Framebuffer& fb, const Rect& r, Color c, int thickness = 1);