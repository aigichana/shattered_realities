#include "demos.h"
#include "animation.h"
#include "circles.h"
#include "clipping.h"
#include "curves.h"
#include "font.h"
#include "lines.h"
#include "polygon.h"
#include "raster.h"
#include "scene.h"
#include "sprites.h"
#include "transform.h"
#include <cmath>
#include <cstdio>

namespace {

const Color BG{245, 245, 248}, INK{30, 30, 40}, GREY{175, 175, 185}, FAINT{225, 225, 232};
const Color RED{200, 50, 60}, BLUE{40, 90, 200}, GREEN{40, 150, 90}, ORANGE{235, 140, 40};

int I(double v) { return (int)std::lround(v); }

// ------------------------------------------------------------- algorithm checks
struct LineCompare { int differ = 0, total = 0; };

// Draws 360 lines of radius 90 with both algorithms and counts differing pixels.
const LineCompare& compareDdaBresenham() {
    static const LineCompare result = [] {
        LineCompare r;
        for (int i = 0; i < 360; ++i) {
            double a = 2 * M_PI * i / 360;
            int x1 = 100 + I(90 * std::cos(a)), y1 = 100 + I(90 * std::sin(a));
            Framebuffer A(200, 200, {0, 0, 0}), B(200, 200, {0, 0, 0});
            drawLineDDA(A, 100, 100, x1, y1, {255, 255, 255});
            drawLineBresenham(B, 100, 100, x1, y1, {255, 255, 255});
            for (int y = 0; y < 200; ++y)
                for (int x = 0; x < 200; ++x) {
                    Color p = A.getPixel(x, y), q = B.getPixel(x, y);
                    if (p.r || q.r) { ++r.total; if (p.r != q.r) ++r.differ; }
                }
        }
        return r;
    }();
    return result;
}

struct ClipLine { double x0, y0, x1, y1; };
const Rect CLIP_WIN{190, 130, 450, 350};
const ClipLine CLIP_LINES[] = {{60, 80, 560, 400},  {100, 380, 520, 90}, {250, 180, 400, 300},
                               {30, 240, 610, 250}, {320, 20, 330, 460}, {40, 40, 150, 100},
                               {500, 380, 600, 440}, {160, 330, 480, 140}, {200, 60, 440, 420},
                               {20, 440, 240, 300}};

// Runs both line clippers on the test lines and counts disagreements.
int compareClippers() {
    int mismatch = 0;
    for (const ClipLine& l : CLIP_LINES) {
        double a0 = l.x0, a1 = l.y0, a2 = l.x1, a3 = l.y1;
        double b0 = l.x0, b1 = l.y0, b2 = l.x1, b3 = l.y1;
        bool okA = clipLineCohenSutherland(a0, a1, a2, a3, CLIP_WIN);
        bool okB = clipLineLiangBarsky(b0, b1, b2, b3, CLIP_WIN);
        if (okA != okB || (okA && (std::fabs(a0 - b0) > 1e-6 || std::fabs(a1 - b1) > 1e-6 ||
                                   std::fabs(a2 - b2) > 1e-6 || std::fabs(a3 - b3) > 1e-6)))
            ++mismatch;
    }
    return mismatch;
}

// ------------------------------------------------------------------ 01 lines
void renderLines(Framebuffer& fb, double) {
    fb.clear(BG);
    // Left: DDA fan. Right: Bresenham fan.
    const int n = 36;
    for (int i = 0; i < n; ++i) {
        double a = 2 * M_PI * i / n;
        int dx = I(140 * std::cos(a)), dy = I(140 * std::sin(a));
        drawLineDDA(fb, 160, 240, 160 + dx, 240 + dy, BLUE);
        drawLineBresenham(fb, 480, 240, 480 + dx, 240 + dy, RED);
    }
    drawLineThick(fb, 320, 20, 320, 460, 2, GREY);  // divider
    drawLineThick(fb, 40, 430, 280, 400, 4, INK);    // thick lines along the bottom
    drawLineThick(fb, 360, 400, 600, 430, 4, INK);

    drawTextCentered(fb, 160, 72, "DDA", BLUE, 2);
    drawTextCentered(fb, 480, 72, "BRESENHAM", RED, 2);
    drawTextCentered(fb, 160, 386, "FLOATING POINT, ROUNDS EACH STEP", INK);
    drawTextCentered(fb, 480, 386, "INTEGER-ONLY DECISION PARAMETER", INK);
    const LineCompare& lc = compareDdaBresenham();
    char buf[96];
    std::snprintf(buf, sizeof buf, "360 TEST LINES: %d OF %d LIT PIXELS DIFFER (%.2f%%)", lc.differ,
                  lc.total, 100.0 * lc.differ / lc.total);
    drawTextCentered(fb, 320, 452, buf, INK);
    drawTextCentered(fb, 320, 464, "THICK LINES = SQUARE BRUSH STAMPED ALONG THE BRESENHAM PATH", GREY);
}

// ------------------------------------------------- 02 circles and ellipses
void renderCircles(Framebuffer& fb, double) {
    fb.clear(BG);
    for (int r = 15; r <= 100; r += 17)
        drawCircleMidpoint(fb, 160, 120, r, lerpColor(BLUE, RED, (r - 15) / 85.0));
    for (int k = 1; k <= 6; ++k)
        drawEllipseMidpoint(fb, 480, 120, 18 * k, 9 * k, lerpColor(GREEN, ORANGE, k / 6.0));
    fillCircleMidpoint(fb, 110, 270, 40, BLUE);
    fillEllipseMidpoint(fb, 230, 270, 55, 28, RED);
    fillEllipseMidpoint(fb, 230, 270, 30, 12, BG);  // ring effect
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

    drawText(fb, 10, 6, "MIDPOINT CIRCLE: 1 OCTANT, 8-WAY SYMMETRY", INK);
    const char* ell = "MIDPOINT ELLIPSE: 2 REGIONS";
    drawText(fb, 630 - textWidth(ell), 6, ell, INK);
    drawTextCentered(fb, 320, 336, "FILLED BY DRAWING HORIZONTAL SPANS BETWEEN SYMMETRIC POINTS", INK);
    drawText(fb, 392, 392, "BUILT ONLY FROM", GREY);
    drawText(fb, 392, 404, "CIRCLES + ELLIPSES", GREY);
}

// ------------------------------------------------- 03 polygons and filling
void renderPolygons(Framebuffer& fb, double) {
    fb.clear(BG);
    Polygon tri = {{70, 150}, {190, 150}, {130, 40}};
    fillPolygonScanline(fb, tri, BLUE);
    drawPolygonOutline(fb, tri, INK, 2);
    Polygon arrow = {{250, 90}, {330, 90}, {330, 50}, {400, 110}, {330, 170}, {330, 130}, {250, 130}};
    fillPolygonScanline(fb, arrow, RED);
    drawPolygonOutline(fb, arrow, INK, 2);
    Polygon star;  // five-point star: self-intersecting, even-odd rule leaves a hole
    for (int i = 0; i < 5; ++i) {
        double a = -M_PI / 2 + 2 * M_PI * (i * 2 % 5) / 5.0;
        star.push_back({520 + 70 * std::cos(a), 105 + 70 * std::sin(a)});
    }
    fillPolygonScanline(fb, star, ORANGE);
    drawPolygonOutline(fb, star, INK, 2);
    Polygon hex = regularPolygon(110, 320, 70, 6);
    fillPolygonScanline(fb, hex, GREEN);
    drawPolygonOutline(fb, hex, INK, 2);
    drawCircleMidpoint(fb, 300, 320, 65, INK);  // boundary fill inside a circle outline
    drawCircleMidpoint(fb, 300, 320, 64, INK);
    boundaryFill(fb, 300, 320, {120, 190, 230}, INK);
    drawRect(fb, {400, 255, 560, 385}, INK, 2);  // ... and inside a rectangle with an obstacle
    drawCircleMidpoint(fb, 480, 320, 30, INK);
    boundaryFill(fb, 410, 265, {240, 200, 90}, INK);
    floodFill(fb, 480, 320, {200, 90, 160});  // flood fill recolours the obstacle interior
    Polygon outer = regularPolygon(320, 435, 40, 4, M_PI / 4);  // polygon with a hole
    fillPolygonScanline(fb, outer, INK);
    fillPolygonScanline(fb, regularPolygon(320, 435, 18, 4, M_PI / 4), BG);

    drawText(fb, 10, 6, "SCANLINE FILL USES THE EVEN-ODD RULE; SEED FILLS USE AN EXPLICIT STACK", GREY);
    drawTextCentered(fb, 130, 162, "TRIANGLE", INK);
    drawTextCentered(fb, 325, 182, "CONCAVE", INK);
    drawTextCentered(fb, 520, 172, "SELF-INTERSECTING", INK);
    drawTextCentered(fb, 520, 184, "(HOLE FROM EVEN-ODD RULE)", GREY);
    drawTextCentered(fb, 110, 392, "SCANLINE FILL", INK);
    drawTextCentered(fb, 300, 392, "BOUNDARY FILL", INK);
    drawTextCentered(fb, 480, 392, "BOUNDARY + FLOOD FILL", INK);
    drawText(fb, 372, 432, "POLYGON WITH A HOLE", INK);
}

// ------------------------------------------------------------ 04 clipping
void renderClipping(Framebuffer& fb, double) {
    fb.clear(BG);
    int idx = 0;
    for (const ClipLine& l : CLIP_LINES) {
        drawLineBresenham(fb, I(l.x0), I(l.y0), I(l.x1), I(l.y1), {210, 210, 218});  // faint original
        double a0 = l.x0, a1 = l.y0, a2 = l.x1, a3 = l.y1;
        if (clipLineCohenSutherland(a0, a1, a2, a3, CLIP_WIN))
            drawLineThick(fb, I(a0), I(a1), I(a2), I(a3), 3, (idx % 2 == 0) ? RED : BLUE);
        ++idx;
    }
    Polygon tri = {{150, 200}, {300, 120}, {260, 300}};
    Polygon pent = regularPolygon(450, 330, 70, 5, -M_PI / 2);
    for (const Polygon& p : {tri, pent}) {
        drawPolygonOutline(fb, p, {170, 170, 180}, 1);
        Polygon c = clipPolygonSutherlandHodgman(p, CLIP_WIN);
        fillPolygonScanline(fb, c, {60, 170, 110});
        drawPolygonOutline(fb, c, INK, 2);
    }
    drawRect(fb, CLIP_WIN, INK, 3);

    drawText(fb, 10, 6, "FAINT = ORIGINAL LINE      BOLD = PART KEPT BY THE CLIPPER", INK);
    drawText(fb, 192, 117, "CLIP WINDOW", INK);
    char buf[96];
    std::snprintf(buf, sizeof buf, "COHEN-SUTHERLAND VS LIANG-BARSKY: %d DISAGREEMENTS ON 10 LINES",
                  compareClippers());
    drawText(fb, 10, 452, buf, INK);
    drawText(fb, 10, 464, "GREEN = POLYGONS CLIPPED EDGE BY EDGE (SUTHERLAND-HODGMAN)", {40, 120, 80});
}

// -------------------------------------------------------------- 05 curves
void renderCurves(Framebuffer& fb, double) {
    fb.clear(BG);
    std::vector<Point> cubic = {{40, 200}, {90, 40}, {230, 40}, {290, 190}};
    drawControlPolygon(fb, cubic, GREY, GREY);
    drawBezier(fb, cubic, RED, 3);
    std::vector<Point> quad = {{350, 190}, {450, 30}, {600, 190}};
    drawControlPolygon(fb, quad, GREY, GREY);
    drawBezier(fb, quad, BLUE, 3);
    std::vector<Point> quartic = {{40, 440}, {80, 250}, {160, 440}, {240, 250}, {300, 420}};
    drawControlPolygon(fb, quartic, GREY, GREY);
    drawBezier(fb, quartic, GREEN, 3);
    // Catmull-Rom: the curve passes through every point (drawn on top so they stay visible)
    auto knots = [&](const std::vector<Point>& pts, Color c) {
        for (const Point& p : pts) {
            fillCircleMidpoint(fb, I(p.x), I(p.y), 4, c);
            drawCircleMidpoint(fb, I(p.x), I(p.y), 4, INK);
        }
    };
    std::vector<Point> wave = {{350, 345}, {390, 268}, {440, 350}, {490, 262}, {540, 340}, {600, 272}};
    drawPolyline(fb, wave, {215, 215, 225}, 1);
    drawCatmullRom(fb, wave, ORANGE, false, 3);
    knots(wave, ORANGE);
    std::vector<Point> blob = {{440, 392}, {478, 380}, {512, 394}, {502, 428}, {458, 432}, {428, 414}};
    drawPolyline(fb, blob, {215, 215, 225}, 1);
    drawCatmullRom(fb, blob, INK, true, 2);
    knots(blob, {255, 255, 255});
    drawText(fb, 528, 400, "CLOSED LOOP", GREY);
    drawText(fb, 528, 412, "(WRAPS AROUND)", GREY);

    drawText(fb, 10, 6, "GREY = CONTROL POLYGON", GREY);
    drawTextCentered(fb, 165, 212, "CUBIC BEZIER", RED);
    drawTextCentered(fb, 475, 212, "QUADRATIC BEZIER", BLUE);
    drawTextCentered(fb, 170, 455, "QUARTIC BEZIER (DE CASTELJAU, ANY DEGREE)", GREEN);
    drawTextCentered(fb, 475, 455, "CATMULL-ROM: PASSES THROUGH POINTS", ORANGE);
}

// ---------------------------------------------------- 06 transformations
// Asymmetric house (chimney on the right) so reflections are visible. Local origin = centre.
const Polygon HOUSE = {{-30, 30}, {30, 30}, {30, -10}, {20, -20}, {20, -38},
                       {10, -38}, {10, -30}, {0, -40}, {-30, -10}};

void drawMatrix(Framebuffer& fb, int x, int y, const Mat3& m, Color c) {
    for (int r = 0; r < 3; ++r) {
        char buf[64];
        std::snprintf(buf, sizeof buf, "|%6.2f %6.2f %6.1f|", m.m[r][0], m.m[r][1], m.m[r][2]);
        drawText(fb, x, y + r * 10, buf, c);
    }
}

void transformPanel(Framebuffer& fb, int col, int row, const std::string& title, const Mat3& m,
                    Color fill, bool showReflectAxis = false) {
    const int x0 = col * 213, y0 = row * 240, w = 213, h = 240;
    const Point O{x0 + 106.0, y0 + 102.0};
    for (int gx = x0 + 6; gx < x0 + w; gx += 20) drawLineBresenham(fb, gx, y0 + 18, gx, y0 + 186, FAINT);
    for (int gy = y0 + 22; gy < y0 + 188; gy += 20) drawLineBresenham(fb, x0 + 4, gy, x0 + w - 4, gy, FAINT);
    drawLineBresenham(fb, x0 + 4, I(O.y), x0 + w - 4, I(O.y), GREY);  // local axes
    drawLineBresenham(fb, I(O.x), y0 + 18, I(O.x), y0 + 186, showReflectAxis ? ORANGE : GREY);
    drawRect(fb, {(double)x0 + 2, (double)y0 + 2, (double)x0 + w - 2, (double)y0 + h - 2}, GREY, 1);

    Mat3 toPanel = translate2D(O.x, O.y);
    drawPolygonOutline(fb, transformPolygon(HOUSE, toPanel), GREY, 1);  // original
    Polygon moved = transformPolygon(HOUSE, toPanel * m);               // transformed
    fillPolygonScanline(fb, moved, fill);
    drawPolygonOutline(fb, moved, INK, 2);
    fillCircleMidpoint(fb, I(O.x), I(O.y), 3, INK);                     // pivot / origin

    drawText(fb, x0 + 8, y0 + 7, title, INK);
    drawMatrix(fb, x0 + 14, y0 + 196, m, INK);
}

void renderTransforms(Framebuffer& fb, double time) {
    fb.clear(BG);
    char t[64];
    double tx = 50 * std::sin(time * 1.2), ty = 25 * std::sin(time * 2.4);
    std::snprintf(t, sizeof t, "TRANSLATE T(%.1f, %.1f)", tx, ty);
    transformPanel(fb, 0, 0, t, translate2D(tx, ty), BLUE);

    double sx = 1.3 + 0.4 * std::sin(time * 1.5), sy = 1.0 - 0.3 * std::sin(time * 1.5);
    std::snprintf(t, sizeof t, "SCALE S(%.2f, %.2f)", sx, sy);
    transformPanel(fb, 1, 0, t, scale2D(sx, sy), GREEN);

    double a = std::fmod(time * 0.9, 2 * M_PI);
    std::snprintf(t, sizeof t, "ROTATE R(%.0f DEG)", a * 180 / M_PI);
    transformPanel(fb, 2, 0, t, rotate2D(a), RED);

    double sh = 0.7 * std::sin(time * 1.3);
    std::snprintf(t, sizeof t, "SHEAR SH(%.2f, 0)", sh);
    transformPanel(fb, 0, 1, t, shear2D(sh, 0), ORANGE);

    transformPanel(fb, 1, 1, "REFLECT ACROSS Y-AXIS", reflect2D(false, true), {150, 90, 190}, true);

    // Composite: same three matrices, different order -> different result
    double ca = time * 0.6;
    Mat3 S = scale2D(0.6, 0.6), R = rotate2D(ca), T = translate2D(55, 0);
    Mat3 trs = T * R * S, rts = R * T * S;
    const int x0 = 426, y0 = 240;
    transformPanel(fb, 2, 1, "COMPOSITE: ORDER MATTERS", trs, GREEN);
    Polygon other = transformPolygon(HOUSE, translate2D(x0 + 106.0, y0 + 102.0) * rts);
    fillPolygonScanline(fb, other, RED);
    drawPolygonOutline(fb, other, INK, 2);
    drawText(fb, x0 + 8, y0 + 18, "T*R*S", GREEN);
    drawText(fb, x0 + 8, y0 + 28, "R*T*S", RED);
}

// ---------------------------------------------------- 07 raster operations
// A colourful crystal "shard" sprite on a magenta colour-key background.
struct ShardSprite {
    Framebuffer pixels{80, 80, SPRITE_KEY};
    Mask mask;
    Framebuffer andMask{1, 1}, orSprite{1, 1};
};

const ShardSprite& shard() {
    static const ShardSprite s = [] {
        ShardSprite r;
        Framebuffer& f = r.pixels;
        Point c{40, 40};
        Polygon outer = {{40, 4}, {70, 28}, {60, 72}, {20, 72}, {10, 28}};
        const Color facet[5] = {{120, 220, 230}, {220, 70, 80}, {245, 190, 60}, {150, 60, 140}, {30, 120, 150}};
        for (size_t i = 0; i < outer.size(); ++i)
            fillPolygonScanline(f, {outer[i], outer[(i + 1) % outer.size()], c}, facet[i]);
        for (const Point& p : outer) drawLineBresenham(f, 40, 40, I(p.x), I(p.y), {255, 255, 255});
        drawPolygonOutline(f, outer, INK, 2);
        fillCircleMidpoint(f, 34, 20, 4, {255, 255, 255});
        r.mask = makeColorKeyMask(f, SPRITE_KEY);
        r.andMask = makeAndMask(r.mask);
        r.orSprite = makeOrSprite(f, r.mask);
        return r;
    }();
    return s;
}

void drawPattern(Framebuffer& fb, IRect r) {
    for (int y = r.y; y < r.y + r.h; ++y)
        for (int x = r.x; x < r.x + r.w; ++x)
            fb.setPixel(x, y, ((x + y) / 10) % 2 ? Color{90, 160, 210} : Color{245, 215, 140});
    drawRect(fb, {(double)r.x, (double)r.y, (double)r.x + r.w - 1, (double)r.y + r.h - 1}, INK, 1);
}

void renderRaster(Framebuffer& fb, double time) {
    fb.clear(BG);
    const ShardSprite& s = shard();
    const IRect full{0, 0, 80, 80};

    // Row A: sprite sheet (source of every sprite blit) and the mask built from its colour key
    const SpriteSheet& sheet = shadowWalkerSheet();
    drawText(fb, 10, 6, "SPRITE SHEET (MAGENTA = COLOUR KEY)", INK);
    blit(fb, 10, 18, sheet.pixels, {0, 0, sheet.pixels.width(), sheet.pixels.height()});
    int cur = (int)(time * 9) % sheet.frames;
    IRect fr = sheet.frame(cur);
    drawRect(fb, {10.0 + fr.x, 18.0, 10.0 + fr.x + fr.w - 1, 18.0 + fr.h - 1}, {255, 220, 0}, 2);
    drawText(fb, 10, 118, "SOURCE RECT = FRAME i, COPIED TO ANY DESTINATION", GREY);
    blit(fb, 430, 18, s.pixels, full);
    blit(fb, 530, 18, s.andMask, full);
    drawTextCentered(fb, 470, 102, "SOURCE", INK);
    drawTextCentered(fb, 570, 102, "MASK", INK);
    drawTextCentered(fb, 520, 118, "1 BIT: IN / OUT", GREY);

    // Row B: one sprite, five ways of combining it with the destination
    drawText(fb, 10, 136, "LOGICAL RASTER OPS: THE SAME SPRITE BLITTED ONTO A PATTERN", INK);
    const int bob = I(10 + 6 * std::sin(time * 2));
    const char* names[5] = {"COPY", "AND MASK", "OR SPRITE", "AND THEN OR", "XOR"};
    const char* notes[5] = {"BOX VISIBLE", "CUTS A HOLE", "COLOURS LEAK", "CLEAN SPRITE", "INVERTS BITS"};
    for (int i = 0; i < 5; ++i) {
        IRect tile{6 + i * 126, 148, 118, 100};
        drawPattern(fb, tile);
        int dx = tile.x + 19, dy = tile.y + bob;
        if (i == 0) blit(fb, dx, dy, s.pixels, full, RasterOp::Copy);
        if (i == 1 || i == 3) blit(fb, dx, dy, s.andMask, full, RasterOp::And);
        if (i == 2 || i == 3) blit(fb, dx, dy, s.orSprite, full, RasterOp::Or);
        if (i == 4) blit(fb, dx, dy, s.pixels, full, RasterOp::Xor);
        drawTextCentered(fb, tile.x + 59, 252, names[i], INK);
        drawTextCentered(fb, tile.x + 59, 263, notes[i], GREY);
    }

    // Row C: alpha blending and XOR-erasable shapes
    drawText(fb, 10, 284, "ALPHA BLENDING: FINAL = SRC * A + DST * (1 - A)", INK);
    const double alphas[4] = {0.25, 0.5, 0.75, 1.0};
    for (int i = 0; i < 4; ++i) {
        IRect tile{6 + i * 108, 296, 100, 100};
        drawPattern(fb, tile);
        blitAlpha(fb, tile.x + 10, tile.y + 10, s.pixels, full, alphas[i], &s.mask);
        char buf[16];
        std::snprintf(buf, sizeof buf, "A = %.2f", alphas[i]);
        drawTextCentered(fb, tile.x + 50, 400, buf, INK);
    }
    drawText(fb, 446, 284, "XOR: DRAW TWICE = ERASED", INK);
    for (int i = 0; i < 2; ++i) {
        IRect tile{446 + i * 96, 296, 88, 100};
        drawPattern(fb, tile);
        Framebuffer layer(88, 100, {0, 0, 0});  // a "selection box" drawn into its own layer
        int bx = I(14 + 18 * std::sin(time * 1.4)), by = I(30 + 14 * std::cos(time * 1.1));
        fillRect(layer, {bx, by, 46, 36}, {255, 255, 255});
        for (int pass = 0; pass <= i; ++pass) blit(fb, tile.x, tile.y, layer, {0, 0, 88, 100}, RasterOp::Xor);
        drawTextCentered(fb, tile.x + 44, 400, i == 0 ? "ONCE" : "TWICE", INK);
    }
    drawText(fb, 10, 424, "MASK: EACH PIXEL IS IN OR OUT.   ALPHA: PARTIAL VISIBILITY (0 = CLEAR, 1 = OPAQUE).", GREY);
    drawText(fb, 10, 436, "THE SHADOW FIGURE IN THE SCENE IS DRAWN THIS WAY: SHEET FRAME + MASK + ALPHA.", GREY);
    drawText(fb, 10, 448, "THE LIVE VIEWER PRESENTS EVERY FRAME WITH ONE BLOCK TRANSFER (STRETCHDIBITS).", GREY);
}

// ---------------------------------------------------------- 08 animation
void renderAnimation(Framebuffer& fb, double time) {
    fb.clear(BG);
    const SpriteSheet& sheet = shadowWalkerSheet();

    // Tweening: two keyframes, in-betweens generated with four easing curves
    drawText(fb, 10, 6, "KEYFRAMES (SQUARES) + TWEENING. DOT SPACING SHOWS SPEED: CLOSE DOTS = SLOW", INK);
    const Ease eases[4] = {Ease::Linear, Ease::In, Ease::Out, Ease::InOut};
    const Color cols[4] = {BLUE, RED, GREEN, ORANGE};
    for (int r = 0; r < 4; ++r) {
        int y = 36 + r * 28;
        KeyframeTrack track;
        track.add(0, 110, eases[r]).add(2, 600, Ease::Linear).add(3, 600);  // move 2 s, hold 1 s
        drawText(fb, 10, y - 3, easeName(eases[r]), cols[r]);
        drawLineBresenham(fb, 110, y, 600, y, FAINT);
        for (int i = 0; i <= 10; ++i) {  // the in-betweens at 10 equal time steps
            int x = I(track.sample(i * 0.2));
            drawCircleMidpoint(fb, x, y, 3, GREY);
        }
        for (int kx : {110, 600}) fillRect(fb, {kx - 5, y - 5, 11, 11}, INK);
        fillCircleMidpoint(fb, I(track.sample(std::fmod(time, 3.0))), y, 7, cols[r]);
    }

    // Frame-based sprite animation
    drawText(fb, 10, 152, "FRAME-BASED SPRITE ANIMATION: SHEET -> CURRENT FRAME, OWN 9 FPS TIMER", INK);
    blitAlpha(fb, 10, 166, sheet.pixels, {0, 0, sheet.pixels.width(), sheet.frameH}, 1.0, &sheet.mask);
    int cur = (int)(time * 9) % sheet.frames;
    for (int i = 0; i < sheet.frames; ++i) {
        char n[4];
        std::snprintf(n, sizeof n, "%d", i);
        drawTextCentered(fb, 10 + i * sheet.frameW + 24, 266, n, i == cur ? RED : GREY);
    }
    drawRect(fb, {10.0 + cur * sheet.frameW, 166, 10.0 + (cur + 1) * sheet.frameW - 1, 261}, RED, 2);
    Framebuffer one(sheet.frameW, sheet.frameH, {232, 232, 240});  // current frame, then scaled 1.5x
    blitAlpha(one, 0, 0, sheet.pixels, sheet.frame(cur), 1.0, &sheet.mask);
    drawLineThick(fb, 410, 214, 455, 214, 2, GREY);
    drawLineThick(fb, 448, 208, 455, 214, 2, GREY);
    drawLineThick(fb, 448, 220, 455, 214, 2, GREY);
    blitScaled(fb, {470, 160, 72, 144}, one, {0, 0, sheet.frameW, sheet.frameH});
    drawText(fb, 556, 200, "SCALED", GREY);
    drawText(fb, 556, 212, "BLIT 1.5X", GREY);
    char fbuf[24];
    std::snprintf(fbuf, sizeof fbuf, "FRAME %d / %d", cur, sheet.frames);
    drawText(fb, 556, 230, fbuf, RED);

    // Position-based animation with delta time + onion-skin trail
    drawText(fb, 10, 318, "POSITION-BASED: X_NEW = X_OLD + V * DT     (V = 70 PX/S)", INK);
    drawText(fb, 10, 330, "COMBINED WITH THE FRAME TIMER ABOVE = WALKING. FADED COPIES = EARLIER FRAMES.", GREY);
    drawLineThick(fb, 0, 441, 639, 441, 2, INK);
    const double speed = 70, span = 640 + 48;
    for (int ghost = 3; ghost >= 0; --ghost) {
        double tg = time - ghost * 0.4;
        double x = std::fmod(tg * speed + 260 + span, span) - 48;
        drawSprite(fb, sheet, (int)(tg * 9) % sheet.frames, I(x), 441 - sheet.frameH,
                   ghost == 0 ? 1.0 : 0.1 + 0.08 * (3 - ghost));
    }
    drawText(fb, 10, 452, "SAME SPEED AT ANY FPS: 60 FPS -> DT = 0.017 S, 1.17 PX/FRAME.  30 FPS -> 2.33 PX/FRAME", INK);
    drawText(fb, 10, 464, "DOUBLE BUFFERING: PRESS B IN THE LIVE VIEWER TO DRAW WITHOUT IT AND SEE FLICKER", GREY);
}

}  // namespace

const std::vector<DemoPage>& demoPages() {
    static const std::vector<DemoPage> pages = {
        {"01_lines_dda_bresenham", "LINE DRAWING", "DDA VS BRESENHAM - SAME 36 DIRECTIONS", renderLines, false},
        {"02_circles_ellipses", "CIRCLES AND ELLIPSES", "MIDPOINT ALGORITHMS, OUTLINE AND FILLED", renderCircles, false},
        {"03_polygons_filling", "POLYGONS AND FILLING", "SCANLINE, BOUNDARY AND FLOOD FILL", renderPolygons, false},
        {"04_clipping", "CLIPPING", "COHEN-SUTHERLAND, LIANG-BARSKY, SUTHERLAND-HODGMAN", renderClipping, false},
        {"05_curves_bezier_spline", "CURVES", "BEZIER (DE CASTELJAU) AND CATMULL-ROM SPLINES", renderCurves, false},
        {"06_transformations", "2D TRANSFORMATIONS", "3X3 HOMOGENEOUS MATRICES: P' = M * [X Y 1]", renderTransforms, true},
        {"07_raster_ops", "RASTER OPERATIONS", "BIT-BLIT, AND / OR / XOR, MASKS, ALPHA, SPRITES", renderRaster, true},
        {"08_animation", "ANIMATION", "KEYFRAMES, TWEENING, EASING, SPRITE FRAMES, DELTA TIME", renderAnimation, true},
    };
    return pages;
}

void drawCaptionBar(Framebuffer& page, const std::string& title, const std::string& subtitle,
                    const std::string& right) {
    fillRect(page, {0, 0, page.width(), CAPTION_H}, {22, 20, 32});
    // accent stripe: the scene's own colour journey, healthy teal -> shattered red
    for (int x = 0; x < page.width(); ++x) {
        Color c = lerpColor({60, 190, 200}, {200, 40, 60}, x / double(page.width() - 1));
        for (int y = CAPTION_H - 3; y < CAPTION_H; ++y) page.setPixel(x, y, c);
    }
    drawText(page, 12, 8, title, {245, 245, 250}, 2);
    drawText(page, 12, 28, subtitle, {160, 160, 185});
    const std::string brand = "SHATTERED REALITIES";
    drawText(page, page.width() - 12 - textWidth(brand), 8, brand, {120, 200, 210});
    drawText(page, page.width() - 12 - textWidth(right), 24, right, {200, 120, 130});
}

Framebuffer composePage(const Framebuffer& canvas, const std::string& title,
                        const std::string& subtitle, const std::string& right) {
    Framebuffer page(PAGE_W, PAGE_H);
    drawCaptionBar(page, title, subtitle, right);
    blit(page, 0, CAPTION_H, canvas, {0, 0, canvas.width(), canvas.height()});
    return page;
}

void exportAll(const std::string& dir) {
    auto save = [&](const Framebuffer& fb, const std::string& name) {
        std::string path = dir + "/" + name + ".png";
        if (fb.savePNG(path)) std::printf("  wrote %s\n", path.c_str());
        else std::printf("  FAILED to write %s\n", path.c_str());
    };
    const double STILL_TIME = 1.3;  // moment captured for the animated pages

    std::printf("Rendering technique demos...\n");
    const auto& pages = demoPages();
    for (size_t i = 0; i < pages.size(); ++i) {
        Framebuffer canvas(CANVAS_W, CANVAS_H, BG);
        pages[i].render(canvas, STILL_TIME);
        char right[32];
        std::snprintf(right, sizeof right, "TECHNIQUE %zu / %zu", i + 1, pages.size());
        save(composePage(canvas, pages[i].title, pages[i].subtitle, right), pages[i].file);
    }
    const LineCompare& lc = compareDdaBresenham();
    std::printf("  check: DDA vs Bresenham: %d of %d lit pixels differ (%.2f%%)\n", lc.differ, lc.total,
                100.0 * lc.differ / lc.total);
    std::printf("  check: Cohen-Sutherland vs Liang-Barsky disagreements on 10 test lines: %d\n",
                compareClippers());

    std::printf("Rendering Shattered Realities scene stages...\n");
    struct Stage { double t; const char* file; };
    const Stage stages[] = {{0.0, "09_scene_stage1_healthy"},
                            {0.35, "10_scene_stage2_unease"},
                            {0.7, "11_scene_stage3_distorting"},
                            {1.0, "12_scene_stage4_shattered"}};
    for (int i = 0; i < 4; ++i) {
        Framebuffer canvas(CANVAS_W, CANVAS_H);
        renderScene(canvas, stages[i].t);
        char title[64], sub[96];
        std::snprintf(title, sizeof title, "STAGE %d: %s", i + 1, stageName(stages[i].t));
        std::snprintf(sub, sizeof sub, "DISTORTION = %.2f  -  ONE VALUE DRIVES EVERY ALGORITHM", stages[i].t);
        save(composePage(canvas, title, sub, "THE SCENE"), stages[i].file);
    }
    Framebuffer canvas(CANVAS_W, CANVAS_H);
    SceneOptions opt;
    opt.construction = true;
    renderScene(canvas, 0.7, opt);
    save(composePage(canvas, "CONSTRUCTION LINES",
                     "BEZIER CONTROL POINTS, SPLINE POINTS, PIVOTS, CLIP WINDOW (XOR OVERLAY)", "THE SCENE"),
         "13_scene_construction_lines");
    std::printf("Done.\n");
}
