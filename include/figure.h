// figure.h - a posable character built as a small hierarchy.
// The body matrix M = T(hip) * R(lean) * S(scale) places the torso; the neck, head
// and shoulders are defined in the torso's local coordinates, so they inherit every
// transformation of the torso. Arms are cubic Bezier curves; legs are two-segment
// limbs whose knee is solved from the hip and the planted foot.
#pragma once
#include "framebuffer.h"
#include "story.h"
#include <functional>
#include <vector>

struct Palette {
    Color skin, shirt, trousers, shoes, hair;
    bool longHair;
};

struct FigureFx {
    double distortion = 0;  // affects this figure's face and trembling
    double high = 0;
    double time = 0;
    std::function<Color(Color)> shade;  // scene lighting / colour decay
};

// Construction geometry reported back for the overlay.
struct FigureGuides {
    std::vector<std::vector<Point>> bezier;  // arm and mouth control points
    std::vector<Point> pivots;               // the hip: origin of the body matrix
};

void drawFigure(Framebuffer& fb, const ActorState& a, const Palette& pal, const FigureFx& fx,
                FigureGuides* guides = nullptr);
