// figure.cpp - draws one posed character with the project's own primitives.
#include "figure.h"
#include "circles.h"
#include "curves.h"
#include "lines.h"
#include "polygon.h"
#include "transform.h"
#include <algorithm>
#include <cmath>

namespace {

int I(double v) { return (int)std::lround(v); }

double hash01(unsigned a, unsigned b) {
    unsigned h = a * 2654435761u ^ (b + 0x9e3779b9u) * 40503u;
    h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; h *= 3266489917u; h ^= h >> 16;
    return (h & 0xFFFFFF) / double(0x1000000);
}
double noise(unsigned a, unsigned b) { return hash01(a, b) * 2.0 - 1.0; }

// Bezier control points of the screen-right arm for each key pose, in the torso's
// local coordinates (origin at the hip, y pointing down). The left arm mirrors x.
const Point ARM_POSES[5][4] = {
    {{30, -79}, {45, -53}, {45, -27}, {37, -1}},      // Relaxed: hanging at the side
    {{30, -79}, {58, -88}, {66, -128}, {54, -162}},   // Raised: above the head
    {{30, -79}, {62, -40}, {46, -92}, {8, -109}},     // ToMouth
    {{30, -79}, {70, -72}, {64, -138}, {27, -146}},   // Clutch: gripping the head
    {{30, -79}, {60, -40}, {28, -12}, {-8, -38}},     // Hug: across the stomach
};

// A limb segment as a filled quadrilateral around the line a-b.
void segment(Framebuffer& fb, Point a, Point b, double half, Color fill, Color edge) {
    double dx = b.x - a.x, dy = b.y - a.y, len = std::hypot(dx, dy);
    if (len < 1e-6) return;
    double nx = -dy / len * half, ny = dx / len * half;
    Polygon q = {{a.x + nx, a.y + ny}, {b.x + nx, b.y + ny}, {b.x - nx, b.y - ny}, {a.x - nx, a.y - ny}};
    fillPolygonScanline(fb, q, fill);
    drawPolygonOutline(fb, q, edge, 1);
}

}  // namespace

void drawFigure(Framebuffer& fb, const ActorState& a, const Palette& pal, const FigureFx& fx,
                FigureGuides* guides) {
    if (!a.visible) return;
    const double s = a.scale, t = fx.distortion, time = fx.time;
    auto C = [&](Color c) { return fx.shade ? fx.shade(c) : c; };
    const Color ink = C({25, 25, 38});
    auto R = [&](double v) { return std::max(1, I(v * s)); };  // scaled radius

    // ---------------- body matrix: everything above the hip hangs off this
    const double stride = std::sin(a.walkPhase);
    const double hipHeight = lerp(71, 24, a.crouch);
    const Point hip{a.x, a.groundY - hipHeight * s - a.lift - std::fabs(stride) * 2.5 * a.walkAmount * s};
    const double breath = std::sin(time * 2.2);
    const Mat3 body = translate2D(hip.x, hip.y) * rotate2D(a.lean) *
                      scale2D(s * (1 + 0.012 * breath), s * (1 + 0.02 * breath));
    auto L = [&](double lx, double ly) { return body.apply({lx, ly}); };
    auto toLocal = [&](Point w) {  // inverse of the body matrix
        double dx = w.x - hip.x, dy = w.y - hip.y, c = std::cos(a.lean), sn = std::sin(a.lean);
        return Point{(dx * c + dy * sn) / s, (-dx * sn + dy * c) / s};
    };
    if (guides) guides->pivots.push_back(hip);

    // ---------------- legs: foot is planted, knee solved from hip and ankle (two-bone IK)
    const double LEG = 31.5 * s;
    for (int side = -1; side <= 1; side += 2) {
        const Point joint = L(14.0 * side, -3);
        const double phase = a.walkPhase + (side > 0 ? M_PI : 0);
        Point stand{a.x + side * (17 + 14 * a.crouch) * s, a.groundY - 6 * s - a.lift};
        Point step{a.x + (a.facing != 0 ? a.facing : 1) * 15 * s * std::cos(phase) + side * 5 * s,
                   a.groundY - 6 * s - a.lift - std::max(0.0, std::sin(phase)) * 8 * s};
        const Point ankle = lerpPoint(stand, step, a.walkAmount);

        double dx = ankle.x - joint.x, dy = ankle.y - joint.y, d = std::hypot(dx, dy);
        Point knee{(joint.x + ankle.x) / 2, (joint.y + ankle.y) / 2};
        if (d > 1e-6 && d < 2 * LEG) {
            double off = std::sqrt(LEG * LEG - d * d / 4);  // how far the knee sticks out
            double nx = -dy / d, ny = dx / d;
            // bend outwards when standing or sitting, forwards when walking
            double want = lerp((double)side, (double)(a.facing != 0 ? a.facing : side), a.walkAmount);
            if (nx * want < 0) { nx = -nx; ny = -ny; }
            knee = {knee.x + nx * off, knee.y + ny * off};
        }
        const Color cloth = C(pal.trousers);
        segment(fb, joint, knee, 8 * s, cloth, ink);
        segment(fb, knee, ankle, 7 * s, cloth, ink);
        fillCircleMidpoint(fb, I(knee.x), I(knee.y), R(7), cloth);
        const int fx0 = I(ankle.x + a.facing * 5 * s), fy0 = I(ankle.y + 6 * s);
        fillEllipseMidpoint(fb, fx0, fy0, R(16), R(6), C(pal.shoes));
        drawEllipseMidpoint(fb, fx0, fy0, R(16), R(6), ink);
    }

    // ---------------- head position (needed first: long hair hangs behind the neck and shoulders)
    const double tremble = std::sin(time * 23) * t * t * 2.0 * s;
    Point hc = L(0, -127);
    hc.x += tremble + a.facing * 2 * s;
    hc.y += a.headDroop * s;
    if (pal.longHair) {
        fillEllipseMidpoint(fb, I(hc.x), I(hc.y + 10 * s), R(33), R(44), C(pal.hair));
        drawEllipseMidpoint(fb, I(hc.x), I(hc.y + 10 * s), R(33), R(44), ink);
    }

    // ---------------- torso and neck (local coordinates -> body matrix)
    const Polygon torso = {L(-30, -85), L(30, -85), L(37, 0), L(-37, 0)};
    fillPolygonScanline(fb, torso, C(pal.shirt));
    drawPolygonOutline(fb, torso, ink, 2);
    const Polygon neck = {L(-7, -104), L(7, -104), L(7, -83), L(-7, -83)};
    fillPolygonScanline(fb, neck, C(pal.skin));
    drawPolygonOutline(fb, neck, ink, 1);

    // ---------------- head
    fillEllipseMidpoint(fb, I(hc.x), I(hc.y), R(27), R(33), C(pal.skin));
    drawEllipseMidpoint(fb, I(hc.x), I(hc.y), R(27), R(33), ink);
    Polygon hair = {{-29, -8}, {-25, -32}, {0, -40}, {25, -32}, {29, -8}, {17, -22}, {0, -26}, {-17, -22}};
    hair = transformPolygon(hair, translate2D(hc.x, hc.y) * scale2D(s, s));
    fillPolygonScanline(fb, hair, C(pal.hair));
    drawPolygonOutline(fb, hair, ink, 1);

    // eyes: blink now and then; pupils dilate and the whites redden with the distortion
    const bool blink = std::fmod(time + pal.longHair * 1.3, 3.7) < 0.13;
    const double sad = clamp01(-a.mouth);
    for (int side = -1; side <= 1; side += 2) {
        const double ex = hc.x + side * 11 * s, ey = hc.y - 4 * s + (side > 0 ? 6 * t * s : 0);
        // eyebrow: the inner end rises when sad
        drawLineThick(fb, I(ex - side * 7 * s), I(ey - (12 + 4 * sad) * s), I(ex + side * 7 * s),
                      I(ey - (12 - 2 * sad) * s), s > 0.8 ? 2 : 1, ink);
        if (blink || a.eyesClosed > 0.5) {
            drawBezier(fb, {{ex - 7 * s, ey}, {ex, ey + 4 * s}, {ex + 7 * s, ey}}, ink, s > 0.8 ? 2 : 1, 12);
            continue;
        }
        const int rx = R(8), ry = R(5 + 4 * std::max(t, 0.6 * fx.high));
        fillEllipseMidpoint(fb, I(ex), I(ey), rx, ry, lerpColor(C({255, 255, 255}), {235, 170, 170}, t));
        drawEllipseMidpoint(fb, I(ex), I(ey), rx, ry, ink);
        fillCircleMidpoint(fb, I(ex + a.facing * 2 * s + noise(85, side + 1) * t * 2), I(ey),
                           R(2 + 4 * t + 3 * fx.high), ink);
        if (t > 0.4)  // bloodshot
            for (int r = 0; r < 4; ++r) {
                double ang = noise(86 + side, r) * 1.2 + (side > 0 ? 0 : M_PI);
                drawLineBresenham(fb, I(ex + std::cos(ang) * rx), I(ey + std::sin(ang) * ry * 0.5),
                                  I(ex + std::cos(ang) * (rx - 4 * s)), I(ey + std::sin(ang) * 2), {175, 25, 40});
            }
    }
    if (a.tear > 0) {  // a tear runs down the cheek, again and again
        double fall = std::fmod(time * 11, 26);
        fillEllipseMidpoint(fb, I(hc.x - 11 * s), I(hc.y + (4 + fall) * s), R(2), R(3), {120, 190, 255});
    }
    // mouth: a Bezier curve whose control points move from smile to frown
    {
        const double bend = 11 * a.mouth * s, y = hc.y + 17 * s;
        std::vector<Point> m = {{hc.x - 13 * s, y}, {hc.x - 6 * s, y + bend},
                                {hc.x + 6 * s, y + bend + noise(87, 0) * t * 6}, {hc.x + 13 * s, y}};
        drawBezier(fb, m, lerpColor(C({150, 50, 60}), ink, t), s > 0.8 ? 2 : 1, 30);
        if (guides) guides->bezier.push_back(m);
    }

    // ---------------- arms: interpolate the control points between two key poses
    for (int side = -1; side <= 1; side += 2) {
        const ArmState& arm = side < 0 ? a.armL : a.armR;
        const Point target = toLocal(side < 0 ? a.reachL : a.reachR);
        auto controls = [&](ArmPose p, int i) -> Point {
            if (p == ArmPose::Reach) {  // from the shoulder to a target, with a sagging elbow
                const Point sh{30.0 * side, -79};
                const double u[4] = {0, 0.33, 0.72, 1}, sagX[4] = {0, 8, 4, 0}, sagY[4] = {0, 16, 12, 0};
                return {sh.x + (target.x - sh.x) * u[i] + sagX[i] * side, sh.y + (target.y - sh.y) * u[i] + sagY[i]};
            }
            const Point& q = ARM_POSES[(int)p][i];
            return {q.x * side, q.y};
        };
        const double raised = (arm.from == ArmPose::Raised ? 1 - arm.mix : 0) + (arm.to == ArmPose::Raised ? arm.mix : 0);
        const double wiggle = std::sin(time * 8 + side) * a.wave * raised;
        std::vector<Point> c(4);
        for (int i = 0; i < 4; ++i) {
            Point l = lerpPoint(controls(arm.from, i), controls(arm.to, i), arm.mix);
            if (i >= 2) l.x += wiggle * (i == 3 ? 13 : 6);
            c[i] = L(l.x, l.y);
            if (i > 0) { c[i].x += noise(90, i) * t * 5 * s; c[i].y += noise(91, i) * t * 5 * s; }
        }
        drawBezier(fb, c, C(pal.shirt), std::max(3, I(11 * s)), 40);  // sleeve
        drawBezier(fb, c, ink, 1, 40);
        fillCircleMidpoint(fb, I(c[3].x), I(c[3].y), R(7), C(pal.skin));  // hand
        drawCircleMidpoint(fb, I(c[3].x), I(c[3].y), R(7), ink);
        if (a.pillInHand && side < 0) {
            fillEllipseMidpoint(fb, I(c[3].x), I(c[3].y - 8 * s), R(5), R(3), {250, 250, 250});
            drawEllipseMidpoint(fb, I(c[3].x), I(c[3].y - 8 * s), R(5), R(3), ink);
        }
        if (guides) guides->bezier.push_back(c);
    }
}
