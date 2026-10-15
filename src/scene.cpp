// scene.cpp - "healthy room" that decays as the distortion value t rises from 0 to 1.
// Algorithms used: Bresenham lines, midpoint circle/ellipse, scanline polygon fill,
// Sutherland-Hodgman clipping (sun in the window), Bezier curves (arms, mouth, mug
// handle), Catmull-Rom splines (smoke) and colour interpolation (healthy -> decayed).
#include "scene.h"
#include "circles.h"
#include "clipping.h"
#include "curves.h"
#include "lines.h"
#include "polygon.h"
#include <algorithm>
#include <cmath>

namespace {

// ---------- deterministic noise (hash based, so each vertex jitters consistently)
double hash01(unsigned a, unsigned b) {
    unsigned h = a * 2654435761u ^ (b + 0x9e3779b9u) * 40503u;
    h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; h *= 3266489917u; h ^= h >> 16;
    return (h & 0xFFFFFF) / double(0x1000000);
}
double noise(unsigned a, unsigned b) { return hash01(a, b) * 2.0 - 1.0; }  // [-1, 1)

double clamp01(double v) { return std::max(0.0, std::min(1.0, v)); }
double lerp(double a, double b, double t) { return a + (b - a) * t; }
double smooth(double t) { t = clamp01(t); return t * t * (3 - 2 * t); }
Point lerpP(Point a, Point b, double t) { return {lerp(a.x, b.x, t), lerp(a.y, b.y, t)}; }

// ---------- colour decay: desaturate, darken and tint towards a sickly dark red
Color decay(Color c, double t) {
    c = desaturate(c, 0.85 * t);
    c = darken(c, 1.0 - 0.55 * t);
    return lerpColor(c, {95, 12, 28}, 0.25 * t);
}

// ---------- geometry helpers
Polygon warp(const Polygon& p, unsigned id, double amount) {
    Polygon out = p;
    for (size_t i = 0; i < out.size(); ++i) {
        out[i].x += noise(id, (unsigned)i * 2) * amount;
        out[i].y += noise(id, (unsigned)i * 2 + 1) * amount;
    }
    return out;
}
Polygon translate(const Polygon& p, double dx, double dy) {
    Polygon out = p;
    for (Point& q : out) { q.x += dx; q.y += dy; }
    return out;
}
Polygon rotate(const Polygon& p, Point pivot, double angle) {  // 2D rotation matrix
    double c = std::cos(angle), s = std::sin(angle);
    Polygon out = p;
    for (Point& q : out) {
        double x = q.x - pivot.x, y = q.y - pivot.y;
        q = {pivot.x + x * c - y * s, pivot.y + x * s + y * c};
    }
    return out;
}
int I(double v) { return (int)std::lround(v); }

void shape(Framebuffer& fb, const Polygon& p, Color fill, Color edge, int thick = 1) {
    fillPolygonScanline(fb, p, fill);
    drawPolygonOutline(fb, p, edge, thick);
}
void line(Framebuffer& fb, Point a, Point b, Color c, int thick = 1) {
    drawLineThick(fb, I(a.x), I(a.y), I(b.x), I(b.y), thick, c);
}
void ring(Framebuffer& fb, double cx, double cy, int r, Color c) {
    drawCircleMidpoint(fb, I(cx), I(cy), r, c);
}
void disc(Framebuffer& fb, double cx, double cy, int r, Color c) {
    fillCircleMidpoint(fb, I(cx), I(cy), r, c);
}
void oval(Framebuffer& fb, double cx, double cy, int rx, int ry, Color fill, Color edge) {
    fillEllipseMidpoint(fb, I(cx), I(cy), rx, ry, fill);
    drawEllipseMidpoint(fb, I(cx), I(cy), rx, ry, edge);
}

// ---------- raster post-processing: vignette + horizontal "tearing" glitch bands
void postProcess(Framebuffer& fb, double t) {
    if (t <= 0) return;
    const int W = fb.width(), H = fb.height();
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            double dx = (x - W / 2.0) / (W / 2.0), dy = (y - H / 2.0) / (H / 2.0);
            double d = std::sqrt(dx * dx + dy * dy) / 1.414;
            fb.setPixel(x, y, darken(fb.getPixel(x, y), 1.0 - 0.8 * t * d * d));
        }
    int bands = (int)(7 * smooth((t - 0.35) / 0.65));
    for (int k = 0; k < bands; ++k) {
        int y0 = I(hash01(900, k) * (H - 40)), h = 4 + I(hash01(901, k) * 14);
        int shift = I(noise(902, k) * 45 * t);
        for (int y = y0; y < y0 + h && y < H; ++y) {
            std::vector<Color> row(W);
            for (int x = 0; x < W; ++x) row[x] = fb.getPixel(((x - shift) % W + W) % W, y);
            for (int x = 0; x < W; ++x) fb.setPixel(x, y, row[x]);
        }
    }
}

}  // namespace

void renderScene(Framebuffer& fb, double t) {
    t = clamp01(t);
    const Color ink = decay({25, 25, 38}, t);
    const double wob = t * 16;  // how far room surfaces drift apart

    auto D = [&](Color c) { return decay(c, t); };  // shorthand: decayed colour
    fb.clear(D({20, 20, 28}));

    // ================= ROOM (one-point perspective) =================
    Polygon ceiling = {{0, 0}, {640, 0}, {520, 60}, {120, 60}};
    Polygon leftWall = {{0, 0}, {120, 60}, {120, 300}, {0, 420}};
    Polygon rightWall = {{640, 0}, {520, 60}, {520, 300}, {640, 420}};
    Polygon backWall = rectPolygon(120, 60, 520, 300);
    Polygon floor = {{0, 420}, {120, 300}, {520, 300}, {640, 420}, {640, 480}, {0, 480}};
    shape(fb, warp(ceiling, 1, wob), D({226, 226, 232}), ink, 2);
    shape(fb, warp(leftWall, 2, wob), D({206, 214, 226}), ink, 2);
    shape(fb, warp(rightWall, 3, wob), D({198, 206, 220}), ink, 2);
    shape(fb, warp(backWall, 4, wob), D({232, 236, 242}), ink, 2);
    shape(fb, warp(floor, 5, wob), D({196, 150, 104}), ink, 2);

    // Floor boards: lines fanning out from the back wall; ends jitter with t
    for (int i = 1; i < 8; ++i) {
        Point a{120 + 50.0 * i, 300}, b{80.0 * i, 480};
        a.x += noise(10, i) * t * 14; b.x += noise(11, i) * t * 40;
        line(fb, a, b, D({140, 98, 62}), 1);
    }
    for (double y : {330.0, 362.0, 405.0, 458.0}) {
        double xl = std::max(0.0, 120 - (y - 300)), xr = std::min(640.0, 520 + (y - 300));
        double yy = y + noise(12, (unsigned)y) * t * 10;
        line(fb, {xl, yy}, {xr, yy + noise(13, (unsigned)y) * t * 8}, D({140, 98, 62}), 1);
    }

    // ================= WINDOW (sun clipped by Sutherland-Hodgman) =================
    Rect glass{196, 106, 284, 194};
    shape(fb, warp(rectPolygon(186, 96, 294, 204), 20, t * 10), D({150, 100, 60}), ink, 2);
    fillPolygonScanline(fb, rectPolygon(glass.xmin, glass.ymin, glass.xmax, glass.ymax),
                        lerpColor({140, 200, 245}, {40, 12, 30}, t));
    {   // the sun drifts out of the window as reality slips; clipping hides the overflow
        Polygon sun = regularPolygon(lerp(240, 300, smooth(t)), lerp(138, 205, smooth(t)), 22, 28);
        Polygon clipped = clipPolygonSutherlandHodgman(sun, glass);
        if (clipped.size() >= 3)
            fillPolygonScanline(fb, clipped, lerpColor({255, 214, 70}, {150, 20, 25}, t));
    }
    line(fb, {240, 106}, {240, 194}, D({150, 100, 60}), 3);
    line(fb, {196, 150}, {284, 150}, D({150, 100, 60}), 3);
    drawRect(fb, glass, ink, 2);

    // ================= DOOR =================
    Polygon door = rectPolygon(405, 125, 485, 300);
    Polygon doorShape = rotate(warp(door, 30, t * 18), {405, 300}, -t * 0.12);
    shape(fb, doorShape, D({120, 78, 48}), ink, 2);
    shape(fb, warp(rectPolygon(415, 140, 475, 205), 31, t * 8), D({140, 94, 60}), ink, 1);
    shape(fb, warp(rectPolygon(415, 218, 475, 285), 32, t * 8), D({140, 94, 60}), ink, 1);
    disc(fb, 472 + noise(33, 0) * t * 6, 215, 5, D({235, 200, 90}));

    // ================= HANGING LAMP =================
    {
        double lx = 320 + noise(40, 0) * t * 18, ly = 150 + noise(40, 1) * t * 8;
        line(fb, {320, 0}, {lx, ly - 25}, ink, 2);
        Polygon shade = {{lx - 20, ly - 25}, {lx + 20, ly - 25}, {lx + 34, ly}, {lx - 34, ly}};
        shape(fb, warp(shade, 41, t * 6), D({220, 70, 70}), ink, 2);
        Color glow = lerpColor({255, 225, 120}, {210, 30, 45}, t);
        disc(fb, lx, ly + 7, 8, glow);
        int rings = 5 - (int)(2 * t);  // halo breaks up as t rises
        for (int k = 0; k < rings; ++k)
            ring(fb, lx + noise(42, k) * t * 8, ly + 7 + noise(43, k) * t * 8, 16 + k * 9,
                 lerpColor(glow, D({232, 236, 242}), (k + 1.0) / (rings + 1.0)));
    }

    // ================= TABLE with mug (and, later, bottle and pills) =================
    Polygon tabTop = {{50, 322}, {235, 322}, {275, 352}, {10, 352}};
    line(fb, {62, 322}, {60, 405}, ink, 3);   // back legs first, so the top covers them
    line(fb, {224, 322}, {226, 405}, ink, 3);
    shape(fb, warp(tabTop, 50, t * 6), D({150, 104, 66}), ink, 2);
    shape(fb, rectPolygon(10, 352, 275, 366), D({128, 86, 52}), ink, 2);
    line(fb, {22, 366}, {22, 430}, ink, 5);
    line(fb, {262, 366}, {262, 430}, ink, 5);
    {   // mug: polygon body, ellipse rim, Bezier handle - tips over as t rises
        Polygon body = rectPolygon(70, 296, 100, 334);
        Point pivot{85, 334};
        double ang = t * 0.55;
        body = rotate(warp(body, 60, t * 3), pivot, ang);
        shape(fb, body, D({240, 240, 245}), ink, 2);
        Point rim = rotate({{85, 296}}, pivot, ang)[0];
        oval(fb, rim.x, rim.y, 15, 5, D({255, 255, 255}), ink);
        std::vector<Point> hd = rotate({{100, 304}, {120, 302}, {120, 328}, {99, 326}}, pivot, ang);
        drawBezier(fb, hd, ink, 3, 40);
    }
    if (t > 0.25) {   // medicine bottle, tipped over, appears with the distortion
        double a = lerp(0.3, 1.4, smooth((t - 0.25) / 0.75));
        Point pv{175, 336};
        Polygon body = rotate(rectPolygon(160, 296, 190, 336), pv, a);
        Polygon neck = rotate(rectPolygon(168, 284, 182, 296), pv, a);
        Polygon cap = rotate(rectPolygon(166, 278, 184, 285), pv, a);
        shape(fb, body, D({230, 150, 40}), ink, 2);
        shape(fb, neck, D({230, 150, 40}), ink, 1);
        shape(fb, cap, D({240, 240, 240}), ink, 1);
        int pills = (int)std::lround(7 * smooth((t - 0.3) / 0.7));
        for (int i = 0; i < pills; ++i) {
            double px = 195 + i * 11 + noise(70, i) * 5, py = 340 + (i % 2) * 6 + noise(71, i) * 2;
            oval(fb, px, py, 5, 3, D({250, 250, 250}), ink);
        }
    }

    // ================= CHARACTER =================
    const double s = smooth(t);
    const double hx = 335 + 9 * s + noise(80, 0) * t * 3, hy = 248 + 12 * s;  // head droops
    // legs and shoes
    shape(fb, warp({{315, 372}, {333, 372}, {331, 440}, {311, 440}}, 81, t * 5), D({70, 80, 110}), ink, 2);
    shape(fb, warp({{337, 372}, {355, 372}, {359, 440}, {339, 440}}, 82, t * 5), D({70, 80, 110}), ink, 2);
    oval(fb, 318, 446, 16, 6, D({40, 40, 48}), ink);
    oval(fb, 352, 446, 16, 6, D({40, 40, 48}), ink);
    // torso (slumps and warps), neck
    shape(fb, warp({{305, 290}, {365, 290}, {372, 375}, {298, 375}}, 83, t * 8), D({40, 150, 160}), ink, 2);
    shape(fb, rectPolygon(328, 272, 342, 292), D({230, 190, 160}), ink, 1);
    // arms: cubic Bezier whose control points slide from "relaxed" to "clutching"
    struct Arm { Point h[4], d[4]; };
    Arm armL = {{{305, 296}, {290, 322}, {290, 348}, {298, 374}},
                {{305, 296}, {270, 305}, {262, 255}, {hx - 32, hy + 6}}};
    Arm armR = {{{365, 296}, {380, 322}, {380, 348}, {372, 374}},
                {{365, 296}, {392, 330}, {365, 360}, {338, 335}}};
    for (const Arm* arm : {&armL, &armR}) {
        std::vector<Point> c(4);
        for (int i = 0; i < 4; ++i) {
            c[i] = lerpP(arm->h[i], arm->d[i], s);
            if (i > 0) { c[i].x += noise(90, i) * t * 6; c[i].y += noise(91, i) * t * 6; }
        }
        drawBezier(fb, c, D({40, 150, 160}), 11, 40);   // sleeve
        drawBezier(fb, c, ink, 1, 40);
        disc(fb, c[3].x, c[3].y, 7, D({230, 190, 160}));  // hand
        ring(fb, c[3].x, c[3].y, 7, ink);
    }
    // head and hair
    oval(fb, hx, hy, 27, 33, D({230, 190, 160}), ink);
    Polygon hair = translate({{306, 240}, {310, 216}, {335, 208}, {360, 216}, {364, 240},
                              {352, 226}, {335, 222}, {318, 226}}, hx - 335, hy - 248);
    shape(fb, warp(hair, 84, t * 4), D({70, 45, 30}), ink, 1);
    // eyes: sclera ellipse, pupil circle (dilates), bloodshot lines appear with t
    for (int side = -1; side <= 1; side += 2) {
        double ex = hx + side * 11, ey = hy - 4 + (side > 0 ? 6 * t : 0);
        int rx = 8, ry = 5 + I(4 * t);
        oval(fb, ex, ey, rx, ry, lerpColor({255, 255, 255}, {235, 170, 170}, t), ink);
        disc(fb, ex + noise(85, side) * t * 2, ey, 2 + I(4 * t), ink);
        if (t > 0.4)
            for (int k = 0; k < 4; ++k) {
                double a = noise(86 + side, k) * 1.2 + (side > 0 ? 0 : M_PI);
                line(fb, {ex + std::cos(a) * rx, ey + std::sin(a) * ry * 0.5},
                     {ex + std::cos(a) * (rx - 4), ey + std::sin(a) * 2}, {175, 25, 40}, 1);
            }
    }
    // mouth: Bezier curve; control points flip from smile to frown
    {
        double bend = 11 * (1 - 2 * s);
        std::vector<Point> m = {{hx - 13, hy + 17}, {hx - 6, hy + 17 + bend},
                                {hx + 6, hy + 17 + bend + noise(87, 0) * t * 6}, {hx + 13, hy + 17}};
        drawBezier(fb, m, lerpColor({150, 50, 60}, ink, t), 2, 30);
    }

    // ================= SMOKE / HAZE: Catmull-Rom wisps and particles =================
    int wisps = (int)std::lround(8 * smooth((t - 0.15) / 0.85));
    for (int i = 0; i < wisps; ++i) {
        double bx = 70 + hash01(100, i) * 500;
        std::vector<Point> pts;
        for (int k = 0; k < 6; ++k)
            pts.push_back({bx + 28 * std::sin(k * 1.2 + i) + noise(101, i * 8 + k) * 12, 440 - k * 62.0});
        int g = 150 - I(50 * hash01(102, i));
        drawCatmullRom(fb, pts, {(uint8_t)g, (uint8_t)g, (uint8_t)(g + 8)}, false, 2, 16);
    }
    int particles = (int)std::lround(60 * smooth((t - 0.1) / 0.9));
    for (int i = 0; i < particles; ++i) {
        double px = hash01(110, i) * 640, py = hash01(111, i) * 480;
        int r = 1 + I(hash01(112, i) * 3);
        disc(fb, px, py, r, lerpColor({150, 150, 160}, {170, 30, 45}, hash01(113, i)));
    }

    postProcess(fb, t);
}
