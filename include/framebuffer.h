//
// Created by ivang on 6/10/2026.
//

// framebuffer.h - a software pixel buffer. Every graphics algorithm in this
// project writes into this buffer (no graphics library is used).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct Color {
    uint8_t r = 0, g = 0, b = 0;
};

// Colour helpers (used for the "healthy -> decayed" colour interpolation)
Color lerpColor(Color a, Color b, double t);  // linear interpolation, t in [0,1]
Color desaturate(Color c, double amount);     // 0 = unchanged, 1 = grey
Color darken(Color c, double factor);         // multiply brightness

class Framebuffer {
public:
    Framebuffer(int w, int h, Color bg = {0, 0, 0});
    int width() const { return w_; }
    int height() const { return h_; }
    bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < w_ && y < h_; }
    void clear(Color c);
    void setPixel(int x, int y, Color c) {  // ignores out-of-bounds writes
        if (inBounds(x, y)) data_[(size_t)y * w_ + x] = c;
    }
    Color getPixel(int x, int y) const {  // returns black if out-of-bounds
        return inBounds(x, y) ? data_[(size_t)y * w_ + x] : Color{0, 0, 0};
    }
    // Direct access to one scanline (no bounds check) for block operations.
    Color* row(int y) { return data_.data() + (size_t)y * w_; }
    const Color* row(int y) const { return data_.data() + (size_t)y * w_; }
    void hline(int x0, int x1, int y, Color c);  // horizontal span (inclusive)
    bool savePPM(const std::string& path) const;
    bool saveBMP(const std::string& path) const;
    bool savePNG(const std::string& path) const;  // implemented in png.cpp
    const Color* data() const { return data_.data(); }

private:
    int w_, h_;
    std::vector<Color> data_;
};