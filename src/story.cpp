// story.cpp - the keyframes of the story. Seven acts, about 100 seconds:
//   1 an ordinary day   2 just once   3 the high   4 one more (dependence)
//   5 shattered         6 rock bottom 7 a hand reaches out (recovery)
#include "story.h"
#include <algorithm>
#include <cmath>

namespace {

const double DURATION = 101;
const double PI = 3.14159265358979323846;

struct Act { double start; const char* title; };
const Act ACTS[] = {{0, "AN ORDINARY DAY"}, {12.5, "JUST ONCE"},   {22.5, "THE HIGH"},
                    {37.5, "ONE MORE"},     {58.5, "SHATTERED"},   {72, "ROCK BOTTOM"},
                    {80, "A HAND REACHES OUT"}};
const int ACT_COUNT = sizeof ACTS / sizeof ACTS[0];

struct Caption { double t0, t1; const char* text; };
const Caption CAPTIONS[] = {
    {9.4, 12.6, "AN ORDINARY ROOM. AN ORDINARY LIFE."},
    {13, 17.2, "THEN ONE DAY, A SMALL BOTTLE."},
    {17.6, 22.4, "JUST ONCE. WHAT COULD GO WRONG?"},
    {25.5, 31.5, "FOR A MOMENT, EVERYTHING GLOWS."},
    {33, 37.4, "BUT THE GLOW NEVER LASTS."},
    {39, 44.6, "THE BODY STARTS ASKING FOR MORE."},
    {46.5, 51, "AND MORE."},
    {52.5, 58, "DAYS BLUR. NOTHING ELSE MATTERS."},
    {63, 67.6, "UNTIL REALITY SHATTERS."},
    {68, 71.8, "THE SHADOWS FEEL REAL."},
    {74, 79.6, "ROCK BOTTOM. ALONE IN THE DARK."},
    {81, 86, "BUT THE DOOR WAS NEVER LOCKED."},
    {88, 94.6, "RECOVERY BEGINS WITH ASKING FOR HELP."},
};

// Keyframes of an arm: each key names a pose; between two keys the control
// points of the Bezier arm are interpolated (pose-to-pose animation).
struct PoseTrack {
    struct Key { double t; ArmPose pose; };
    std::vector<Key> keys;
    PoseTrack& add(double t, ArmPose p) {
        keys.push_back({t, p});
        return *this;
    }
    ArmState sample(double t) const {
        if (t <= keys.front().t) return {keys.front().pose, keys.front().pose, 0};
        if (t >= keys.back().t) return {keys.back().pose, keys.back().pose, 0};
        size_t i = 0;
        while (i + 1 < keys.size() && keys[i + 1].t < t) ++i;
        double u = (t - keys[i].t) / (keys[i + 1].t - keys[i].t);
        return {keys[i].pose, keys[i + 1].pose, easeInOut(u)};
    }
};

// A walking path: keyframed floor position, plus the distance walked so far
// (integrated once), which drives the walk cycle.
struct Path {
    KeyframeTrack x, gy;
    std::vector<double> dist;
    static constexpr double STEP = 1.0 / 60;
    void bake() {
        double d = 0;
        Point prev{x.sample(0), gy.sample(0)};
        for (double t = 0; t <= DURATION + 1; t += STEP) {
            Point p{x.sample(t), gy.sample(t)};
            d += std::hypot(p.x - prev.x, p.y - prev.y);
            dist.push_back(d);
            prev = p;
        }
    }
    double distance(double t) const {
        double f = std::clamp(t, 0.0, DURATION) / STEP;
        size_t i = std::min((size_t)f, dist.size() - 2);
        return lerp(dist[i], dist[i + 1], f - (double)i);
    }
    void pose(double t, ActorState& a) const {
        a.x = x.sample(t);
        a.groundY = gy.sample(t);
        a.scale = lerp(0.6, 1.0, clamp01((a.groundY - 300) / 146));  // further away = smaller
        a.walkPhase = distance(t) / 11.0;
        a.walkAmount = clamp01((distance(t + 0.05) - distance(t - 0.05)) / 0.1 / 25.0);
        double dx = x.sample(t + 0.05) - x.sample(t - 0.05);
        a.facing = dx > 0.25 ? 1 : dx < -0.25 ? -1 : 0;
    }
};

struct Timeline {
    KeyframeTrack distortion, high, sun, door, shadows, crack, flash, shake, spot, fade, zoom, focusX,
        focusY, pills, glow, bottle, glitch, title, end;
    KeyframeTrack crouch, lean, droop, mouth, closed, wave, helperWave, helperMouth;
    Path hero, helper;
    PoseTrack heroL, heroR, helperL, helperR;

    Timeline() {
        using enum ArmPose;
        const Ease LIN = Ease::Linear, OUT = Ease::Out;

        // ---- the world
        distortion.add(0, 0).add(20, 0).add(24, 0.08).add(36, 0.1).add(40, 0.25).add(47, 0.38)
            .add(53, 0.52).add(58, 0.66).add(61.8, 0.78, Ease::In).add(62.4, 1).add(87.5, 1)
            .add(91, 0.3).add(95, 0).add(DURATION, 0);
        high.add(20.3, 0).add(22, 0.25).add(25.5, 1).add(33.5, 1).add(37.5, 0).add(44, 0)
            .add(44.8, 0.45).add(46.8, 0).add(51.6, 0).add(52.2, 0.25).add(53.6, 0);
        // morning -> afternoon, then three days rush past, a long night, and a new morning
        sun.add(0, 0.30 * PI, LIN).add(20, 0.42 * PI, LIN).add(37, 0.62 * PI).add(58, 7.30 * PI, LIN)
            .add(87.5, 7.5 * PI).add(91.5, 8.25 * PI, LIN).add(DURATION, 8.35 * PI);
        door.add(3.0, 0).add(4.2, 1).add(7.8, 1).add(8.8, 0).add(80, 0).add(81.3, 1);
        shadows.add(59, 0).add(62.5, 1).add(87.5, 1).add(90, 0);
        crack.add(62.3, 0, OUT).add(62.9, 1).add(88, 1).add(91, 0);
        flash.add(62.25, 0, LIN).add(62.4, 1, OUT).add(63.2, 0);
        shake.add(62.3, 0, LIN).add(62.45, 1, OUT).add(64, 0.15).add(72, 0.1).add(74, 0);
        spot.add(72, 0).add(75, 1).add(80.5, 1).add(84.5, 0);
        fade.add(0, 1).add(1.5, 0).add(99, 0).add(100.5, 1);
        zoom.add(14.5, 1).add(17, 1.45).add(20.5, 1.45).add(23.5, 1).add(62.2, 1, OUT).add(63, 1.25)
            .add(71, 1.3).add(74, 1.15).add(81, 1.15).add(84, 1);
        focusX.add(24, 250).add(60, 335);
        focusY.add(24, 290).add(60, 330);
        pills.add(43, 0).add(44, 2).add(50.6, 2).add(51.4, 4).add(55.8, 4).add(56.6, 7).add(61.5, 7).add(62.6, 9).add(90.5, 9).add(93.5, 0);
        glow.add(12.6, 0).add(13.6, 1).add(16.8, 1).add(17.6, 0);
        bottle.add(91.5, 1).add(93.5, 0);
        glitch.add(72, 1).add(75, 0.5).add(81, 0.5).add(84.5, 0.12);
        title.add(0.3, 0).add(1.2, 1).add(3.2, 1).add(4.2, 0);
        end.add(95.5, 0).add(96.6, 1);

        // ---- the main character: enters through the door, then walks between
        // the middle of the room (x = 335) and the table (x = 296)
        hero.x.add(4.4, 445).add(7.8, 335).add(13.5, 335).add(16.5, 296).add(24, 296).add(27, 335)
            .add(40, 335).add(42, 296).add(45, 296).add(47, 360).add(48.2, 360).add(50, 296)
            .add(52, 296).add(53.5, 400).add(53.6, 400).add(55.2, 296).add(59, 296).add(61, 335);
        hero.gy.add(4.4, 300).add(7.8, 446);
        hero.bake();

        crouch.add(62.5, 0).add(68, 0.45).add(72, 0.5).add(74.5, 1).add(87.8, 1).add(90.8, 0);
        lean.add(36, 0).add(39, 0.07).add(61, 0.1).add(74.5, 0.16).add(87, 0.1).add(91, 0);
        droop.add(37, 0).add(39.5, 8).add(62, 10).add(74.5, 20).add(86, 20).add(87.2, 4).add(91, 0);
        mouth.add(37, 1).add(39, -0.4).add(44, -0.4).add(44.6, 0.5).add(46.5, -0.6).add(51.6, -0.6)
            .add(52, 0.2).add(53.4, -0.8).add(58, -1).add(87, -1).add(88.5, -0.2).add(92, 1);
        closed.add(19.3, 0).add(19.5, 1).add(20.3, 1).add(20.5, 0).add(74.6, 0).add(75.2, 1).add(86, 1).add(86.4, 0);
        wave.add(8.6, 0).add(9.0, 1).add(10.4, 1).add(10.8, 0).add(93.4, 0).add(93.8, 1).add(96.5, 1);

        // screen-left arm: the one that reaches for the bottle
        heroL.add(0, Relaxed).add(16.3, Relaxed).add(17.3, Reach).add(17.8, Reach).add(18.8, ToMouth)
            .add(19.6, ToMouth).add(20.6, Relaxed).add(25, Relaxed).add(26.5, Raised).add(34, Raised)
            .add(36.5, Relaxed).add(42, Relaxed).add(42.6, Reach).add(43, Reach).add(43.7, ToMouth)
            .add(44.3, ToMouth).add(45, Relaxed).add(49.8, Relaxed).add(50.4, Reach).add(50.7, Reach)
            .add(51.3, ToMouth).add(51.8, ToMouth).add(52.3, Relaxed).add(55, Relaxed).add(55.6, Reach)
            .add(55.9, Reach).add(56.5, ToMouth).add(57.3, ToMouth).add(58, Relaxed).add(59.5, Relaxed)
            .add(61, Clutch).add(70, Clutch).add(73.5, Hug).add(91, Hug).add(92.5, Relaxed)
            .add(93.2, Relaxed).add(93.9, Raised).add(DURATION, Raised);
        // screen-right arm: waves hello, holds the stomach, takes the helper's hand
        heroR.add(0, Relaxed).add(8.3, Relaxed).add(8.9, Raised).add(10.6, Raised).add(11.4, Relaxed)
            .add(25, Relaxed).add(26.5, Raised).add(34, Raised).add(36.5, Relaxed).add(38.5, Hug)
            .add(59.5, Hug).add(61, Clutch).add(70, Clutch).add(73.5, Hug).add(86.2, Hug)
            .add(87.5, Reach).add(91, Reach).add(92.5, Relaxed);

        // ---- the helper: appears in the doorway and walks over
        helper.x.add(82.5, 445).add(86.5, 414);
        helper.gy.add(82.5, 300).add(86.5, 446);
        helper.bake();
        helperL.add(0, Relaxed).add(86.2, Relaxed).add(87.4, Reach).add(DURATION, Reach);
        helperR.add(0, Relaxed).add(93.2, Relaxed).add(93.9, Raised).add(DURATION, Raised);
        helperWave.add(93.4, 0).add(93.8, 1);
        helperMouth.add(84, 0.2).add(91, 1);
    }
};

const Timeline& TL() {
    static const Timeline t;
    return t;
}

bool within(double t, double a, double b) { return t >= a && t <= b; }

}  // namespace

double storyDuration() { return DURATION; }
int actCount() { return ACT_COUNT; }
const char* actTitle(int act) { return ACTS[std::clamp(act, 0, ACT_COUNT - 1)].title; }
double actStart(int act) { return ACTS[std::clamp(act, 0, ACT_COUNT - 1)].start; }
const KeyframeTrack& distortionTrack() { return TL().distortion; }

StoryState sampleStory(double t) {
    const Timeline& L = TL();
    t = std::clamp(t, 0.0, DURATION);
    StoryState s;
    s.time = t;
    for (int i = 0; i < ACT_COUNT; ++i)
        if (t >= ACTS[i].start) s.act = i;

    s.distortion = L.distortion.sample(t);
    s.high = L.high.sample(t);
    s.sunAngle = L.sun.sample(t);
    s.doorOpen = L.door.sample(t);
    s.shadows = L.shadows.sample(t);
    s.crack = L.crack.sample(t);
    s.glitch = L.glitch.sample(t);
    s.flash = L.flash.sample(t);
    s.shake = L.shake.sample(t);
    s.spotlight = L.spot.sample(t);
    s.fade = L.fade.sample(t);
    s.zoom = L.zoom.sample(t);
    s.focus = {L.focusX.sample(t), L.focusY.sample(t)};
    s.pills = L.pills.sample(t);
    s.bottleGlow = L.glow.sample(t);
    s.bottleAlpha = L.bottle.sample(t);
    s.titleAlpha = L.title.sample(t);
    s.endAlpha = L.end.sample(t);
    for (const Caption& c : CAPTIONS)
        if (within(t, c.t0, c.t1)) {
            s.caption = c.text;
            s.captionAlpha = clamp01(std::min(t - c.t0, c.t1 - t) / 0.6);  // fade in and out
        }

    // ---- main character
    ActorState& h = s.hero;
    h.visible = t >= 3.0;
    L.hero.pose(t, h);
    h.crouch = L.crouch.sample(t);
    h.lean = L.lean.sample(t) + 0.12 * s.high * std::sin(t * 2.6);  // sways while high
    h.lift = s.high * (8 + 5 * std::sin(t * 2.2));
    h.headDroop = L.droop.sample(t);
    h.mouth = L.mouth.sample(t);
    h.eyesClosed = L.closed.sample(t);
    h.wave = std::max(L.wave.sample(t), 0.7 * s.high);  // waves hello; arms dance while high
    h.tear = within(t, 75.5, 86) ? 1 : 0;
    h.armL = L.heroL.sample(t);
    h.armR = L.heroR.sample(t);
    h.pillInHand = within(t, 17.8, 19.2) || within(t, 43, 44.1) || within(t, 50.7, 51.6) || within(t, 55.9, 56.9);
    h.reachL = {191, 304};  // the bottle on the table

    // ---- helper
    ActorState& p = s.helper;
    p.visible = t >= 80.4;
    L.helper.pose(t, p);
    p.mouth = L.helperMouth.sample(t);
    p.wave = L.helperWave.sample(t);
    p.armL = L.helperL.sample(t);
    p.armR = L.helperR.sample(t);

    // The two hands meet between the characters and rise as the hero stands up.
    // Afterwards the helper's hand moves to the hero's shoulder.
    Point meet{378, lerp(322, 350, h.crouch)};
    h.reachR = meet;
    p.reachL = lerpPoint(meet, {364, 300}, easeInOut((t - 91) / 1.4));
    return s;
}
