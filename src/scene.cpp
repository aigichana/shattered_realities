// scene.cpp - "healthy room" that decays as the distortion value t rises from 0 to 1.
// Algorithms used: Bresenham lines, midpoint circle/ellipse, scanline polygon fill,
// Sutherland-Hodgman clipping (sun in the window), Bezier curves (arms, mouth, mug
// handle), Catmull-Rom splines (smoke), homogeneous 2D transformations (door, mug,
// bottle, lamp, breathing torso), colour interpolation (healthy -> decayed),
// alpha-blended masked sprite animation (shadow walker) and raster post-processing.
#include "scene.h"
#include "animation.h"
#include "circles.h"
#include "clipping.h"
#include "curves.h"
#include "font.h"
#include "lines.h"
#include "polygon.h"
#include "raster.h"
#include "sprites.h"
#include "transform.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace {

// ---------- deterministic noise (hash based, so each vertex jitters consistently)
double hash01(unsigned a, unsigned b) {
    unsigned h = a * 2654435761u ^ (b + 0x9e3779b9u) * 40503u;
    h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; h *= 3266489917u; h ^= h >> 16;
    return (h & 0xFFFFFF) / double(0x1000000);
}
double noise(unsigned a, unsigned b) { return hash01(a, b) * 2.0 - 1.0; }  // [-1, 1)

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
Polygon rotate(const Polygon& p, Point pivot, double angle) {
    return transformPolygon(p, rotateAbout(angle, pivot));
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

// ---------- "construction lines": the hidden geometry behind the picture
struct Construction {
    std::vector<std::vector<Point>> bezier;   // control polygons
    std::vector<std::vector<Point>> splines;  // Catmull-Rom points
    std::vector<Point> pivots;                // rotation / scaling pivots
    std::vector<Rect> clips;                  // clip windows
};

// Drawn into a separate layer and combined with XOR: always visible on any
// background, and XOR-ing the same layer again would erase it exactly.
void drawConstruction(Framebuffer& fb, const Construction& k) {
    Framebuffer layer(fb.width(), fb.height(), {0, 0, 0});
    const Color W{255, 255, 255}, Y{255, 255, 0}, C{0, 255, 255};
    for (const auto& c : k.bezier) drawControlPolygon(layer, c, Y, Y);
    for (const auto& s : k.splines)
        for (const Point& p : s) drawCircleMidpoint(layer, I(p.x), I(p.y), 3, C);
    for (const Point& p : k.pivots) {
        drawCircleMidpoint(layer, I(p.x), I(p.y), 6, W);
        line(layer, {p.x - 9, p.y}, {p.x + 9, p.y}, W);
        line(layer, {p.x, p.y - 9}, {p.x, p.y + 9}, W);
    }
    for (const Rect& r : k.clips) drawRect(layer, r, C, 1);
    blit(fb, 0, 0, layer, {0, 0, layer.width(), layer.height()}, RasterOp::Xor);

    // Labels are drawn normally (XOR text is hard to read over mid-grey pixels).
    if (!k.clips.empty()) drawTextShadow(fb, I(k.clips[0].xmin) - 10, I(k.clips[0].ymin) - 22, "CLIP WINDOW", C);
    if (k.bezier.size() > 1) drawTextShadow(fb, I(k.bezier[1][1].x) + 12, I(k.bezier[1][1].y) - 4, "BEZIER CONTROL POINTS", Y);
    if (!k.splines.empty()) {  // label the top of the tallest wisp, where it is least crowded
        const Point* top = &k.splines[0].back();
        for (const auto& s : k.splines)
            if (s.back().y < top->y) top = &s.back();
        int lx = I(top->x) + 8;
        if (lx + textWidth("CATMULL-ROM POINTS") > fb.width() - 4) lx = I(top->x) - 8 - textWidth("CATMULL-ROM POINTS");
        drawTextShadow(fb, lx, I(top->y) - 4, "CATMULL-ROM POINTS", C);
    }
    if (!k.pivots.empty()) drawTextShadow(fb, I(k.pivots[0].x) + 12, I(k.pivots[0].y) + 6, "ROTATION PIVOT", W);
}

// ---------- raster post-processing: vignette + horizontal "tearing" glitch bands
void postProcess(Framebuffer& fb, double t, double time) {
    if (t <= 0) return;
    const int W = fb.width(), H = fb.height();
    // vignette: darken by squared distance from the centre (0 at centre, 1 at the corners).
    // The distances never change, so they are computed once and reused every frame.
    static std::vector<float> dist2;
    static int tw = 0, th = 0;
    if (tw != W || th != H) {
        tw = W; th = H;
        dist2.resize((size_t)W * H);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                double dx = (x - W / 2.0) / (W / 2.0), dy = (y - H / 2.0) / (H / 2.0);
                dist2[(size_t)y * W + x] = (float)((dx * dx + dy * dy) / 2.0);
            }
    }
    for (int y = 0; y < H; ++y) {
        Color* row = fb.row(y);
        const float* d2 = &dist2[(size_t)y * W];
        for (int x = 0; x < W; ++x) {
            double f = 1.0 - 0.8 * t * d2[x];
            row[x] = {(uint8_t)(row[x].r * f), (uint8_t)(row[x].g * f), (uint8_t)(row[x].b * f)};
        }
    }
    // glitch bands pick new random rows a few times per second when animated
    unsigned seed = (unsigned)(time * 6) * 7;
    int bands = (int)(7 * easeInOut((t - 0.35) / 0.65));
    for (int k = 0; k < bands; ++k) {
        int y0 = I(hash01(900 + seed, k) * (H - 40)), h = 4 + I(hash01(901 + seed, k) * 14);
        int shift = I(noise(902 + seed, k) * 45 * t);
        // a horizontal band is one block transfer of the row to itself, shifted
        Framebuffer band(W, h);
        blit(band, 0, 0, fb, {0, y0, W, h});
        blit(fb, shift, y0, band, {0, 0, W, h});
        blit(fb, shift - (shift > 0 ? W : -W), y0, band, {0, 0, W, h});  // wrap-around
    }
}

}  // namespace

const char* stageName(double t) {
    if (t < 0.15) return "HEALTHY";
    if (t < 0.5) return "UNEASE";
    if (t < 0.85) return "DISTORTING";
    return "SHATTERED";
}

void renderScene(Framebuffer& fb, double t, const SceneOptions& opt) {
    t = clamp01(t);
    const double time = opt.time;
    Construction k;
    const Color ink = decay({25, 25, 38}, t);
    const double wob = t * 16 * (1 + 0.3 * std::sin(time * 2.3));  // walls "breathe" when animated

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
        Polygon sun = regularPolygon(lerp(240, 300, easeInOut(t)), lerp(138, 205, easeInOut(t)), 22, 28);
        Polygon clipped = clipPolygonSutherlandHodgman(sun, glass);
        if (clipped.size() >= 3)
            fillPolygonScanline(fb, clipped, lerpColor({255, 214, 70}, {150, 20, 25}, t));
        k.clips.push_back(glass);
    }
    line(fb, {240, 106}, {240, 194}, D({150, 100, 60}), 3);
    line(fb, {196, 150}, {284, 150}, D({150, 100, 60}), 3);
    drawRect(fb, glass, ink, 2);

    // ================= DOOR (rotates about its bottom-left hinge) =================
    Polygon door = rectPolygon(405, 125, 485, 300);
    Polygon doorShape = rotate(warp(door, 30, t * 18), {405, 300}, -t * 0.12);
    shape(fb, doorShape, D({120, 78, 48}), ink, 2);
    shape(fb, warp(rectPolygon(415, 140, 475, 205), 31, t * 8), D({140, 94, 60}), ink, 1);
    shape(fb, warp(rectPolygon(415, 218, 475, 285), 32, t * 8), D({140, 94, 60}), ink, 1);
    disc(fb, 472 + noise(33, 0) * t * 6, 215, 5, D({235, 200, 90}));
    k.pivots.push_back({405, 300});

    // ================= SHADOW WALKER (masked, alpha-blended sprite animation) =================
    // Appears as reality breaks: frame-based (walk cycle) + position-based (delta time) animation.
    if (t > 0.45) {
        const SpriteSheet& sheet = shadowWalkerSheet();
        const double speed = 40, range = 330;            // pixels per second, path length
        double u = std::fmod(time * speed, 2 * range);   // ping-pong along the back wall
        bool facingLeft = u > range;
        double x = 130 + (facingLeft ? 2 * range - u : u);
        int frame = (int)(time * 9) % sheet.frames;      // 9 poses per second, own timer
        double alpha = 0.8 * easeInOut((t - 0.45) / 0.4);  // ghostly: partially transparent
        drawSprite(fb, sheet, frame, I(x), 300 - sheet.frameH + 2, alpha, facingLeft);
    }

    // ================= HANGING LAMP (swings: rotation about the ceiling hook) =================
    {
        Point hook{320, 0};
        Mat3 swing = rotateAbout(std::sin(time * 1.7) * (0.04 + 0.18 * t), hook);
        double lx = 320 + noise(40, 0) * t * 18, ly = 150 + noise(40, 1) * t * 8;
        Point top = swing.apply({lx, ly - 25}), bulb = swing.apply({lx, ly + 7});
        line(fb, hook, top, ink, 2);
        Polygon shade = {{lx - 20, ly - 25}, {lx + 20, ly - 25}, {lx + 34, ly}, {lx - 34, ly}};
        shape(fb, transformPolygon(warp(shade, 41, t * 6), swing), D({220, 70, 70}), ink, 2);
        Color glow = lerpColor({255, 225, 120}, {210, 30, 45}, t);
        // flicker: the bulb dims randomly when reality is unstable
        if (t > 0.5 && hash01(44, (unsigned)(time * 12)) < 0.25 * t) glow = darken(glow, 0.45);
        disc(fb, bulb.x, bulb.y, 8, glow);
        int rings = 5 - (int)(2 * t);  // halo breaks up as t rises
        for (int r = 0; r < rings; ++r)
            ring(fb, bulb.x + noise(42, r) * t * 8, bulb.y + noise(43, r) * t * 8, 16 + r * 9,
                 lerpColor(glow, D({232, 236, 242}), (r + 1.0) / (rings + 1.0)));
        k.pivots.push_back(hook);
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
        Point rim = rotateAbout(ang, pivot).apply({85, 296});
        oval(fb, rim.x, rim.y, 15, 5, D({255, 255, 255}), ink);
        std::vector<Point> hd = rotate({{100, 304}, {120, 302}, {120, 328}, {99, 326}}, pivot, ang);
        drawBezier(fb, hd, ink, 3, 40);
        k.pivots.push_back(pivot);
    }
    if (t > 0.25) {   // medicine bottle, tipped over, appears with the distortion
        double a = lerp(0.3, 1.4, easeInOut((t - 0.25) / 0.75));
        Point pv{175, 336};
        Polygon body = rotate(rectPolygon(160, 296, 190, 336), pv, a);
        Polygon neck = rotate(rectPolygon(168, 284, 182, 296), pv, a);
        Polygon cap = rotate(rectPolygon(166, 278, 184, 285), pv, a);
        shape(fb, body, D({230, 150, 40}), ink, 2);
        shape(fb, neck, D({230, 150, 40}), ink, 1);
        shape(fb, cap, D({240, 240, 240}), ink, 1);
        int pills = (int)std::lround(7 * easeInOut((t - 0.3) / 0.7));
        for (int i = 0; i < pills; ++i) {
            double px = 195 + i * 11 + noise(70, i) * 5, py = 340 + (i % 2) * 6 + noise(71, i) * 2;
            oval(fb, px, py, 5, 3, D({250, 250, 250}), ink);
        }
    }

    // ================= SMOKE: Catmull-Rom wisps, alpha-blended from their own layer =================
    // The wisps are drawn into a colour-keyed layer, then composited with the same
    // mask + alpha blit as the sprites, so the haze is translucent. They sit behind the
    // character, rise from the floor and have different heights.
    const int wisps = (int)std::lround(8 * easeInOut((t - 0.15) / 0.85));
    if (wisps > 0) {
        Framebuffer haze(fb.width(), fb.height(), SPRITE_KEY);
        for (int i = 0; i < wisps; ++i) {
            const double bx = 70 + hash01(100, i) * 500, base = 452 - 24 * hash01(104, i);
            const int n = 4 + (int)(3 * hash01(103, i));  // 4 to 6 points
            std::vector<Point> pts;
            for (int j = 0; j < n; ++j)
                pts.push_back({bx + 26 * std::sin(j * 1.2 + i + time * 0.9) + noise(101, i * 8 + j) * 10,
                               base - j * 55.0});
            const int g = 215 - I(45 * hash01(102, i));
            drawCatmullRom(haze, pts, {(uint8_t)g, (uint8_t)g, (uint8_t)(g + 10)}, false, 3, 16);
            k.splines.push_back(pts);
        }
        const Mask m = makeColorKeyMask(haze, SPRITE_KEY);
        blitAlpha(fb, 0, 0, haze, {0, 0, haze.width(), haze.height()}, 0.3 + 0.3 * t, &m);
    }

    // ================= CHARACTER =================
    const double s = easeInOut(t);
    const double shake = std::sin(time * 23) * t * t * 2.0;  // trembling at high distortion
    const double hx = 335 + 9 * s + noise(80, 0) * t * 3 + shake, hy = 248 + 12 * s;  // head droops
    // legs and shoes
    shape(fb, warp({{315, 372}, {333, 372}, {331, 440}, {311, 440}}, 81, t * 5), D({70, 80, 110}), ink, 2);
    shape(fb, warp({{337, 372}, {355, 372}, {359, 440}, {339, 440}}, 82, t * 5), D({70, 80, 110}), ink, 2);
    oval(fb, 318, 446, 16, 6, D({40, 40, 48}), ink);
    oval(fb, 352, 446, 16, 6, D({40, 40, 48}), ink);
    // torso (slumps and warps; breathes by scaling about the waist), neck
    Mat3 breathe = scaleAbout(1.0 + 0.012 * std::sin(time * 2.2), 1.0 + 0.02 * std::sin(time * 2.2), {335, 375});
    shape(fb, transformPolygon(warp({{305, 290}, {365, 290}, {372, 375}, {298, 375}}, 83, t * 8), breathe),
          D({40, 150, 160}), ink, 2);
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
            c[i] = lerpPoint(arm->h[i], arm->d[i], s);
            if (i > 0) { c[i].x += noise(90, i) * t * 6; c[i].y += noise(91, i) * t * 6; }
        }
        drawBezier(fb, c, D({40, 150, 160}), 11, 40);   // sleeve
        drawBezier(fb, c, ink, 1, 40);
        disc(fb, c[3].x, c[3].y, 7, D({230, 190, 160}));  // hand
        ring(fb, c[3].x, c[3].y, 7, ink);
        k.bezier.push_back(c);
    }
    // head and hair
    oval(fb, hx, hy, 27, 33, D({230, 190, 160}), ink);
    Polygon hair = transformPolygon({{306, 240}, {310, 216}, {335, 208}, {360, 216}, {364, 240},
                                     {352, 226}, {335, 222}, {318, 226}},
                                    translate2D(hx - 335, hy - 248));
    shape(fb, warp(hair, 84, t * 4), D({70, 45, 30}), ink, 1);
    // eyes: sclera ellipse, pupil circle (dilates), bloodshot lines appear with t
    for (int side = -1; side <= 1; side += 2) {
        double ex = hx + side * 11, ey = hy - 4 + (side > 0 ? 6 * t : 0);
        int rx = 8, ry = 5 + I(4 * t);
        oval(fb, ex, ey, rx, ry, lerpColor({255, 255, 255}, {235, 170, 170}, t), ink);
        disc(fb, ex + noise(85, side) * t * 2, ey, 2 + I(4 * t), ink);
        if (t > 0.4)
            for (int r = 0; r < 4; ++r) {
                double a = noise(86 + side, r) * 1.2 + (side > 0 ? 0 : M_PI);
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
        k.bezier.push_back(m);
    }

    // ================= DUST PARTICLES (position-based animation) =================
    int particles = (int)std::lround(60 * easeInOut((t - 0.1) / 0.9));
    for (int i = 0; i < particles; ++i) {
        // particles rise at their own speed: y = y0 - v * time, wrapped to the screen
        double speed = 8 + 22 * hash01(114, i);
        double px = hash01(110, i) * 640 + std::sin(time * 0.7 + i) * 6;
        double py = std::fmod(hash01(111, i) * 480 - time * speed + 480 * 100, 480);
        int r = 1 + I(hash01(112, i) * 3);
        disc(fb, px, py, r, lerpColor({150, 150, 160}, {170, 30, 45}, hash01(113, i)));
    }

    postProcess(fb, t, time);
    if (opt.construction) drawConstruction(fb, k);
}
