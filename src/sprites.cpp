// sprites.cpp - draws each pose of a walk cycle into one sprite sheet.
// Limbs are posed with rotation matrices about the hip / shoulder pivots.
#include "sprites.h"
#include "circles.h"
#include "lines.h"
#include "polygon.h"
#include "transform.h"
#include <algorithm>
#include <cmath>

namespace {

int I(double v) { return (int)std::lround(v); }

// One limb = two segments (upper + lower) hanging down from a pivot.
// upperAngle swings the whole limb, bend folds the lower segment at the joint.
void drawLimb(Framebuffer& fb, Point pivot, double len, double upperAngle, double bend, Color c,
              int thick) {
    Point joint = rotateAbout(upperAngle, pivot).apply({pivot.x, pivot.y + len});
    Point end = rotateAbout(upperAngle + bend, joint).apply({joint.x, joint.y + len});
    drawLineThick(fb, I(pivot.x), I(pivot.y), I(joint.x), I(joint.y), thick, c);
    drawLineThick(fb, I(joint.x), I(joint.y), I(end.x), I(end.y), thick, c);
    fillCircleMidpoint(fb, I(joint.x), I(joint.y), thick / 2, c);  // round the joint
}

void drawWalkerPose(Framebuffer& fb, int ox, double phase) {
    const Color body{14, 10, 20}, back{40, 30, 52}, eye{230, 40, 55};
    const double bob = -std::fabs(std::sin(phase)) * 2.5;  // body rises mid-stride
    const double swing = 0.5 * std::sin(phase);
    // Positive angles rotate a downward limb backwards (towards -x) for a right-facing figure.
    Point hip{ox + 24.0, 58 + bob}, shoulder{ox + 25.0, 31 + bob};

    // far-side limbs first (painter's algorithm), in a lighter shade for depth
    drawLimb(fb, hip, 17, -swing, 0.7 * std::max(0.0, std::sin(phase + 1.2)), back, 6);
    drawLimb(fb, shoulder, 13, swing * 0.9, -0.5, back, 5);

    Polygon torso = {{ox + 17.0, 27 + bob}, {ox + 32.0, 27 + bob}, {ox + 31.0, 60 + bob}, {ox + 18.0, 60 + bob}};
    torso = transformPolygon(torso, shear2D(0.08, 0) * translate2D(-2, 0));  // lean into the walk
    fillPolygonScanline(fb, torso, body);
    fillCircleMidpoint(fb, ox + 27, I(15 + bob), 10, body);                   // head
    fillCircleMidpoint(fb, ox + 31, I(14 + bob), 2, eye);                     // glowing eye

    // near-side limbs
    drawLimb(fb, hip, 17, swing, 0.7 * std::max(0.0, std::sin(-phase + 1.2)), body, 6);
    drawLimb(fb, shoulder, 13, -swing * 0.9, -0.5, body, 5);
}

}  // namespace

const SpriteSheet& shadowWalkerSheet() {
    static const SpriteSheet sheet = [] {
        const int fw = 48, fh = 96, frames = 8;
        Framebuffer fb(fw * frames, fh, SPRITE_KEY);
        for (int i = 0; i < frames; ++i) drawWalkerPose(fb, i * fw, 2 * M_PI * i / frames);
        Mask m = makeColorKeyMask(fb, SPRITE_KEY);
        return SpriteSheet{fb, m, fw, fh, frames};
    }();
    return sheet;
}
