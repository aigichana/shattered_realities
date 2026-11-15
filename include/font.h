// font.h - a tiny 5x7 bitmap font. Each glyph is a binary mask: a pixel is
// written only where the mask bit is 1, everything else keeps the background
// (a masked block transfer). Lower-case letters are drawn as capitals.
#pragma once
#include "framebuffer.h"
#include <string>

constexpr int GLYPH_W = 5, GLYPH_H = 7, GLYPH_ADVANCE = 6, LINE_HEIGHT = 10;

// Draws text with its top-left corner at (x, y). scale multiplies every font pixel.
// Returns the x coordinate just after the last character.
int drawText(Framebuffer& fb, int x, int y, const std::string& text, Color c, int scale = 1);

// Same, with a 1-font-pixel dark drop shadow so text stays readable on any background.
int drawTextShadow(Framebuffer& fb, int x, int y, const std::string& text, Color c, int scale = 1,
                   Color shadow = {0, 0, 0});

int textWidth(const std::string& text, int scale = 1);

// Draws text horizontally centred on cx.
void drawTextCentered(Framebuffer& fb, int cx, int y, const std::string& text, Color c,
                      int scale = 1);
