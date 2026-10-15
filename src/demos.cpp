#include "demos.h"
#include "circles.h"
#include "clipping.h"
#include "curves.h"
#include "framebuffer.h"
#include "lines.h"
#include "polygon.h"
#include <cmath>
#include <cstdio>

static const int W = 640, H = 480;
static const Color BG{245, 245, 248}, INK{30, 30, 40}, GREY{175, 175, 185};
static const Color RED{200, 50, 60}, BLUE{40, 90, 200}, GREEN{40, 150, 90}, ORANGE{235, 140, 40};

static void save(const Framebuffer& fb, const std::string& dir, const std::string& name) {
    fb.saveBMP(dir + "/" + name + ".bmp");
    std::printf("  wrote %s/%s.bmp\n", dir.c_str(), name.c_str());
}

// ------------------------------------------------------------------ lines
void demoLines(const std::string& dir) {
    Framebuffer fb(W, H, BG);
    // Left: DDA fan. Right: Bresenham fan.
    const int n = 36;
    for (int i = 0; i < n; ++i) {
        double a = 2 * M_PI * i / n;
        int dx = (int)std::lround(140 * std::cos(a)), dy = (int)std::lround(140 * std::sin(a));
        drawLineDDA(fb, 160, 240, 160 + dx, 240 + dy, BLUE);
        drawLineBresenham(fb, 480, 240, 480 + dx, 240 + dy, RED);
    }
    drawLineThick(fb, 320, 20, 320, 460, 2, GREY);  // divider
    // Thick lines along the bottom
    drawLineThick(fb, 40, 430, 280, 400, 4, INK);
    drawLineThick(fb, 360, 400, 600, 430, 4, INK);
    save(fb, dir, "01_lines_dda_bresenham");

    // Numerical comparison of the two algorithms (printed for the README)
    int total = 0, diff = 0;
    for (int i = 0; i < 360; ++i) {
        double a = 2 * M_PI * i / 360;
        int x1 = 100 + (int)std::lround(90 * std::cos(a)), y1 = 100 + (int)std::lround(90 * std::sin(a));
        Framebuffer A(200, 200, {0, 0, 0}), B(200, 200, {0, 0, 0});
        drawLineDDA(A, 100, 100, x1, y1, {255, 255, 255});
        drawLineBresenham(B, 100, 100, x1, y1, {255, 255, 255});
        for (int y = 0; y < 200; ++y)
            for (int x = 0; x < 200; ++x) {
                Color p = A.getPixel(x, y), q = B.getPixel(x, y);
                if (p.r || q.r) { ++total; if (p.r != q.r) ++diff; }
            }
    }
    std::printf("  DDA vs Bresenham: %d of %d lit pixels differ (%.2f%%)\n", diff, total,
                100.0 * diff / total);
}

// ------------------------------------------------- circles and ellipses
void demoCirclesEllipses(const std::string& dir) {
    Framebuffer fb(W, H, BG);
    // Concentric midpoint circles (left) and ellipses (right)
    for (int r = 15; r <= 100; r += 17)
        drawCircleMidpoint(fb, 160, 120, r, lerpColor(BLUE, RED, (r - 15) / 85.0));
    for (int k = 1; k <= 6; ++k)
        drawEllipseMidpoint(fb, 480, 120, 18 * k, 9 * k, lerpColor(GREEN, ORANGE, k / 6.0));
    // Filled shapes
    fillCircleMidpoint(fb, 110, 270, 40, BLUE);
    fillEllipseMidpoint(fb, 230, 270, 55, 28, RED);
    fillEllipseMidpoint(fb, 230, 270, 30, 12, BG);   // ring effect
    fillEllipseMidpoint(fb, 450, 270, 28, 55, GREEN);
    fillCircleMidpoint(fb, 540, 270, 40, ORANGE);
    // Simplified character head built only from circles and ellipses
    fillEllipseMidpoint(fb, 320, 400, 52, 62, {230, 190, 160});
    drawEllipseMidpoint(fb, 320, 400, 52, 62, INK);
    for (int s = -1; s <= 1; s += 2) {
        fillEllipseMidpoint(fb, 320 + s * 20, 385, 14, 9, {255, 255, 255});
        drawEllipseMidpoint(fb, 320 + s * 20, 385, 14, 9, INK);
        fillCircleMidpoint(fb, 320 + s * 20, 385, 5, INK);
    }
    drawEllipseMidpoint(fb, 320, 425, 16, 8, INK);
    save(fb, dir, "02_circles_ellipses");
}

// ------------------------------------------------- polygons and filling
void demoPolygonsFill(const std::string& dir) {
    Framebuffer fb(W, H, BG);
    // Triangle (scanline fill)
    Polygon tri = {{70, 150}, {190, 150}, {130, 40}};
    fillPolygonScanline(fb, tri, BLUE);
    drawPolygonOutline(fb, tri, INK, 2);
    // Concave "arrow" polygon
    Polygon arrow = {{250, 90}, {330, 90}, {330, 50}, {400, 110}, {330, 170}, {330, 130}, {250, 130}};
    fillPolygonScanline(fb, arrow, RED);
    drawPolygonOutline(fb, arrow, INK, 2);
    // Five-point star: self-intersecting, even-odd rule leaves a hole
    Polygon star;
    for (int i = 0; i < 5; ++i) {
        double a = -M_PI / 2 + 2 * M_PI * (i * 2 % 5) / 5.0;
        star.push_back({520 + 70 * std::cos(a), 105 + 70 * std::sin(a)});
    }
    fillPolygonScanline(fb, star, ORANGE);
    drawPolygonOutline(fb, star, INK, 2);
    // Hexagon
    Polygon hex = regularPolygon(110, 320, 70, 6);
    fillPolygonScanline(fb, hex, GREEN);
    drawPolygonOutline(fb, hex, INK, 2);
    // Boundary fill inside a circle outline and a rectangle outline
    drawCircleMidpoint(fb, 300, 320, 65, INK);
    drawCircleMidpoint(fb, 300, 320, 64, INK);
    boundaryFill(fb, 300, 320, {120, 190, 230}, INK);
    drawRect(fb, {400, 255, 560, 385}, INK, 2);
    drawCircleMidpoint(fb, 480, 320, 30, INK);  // obstacle inside the rectangle
    boundaryFill(fb, 410, 265, {240, 200, 90}, INK);
    // Flood fill demo: fill the obstacle interior with a different colour
    floodFill(fb, 480, 320, {200, 90, 160});
    // Polygon with a hole built by drawing an inner polygon in the background colour
    Polygon outer = regularPolygon(320, 435, 40, 4, M_PI / 4);
    fillPolygonScanline(fb, outer, INK);
    fillPolygonScanline(fb, regularPolygon(320, 435, 18, 4, M_PI / 4), BG);
    save(fb, dir, "03_polygons_filling");
}

// ------------------------------------------------------------ clipping
void demoClipping(const std::string& dir) {
    Framebuffer fb(W, H, BG);
    Rect win{190, 130, 450, 350};
    // Test lines: some inside, some crossing, some outside
    struct L { double x0, y0, x1, y1; };
    L lines[] = {{60, 80, 560, 400}, {100, 380, 520, 90}, {250, 180, 400, 300}, {30, 240, 610, 250},
                 {320, 20, 330, 460}, {40, 40, 150, 100}, {500, 380, 600, 440}, {160, 330, 480, 140},
                 {200, 60, 440, 420}, {20, 440, 240, 300}};
    int idx = 0, mismatch = 0;
    for (const L& l : lines) {
        // faint original
        drawLineBresenham(fb, (int)l.x0, (int)l.y0, (int)l.x1, (int)l.y1, {210, 210, 218});
        double a0 = l.x0, a1 = l.y0, a2 = l.x1, a3 = l.y1;
        double b0 = l.x0, b1 = l.y0, b2 = l.x1, b3 = l.y1;
        bool okA = clipLineCohenSutherland(a0, a1, a2, a3, win);
        bool okB = clipLineLiangBarsky(b0, b1, b2, b3, win);
        if (okA != okB || (okA && (std::fabs(a0 - b0) > 1e-6 || std::fabs(a1 - b1) > 1e-6 ||
                                   std::fabs(a2 - b2) > 1e-6 || std::fabs(a3 - b3) > 1e-6)))
            ++mismatch;
        if (okA) {
            Color c = (idx % 2 == 0) ? RED : BLUE;  // alternate to tell lines apart
            drawLineThick(fb, (int)std::lround(a0), (int)std::lround(a1), (int)std::lround(a2),
                          (int)std::lround(a3), 3, c);
        }
        ++idx;
    }
    // Polygons clipped with Sutherland-Hodgman
    Polygon tri = {{150, 200}, {300, 120}, {260, 300}};
    Polygon pent = regularPolygon(450, 330, 70, 5, -M_PI / 2);
    for (const Polygon& p : {tri, pent}) {
        drawPolygonOutline(fb, p, {170, 170, 180}, 1);
        Polygon c = clipPolygonSutherlandHodgman(p, win);
        fillPolygonScanline(fb, c, {60, 170, 110});
        drawPolygonOutline(fb, c, INK, 2);
    }
    drawRect(fb, win, INK, 3);
    save(fb, dir, "04_clipping");
    std::printf("  Cohen-Sutherland vs Liang-Barsky disagreements on 10 test lines: %d\n", mismatch);
}

// -------------------------------------------------------------- curves
void demoCurves(const std::string& dir) {
    Framebuffer fb(W, H, BG);
    // Cubic Bezier with control polygon
    std::vector<Point> cubic = {{40, 200}, {90, 40}, {230, 40}, {290, 190}};
    drawControlPolygon(fb, cubic, GREY, GREY);
    drawBezier(fb, cubic, RED, 3);
    // Quadratic Bezier
    std::vector<Point> quad = {{350, 190}, {450, 30}, {600, 190}};
    drawControlPolygon(fb, quad, GREY, GREY);
    drawBezier(fb, quad, BLUE, 3);
    // Higher-degree (quartic) Bezier showing de Casteljau handles any degree
    std::vector<Point> quartic = {{40, 440}, {80, 250}, {160, 440}, {240, 250}, {300, 420}};
    drawControlPolygon(fb, quartic, GREY, GREY);
    drawBezier(fb, quartic, GREEN, 3);
    // Catmull-Rom spline through points (open)
    std::vector<Point> wave = {{350, 400}, {390, 290}, {440, 420}, {490, 280}, {540, 410}, {600, 300}};
    drawPolyline(fb, wave, {215, 215, 225}, 1);
    for (const Point& p : wave) fillCircleMidpoint(fb, (int)p.x, (int)p.y, 4, ORANGE);
    drawCatmullRom(fb, wave, ORANGE, false, 3);
    // Closed Catmull-Rom blob
    std::vector<Point> blob = {{440, 345}, {480, 330}, {510, 360}, {480, 395}, {430, 395}, {415, 365}};
    drawCatmullRom(fb, blob, INK, true, 2);
    save(fb, dir, "05_curves_bezier_spline");
}