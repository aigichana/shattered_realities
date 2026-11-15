#include "animation.h"
#include <algorithm>

double lerp(double a, double b, double t) { return a + (b - a) * t; }
Point lerpPoint(Point a, Point b, double t) { return {lerp(a.x, b.x, t), lerp(a.y, b.y, t)}; }
double clamp01(double v) { return std::max(0.0, std::min(1.0, v)); }

double ease(Ease e, double t) {
    t = clamp01(t);
    switch (e) {
        case Ease::Linear: return t;
        case Ease::In: return t * t;                          // slow start
        case Ease::Out: return 1 - (1 - t) * (1 - t);         // slow finish
        case Ease::InOut: return t * t * (3 - 2 * t);         // slow start and finish
    }
    return t;
}

const char* easeName(Ease e) {
    switch (e) {
        case Ease::Linear: return "LINEAR";
        case Ease::In: return "EASE IN";
        case Ease::Out: return "EASE OUT";
        case Ease::InOut: return "EASE IN-OUT";
    }
    return "?";
}

KeyframeTrack& KeyframeTrack::add(double time, double value, Ease e) {
    keys_.push_back({time, value, e});
    return *this;
}

double KeyframeTrack::sample(double time) const {
    if (keys_.empty()) return 0;
    if (time <= keys_.front().time) return keys_.front().value;
    if (time >= keys_.back().time) return keys_.back().value;
    // find the segment [k0, k1] that contains time
    size_t i = 0;
    while (i + 1 < keys_.size() && keys_[i + 1].time < time) ++i;
    const Keyframe& k0 = keys_[i];
    const Keyframe& k1 = keys_[i + 1];
    double t = (time - k0.time) / (k1.time - k0.time);  // 0 at k0, 1 at k1
    return lerp(k0.value, k1.value, ease(k0.ease, t));
}

void FrameAnimator::update(double dt) {
    accumulator += dt;
    const double frameTime = 1.0 / fps;
    while (accumulator >= frameTime) {  // may skip several poses after a long frame
        accumulator -= frameTime;
        current = (current + 1) % frames;
    }
}

FrameClock::FrameClock() : last_(std::chrono::steady_clock::now()) {}

double FrameClock::tick() {
    auto now = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(now - last_).count();
    last_ = now;
    dt = std::min(dt, 0.1);  // avoid a huge jump after the window was dragged or paused
    if (dt > 0) fps_ = fps_ == 0 ? 1 / dt : fps_ * 0.9 + (1 / dt) * 0.1;
    return dt;
}
