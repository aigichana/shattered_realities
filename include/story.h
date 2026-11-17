// story.h - the storyboard of Shattered Realities as data.
// Every property that changes during the story (distortion, where the characters
// stand, how their arms are posed, the camera zoom, the captions...) is a keyframe
// track. sampleStory(time) reads all tracks at one moment, so the whole animation
// is a pure function of time: it can be paused, scrubbed and replayed exactly.
#pragma once
#include "animation.h"
#include "geometry.h"

// Key poses of an arm. An arm is a cubic Bezier curve; each pose is one set of
// four control points, and the in-betweens interpolate the control points.
enum class ArmPose { Relaxed, Raised, ToMouth, Clutch, Hug, Reach };

struct ArmState {
    ArmPose from = ArmPose::Relaxed, to = ArmPose::Relaxed;
    double mix = 0;  // 0 = from, 1 = to (already eased)
};

// Everything needed to pose one character at one moment.
struct ActorState {
    bool visible = false;
    double x = 0, groundY = 0;   // where the feet stand on the floor
    double scale = 1;            // smaller when further away (near the back wall)
    double walkPhase = 0;        // advances with the DISTANCE walked, so feet do not slide
    double walkAmount = 0;       // 0 = standing still, 1 = walking
    int facing = 0;              // -1 left, +1 right, 0 towards the viewer
    double crouch = 0;           // 0 = standing, 1 = sitting on the floor
    double lean = 0;             // torso rotation in radians
    double lift = 0;             // floating above the floor (euphoria)
    double headDroop = 0;        // pixels
    double mouth = 1;            // +1 = smile, -1 = frown
    double eyesClosed = 0;
    double wave = 0;             // waving amount for a raised arm
    double tear = 0;
    bool pillInHand = false;
    ArmState armL, armR;         // screen-left and screen-right arm
    Point reachL, reachR;        // world targets for ArmPose::Reach
};

struct StoryState {
    double time = 0;
    int act = 0;
    double distortion = 0;   // 0 = healthy reality, 1 = shattered
    double high = 0;         // euphoria: colour cycling, wobble, floating
    double sunAngle = 0;     // radians along the sky arc: 0 = sunrise, pi = sunset, then night
    double doorOpen = 0;
    double shadows = 0;      // opacity of the shadow walkers
    double crack = 0;        // how far the cracks have spread
    double glitch = 1;       // strength of the tearing bands (they calm down when help arrives)
    double flash = 0, shake = 0, spotlight = 0, fade = 0;
    double zoom = 1;         // camera
    Point focus{320, 240};
    double pills = 0;        // pills lying around
    double bottleGlow = 0;
    double bottleAlpha = 1;  // the bottle fades away during the recovery
    double titleAlpha = 0, endAlpha = 0;
    const char* caption = "";
    double captionAlpha = 0;
    ActorState hero, helper;
};

StoryState sampleStory(double time);
double storyDuration();

int actCount();
const char* actTitle(int act);
double actStart(int act);

// The main track, shown in the viewer's timeline.
const KeyframeTrack& distortionTrack();
