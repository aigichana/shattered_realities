#include "framebuffer.h"
#include <algorithm>
#include <cmath>
#include <fstream>

Color lerpColor(Color a, Color b, double t) {
    t = std::max(0.0, std::min(1.0, t));
    auto mix = [t](uint8_t p, uint8_t q) { return (uint8_t)std::lround(p + (q - p) * t); };
    return {mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b)};
}

Color desaturate(Color c, double amount) {
    double grey = 0.299 * c.r + 0.587 * c.g + 0.114 * c.b;  // luminance
    Color g{(uint8_t)grey, (uint8_t)grey, (uint8_t)grey};
    return lerpColor(c, g, amount);
}

Color darken(Color c, double f) {
    f = std::max(0.0, f);
    auto m = [f](uint8_t v) { return (uint8_t)std::min(255.0, v * f); };
    return {m(c.r), m(c.g), m(c.b)};
}

Framebuffer::Framebuffer(int w, int h, Color bg) : w_(w), h_(h), data_((size_t)w * h, bg) {}

void Framebuffer::clear(Color c) { std::fill(data_.begin(), data_.end(), c); }

void Framebuffer::hline(int x0, int x1, int y, Color c) {
    if (x0 > x1) std::swap(x0, x1);
    if (y < 0 || y >= h_) return;
    x0 = std::max(x0, 0);
    x1 = std::min(x1, w_ - 1);
    if (x0 <= x1) std::fill(row(y) + x0, row(y) + x1 + 1, c);
}

bool Framebuffer::savePPM(const std::string& path) const {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << "P6\n" << w_ << " " << h_ << "\n255\n";
    for (const Color& c : data_) f.write(reinterpret_cast<const char*>(&c), 3);
    return (bool)f;
}

bool Framebuffer::saveBMP(const std::string& path) const {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    int rowBytes = (w_ * 3 + 3) & ~3;  // rows are padded to 4 bytes
    uint32_t imgSize = (uint32_t)rowBytes * h_;
    uint32_t fileSize = 54 + imgSize;
    uint8_t hdr[54] = {'B', 'M'};
    auto put32 = [&](int off, uint32_t v) {
        for (int i = 0; i < 4; ++i) hdr[off + i] = (v >> (8 * i)) & 0xFF;
    };
    put32(2, fileSize);
    put32(10, 54);       // pixel data offset
    put32(14, 40);       // DIB header size
    put32(18, (uint32_t)w_);
    put32(22, (uint32_t)h_);
    hdr[26] = 1;         // colour planes
    hdr[28] = 24;        // bits per pixel
    put32(34, imgSize);
    f.write(reinterpret_cast<char*>(hdr), 54);
    std::vector<uint8_t> row(rowBytes, 0);
    for (int y = h_ - 1; y >= 0; --y) {  // BMP stores rows bottom-up
        for (int x = 0; x < w_; ++x) {
            Color c = data_[(size_t)y * w_ + x];
            row[x * 3 + 0] = c.b;  // BMP is BGR
            row[x * 3 + 1] = c.g;
            row[x * 3 + 2] = c.r;
        }
        f.write(reinterpret_cast<char*>(row.data()), rowBytes);
    }
    return (bool)f;
}