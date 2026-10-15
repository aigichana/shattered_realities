//
// Created by ivang on 6/10/2026.
//

// circles.h - midpoint circle and ellipse algorithms
#pragma once
#include "framebuffer.h"

void drawCircleMidpoint(Framebuffer& fb, int cx, int cy, int r, Color c);
void fillCircleMidpoint(Framebuffer& fb, int cx, int cy, int r, Color c);
void drawEllipseMidpoint(Framebuffer& fb, int cx, int cy, int rx, int ry, Color c);
void fillEllipseMidpoint(Framebuffer& fb, int cx, int cy, int rx, int ry, Color c);