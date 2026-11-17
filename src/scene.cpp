// scene.cpp - the room of Shattered Realities and everything that happens in it.
// The storyboard (story.cpp) supplies the values for one moment; this file turns
// them into pixels. Algorithms used: Bresenham lines, midpoint circle/ellipse,
// scanline polygon fill, Sutherland-Hodgman clipping (sun, moon and clouds in the
// window), Bezier curves (arms, mouths, plant, mug handle), Catmull-Rom splines
// (smoke), homogeneous 2D transformations (door, clock hands, lamp, picture, bottles,
// characters), colour interpolation (day/night, healthy -> decayed), alpha blending
// (light, shadows, smoke, captions), masked sprite animation (shadow walkers) and
// raster post-processing (colour wash, row shifts, vignette, spotlight, zoom).
#include "scene.h"
#include "animation.h"
#include "circles.h"
#include "clipping.h"
#include "curves.h"
#include "figure.h"
#include "font.h"
#include "lines.h"
#include "polygon.h"
#include "raster.h"
#include "sprites.h"
#include "story.h"
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
// ---------- night: darker and bluer
Color nightTint(Color c, double n) {
    return lerpColor(c, {(uint8_t)(c.r * 0.42), (uint8_t)(c.g * 0.46), (uint8_t)(c.b * 0.66)}, n);
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

// Translucent polygon: scanline-filled into a small colour-keyed layer, then
// composited with the same mask + alpha blit the sprites use.
void polyAlpha(Framebuffer& fb, const Polygon& p, Color c, double alpha) {
    if (alpha <= 0.004 || p.size() < 3) return;
    double x0 = p[0].x, x1 = p[0].x, y0 = p[0].y, y1 = p[0].y;
    for (const Point& q : p) {
        x0 = std::min(x0, q.x); x1 = std::max(x1, q.x);
        y0 = std::min(y0, q.y); y1 = std::max(y1, q.y);
    }
    const int ox = std::max(0, (int)std::floor(x0)), oy = std::max(0, (int)std::floor(y0));
    const int w = std::min(fb.width(), (int)std::ceil(x1) + 1) - ox, h = std::min(fb.height(), (int)std::ceil(y1) + 1) - oy;
    if (w <= 0 || h <= 0) return;
    Framebuffer layer(w, h, SPRITE_KEY);
    fillPolygonScanline(layer, transformPolygon(p, translate2D(-ox, -oy)), c);
    const Mask m = makeColorKeyMask(layer, SPRITE_KEY);
    blitAlpha(fb, ox, oy, layer, {0, 0, w, h}, std::min(1.0, alpha), &m);
}

// Text that can fade: drawn into a layer, masked, alpha-blitted.
void textAlpha(Framebuffer& fb, int cx, int y, const std::string& s, Color c, int scale, double alpha) {
    if (alpha <= 0.004) return;
    const int w = textWidth(s, scale) + 2 * scale, h = 9 * scale;
    Framebuffer layer(w, h, SPRITE_KEY);
    drawTextShadow(layer, 0, 0, s, c, scale, {8, 6, 14});
    const Mask m = makeColorKeyMask(layer, SPRITE_KEY);
    blitAlpha(fb, cx - w / 2, y, layer, {0, 0, w, h}, std::min(1.0, alpha), &m);
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
    if (k.bezier.size() > 2) drawTextShadow(fb, I(k.bezier[2][1].x) + 12, I(k.bezier[2][1].y) - 4, "BEZIER CONTROL POINTS", Y);
    if (!k.splines.empty()) {  // label the top of the tallest wisp, where it is least crowded
        const Point* top = &k.splines[0].back();
        for (const auto& s : k.splines)
            if (s.back().y < top->y) top = &s.back();
        int lx = I(top->x) + 8;
        if (lx + textWidth("CATMULL-ROM POINTS") > fb.width() - 4) lx = I(top->x) - 8 - textWidth("CATMULL-ROM POINTS");
        drawTextShadow(fb, lx, I(top->y) - 4, "CATMULL-ROM POINTS", C);
    }
    if (!k.pivots.empty()) drawTextShadow(fb, I(k.pivots[0].x) - 100, I(k.pivots[0].y) + 8, "ROTATION PIVOT", W);
}

// ---------- euphoria: saturated colours, a cycling colour wash and rows that sway
void highEffect(Framebuffer& fb, double high, double time) {
    if (high <= 0.004) return;
    const int W = fb.width(), H = fb.height();
    Color wash[256];  // one cycle of the colour wheel, looked up per pixel
    for (int i = 0; i < 256; ++i) {
        double ph = i * 2 * M_PI / 256;
        wash[i] = {(uint8_t)(128 + 127 * std::sin(ph)), (uint8_t)(128 + 127 * std::sin(ph + 2.094)),
                   (uint8_t)(128 + 127 * std::sin(ph + 4.189))};
    }
    const int mix = (int)(62 * high), sat = (int)(90 * high), scroll = (int)(time * 70);
    std::vector<Color> tmp(W);
    for (int y = 0; y < H; ++y) {
        Color* row = fb.row(y);
        for (int x = 0; x < W; ++x) {
            Color c = row[x];
            const int grey = (c.r * 77 + c.g * 150 + c.b * 29) >> 8;
            const Color& w = wash[(x / 2 + y / 3 + scroll) & 255];
            auto ch = [&](int v, int wv) {
                v += (v - grey) * sat / 256;                 // push away from grey: more saturated
                v = (v * (256 - mix) + wv * mix) >> 8;       // blend with the colour wheel
                return (uint8_t)std::clamp(v, 0, 255);
            };
            row[x] = {ch(c.r, w.r), ch(c.g, w.g), ch(c.b, w.b)};
        }
        // sway: each row is block-transferred sideways by a sine of its height
        const int shift = I(5 * high * std::sin(y * 0.035 + time * 2.5));
        if (shift != 0) {
            std::copy(row, row + W, tmp.begin());
            for (int x = 0; x < W; ++x) row[x] = tmp[std::clamp(x - shift, 0, W - 1)];
        }
    }
}

// ---------- raster post-processing: vignette + horizontal "tearing" glitch bands
void postProcess(Framebuffer& fb, double t, double time, double glitch) {
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
            const int f = (int)(256 * (1.0f - 0.8f * (float)t * d2[x]));  // brightness in 1/256ths
            row[x] = {(uint8_t)(row[x].r * f >> 8), (uint8_t)(row[x].g * f >> 8), (uint8_t)(row[x].b * f >> 8)};
        }
    }
    // glitch bands pick new random rows a few times per second
    unsigned seed = (unsigned)(time * 6) * 7;
    int bands = (int)std::lround(7 * easeInOut((t - 0.35) / 0.65) * glitch);
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

// ---------- spotlight: everything outside a circle around the character goes dark
void spotlight(Framebuffer& fb, Point c, double amount) {
    if (amount <= 0.004) return;
    const double radius = lerp(560, 105, amount), soft = 70;
    const int darkest = (int)(256 * (1.0 - 0.9 * amount));
    for (int y = 0; y < fb.height(); ++y) {
        Color* row = fb.row(y);
        for (int x = 0; x < fb.width(); ++x) {
            const double d2 = (x - c.x) * (x - c.x) + (y - c.y) * (y - c.y);
            if (d2 <= radius * radius) continue;  // inside the light: untouched
            int f = darkest;                      // far outside: fully dark
            if (d2 < (radius + soft) * (radius + soft))  // the soft edge in between
                f = (int)(256 * (1.0 - 0.9 * amount * (std::sqrt(d2) - radius) / soft));
            row[x] = {(uint8_t)(row[x].r * f >> 8), (uint8_t)(row[x].g * f >> 8), (uint8_t)(row[x].b * f >> 8)};
        }
    }
}

// ---------- cracks: reality breaks like a pane of glass (Bresenham polylines)
void drawCracks(Framebuffer& fb, Point o, double amount) {
    if (amount <= 0.004) return;
    const int rays = 9, steps = 24;
    const Color light{206, 210, 226}, dark{12, 10, 20};
    auto crackLine = [&](Point a, Point b, int thick) {
        line(fb, {a.x + 1, a.y + 1}, {b.x + 1, b.y + 1}, dark, thick);
        line(fb, a, b, light, thick);
    };
    std::vector<std::vector<Point>> pts(rays);
    for (int i = 0; i < rays; ++i) {
        double ang = 2 * M_PI * i / rays + noise(700, i) * 0.22;
        for (int j = 0; j <= steps; ++j) {
            double r = j * 30.0, jit = j == 0 ? 0 : noise(701 + i, j) * 9;
            pts[i].push_back({o.x + std::cos(ang) * r - std::sin(ang) * jit, o.y + std::sin(ang) * r + std::cos(ang) * jit});
        }
    }
    const double reach = amount * steps;
    for (int i = 0; i < rays; ++i)
        for (int j = 0; j < steps; ++j) {
            double f = clamp01(reach - j);
            if (f <= 0) break;
            crackLine(pts[i][j], lerpPoint(pts[i][j], pts[i][j + 1], f), j < 3 ? 2 : 1);
            if (j % 3 == 2 && f >= 1) {  // side branch
                double ang = std::atan2(pts[i][j + 1].y - pts[i][j].y, pts[i][j + 1].x - pts[i][j].x) +
                             (hash01(720 + i, j) > 0.5 ? 0.8 : -0.8);
                crackLine(pts[i][j], {pts[i][j].x + 26 * std::cos(ang), pts[i][j].y + 26 * std::sin(ang)}, 1);
            }
        }
    for (int j : {3, 7})  // rings of the spider web
        if (reach > j + 1)
            for (int i = 0; i < rays; ++i) crackLine(pts[i][j], pts[(i + 1) % rays][j], 1);
}

// ---------- camera: zoom towards a focus point (nearest-neighbour block scaling)
void zoomView(Framebuffer& fb, double z, Point focus, Point offset) {
    if (z <= 1.001) return;
    const int W = fb.width(), H = fb.height();
    const double vw = W / z, vh = H / z;
    const double x0 = std::clamp(focus.x - vw / 2 + offset.x, 0.0, W - vw);
    const double y0 = std::clamp(focus.y - vh / 2 + offset.y, 0.0, H - vh);
    const Framebuffer src = fb;
    for (int y = 0; y < H; ++y) {
        const Color* srow = src.row(std::min(H - 1, (int)(y0 + (y + 0.5) / z)));
        Color* drow = fb.row(y);
        for (int x = 0; x < W; ++x) drow[x] = srow[std::min(W - 1, (int)(x0 + (x + 0.5) / z))];
    }
}

// A pill bottle standing on pv, rotated about it.
void drawBottle(Framebuffer& fb, Point pv, double ang, const std::function<Color(Color)>& D, Color ink) {
    const Mat3 m = rotateAbout(ang, pv);
    auto part = [&](double x0, double y0, double x1, double y1, Color c, int thick) {
        shape(fb, transformPolygon(rectPolygon(pv.x + x0, pv.y + y0, pv.x + x1, pv.y + y1), m), D(c), ink, thick);
    };
    part(-15, -40, 15, 0, {230, 150, 40}, 2);
    part(-7, -52, 7, -40, {230, 150, 40}, 1);
    part(-9, -58, 9, -51, {240, 240, 240}, 1);
    part(-15, -30, 15, -12, {248, 246, 240}, 1);  // label
    Point a = m.apply({pv.x - 9, pv.y - 21}), b = m.apply({pv.x + 9, pv.y - 21});
    line(fb, a, b, D({200, 50, 60}), 2);
}

}  // namespace

void renderStory(Framebuffer& fb, double storyTime, const SceneOptions& opt) {
    const StoryState st = sampleStory(storyTime);
    const double t = st.distortion, time = st.time, high = st.high;
    Construction k;

    // ---- sky state: one angle places the sun and moon and decides day or night
    const double elevation = std::sin(st.sunAngle);
    const double night = clamp01(0.5 - elevation * 2.5);
    const double dusk = clamp01(1 - std::fabs(elevation) * 3.5);

    // room colours: decayed by the distortion, darker and bluer at night
    const double dark = night * (0.85 - 0.4 * t);
    std::function<Color(Color)> D = [&](Color c) { return nightTint(decay(c, t), dark); };
    // the main character decays less, so the figure stays readable in the dark
    std::function<Color(Color)> heroShade = [&](Color c) { return nightTint(decay(c, 0.72 * t), night * 0.3); };
    const Color ink = D({25, 25, 38});
    const double wob = t * 16 * (1 + 0.3 * std::sin(time * 2.3)) + high * 4 * std::sin(time * 3.1);  // walls "breathe"
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
    // sunlight from the window falls on the floor during the day
    polyAlpha(fb, {{205, 318}, {300, 318}, {345, 396}, {218, 396}}, {255, 244, 190}, 0.2 * (1 - night) * (1 - t));
    // rug
    oval(fb, 335, 452, 124, 22, D({168, 62, 74}), ink);
    oval(fb, 335, 452, 96, 15, D({214, 168, 104}), D({120, 44, 56}));

    // ================= WINDOW: sun, moon and clouds clipped by Sutherland-Hodgman =================
    const Rect glass{196, 106, 284, 194};
    shape(fb, warp(rectPolygon(186, 96, 294, 204), 20, t * 10), D({150, 100, 60}), ink, 2);
    Color sky = lerpColor({140, 200, 245}, {16, 18, 48}, night);
    sky = lerpColor(sky, {245, 150, 90}, 0.75 * dusk);
    sky = lerpColor(sky, {40, 12, 30}, 0.8 * t);
    fillPolygonScanline(fb, rectPolygon(glass.xmin, glass.ymin, glass.xmax, glass.ymax), sky);
    {
        auto clipped = [&](const Polygon& p, Color c) {  // only the part inside the glass is drawn
            Polygon q = clipPolygonSutherlandHodgman(p, glass);
            if (q.size() >= 3) fillPolygonScanline(fb, q, c);
        };
        for (int i = 0; i < 14; ++i) {  // stars
            int sx = 198 + I(hash01(30, i) * 84), sy = 108 + I(hash01(31, i) * 84);
            if (night > 0.5 && hash01(32 + i, (unsigned)(time * 3)) > 0.25) fb.setPixel(sx, sy, {235, 235, 255});
        }
        const double sa = st.sunAngle, ma = st.sunAngle + M_PI;
        clipped(regularPolygon(240 - 72 * std::cos(sa), 206 - 74 * std::sin(sa), 20, 28),
                lerpColor({255, 214, 70}, {150, 20, 25}, t));
        const Point moon{240 - 72 * std::cos(ma), 206 - 74 * std::sin(ma)};
        clipped(regularPolygon(moon.x, moon.y, 15, 24), lerpColor({236, 236, 216}, {190, 40, 50}, t));
        clipped(regularPolygon(moon.x + 7, moon.y - 4, 12, 24), sky);  // bite out of the moon: a crescent
        for (int i = 0; i < 2; ++i) {  // clouds drift past and are cut by the window edges
            const double cx = 150 + std::fmod(time * (5 + 2 * i) + i * 95, 190), cy = 122 + i * 26;
            const Color cc = lerpColor(lerpColor({255, 255, 255}, sky, 0.15 + 0.7 * night), sky, 0.7 * t);
            clipped(regularPolygon(cx, cy, 9, 14), cc);
            clipped(regularPolygon(cx + 12, cy - 4, 11, 14), cc);
            clipped(regularPolygon(cx + 25, cy, 8, 14), cc);
        }
        k.clips.push_back(glass);
    }
    line(fb, {240, 106}, {240, 194}, D({150, 100, 60}), 3);
    line(fb, {196, 150}, {284, 150}, D({150, 100, 60}), 3);
    drawRect(fb, glass, ink, 2);

    // ================= PICTURE on the wall: tilts on its nail as things fall apart =================
    {
        const Point nail{154, 110};
        const Mat3 m = rotateAbout(0.42 * easeInOut(t) + 0.02 * t * std::sin(time * 1.3), nail);
        auto P = [&](double x, double y) { return m.apply({x, y}); };
        line(fb, nail, P(134, 120), ink);
        line(fb, nail, P(174, 120), ink);
        shape(fb, transformPolygon(rectPolygon(130, 120, 178, 172), m), D({120, 82, 50}), ink, 2);
        shape(fb, transformPolygon(rectPolygon(135, 125, 173, 167), m), D({176, 220, 244}), ink, 1);
        fillPolygonScanline(fb, transformPolygon({{135, 156}, {173, 150}, {173, 167}, {135, 167}}, m), D({120, 190, 110}));
        for (int i = 0; i < 2; ++i) {  // two happy people
            Point h = P(146 + i * 16, 144), b = P(146 + i * 16, 158);
            disc(fb, b.x, b.y, 7, D(i ? Color{245, 170, 50} : Color{40, 150, 160}));
            disc(fb, h.x, h.y, 5, D({230, 190, 160}));
        }
        if (t > 0.55) {  // the glass cracks
            line(fb, P(137, 128), P(152, 146), D({250, 250, 255}));
            line(fb, P(152, 146), P(148, 158), D({250, 250, 255}));
            line(fb, P(152, 146), P(170, 152), D({250, 250, 255}));
        }
        disc(fb, nail.x, nail.y, 2, ink);
        k.pivots.push_back(nail);
    }

    // ================= CLOCK: hands are rotated about the centre; they race when days blur =================
    {
        const Point c{362, 98};
        disc(fb, c.x, c.y, 19, D({120, 82, 50}));
        disc(fb, c.x, c.y, 16, D({250, 248, 240}));
        ring(fb, c.x, c.y, 19, ink);
        for (int i = 0; i < 12; ++i) {
            const Mat3 m = rotateAbout(i * M_PI / 6, c);
            line(fb, m.apply({c.x, c.y - 15}), m.apply({c.x, c.y - (i % 3 == 0 ? 11 : 13)}), ink);
        }
        const double hours = 6 + st.sunAngle / (2 * M_PI) * 24 + noise(46, 0) * t * 3;
        line(fb, c, rotateAbout(hours / 12 * 2 * M_PI, c).apply({c.x, c.y - 8}), ink, 2);
        line(fb, c, rotateAbout(hours * 2 * M_PI, c).apply({c.x, c.y - 13}), ink, 1);
        line(fb, c, rotateAbout(std::floor(time) * M_PI / 30, c).apply({c.x, c.y - 13}), D({200, 50, 60}), 1);
        disc(fb, c.x, c.y, 2, ink);
        k.pivots.push_back(c);
    }

    // ================= HANGING LAMP (swings: rotation about the ceiling hook) =================
    {
        Point hook{320, 0};
        Mat3 swing = rotateAbout(std::sin(time * 1.7) * (0.04 + 0.18 * t), hook);
        double lx = 320 + noise(40, 0) * t * 18, ly = 150 + noise(40, 1) * t * 8;
        Point top = swing.apply({lx, ly - 25}), bulb = swing.apply({lx, ly + 7});
        Color glow = lerpColor({255, 225, 120}, {210, 30, 45}, t);
        // flicker: the bulb dims randomly when reality is unstable
        const bool dim = t > 0.5 && hash01(44, (unsigned)(time * 12)) < 0.25 * t;
        if (dim) glow = darken(glow, 0.45);
        // cone of light on the wall and floor, stronger at night
        polyAlpha(fb, {swing.apply({lx - 30, ly}), swing.apply({lx + 30, ly}), swing.apply({lx + 175, 480}), swing.apply({lx - 175, 480})},
                  glow, (0.05 + 0.15 * night) * (dim ? 0.3 : 1));
        line(fb, hook, top, ink, 2);
        Polygon shade = {{lx - 20, ly - 25}, {lx + 20, ly - 25}, {lx + 34, ly}, {lx - 34, ly}};
        shape(fb, transformPolygon(warp(shade, 41, t * 6), swing), D({220, 70, 70}), ink, 2);
        disc(fb, bulb.x, bulb.y, 8, glow);
        int rings = 5 - (int)(2 * t);  // halo breaks up as t rises
        for (int r = 0; r < rings; ++r)
            ring(fb, bulb.x + noise(42, r) * t * 8, bulb.y + noise(43, r) * t * 8, 16 + r * 9,
                 lerpColor(glow, D({232, 236, 242}), (r + 1.0) / (rings + 1.0)));
        k.pivots.push_back(hook);
    }

    // ================= characters =================
    const Palette heroLook{{230, 190, 160}, {40, 150, 160}, {70, 80, 110}, {40, 40, 48}, {70, 45, 30}, false};
    const Palette helperLook{{204, 152, 118}, {245, 170, 50}, {96, 74, 128}, {60, 40, 40}, {40, 28, 24}, true};
    FigureGuides heroGuides, helperGuides;
    auto drawActor = [&](const ActorState& a, bool isHero) {
        if (!a.visible) return;
        // soft shadow on the floor: an ellipse polygon, alpha-blended
        const double sw = 36 * a.scale * (1 - 0.02 * a.lift);
        polyAlpha(fb, transformPolygon(regularPolygon(0, 0, 1, 20), translate2D(a.x, a.groundY + 2) * scale2D(sw, sw * 0.2)),
                  {10, 8, 16}, 0.3);
        FigureFx fx;
        fx.time = time;
        if (isHero) {
            fx.distortion = t;
            fx.high = high;
            fx.shade = heroShade;
            drawFigure(fb, a, heroLook, fx, &heroGuides);
        } else {  // the helper is never touched by the distortion: full colour in a grey world
            fx.shade = [&](Color c) { return nightTint(c, night * 0.3); };
            drawFigure(fb, a, helperLook, fx, &helperGuides);
        }
    };
    const bool heroInDoorway = st.hero.groundY < 308, helperInDoorway = st.helper.groundY < 308;

    // ================= DOOR: scaled about its hinge to swing open; light pours in =================
    {
        const Point hinge{405, 300};
        fillPolygonScanline(fb, rectPolygon(405, 125, 485, 300), lerpColor(D({30, 26, 40}), {255, 246, 214}, st.doorOpen));
        if (helperInDoorway) drawActor(st.helper, false);  // standing behind the door plane
        if (heroInDoorway) drawActor(st.hero, true);
        const Mat3 m = rotateAbout(-t * 0.12, hinge) * scaleAbout(std::cos(st.doorOpen * 1.3), 1, hinge);
        shape(fb, transformPolygon(warp(rectPolygon(405, 125, 485, 300), 30, t * 12), m), D({120, 78, 48}), ink, 2);
        shape(fb, transformPolygon(warp(rectPolygon(415, 140, 475, 205), 31, t * 6), m), D({140, 94, 60}), ink, 1);
        shape(fb, transformPolygon(warp(rectPolygon(415, 218, 475, 285), 32, t * 6), m), D({140, 94, 60}), ink, 1);
        const Point knob = m.apply({472, 215});
        disc(fb, knob.x, knob.y, 4, D({235, 200, 90}));
        // the light from the doorway spreads over the floor
        polyAlpha(fb, {{405, 300}, {485, 300}, {610, 480}, {300, 480}}, {255, 240, 190},
                  st.doorOpen * (0.1 + 0.3 * std::max(t, night)));
        k.pivots.insert(k.pivots.begin(), hinge);
    }

    // ================= SHADOW WALKERS (masked, alpha-blended sprite animation) =================
    // Frame-based (walk cycle) + position-based animation; they pace the back wall.
    if (st.shadows > 0.01) {
        const SpriteSheet& sheet = shadowWalkerSheet();
        for (int i = 0; i < 2; ++i) {
            const double speed = 40 - 12 * i, range = 330;  // pixels per second, path length
            double u = std::fmod(time * speed + i * 290, 2 * range);  // ping-pong along the wall
            bool facingLeft = u > range;
            double x = 130 + (facingLeft ? 2 * range - u : u);
            int frame = (int)(time * (9 - 2 * i)) % sheet.frames;  // own frame timer
            drawSprite(fb, sheet, frame, I(x), 300 - sheet.frameH + 2, (0.8 - 0.25 * i) * st.shadows, facingLeft);
        }
    }

    // ================= PLANT: Bezier stems that droop and brown =================
    {
        const double wilt = easeInOut(t);
        shape(fb, {{556, 398}, {604, 398}, {597, 438}, {563, 438}}, D({190, 96, 60}), ink, 2);
        shape(fb, rectPolygon(552, 392, 608, 401), D({206, 110, 70}), ink, 1);
        const Color leaf = D(lerpColor({60, 160, 80}, {140, 110, 50}, wilt));
        const double tipX[5] = {-46, -24, 2, 26, 48}, tipY[5] = {-58, -92, -108, -90, -54};
        for (int i = 0; i < 5; ++i) {
            const Point base{580, 393};
            const double sway = std::sin(time * 1.1 + i) * 2.5;
            Point tip{base.x + tipX[i] * (1 + 0.25 * wilt) + sway, base.y + tipY[i] * (1 - 0.72 * wilt)};
            std::vector<Point> stem = {base, {base.x + tipX[i] * 0.2, base.y + tipY[i] * 0.7},
                                       {base.x + tipX[i] * 0.8 + sway, base.y + tipY[i] * (1.05 - 0.4 * wilt)}, tip};
            drawBezier(fb, stem, leaf, 3, 24);
            oval(fb, tip.x, tip.y, 9, 5, leaf, ink);
            if (i == 2) k.bezier.push_back(stem);
        }
    }

    // ================= TABLE with mug, bottles and pills =================
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
    if (t > 0.42) drawBottle(fb, {150, 348}, -1.5, D, ink);              // an empty one, knocked over
    if (st.bottleAlpha >= 0.999) {
        drawBottle(fb, {175, 336}, 1.4 * easeInOut((t - 0.6) / 0.25), D, ink);  // the bottle: falls over late
    } else if (st.bottleAlpha > 0.004) {  // fading away: drawn in its own layer, then alpha-blitted
        Framebuffer layer(140, 140, SPRITE_KEY);
        drawBottle(layer, {70, 100}, 1.4 * easeInOut((t - 0.6) / 0.25), D, ink);
        const Mask m = makeColorKeyMask(layer, SPRITE_KEY);
        blitAlpha(fb, 175 - 70, 336 - 100, layer, {0, 0, 140, 140}, st.bottleAlpha, &m);
    }
    if (t > 0.6) drawBottle(fb, {255, 458}, 1.5, D, ink);                // another on the floor
    if (st.bottleGlow > 0.01)  // the bottle catches the eye: pulsing midpoint circles
        for (int r = 0; r < 3; ++r) {
            const double pulse = std::fmod(time * 0.9 + r / 3.0, 1.0);
            ring(fb, 175, 312, I(22 + 26 * pulse), lerpColor({255, 220, 90}, D({232, 236, 242}), pulse + (1 - st.bottleGlow) * (1 - pulse)));
        }
    for (int i = 0; i < (int)std::lround(st.pills); ++i) {
        double px = i < 7 ? 198 + i * 10 + noise(70, i) * 4 : 286 + (i - 7) * 17;
        double py = i < 7 ? 340 + (i % 2) * 6 + noise(71, i) * 2 : 462 + noise(71, i) * 3;
        oval(fb, px, py, 5, 3, D({250, 250, 250}), ink);
    }

    // ================= SMOKE: Catmull-Rom wisps, alpha-blended from their own layer =================
    // The wisps are drawn into a colour-keyed layer, then composited with the same
    // mask + alpha blit as the sprites, so the haze is translucent.
    const int wisps = (int)std::lround(8 * easeInOut((t - 0.3) / 0.7));
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

    // the characters in the room, the one further back first (painter's algorithm)
    if (!helperInDoorway && st.helper.groundY < st.hero.groundY) drawActor(st.helper, false);
    if (!heroInDoorway) drawActor(st.hero, true);
    if (!helperInDoorway && st.helper.groundY >= st.hero.groundY) drawActor(st.helper, false);
    for (const FigureGuides* g : {&helperGuides, &heroGuides}) {  // the hero's curves end up first
        k.bezier.insert(k.bezier.begin(), g->bezier.begin(), g->bezier.end());
        k.pivots.insert(k.pivots.end(), g->pivots.begin(), g->pivots.end());
    }

    // ================= STARS of the high: spinning, pulsing star polygons =================
    for (int i = 0; i < (int)std::lround(18 * high); ++i) {
        const double speed = 18 + 30 * hash01(121, i);
        const double sx = hash01(120, i) * 640 + std::sin(time * 1.3 + i) * 14;
        const double sy = std::fmod(hash01(122, i) * 480 - time * speed + 480 * 100, 480);
        Polygon star;
        for (int v = 0; v < 10; ++v) {
            double r = v % 2 ? 4 : 10, a = v * M_PI / 5;
            star.push_back({r * std::sin(a), -r * std::cos(a)});
        }
        const double pulse = 0.7 + 0.4 * std::sin(time * 4 + i);
        star = transformPolygon(star, translate2D(sx, sy) * rotate2D(time * (1 + hash01(123, i)) + i) * scale2D(pulse, pulse));
        const double hue = i * 1.7 + time * 2;
        fillPolygonScanline(fb, star, {(uint8_t)(200 + 55 * std::sin(hue)), (uint8_t)(200 + 55 * std::sin(hue + 2.1)),
                                       (uint8_t)(200 + 55 * std::sin(hue + 4.2))});
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

    // ================= post-processing and camera =================
    highEffect(fb, high, time);
    postProcess(fb, t, time, st.glitch);
    const Point heroChest{st.hero.x, st.hero.groundY - lerp(140, 90, st.hero.crouch)};
    spotlight(fb, heroChest, st.spotlight);
    drawCracks(fb, {118, 78}, st.crack);  // the impact point: top-left corner of the back wall
    if (opt.construction) drawConstruction(fb, k);

    const unsigned shakeSeed = (unsigned)(time * 30);
    zoomView(fb, std::max(st.zoom, 1 + 0.05 * st.shake), st.focus,
             {noise(950, shakeSeed) * 14 * st.shake, noise(951, shakeSeed) * 10 * st.shake});
    const IRect full{0, 0, fb.width(), fb.height()};
    fillRectAlpha(fb, full, {255, 255, 255}, st.flash);
    fillRectAlpha(fb, full, {0, 0, 0}, st.fade);

    // ================= captions, title and end card =================
    if (!opt.captions) return;
    const int cx = fb.width() / 2;
    if (st.captionAlpha > 0) {
        const int w = textWidth(st.caption, 2) + 36;
        fillRectAlpha(fb, {cx - w / 2, 400, w, 32}, {10, 8, 16}, 0.72 * st.captionAlpha);
        textAlpha(fb, cx, 409, st.caption, {250, 250, 252}, 2, st.captionAlpha);
    }
    if (st.titleAlpha > 0) {
        fillRectAlpha(fb, {0, 150, fb.width(), 96}, {10, 8, 16}, 0.6 * st.titleAlpha);
        textAlpha(fb, cx, 164, "SHATTERED REALITIES", {120, 210, 220}, 4, st.titleAlpha);
        textAlpha(fb, cx, 214, "A STORY ABOUT WHAT DRUGS TAKE AWAY", {235, 235, 242}, 2, st.titleAlpha);
    }
    if (st.endAlpha > 0) {
        fillRectAlpha(fb, full, {8, 6, 14}, 0.78 * st.endAlpha);
        textAlpha(fb, cx, 150, "SHATTERED REALITIES", {120, 210, 220}, 4, st.endAlpha);
        textAlpha(fb, cx, 214, "DRUGS SHATTER LIVES.", {230, 90, 100}, 3, st.endAlpha);
        textAlpha(fb, cx, 250, "HELP REBUILDS THEM.", {245, 200, 90}, 3, st.endAlpha);
        textAlpha(fb, cx, 310, "IF YOU OR A FRIEND IS STRUGGLING, TALK TO SOMEONE YOU TRUST.", {225, 225, 235}, 1, st.endAlpha);
        textAlpha(fb, cx, 440, "EVERY PIXEL DRAWN BY OUR OWN GRAPHICS ALGORITHMS IN C++", {130, 130, 150}, 1, st.endAlpha);
    }
}
